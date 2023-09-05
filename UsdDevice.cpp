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

#include <cstdarg>
#include <cstdio>
#include <set>
#include <memory>
#include <sstream>
#include <algorithm>

static char deviceName[] = "usd";

namespace anari {
namespace usd {

const char **query_object_types(ANARIDataType type);
const void *query_object_info(ANARIDataType type,
    const char *subtype,
    const char *infoName,
    ANARIDataType infoType);
const void *query_param_info(ANARIDataType type,
    const char *subtype,
    const char *paramName,
    ANARIDataType paramType,
    const char *infoName,
    ANARIDataType infoType);

const char **query_extensions();

} // namespace usd
} // namespace anari

template <typename T>
inline void writeToVoidP(void *_p, T v)
{
  T *p = (T *)_p;
  *p = v;
}

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
      deviceParams.outputMdlShader
    };

    bridge = std::make_unique<UsdBridge>(bridgeSettings);

    bridge->SetExternalSceneStage(externalSceneStage);

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

      bridge->SetEnableSaving(this->enableSaving);
    }

    return createSuccess;
  }

  std::string outputLocation;
  bool enableSaving = true;
  std::unique_ptr<UsdBridge> bridge;
  SceneStagePtr externalSceneStage{nullptr};

  std::set<std::string> uniqueNames;
};

//---- Make sure to update clearDeviceParameters()
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
)

void UsdDevice::clearDeviceParameters()
{
  deviceUnsetParameter("usd::serialize.hostName");
  deviceUnsetParameter("usd::serialize.location");
  transferWriteToReadParams();
}
//----

UsdDevice::UsdDevice()
  : internals(std::make_unique<UsdDeviceInternals>())
{}

UsdDevice::UsdDevice(ANARILibrary library)
  : DeviceImpl(library), internals(std::make_unique<UsdDeviceInternals>())
{}

UsdDevice::~UsdDevice()
{
  clearCommitList(); // Make sure no more references are held before cleaning up the device (and checking for memleaks)

  clearDeviceParameters(); // Release device parameters with object references

  clearSharedStringList(); // Do the same for shared string references

  //internals->bridge->SaveScene(); //Uncomment to test cleanup of usd files.

#ifdef CHECK_MEMLEAKS
  if(!allocatedObjects.empty())
  {
    std::stringstream errstream;
    errstream << "USD Device memleak reported for: ";
    for(auto ptr : allocatedObjects)
      errstream << "0x" << std::hex << ptr << " of type " << std::dec << ptr->getType() << "; ";

    reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_OPERATION, errstream.str().c_str());
  }
  else
  {
    reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_INFO, ANARI_STATUS_NO_ERROR, "Object reference memleak check complete, no issues found.");
  }
  assert(allocatedObjects.empty());
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

  va_list arglist;
  ((UsdDevice*)device)->reportStatus(nullptr, ANARI_UNKNOWN, severity, ANARI_STATUS_NO_ERROR, message, arglist);
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

void UsdDevice::deviceSetParameter(
  const char *id, ANARIDataType type, const void *mem)
{
  if (strEquals(id, "usd::garbageCollect"))
  {
    // Perform garbage collection on usd objects (needs to move into the user interface)
    if(internals->bridge)
      internals->bridge->GarbageCollect();
  }
  else if(strEquals(id, "usd::removeUnusedNames"))
  {
    internals->uniqueNames.clear();
  }
  else if (strEquals(id, "usd::connection.logVerbosity")) // 0 <= verbosity <= 4, with 4 being the loudest
  {
    if(type == ANARI_INT32)
      UsdBridge::SetConnectionLogVerbosity(*(reinterpret_cast<const int*>(mem)));
  }
  else if(strEquals(id, "usd::sceneStage"))
  {
    if(type == ANARI_VOID_POINTER)
      internals->externalSceneStage = const_cast<void *>(mem);
  }
  else if (strEquals(id, "usd::enableSaving"))
  {
    if(type == ANARI_BOOL)
    {
      internals->enableSaving = *(reinterpret_cast<const bool*>(mem));
      if(internals->bridge)
        internals->bridge->SetEnableSaving(internals->enableSaving);
    }
  }
  else if (strEquals(id, "statusCallback") && type == ANARI_STATUS_CALLBACK)
  {
    userSetStatusFunc = (ANARIStatusCallback)mem;
  }
  else if (strEquals(id, "statusCallbackUserData") && type == ANARI_VOID_POINTER)
  {
    userSetStatusUserData = const_cast<void *>(mem);
  }
  else
  {
    setParam(id, type, mem, this);
  }
}

void UsdDevice::deviceUnsetParameter(const char * id)
{
  if (strEquals(id, "statusCallback"))
  {
    userSetStatusFunc = nullptr;
  }
  else if (strEquals(id, "statusCallbackUserData"))
  {
    userSetStatusUserData = nullptr;
  }
  else if (!strEquals(id, "usd::garbageCollect")
    && !strEquals(id, "usd::removeUnusedNames"))
  {
    resetParam(id);
  }
}

void UsdDevice::deviceRetain()
{
  this->refInc();
}

void UsdDevice::deviceRelease()
{
  this->refDec();
}

void UsdDevice::deviceCommit()
{
  transferWriteToReadParams();

  if(!bridgeInitAttempt)
  {
    initializeBridge();
  }
  else
  {
    const UsdDeviceData& paramData = getReadParams();
    internals->bridge->UpdateBeginEndTime(paramData.timeStep);
  }
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
    LogAllocation(object);
#endif

    return (ANARIArray)(object);
  }
  else
  {
    UsdDataArray* object = new UsdDataArray(appMemory, deleter, userData,
      dataType, numItems1, byteStride1, numItems2, byteStride2, numItems3, byteStride3,
      this);
#ifdef CHECK_MEMLEAKS
    LogAllocation(object);
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
  return ((UsdDataArray*)array)->map(this);
}

void UsdDevice::unmapArray(ANARIArray array)
{
  ((UsdDataArray*)array)->unmap(this);
}

ANARISampler UsdDevice::newSampler(const char *type)
{
  const char* name = makeUniqueName("Sampler");
  UsdSampler* object = new UsdSampler(name, type, this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARISampler)(object);
}

ANARIMaterial UsdDevice::newMaterial(const char *material_type)
{
  const char* name = makeUniqueName("Material");
  UsdMaterial* object = new UsdMaterial(name, material_type, this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIMaterial)(object);
}

ANARIGeometry UsdDevice::newGeometry(const char *type)
{
  const char* name = makeUniqueName("Geometry");
  UsdGeometry* object = new UsdGeometry(name, type, this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIGeometry)(object);
}

ANARISpatialField UsdDevice::newSpatialField(const char * type)
{
  const char* name = makeUniqueName("SpatialField");
  UsdSpatialField* object = new UsdSpatialField(name, type, this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARISpatialField)(object);
}

ANARISurface UsdDevice::newSurface()
{
  const char* name = makeUniqueName("Surface");
  UsdSurface* object = new UsdSurface(name, this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARISurface)(object);
}

ANARIVolume UsdDevice::newVolume(const char *type)
{
  const char* name = makeUniqueName("Volume");
  UsdVolume* object = new UsdVolume(name, this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIVolume)(object);
}

ANARIGroup UsdDevice::newGroup()
{
  const char* name = makeUniqueName("Group");
  UsdGroup* object = new UsdGroup(name, this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIGroup)(object);
}

ANARIInstance UsdDevice::newInstance(const char */*type*/)
{
  const char* name = makeUniqueName("Instance");
  UsdInstance* object = new UsdInstance(name, this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIInstance)(object);
}

ANARIWorld UsdDevice::newWorld()
{
  const char* name = makeUniqueName("World");
  UsdWorld* object = new UsdWorld(name, this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIWorld)(object);
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
  LogAllocation(object);
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

  UsdRenderer* ren = ((UsdFrame*)frame)->getRenderer();
  if(ren)
    ren->saveUsd(this);
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
#ifdef CHECK_MEMLEAKS
  for(auto& commitEntry : commitList)
  {
    LogDeallocation(commitEntry.first.ptr);
  }
#endif

  commitList.resize(0);
}

void UsdDevice::flushCommitList()
{
  // Automatically perform a new commitdata/commitrefs on volumes which are not committed,
  // but for which their (readdata) spatial field is in commitlist. (to update the previous commit)
  for(UsdVolume* volume : volumeList)
  {
    const UsdVolumeData& readParams = volume->getReadParams();
    if(readParams.field)
    {
      //volume not in commitlist
      auto volEntry = std::find_if(commitList.begin(), commitList.end(),
        [&volume](const CommitListType& entry) -> bool { return entry.first.ptr == volume; });
      if(volEntry == commitList.end())
      {
        auto fieldEntry = std::find_if(commitList.begin(), commitList.end(),
          [&readParams](const CommitListType& entry) -> bool { return entry.first.ptr == readParams.field; });

        // spatialfield from readParams is in commit list
        if(fieldEntry != commitList.end())
        {
          UsdBaseObject* baseObject = static_cast<UsdBaseObject*>(volume);
          bool commitRefs = baseObject->doCommitData(this);
          if(commitRefs)
            baseObject->doCommitRefs(this);
        }
      }
    }
  }

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

  clearCommitList();

  lockCommitList = false;
}

void UsdDevice::addToVolumeList(UsdVolume* volume)
{
  auto it = std::find(volumeList.begin(), volumeList.end(), volume);
  if(it == volumeList.end())
    volumeList.emplace_back(volume);
}

void UsdDevice::addToSharedStringList(UsdSharedString* string)
{
  sharedStringList.push_back(helium::IntrusivePtr<UsdSharedString>(string));
}

void UsdDevice::clearSharedStringList()
{
  sharedStringList.resize(0);
}

void UsdDevice::removeFromVolumeList(UsdVolume* volume)
{
  auto it = std::find(volumeList.begin(), volumeList.end(), volume);
  if(it == volumeList.end())
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
        using ObjectType = typename AnariToUsdBridgedObject<typeInt>::Type;
        ObjectType* typedObj = reinterpret_cast<ObjectType*>(object.ptr);

        this->reportStatus(object.ptr, object->getType(), ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_OPERATION,
          "User forgot to at least once commit an ANARI child object of parent object '%s'", typedObj->getName());
      }
    }
  }
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
    else if (type == ANARI_UINT64 && strEquals(name, "version.name.size"))
    {
      if (Assert64bitStringLengthProperty(size, UsdLogInfo(this, this, ANARI_DEVICE, "UsdDevice"), "version.name.size"))
      {
        uint64_t nameLen = strlen(DEVICE_VERSION_NAME)+1;
        memcpy(mem, &nameLen, size);
      }
      return 1;
    }
    if (strEquals(name, "extension") && type == ANARI_STRING_LIST)
    {
      writeToVoidP(mem, anari::usd::query_extensions());
      return 1;
    }
  }
  else
    return ((UsdBaseObject*)object)->getProperty(name, type, mem, size, this);

  return 0;
}

ANARIFrame UsdDevice::newFrame()
{
  UsdFrame* object = new UsdFrame(internals->bridge.get());
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
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
    return ((UsdFrame*)fb)->mapBuffer(channel, width, height, pixelType);
  return nullptr;
}

void UsdDevice::frameBufferUnmap(ANARIFrame fb, const char *channel)
{
  if (fb)
    return ((UsdFrame*)fb)->unmapBuffer(channel);
}

void UsdDevice::setParameter(ANARIObject object,
  const char *name,
  ANARIDataType type,
  const void *mem)
{
  if (handleIsDevice(object)) {
    deviceSetParameter(name, type, mem);
    return;
  } else if (object)
    ((UsdBaseObject*)object)->filterSetParam(name, type, mem, this);
}

void UsdDevice::unsetParameter(ANARIObject object, const char * name)
{
  if (handleIsDevice(object))
    deviceUnsetParameter(name);
  else if (object)
    ((UsdBaseObject*)object)->filterResetParam(name);
}

void UsdDevice::unsetAllParameters(ANARIObject object)
{
  if (handleIsDevice(object))
    ((UsdDevice*)object)->resetParams();
  else if (object)
    ((UsdBaseObject*)object)->resetAllParams();
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
  ((UsdDataArray*)array)->refDec(helium::RefType::PUBLIC);
  return mapArray(array);
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
  ((UsdDataArray*)array)->refDec(helium::RefType::PUBLIC);
  return mapArray(array);
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
  ((UsdDataArray*)array)->refDec(helium::RefType::PUBLIC);
  return mapArray(array);
}

void UsdDevice::unmapParameterArray(ANARIObject object, const char *name)
{
  void* paramAddress = nullptr;
  ANARIDataType paramType = ANARI_UNKNOWN;

  if (handleIsDevice(object))
    paramAddress = ((UsdDevice*)object)->getParam(name, paramType);
  else if (object)
    paramAddress = ((UsdBaseObject*)object)->tempGetParam(name, paramType);

  if(paramAddress && paramType == ANARI_ARRAY)
  {
    ((UsdDataArray*)paramAddress)->unmap(this);
  }
}

void UsdDevice::release(ANARIObject object)
{
  if (object == nullptr)
    return;
  else if (handleIsDevice(object)) {
    deviceRelease();
    return;
  }

  UsdBaseObject* baseObject = (UsdBaseObject*)object;

  bool privatizeArray = baseObject->getType() == ANARI_ARRAY
    && baseObject->useCount(helium::RefType::INTERNAL) > 0
    && baseObject->useCount(helium::RefType::PUBLIC) == 1;

#ifdef CHECK_MEMLEAKS
    LogDeallocation(baseObject);
#endif
  if (baseObject)
    baseObject->refDec(helium::RefType::PUBLIC);

  if (privatizeArray)
    ((UsdDataArray*)baseObject)->privatize();
}

void UsdDevice::retain(ANARIObject object)
{
  if (handleIsDevice(object))
    deviceRetain();
  else if (object)
    ((UsdBaseObject*)object)->refInc(helium::RefType::PUBLIC);
}

void UsdDevice::commitParameters(ANARIObject object)
{
  if (handleIsDevice(object))
    deviceCommit();
  else if(object)
    ((UsdBaseObject*)object)->commit(this);
}

#ifdef CHECK_MEMLEAKS
void UsdDevice::LogAllocation(const UsdBaseObject* ptr)
{
  allocatedObjects.push_back(ptr);
}

void UsdDevice::LogDeallocation(const UsdBaseObject* ptr)
{
  if (ptr)
  {
    auto it = std::find(allocatedObjects.begin(), allocatedObjects.end(), ptr);
    if(it == allocatedObjects.end())
    {
      std::stringstream errstream;
      errstream << "USD Device release of nonexisting or already released/deleted object: 0x" << std::hex << ptr;

      reportStatus(this, ANARI_DEVICE, ANARI_SEVERITY_FATAL_ERROR, ANARI_STATUS_INVALID_OPERATION, errstream.str().c_str());
    }

    if(ptr->useCount() == 1)
    {
      assert(it != allocatedObjects.end());
      allocatedObjects.erase(it);
    }
  }
}

void UsdDevice::LogRawAllocation(const void* ptr)
{
  allocatedRawMemory.push_back(ptr);
}

void UsdDevice::LogRawDeallocation(const void* ptr)
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
#endif


