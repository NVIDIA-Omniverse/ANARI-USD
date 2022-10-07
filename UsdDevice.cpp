// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdDevice.h"
#include "UsdBridge/UsdBridge.h"
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

#ifndef USDDevice_INTERFACE
#define USDDevice_INTERFACE
#endif

extern "C" USDDevice_INTERFACE ANARI_DEFINE_LIBRARY_NEW_DEVICE(
    usd, library, _subtype)
{
  if (strEquals(_subtype, "default") || strEquals(_subtype, "usd"))
    return (ANARIDevice) new UsdDevice(library);
  return nullptr;
}

ANARI_DEFINE_LIBRARY_GET_DEVICE_SUBTYPES(reference, libdata)
{
  static const char *devices[] = { deviceName, nullptr };
  return devices;
}

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

UsdDevice::UsdDevice()
  : internals(std::make_unique<UsdDeviceInternals>())
{}

UsdDevice::UsdDevice(ANARILibrary library)
  : DeviceImpl(library), internals(std::make_unique<UsdDeviceInternals>())
{}

UsdDevice::~UsdDevice()
{
  clearCommitList(); // Make sure no more references are held before cleaning up the device (and checking for memleaks)

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

void UsdDevice::deviceCommit()
{
  transferWriteToReadParams();

  const UsdDeviceData& paramData = getReadParams();

  if(!bridgeInitialized)
  {
    bridgeInitialized = true;

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
  else
  {
    internals->bridge->UpdateBeginEndTime(paramData.timeStep);
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
  uint64_t numItems,
  uint64_t byteStride)
{
  return (ANARIArray1D)CreateDataArray(appMemory, deleter, userData,
    type, numItems, byteStride, 1, 0, 1, 0);
}

ANARIArray2D UsdDevice::newArray2D(const void *appMemory,
  ANARIMemoryDeleter deleter,
  const void *userData,
  ANARIDataType type,
  uint64_t numItems1,
  uint64_t numItems2,
  uint64_t byteStride1,
  uint64_t byteStride2)
{
  return (ANARIArray2D)CreateDataArray(appMemory, deleter, userData,
    type, numItems1, byteStride1, numItems2, byteStride2, 1, 0);
}

ANARIArray3D UsdDevice::newArray3D(const void *appMemory,
  ANARIMemoryDeleter deleter,
  const void *userData,
  ANARIDataType type,
  uint64_t numItems1,
  uint64_t numItems2,
  uint64_t numItems3,
  uint64_t byteStride1,
  uint64_t byteStride2,
  uint64_t byteStride3)
{
  return (ANARIArray3D)CreateDataArray(appMemory, deleter, userData,
    type, numItems1, byteStride1, numItems2, byteStride2, numItems3, byteStride3);
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
  UsdSampler* object = new UsdSampler(name, type, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARISampler)(object);
}

ANARIMaterial UsdDevice::newMaterial(const char *material_type)
{
  const char* name = makeUniqueName("Material");
  UsdMaterial* object = new UsdMaterial(name, material_type, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIMaterial)(object);
}

ANARIGeometry UsdDevice::newGeometry(const char *type)
{
  const char* name = makeUniqueName("Geometry");
  UsdGeometry* object = new UsdGeometry(name, type, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIGeometry)(object);
}

ANARISpatialField UsdDevice::newSpatialField(const char * type)
{
  const char* name = makeUniqueName("SpatialField");
  UsdSpatialField* object = new UsdSpatialField(name, type, internals->bridge.get());
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARISpatialField)(object);
}

ANARISurface UsdDevice::newSurface()
{
  const char* name = makeUniqueName("Surface");
  UsdSurface* object = new UsdSurface(name, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARISurface)(object);
}

ANARIVolume UsdDevice::newVolume(const char *type)
{
  const char* name = makeUniqueName("Volume");
  UsdVolume* object = new UsdVolume(name, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIVolume)(object);
}

ANARIGroup UsdDevice::newGroup()
{
  const char* name = makeUniqueName("Group");
  UsdGroup* object = new UsdGroup(name, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIGroup)(object);
}

ANARIInstance UsdDevice::newInstance()
{
  const char* name = makeUniqueName("Instance");
  UsdInstance* object = new UsdInstance(name, internals->bridge.get(), this);
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIInstance)(object);
}

ANARIWorld UsdDevice::newWorld()
{
  const char* name = makeUniqueName("World");
  UsdWorld* object = new UsdWorld(name, internals->bridge.get());
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIWorld)(object);
}

ANARIRenderer UsdDevice::newRenderer(const char *type)
{
  UsdRenderer* object = new UsdRenderer(internals->bridge.get());
#ifdef CHECK_MEMLEAKS
  LogAllocation(object);
#endif

  return (ANARIRenderer)(object);
}

void UsdDevice::renderFrame(ANARIFrame frame)
{
  // Always commit device changes if not initialized, otherwise no conversion can be performed.
  if(!isInitialized())
    deviceCommit();

  flushCommitList();

  internals->bridge->ResetResourceUpdateState(); // Reset the modified flags for committed shared resources

  UsdRenderer* ren = ((UsdFrame*)frame)->getRenderer();
  if(ren)
    ren->saveUsd();
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
  // Automatically commit volumes which are not committed yet,
  // but for which their (writedata) spatial field is in commitlist.
  for(UsdVolume* volume : volumeList)
  {
    const UsdVolumeData& writeParams = volume->getWriteParams();
    if(writeParams.field)
    {
      //volume not in commitlist
      auto volEntry = std::find_if(commitList.begin(), commitList.end(),
        [&volume](const CommitListType& entry) -> bool { return entry.first.ptr == volume; });
      if(volEntry == commitList.end())
      {
        auto fieldEntry = std::find_if(commitList.begin(), commitList.end(),
          [&writeParams](const CommitListType& entry) -> bool { return entry.first.ptr == writeParams.field; });

        // spatialfield from writeparams is in commit list
        if(fieldEntry != commitList.end())
        {
          volume->commit(this);
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
  sharedStringList.push_back(anari::IntrusivePtr<UsdSharedString>(string));
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
    if (strEquals(name, "version") && type == ANARI_INT32) {
      writeToVoidP(mem, DEVICE_VERSION);
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
    && baseObject->useCount(anari::RefType::INTERNAL) > 0
    && baseObject->useCount(anari::RefType::PUBLIC) == 1;

#ifdef CHECK_MEMLEAKS
    LogDeallocation(baseObject);
#endif
  if (baseObject)
    baseObject->refDec(anari::RefType::PUBLIC);

  if (privatizeArray)
    ((UsdDataArray*)baseObject)->privatize();
}

void UsdDevice::retain(ANARIObject object)
{
  if (handleIsDevice(object))
    deviceRetain();
  else if (object)
    ((UsdBaseObject*)object)->refInc(anari::RefType::PUBLIC);
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
#endif


