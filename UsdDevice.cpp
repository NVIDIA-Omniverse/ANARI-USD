// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdDevice.h"
#include "UsdBridgedBaseObject.h"
#include "UsdDataArray.h"
#include "UsdGeometry.h"
#include "UsdSpatialField.h"
#include "UsdSurface.h"
#include "UsdVolume.h"
#include "UsdInstance.h"
#include "UsdGroup.h"
#include "UsdMaterial.h"
#include "UsdSampler.h"
#include "UsdWorld.h"
#include "UsdRenderer.h"
#include "UsdFrame.h"
#include "UsdLight.h"
#include "UsdCamera.h"
#include "UsdDevice_queries.h"

#include "UsdBridge/Common/UsdBridgeParallelController.h"

#include <cstdarg>
#include <cstdio>
#include <set>
#include <memory>
#include <sstream>
#include <algorithm>
#include <limits>

#ifdef USD_DEVICE_MPI_ENABLED
#include "UsdMpiController.h"
#endif

static char deviceName[] = "usd";

class UsdDeviceInternals
{
public:
  UsdDeviceInternals()
  {
  }

  bool CreateNewBridge(const UsdDeviceData& deviceParams, UsdBridgeLogCallback bridgeStatusFunc, void* userData)
  {
    if (bridge.get())
      bridge->CloseSession();

    UsdBridgeSettings bridgeSettings = {
      UsdSharedString::c_str(deviceParams.hostName),
      outputLocation.c_str(),
      deviceParams.createNewSession,
      deviceParams.outputBinary,
      deviceParams.outputPreviewSurfaceShader,
      deviceParams.outputMdlShader,
      deviceParams.useDisplayColorOpacity
    };

    if(mpiController)
    {
      bridgeSettings.MpiRank = mpiController->GetRank();
      bridgeSettings.MpiSize = mpiController->GetSize();
      bridgeSettings.ParallelController = mpiController.get();
    }

    bridge = std::make_unique<UsdBridge>(bridgeSettings);

    bridge->SetExternalSceneStage(externalSceneStage);
    bridge->SetEnableSaving(this->enableSaving);

    bridgeStatusFunc(UsdBridgeLogLevel::STATUS, userData, "Initializing UsdBridge Session");

    bool createSuccess = bridge->OpenSession(bridgeStatusFunc, userData);

    if (!createSuccess)
    {
      bridge = nullptr;
      bridgeStatusFunc(UsdBridgeLogLevel::STATUS, userData, "UsdBridge Session initialization failed.");
    }
    else
    {
      bridgeStatusFunc(UsdBridgeLogLevel::STATUS, userData, "UsdBridge Session initialization successful.");
    }

    return createSuccess;
  }

  std::string outputLocation;
  bool enableSaving = true;
  std::unique_ptr<UsdBridge> bridge;
  SceneStagePtr externalSceneStage{nullptr};

  // MPI parallel support (KHR_DATA_PARALLEL_MPI)
  std::unique_ptr<UsdBridgeParallelController> mpiController;

  std::set<std::string> uniqueNames;
};

//---- Make sure to update clearDeviceParameters() on refcounted objects
DEFINE_PARAMETER_MAP(UsdDevice,
  REGISTER_PARAMETER_MACRO("usd::serialize.hostName", ANARI_STRING, hostName)
  REGISTER_PARAMETER_MACRO("usd::serialize.location", ANARI_STRING, outputPath)
  REGISTER_PARAMETER_MACRO("usd::serialize.newSession", ANARI_BOOL, createNewSession)
  REGISTER_PARAMETER_MACRO("usd::serialize.outputBinary", ANARI_BOOL, outputBinary)
  REGISTER_PARAMETER_MACRO("usd::time", ANARI_FLOAT64, timeStep)
  REGISTER_PARAMETER_MACRO("usd::writeAtCommit", ANARI_BOOL, writeAtCommit)
  REGISTER_PARAMETER_MACRO("usd::output.material", ANARI_BOOL, outputMaterial)
  REGISTER_PARAMETER_MACRO("usd::output.previewSurfaceShader", ANARI_BOOL, outputPreviewSurfaceShader)
  REGISTER_PARAMETER_MACRO("usd::output.mdlShader", ANARI_BOOL, outputMdlShader)
  REGISTER_PARAMETER_MACRO("usd::output.displayColorOpacity", ANARI_BOOL, useDisplayColorOpacity)
)

void UsdDevice::clearDeviceParameters()
{
  filterResetParam("usd::serialize.hostName");
  filterResetParam("usd::serialize.location");
  transferWriteToReadParams();
}
//----

UsdDevice::UsdDevice()
  : UsdParameterizedBaseObject<UsdDevice, UsdDeviceData>(ANARI_DEVICE)
  , internals(std::make_unique<UsdDeviceInternals>())
{}

UsdDevice::UsdDevice(ANARILibrary library)
  : DeviceImpl(library)
  , UsdParameterizedBaseObject<UsdDevice, UsdDeviceData>(ANARI_DEVICE)
  , internals(std::make_unique<UsdDeviceInternals>())
{}

UsdDevice::~UsdDevice()
{
  // Make sure no more references are held before cleaning up the device (and checking for memleaks)
  clearCommitList(); 

  clearDeviceParameters(); // Release device parameters with object references

  clearResourceStringList(); // Do the same for resource string references

  //internals->bridge->SaveScene(); //Uncomment to test cleanup of usd files.

#ifdef CHECK_MEMLEAKS
  if(!allocatedObjects.empty() || !allocatedStrings.empty() || !allocatedRawMemory.empty())
  {
    std::stringstream errstream;
    errstream << "USD Device memleak reported for: ";
    for(auto ptr : allocatedObjects)
      errstream << "Object ptr: 0x" << std::hex << ptr << " of type: " << std::dec << ptr->getType() << "; ";
    for(auto ptr : allocatedStrings)
      errstream << "String ptr: 0x" << std::hex << ptr << " with content: " << std::dec << ptr->c_str() << "; ";
    for(auto ptr : allocatedRawMemory)
      errstream << "Raw ptr: 0x" << std::hex << ptr << std::dec << "; ";

    reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_OPERATION, errstream.str().c_str());
  }
  else
  {
    reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_INFO, ANARI_STATUS_NO_ERROR, "Reference memleak check complete, no issues found.");
  }
  //assert(allocatedObjects.empty());
#endif
}

void UsdDevice::reportStatus(void* source,
  ANARIDataType sourceType,
  ANARIStatusSeverity severity,
  ANARIStatusCode statusCode,
  const char *format, ...)
{
  va_list arglist;

  va_start(arglist, format);
  reportStatus(source, sourceType, severity, statusCode, format, arglist);
  va_end(arglist);
}

static void reportBridgeStatus(UsdBridgeLogLevel level, void* device, const char *message)
{
  ANARIStatusSeverity severity = UsdBridgeLogLevelToAnariSeverity(level);

  ((UsdDevice*)device)->reportStatus(nullptr, ANARI_UNKNOWN, severity, ANARI_STATUS_NO_ERROR, "%s", message);
}

void UsdDevice::reportStatus(void* source,
  ANARIDataType sourceType,
  ANARIStatusSeverity severity,
  ANARIStatusCode statusCode,
  const char *format,
  va_list& arglist)
{
  va_list arglist_copy;
  va_copy(arglist_copy, arglist);
  int count = std::vsnprintf(nullptr, 0, format, arglist);

  lastStatusMessage.resize(count + 1);

  std::vsnprintf(lastStatusMessage.data(), count + 1, format, arglist_copy);
  va_end(arglist_copy);

  if (statusFunc != nullptr)
  {
    statusFunc(
      statusUserData,
      (ANARIDevice)this,
      (ANARIObject)source,
      sourceType,
      severity,
      statusCode,
      lastStatusMessage.data());
  }
}

void UsdDevice::filterSetParam(
  const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  if (strEquals(name, "usd::garbageCollect"))
  {
    // Perform garbage collection on usd objects (needs to move into the user interface)
    if(internals->bridge)
      internals->bridge->GarbageCollect();
  }
  else if(strEquals(name, "usd::removeUnusedNames"))
  {
    internals->uniqueNames.clear();
  }
  else if (strEquals(name, "usd::connection.logVerbosity")) // 0 <= verbosity <= USDBRIDGE_MAX_LOG_VERBOSITY, with USDBRIDGE_MAX_LOG_VERBOSITY being the loudest
  {
    if(type == ANARI_INT32)
      UsdBridge::SetConnectionLogVerbosity(*(reinterpret_cast<const int*>(mem)));
  }
  else if(strEquals(name, "usd::sceneStage"))
  {
    if(type == ANARI_VOID_POINTER)
      internals->externalSceneStage = const_cast<void *>(mem);
  }
  else if(strEquals(name, "mpiCommunicator"))
  {
    if(type == ANARI_VOID_POINTER)
    {
#ifdef USD_DEVICE_MPI_ENABLED
      if(mem)
        internals->mpiController = std::make_unique<UsdMpiController>(mem);
      else
        internals->mpiController.reset();
#else
      reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_WARNING, ANARI_STATUS_INVALID_OPERATION,
        "mpiCommunicator parameter set but USD device was not built with MPI support (USD_DEVICE_MPI_ENABLED)");
#endif
    }
  }
  else if (strEquals(name, "usd::enableSaving"))
  {
    if(type == ANARI_BOOL)
    {
      internals->enableSaving = *(reinterpret_cast<const bool*>(mem));
      if(internals->bridge)
        internals->bridge->SetEnableSaving(internals->enableSaving);
    }
  }
  else if (strEquals(name, "statusCallback") && type == ANARI_STATUS_CALLBACK)
  {
    userSetStatusFunc = (ANARIStatusCallback)mem;
  }
  else if (strEquals(name, "statusCallbackUserData") && type == ANARI_VOID_POINTER)
  {
    userSetStatusUserData = const_cast<void *>(mem);
  }
  else
  {
    setParam(name, type, mem, this);
  }
}

void UsdDevice::filterResetParam(const char * name)
{
  if (strEquals(name, "statusCallback"))
  {
    userSetStatusFunc = nullptr;
  }
  else if (strEquals(name, "statusCallbackUserData"))
  {
    userSetStatusUserData = nullptr;
  }
  else if (strEquals(name, "mpiCommunicator"))
  {
    internals->mpiController.reset();
  }
  else if (!strEquals(name, "usd::garbageCollect")
    && !strEquals(name, "usd::removeUnusedNames"))
  {
    resetParam(name);
  }
}

void UsdDevice::commit(UsdDevice* device)
{
  transferWriteToReadParams();

  if(!bridgeInitAttempt)
  {
    initializeBridge();
  }

  const UsdDeviceData& paramData = getReadParams();
  internals->bridge->UpdateBeginEndTime(paramData.timeStep);
}

void UsdDevice::initializeBridge()
{
  const UsdDeviceData& paramData = getReadParams();

  bridgeInitAttempt = true;

  statusFunc = userSetStatusFunc ? userSetStatusFunc : defaultStatusCallback();
  statusUserData = userSetStatusUserData ? userSetStatusUserData : defaultStatusCallbackUserPtr();

  if(paramData.outputPath)
    internals->outputLocation = paramData.outputPath->c_str();

  if(internals->outputLocation.empty()) {
    auto *envLocation = getenv("ANARI_USD_SERIALIZE_LOCATION");
    if (envLocation) {
      internals->outputLocation = envLocation;
      reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_INFO, ANARI_STATUS_NO_ERROR,
        "Usd Device parameter 'usd::serialize.location' using ANARI_USD_SERIALIZE_LOCATION value");
    }
  }

  if (internals->outputLocation.empty())
  {
    reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_WARNING, ANARI_STATUS_INVALID_ARGUMENT,
      "Usd Device parameter 'usd::serialize.location' not set, defaulting to './'");
    internals->outputLocation = "./";
  }

  if(internals->mpiController)
  {
    reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_INFO, ANARI_STATUS_NO_ERROR,
      "MPI rank %d/%d: output to '%s'",
      internals->mpiController->GetRank(), internals->mpiController->GetSize(),
      internals->outputLocation.c_str());
  }

  if (!internals->CreateNewBridge(paramData, &reportBridgeStatus, this))
  {
    reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_ERROR, ANARI_STATUS_UNKNOWN_ERROR, "Usd Bridge failed to load");
  }
}

ANARIArray UsdDevice::CreateDataArray(const void *appMemory,
  ANARIMemoryDeleter deleter,
  const void *userData,
  ANARIDataType dataType,
  uint64_t numItems1,
  int64_t byteStride1,
  uint64_t numItems2,
  int64_t byteStride2,
  uint64_t numItems3,
  int64_t byteStride3)
{
  if (!appMemory)
  {
    UsdDataArray* object = new UsdDataArray(dataType, numItems1, numItems2, numItems3, this);
#ifdef CHECK_MEMLEAKS
    logObjAllocation(object);
#endif

    return (ANARIArray)(object);
  }
  else
  {
    UsdDataArray* object = new UsdDataArray(appMemory, deleter, userData,
      dataType, numItems1, byteStride1, numItems2, byteStride2, numItems3, byteStride3,
      this);
#ifdef CHECK_MEMLEAKS
    logObjAllocation(object);
#endif

    return (ANARIArray)(object);
  }
}

ANARIArray1D UsdDevice::newArray1D(const void *appMemory,
  ANARIMemoryDeleter deleter,
  const void *userData,
  ANARIDataType type,
  uint64_t numItems)
{
  return (ANARIArray1D)CreateDataArray(appMemory, deleter, userData,
    type, numItems, 0, 1, 0, 1, 0);
}

ANARIArray2D UsdDevice::newArray2D(const void *appMemory,
  ANARIMemoryDeleter deleter,
  const void *userData,
  ANARIDataType type,
  uint64_t numItems1,
  uint64_t numItems2)
{
  return (ANARIArray2D)CreateDataArray(appMemory, deleter, userData,
    type, numItems1, 0, numItems2, 0, 1, 0);
}

ANARIArray3D UsdDevice::newArray3D(const void *appMemory,
  ANARIMemoryDeleter deleter,
  const void *userData,
  ANARIDataType type,
  uint64_t numItems1,
  uint64_t numItems2,
  uint64_t numItems3)
{
  return (ANARIArray3D)CreateDataArray(appMemory, deleter, userData,
    type, numItems1, 0, numItems2, 0, numItems3, 0);
}

void * UsdDevice::mapArray(ANARIArray array)
{
  return array ? AnariToUsdObjectPtr(array)->map(this) : nullptr;
}

void UsdDevice::unmapArray(ANARIArray array)
{
  if(array)
    AnariToUsdObjectPtr(array)->unmap(this);
}

ANARISampler UsdDevice::newSampler(const char *type)
{
  const char* name = makeUniqueName("Sampler");
  UsdSampler* object = new UsdSampler(name, type, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARISampler)(object);
}

ANARIMaterial UsdDevice::newMaterial(const char *material_type)
{
  const char* name = makeUniqueName("Material");
  UsdMaterial* object = new UsdMaterial(name, material_type, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARIMaterial)(object);
}

ANARIGeometry UsdDevice::newGeometry(const char *type)
{
  const char* name = makeUniqueName("Geometry");
  UsdGeometry* object = new UsdGeometry(name, type, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARIGeometry)(object);
}

ANARISpatialField UsdDevice::newSpatialField(const char * type)
{
  const char* name = makeUniqueName("SpatialField");
  UsdSpatialField* object = new UsdSpatialField(name, type, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARISpatialField)(object);
}

ANARISurface UsdDevice::newSurface()
{
  const char* name = makeUniqueName("Surface");
  UsdSurface* object = new UsdSurface(name, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARISurface)(object);
}

ANARIVolume UsdDevice::newVolume(const char *type)
{
  const char* name = makeUniqueName("Volume");
  UsdVolume* object = new UsdVolume(name, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARIVolume)(object);
}

ANARIGroup UsdDevice::newGroup()
{
  const char* name = makeUniqueName("Group");
  UsdGroup* object = new UsdGroup(name, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARIGroup)(object);
}

ANARIInstance UsdDevice::newInstance(const char */*type*/)
{
  const char* name = makeUniqueName("Instance");
  UsdInstance* object = new UsdInstance(name, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARIInstance)(object);
}

ANARIWorld UsdDevice::newWorld()
{
  const char* name = makeUniqueName("World");
  UsdWorld* object = new UsdWorld(name, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARIWorld)(object);
}

ANARILight UsdDevice::newLight(const char *type)
{
  const char* name = makeUniqueName("Light");
  UsdLight* object = new UsdLight(name, type, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARILight)(object);
}

ANARICamera UsdDevice::newCamera(const char *type)
{
  const char* name = makeUniqueName("Camera");
  UsdCamera* object = new UsdCamera(name, type, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  ANARICamera returnValue = (ANARICamera)(object);

  return returnValue;
}

ANARIObject UsdDevice::newObject(const char *objectType, const char *type)
{
  return nullptr;
}

void (*UsdDevice::getProcAddress(const char *name))(void)
{
  return nullptr;
}

const char **UsdDevice::getObjectSubtypes(ANARIDataType objectType)
{
  return anari::usd::query_object_types(objectType);
}

const void *UsdDevice::getObjectInfo(ANARIDataType objectType,
    const char *objectSubtype,
    const char *infoName,
    ANARIDataType infoType)
{
  return anari::usd::query_object_info(
      objectType, objectSubtype, infoName, infoType);
}

const void *UsdDevice::getParameterInfo(ANARIDataType objectType,
    const char *objectSubtype,
    const char *parameterName,
    ANARIDataType parameterType,
    const char *infoName,
    ANARIDataType infoType)
{
  return anari::usd::query_param_info(objectType,
      objectSubtype,
      parameterName,
      parameterType,
      infoName,
      infoType);
}


ANARIRenderer UsdDevice::newRenderer(const char *type)
{
  UsdRenderer* object = new UsdRenderer();
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARIRenderer)(object);
}

UsdBridge* UsdDevice::getUsdBridge()
{
  return internals->bridge.get();
}

void UsdDevice::renderFrame(ANARIFrame frame)
{
  // Always commit device changes if not initialized, otherwise no conversion can be performed.
  if(!bridgeInitAttempt)
    initializeBridge();

  if(!isInitialized())
    return;

  flushCommitList();

  internals->bridge->ResetResourceUpdateState(); // Reset the modified flags for committed shared resources

  if(frame)
  {
    UsdFrame* frameObjPtr = AnariToUsdObjectPtr(frame);
    frameObjPtr->saveUsd(this);
    frameObjPtr->renderFrame(this);
  }
}

int UsdDevice::frameReady(ANARIFrame frame, ANARIWaitMask mask)
{
  if(!isInitialized())
    return 1;

  if(frame)
  {
    UsdFrame* frameObjPtr = AnariToUsdObjectPtr(frame);
    return frameObjPtr->frameReady(mask, this);
  }
  return 1;
}

const char* UsdDevice::makeUniqueName(const char* name)
{
  std::string proposedBaseName(name);
  proposedBaseName.append("_");

  int postFix = 0;
  std::string proposedName = proposedBaseName + std::to_string(postFix);

  auto empRes = internals->uniqueNames.emplace(proposedName);
  while (!empRes.second)
  {
    ++postFix;
    proposedName = proposedBaseName + std::to_string(postFix);
    empRes = internals->uniqueNames.emplace(proposedName);
  }

  return empRes.first->c_str();
}

bool UsdDevice::nameExists(const char* name)
{
  return internals->uniqueNames.find(name) != internals->uniqueNames.end();
}

void UsdDevice::addToCommitList(UsdBaseObject* object, bool commitData)
{
  if(!object)
    return;

  if(lockCommitList)
  {
    this->reportStatus(object, object->getType(), ANARI_SEVERITY_FATAL_ERROR, ANARI_STATUS_INVALID_OPERATION,
      "Usd device internal error; addToCommitList called while list is locked");
  }
  else
  {
    auto it = std::find_if(commitList.begin(), commitList.end(),
      [&object](const CommitListType& entry) -> bool { return entry.first.ptr == object; });
    if(it == commitList.end())
      commitList.emplace_back(CommitListType(object, commitData));
  }
}

void UsdDevice::clearCommitList()
{
  removePrimsFromUsd(true); // removeList pointers are taken from commitlist

#ifdef CHECK_MEMLEAKS
  for(auto& commitEntry : commitList)
  {
    logObjDeallocation(commitEntry.first.ptr);
  }
#endif

  commitList.resize(0);
}

void UsdDevice::flushCommitList()
{
  lockCommitList = true;

  writeTypeToUsd<(int)ANARI_SAMPLER>();

  writeTypeToUsd<(int)ANARI_SPATIAL_FIELD>();
  writeTypeToUsd<(int)ANARI_GEOMETRY>();
  writeTypeToUsd<(int)ANARI_LIGHT>();

  writeTypeToUsd<(int)ANARI_MATERIAL>();

  writeTypeToUsd<(int)ANARI_SURFACE>();
  writeTypeToUsd<(int)ANARI_VOLUME>();

  writeTypeToUsd<(int)ANARI_GROUP>();
  writeTypeToUsd<(int)ANARI_INSTANCE>();
  writeTypeToUsd<(int)ANARI_WORLD>();

  writeTypeToUsd<(int)ANARI_CAMERA>();
  writeTypeToUsd<(int)ANARI_FRAME>();

  removePrimsFromUsd();

  clearCommitList();

  lockCommitList = false;
}

void UsdDevice::addToVolumeList(UsdVolume* volume)
{
  auto it = std::find(volumeList.begin(), volumeList.end(), volume);
  if(it == volumeList.end())
    volumeList.emplace_back(volume);
}

void UsdDevice::addToResourceStringList(UsdSharedString* string)
{
  resourceStringList.push_back(helium::IntrusivePtr<UsdSharedString>(string));
}

void UsdDevice::clearResourceStringList()
{
  resourceStringList.resize(0);
}

void UsdDevice::removeFromVolumeList(UsdVolume* volume)
{
  auto it = std::find(volumeList.begin(), volumeList.end(), volume);
  if(it != volumeList.end())
  {
    *it = volumeList.back();
    volumeList.pop_back();
  }
}

template<int typeInt>
void UsdDevice::writeTypeToUsd()
{
  for(auto objCommitPair : commitList)
  {
    auto object = objCommitPair.first;
    bool commitData = objCommitPair.second;

    if((int)object->getType() == typeInt)
    {
      using ObjectType = typename AnariToUsdBridgedObject<typeInt>::Type;
      ObjectType* typedObj = reinterpret_cast<ObjectType*>(object.ptr);

      if(!object->deferCommit(this))
      {
        bool commitRefs = true;
        if(commitData)
          commitRefs = object->doCommitData(this);
        if(commitRefs)
          object->doCommitRefs(this);
      }
      else
      {
        this->reportStatus(object.ptr, object->getType(), ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_OPERATION,
          "User forgot to at least once commit an ANARI child object of parent object '%s'", typedObj->getName());
      }

      if(typedObj->getRemovePrim())
      {
        removeList.push_back(object.ptr); // Just raw pointer, removeList is purged upon commitList clear
      }
    }
  }
}

void UsdDevice::removePrimsFromUsd(bool onlyRemoveHandles)
{
  if(!onlyRemoveHandles)
  {
    for(auto baseObj : removeList)
    {
      baseObj->remove(this);
    }
  }
  removeList.resize(0);
}

int UsdDevice::getProperty(ANARIObject object,
    const char *name,
    ANARIDataType type,
    void *mem,
    uint64_t size,
    uint32_t mask)
{
  if ((void *)object == (void *)this)
  {
    if (strEquals(name, "version") && type == ANARI_INT32)
    {
      writeToVoidP(mem, DEVICE_VERSION_BUILD);
      return 1;
    }
    if (strEquals(name, "version.major") && type == ANARI_INT32)
    {
      writeToVoidP(mem, DEVICE_VERSION_MAJOR);
      return 1;
    }
    if (strEquals(name, "version.minor") && type == ANARI_INT32)
    {
      writeToVoidP(mem, DEVICE_VERSION_MINOR);
      return 1;
    }
    if (strEquals(name, "version.patch") && type == ANARI_INT32)
    {
      writeToVoidP(mem, DEVICE_VERSION_PATCH);
      return 1;
    }
    if (strEquals(name, "version.name") && type == ANARI_STRING)
    {
      snprintf((char*)mem, size, "%s", DEVICE_VERSION_NAME);
      return 1;
    }
    else if (strEquals(name, "version.name.size") && type == ANARI_UINT64)
    {
      if (Assert64bitStringLengthProperty(size, UsdLogInfo(this, this, ANARI_DEVICE, "UsdDevice"), "version.name.size"))
      {
        uint64_t nameLen = strlen(DEVICE_VERSION_NAME)+1;
        memcpy(mem, &nameLen, size);
      }
      return 1;
    }
    else if (strEquals(name, "geometryMaxIndex") && type == ANARI_UINT64)
    {
      uint64_t maxIndex = std::numeric_limits<uint64_t>::max(); // Only restricted to int for UsdGeomMesh: GetFaceVertexIndicesAttr() takes a VtArray<int>
      writeToVoidP(mem, maxIndex);
      return 1;
    }
    else if (strEquals(name, "extension") && type == ANARI_STRING_LIST)
    {
      writeToVoidP(mem, anari::usd::query_extensions());
      return 1;
    }
  }
  else if(object)
    return AnariToUsdObjectPtr(object)->getProperty(name, type, mem, size, this);

  return 0;
}

ANARIFrame UsdDevice::newFrame()
{
  const char* name = makeUniqueName("Frame");
  UsdFrame* object = new UsdFrame(name, this);
#ifdef CHECK_MEMLEAKS
  logObjAllocation(object);
#endif

  return (ANARIFrame)(object);
}

const void * UsdDevice::frameBufferMap(
  ANARIFrame fb,
  const char *channel,
  uint32_t *width,
  uint32_t *height,
  ANARIDataType *pixelType)
{
  if (fb)
    return AnariToUsdObjectPtr(fb)->mapBuffer(channel, width, height, pixelType, this);
  return nullptr;
}

void UsdDevice::frameBufferUnmap(ANARIFrame fb, const char *channel)
{
  if (fb)
    return AnariToUsdObjectPtr(fb)->unmapBuffer(channel, this);
}

UsdBaseObject* UsdDevice::getBaseObjectPtr(ANARIObject object)
{
  return handleIsDevice(object) ? this : (UsdBaseObject*)object;
}

void UsdDevice::setParameter(ANARIObject object,
  const char *name,
  ANARIDataType type,
  const void *mem)
{
  if(object)
    getBaseObjectPtr(object)->filterSetParam(name, type, mem, this);
}

void UsdDevice::unsetParameter(ANARIObject object, const char * name)
{
  if(object)
    getBaseObjectPtr(object)->filterResetParam(name);
}

void UsdDevice::unsetAllParameters(ANARIObject object)
{
  if(object)
    getBaseObjectPtr(object)->resetAllParams();
}

void *UsdDevice::mapParameterArray1D(ANARIObject object,
    const char *name,
    ANARIDataType dataType,
    uint64_t numElements1,
    uint64_t *elementStride)
{
  auto array = newArray1D(nullptr, nullptr, nullptr, dataType, numElements1);
  setParameter(object, name, ANARI_ARRAY1D, &array);
  *elementStride = anari::sizeOf(dataType);
  bool paramExists = AnariToUsdObjectPtr(array)->useCount() > 1;
  AnariToUsdObjectPtr(array)->refDec(helium::RefType::PUBLIC);
  return paramExists ? mapArray(array) : nullptr;
}

void *UsdDevice::mapParameterArray2D(ANARIObject object,
    const char *name,
    ANARIDataType dataType,
    uint64_t numElements1,
    uint64_t numElements2,
    uint64_t *elementStride)
{
  auto array = newArray2D(nullptr, nullptr, nullptr, dataType, numElements1, numElements2);
  setParameter(object, name, ANARI_ARRAY2D, &array);
  *elementStride = anari::sizeOf(dataType);
  bool paramExists = AnariToUsdObjectPtr(array)->useCount() > 1;
  AnariToUsdObjectPtr(array)->refDec(helium::RefType::PUBLIC);
  return paramExists ? mapArray(array) : nullptr;
}

void *UsdDevice::mapParameterArray3D(ANARIObject object,
    const char *name,
    ANARIDataType dataType,
    uint64_t numElements1,
    uint64_t numElements2,
    uint64_t numElements3,
    uint64_t *elementStride)
{
  auto array = newArray3D(nullptr,
      nullptr,
      nullptr,
      dataType,
      numElements1,
      numElements2,
      numElements3);
  setParameter(object, name, ANARI_ARRAY3D, &array);
  *elementStride = anari::sizeOf(dataType);
  bool paramExists = AnariToUsdObjectPtr(array)->useCount() > 1;
  AnariToUsdObjectPtr(array)->refDec(helium::RefType::PUBLIC);
  return paramExists ? mapArray(array) : nullptr;
}

void UsdDevice::unmapParameterArray(ANARIObject object, const char *name)
{
  if(!object)
    return;

  ANARIDataType paramType = ANARI_UNKNOWN;
  void* paramAddress = getBaseObjectPtr(object)->getParameter(name, paramType);

  if(paramAddress && anari::isArray(paramType))
  {
    auto arrayAddress = (ANARIArray*)paramAddress;
    if(*arrayAddress)
      AnariToUsdObjectPtr(*arrayAddress)->unmap(this);
  }
}

void UsdDevice::release(ANARIObject object)
{
  if(!object)
    return;

  UsdBaseObject* baseObject = getBaseObjectPtr(object);

  bool privatizeArray = anari::isArray(baseObject->getType())
    && baseObject->useCount(helium::RefType::INTERNAL) > 0
    && baseObject->useCount(helium::RefType::PUBLIC) == 1;

#ifdef CHECK_MEMLEAKS
  if(!handleIsDevice(object))
    logObjDeallocation(baseObject);
#endif

  if (baseObject)
    baseObject->refDec(helium::RefType::PUBLIC);

  if (privatizeArray)
    AnariToUsdObjectPtr((ANARIArray)object)->privatize();
}

void UsdDevice::retain(ANARIObject object)
{
  if(object)
    getBaseObjectPtr(object)->refInc(helium::RefType::PUBLIC);
}

void UsdDevice::commitParameters(ANARIObject object)
{
  if(object)
    getBaseObjectPtr(object)->commit(this);
}

#ifdef CHECK_MEMLEAKS
namespace
{
  template<typename T>
  void SharedLogDeallocation(const T* ptr, std::vector<const T*>& allocations, UsdDevice* device)
  {
    if (ptr)
    {
      auto it = std::find(allocations.begin(), allocations.end(), ptr);
      if(it == allocations.end())
      {
        std::stringstream errstream;
        errstream << "USD Device release of nonexisting or already released/deleted object: 0x" << std::hex << ptr;

        device->reportStatus(device, ANARI_DEVICE, ANARI_SEVERITY_FATAL_ERROR, ANARI_STATUS_INVALID_OPERATION, errstream.str().c_str());
      }

      if(ptr->useCount() == 1)
      {
        assert(it != allocations.end());
        allocations.erase(it);
      }
    }
  }

  template<typename T>
  bool isAllocated(const T* ptr, const std::vector<const T*>& allocations)
  {
    auto it = std::find(allocations.begin(), allocations.end(), ptr);
    return it != allocations.end();
  }
}

void UsdDevice::logObjAllocation(const UsdBaseObject* ptr)
{
  allocatedObjects.push_back(ptr);
}

void UsdDevice::logObjDeallocation(const UsdBaseObject* ptr)
{
  SharedLogDeallocation(ptr, allocatedObjects, this);
}

void UsdDevice::logStrAllocation(const UsdSharedString* ptr)
{
  allocatedStrings.push_back(ptr);
}

void UsdDevice::logStrDeallocation(const UsdSharedString* ptr)
{
  SharedLogDeallocation(ptr, allocatedStrings, this);
}

void UsdDevice::logRawAllocation(const void* ptr)
{
  allocatedRawMemory.push_back(ptr);
}

void UsdDevice::logRawDeallocation(const void* ptr)
{
  if (ptr)
  {
    auto it = std::find(allocatedRawMemory.begin(), allocatedRawMemory.end(), ptr);
    if(it == allocatedRawMemory.end())
    {
      std::stringstream errstream;
      errstream << "USD Device release of nonexisting or already released/deleted raw memory: 0x" << std::hex << ptr;

      reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_FATAL_ERROR, ANARI_STATUS_INVALID_OPERATION, errstream.str().c_str());
    }
    else
      allocatedRawMemory.erase(it);
  }
}

bool UsdDevice::isObjAllocated(const UsdBaseObject* ptr) const
{
  return isAllocated(ptr, allocatedObjects);
}

bool UsdDevice::isStrAllocated(const UsdSharedString* ptr) const
{
  return isAllocated(ptr, allocatedStrings);
}

bool UsdDevice::isRawAllocated(const void* ptr) const
{
  return isAllocated(ptr, allocatedRawMemory);
}
#endif


