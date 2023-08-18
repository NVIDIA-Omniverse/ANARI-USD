// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "anari/backend/DeviceImpl.h"
#include "anari/backend/LibraryImpl.h"
#include "UsdParameterizedObject.h"

#include <vector>
#include <memory>

#ifdef _WIN32
#ifdef anari_library_usd_EXPORTS
#define USDDevice_INTERFACE __declspec(dllexport)
#else
#define USDDevice_INTERFACE __declspec(dllimport)
#endif
#endif

extern "C"
{
#ifdef WIN32
  USDDevice_INTERFACE void __cdecl anari_library_usd_init();
#else
  void anari_library_usd_init();
#endif
}

class UsdDevice;
class UsdDeviceInternals;
class UsdBaseObject;
class UsdVolume;

struct UsdDeviceData
{
  UsdSharedString* hostName = nullptr;
  UsdSharedString* outputPath = nullptr;
  bool createNewSession = true;
  bool outputBinary = false;
  bool writeAtCommit = false;

  double timeStep = 0.0;

  bool outputMaterial = true;
  bool outputPreviewSurfaceShader = true;
  bool outputMdlShader = true;
};

class UsdDevice : public anari::DeviceImpl, helium::RefCounted, public UsdParameterizedObject<UsdDevice, UsdDeviceData>
{
  public:

    UsdDevice();
    UsdDevice(ANARILibrary library);
    ~UsdDevice();

    ///////////////////////////////////////////////////////////////////////////
    // Main virtual interface to accepting API calls
    ///////////////////////////////////////////////////////////////////////////

    // Data Arrays ////////////////////////////////////////////////////////////

    ANARIArray1D newArray1D(const void *appMemory,
      ANARIMemoryDeleter deleter,
      const void *userdata,
      ANARIDataType,
      uint64_t numItems1) override;

    ANARIArray2D newArray2D(const void *appMemory,
      ANARIMemoryDeleter deleter,
      const void *userdata,
      ANARIDataType,
      uint64_t numItems1,
      uint64_t numItems2) override;

    ANARIArray3D newArray3D(const void *appMemory,
      ANARIMemoryDeleter deleter,
      const void *userdata,
      ANARIDataType,
      uint64_t numItems1,
      uint64_t numItems2,
      uint64_t numItems3) override;

    void* mapArray(ANARIArray) override;
    void unmapArray(ANARIArray) override;

    // Renderable Objects /////////////////////////////////////////////////////

    ANARILight newLight(const char *type) override { return nullptr; }

    ANARICamera newCamera(const char *type) override { return nullptr; }

    ANARIGeometry newGeometry(const char *type);
    ANARISpatialField newSpatialField(const char *type) override;

    ANARISurface newSurface() override;
    ANARIVolume newVolume(const char *type) override;

    // Model Meta-Data ////////////////////////////////////////////////////////

    ANARIMaterial newMaterial(const char *material_type) override;

    ANARISampler newSampler(const char *type) override;

    // Instancing /////////////////////////////////////////////////////////////

    ANARIGroup newGroup() override;

    ANARIInstance newInstance(const char *type) override;

    // Top-level Worlds ///////////////////////////////////////////////////////

    ANARIWorld newWorld() override;

    // Query functions ////////////////////////////////////////////////////////

    const char ** getObjectSubtypes(ANARIDataType objectType) override;
    const void* getObjectInfo(ANARIDataType objectType,
        const char* objectSubtype,
        const char* infoName,
        ANARIDataType infoType) override;
    const void* getParameterInfo(ANARIDataType objectType,
        const char* objectSubtype,
        const char* parameterName,
        ANARIDataType parameterType,
        const char* infoName,
        ANARIDataType infoType) override;

    // Object + Parameter Lifetime Management /////////////////////////////////

    void setParameter(ANARIObject object,
      const char *name,
      ANARIDataType type,
      const void *mem) override;

    void unsetParameter(ANARIObject object, const char *name) override;
    void unsetAllParameters(ANARIObject o) override;

    void* mapParameterArray1D(ANARIObject o,
        const char* name,
        ANARIDataType dataType,
        uint64_t numElements1,
        uint64_t *elementStride) override;
    void* mapParameterArray2D(ANARIObject o,
        const char* name,
        ANARIDataType dataType,
        uint64_t numElements1,
        uint64_t numElements2,
        uint64_t *elementStride) override;
    void* mapParameterArray3D(ANARIObject o,
        const char* name,
        ANARIDataType dataType,
        uint64_t numElements1,
        uint64_t numElements2,
        uint64_t numElements3,
        uint64_t *elementStride) override;
    void unmapParameterArray(ANARIObject o,
        const char* name) override;

    void commitParameters(ANARIObject object) override;

    void release(ANARIObject _obj) override;
    void retain(ANARIObject _obj) override;

    // Object Query Interface /////////////////////////////////////////////////

    int getProperty(ANARIObject object,
      const char *name,
      ANARIDataType type,
      void *mem,
      uint64_t size,
      uint32_t mask) override;

    // FrameBuffer Manipulation ///////////////////////////////////////////////

    ANARIFrame newFrame() override;

    const void *frameBufferMap(ANARIFrame fb,
        const char *channel,
        uint32_t *width,
        uint32_t *height,
        ANARIDataType *pixelType) override;

    void frameBufferUnmap(ANARIFrame fb, const char *channel) override;

    // Frame Rendering ////////////////////////////////////////////////////////

    ANARIRenderer newRenderer(const char *type) override;

    void renderFrame(ANARIFrame frame) override;
    int frameReady(ANARIFrame, ANARIWaitMask) override { return 1; }
    void discardFrame(ANARIFrame) override {}

    // USD Specific ///////////////////////////////////////////////////////////

    bool isInitialized() { return getUsdBridge() != nullptr; }
    UsdBridge* getUsdBridge();

    bool nameExists(const char* name);

    void addToCommitList(UsdBaseObject* object, bool commitData);
    bool isFlushingCommitList() const { return lockCommitList; }

    void addToVolumeList(UsdVolume* volume);
    void removeFromVolumeList(UsdVolume* volume);

    // Allows for selected strings to persist,
    // so their pointers can be cached beyond their containing objects' lifetimes
    void addToSharedStringList(UsdSharedString* sharedString);

#ifdef CHECK_MEMLEAKS
    // Memleak checking
    void LogAllocation(const UsdBaseObject* ptr);
    void LogDeallocation(const UsdBaseObject* ptr);
    std::vector<const UsdBaseObject*> allocatedObjects;

    void LogRawAllocation(const void* ptr);
    void LogRawDeallocation(const void* ptr);
    std::vector<const void*> allocatedRawMemory;
#endif

    void reportStatus(void* source,
      ANARIDataType sourceType,
      ANARIStatusSeverity severity,
      ANARIStatusCode statusCode,
      const char *format, ...);
    void reportStatus(void* source,
      ANARIDataType sourceType,
      ANARIStatusSeverity severity,
      ANARIStatusCode statusCode,
      const char *format,
      va_list& arglist);

  protected:
    void deviceSetParameter(const char *id, ANARIDataType type, const void *mem);
    void deviceUnsetParameter(const char *id);
    void deviceRetain();
    void deviceRelease();
    void deviceCommit();

    // USD Specific Cleanup /////////////////////////////////////////////////////////////

    void clearCommitList();
    void flushCommitList();
    void clearDeviceParameters();
    void clearSharedStringList();

    void initializeBridge();

    const char* makeUniqueName(const char* name);

    ANARIArray CreateDataArray(const void *appMemory,
      ANARIMemoryDeleter deleter,
      const void *userData,
      ANARIDataType dataType,
      uint64_t numItems1,
      int64_t byteStride1,
      uint64_t numItems2,
      int64_t byteStride2,
      uint64_t numItems3,
      int64_t byteStride3);

    template<int typeInt>
    void writeTypeToUsd();

    std::unique_ptr<UsdDeviceInternals> internals;

    bool bridgeInitAttempt = false;

    // Using object pointers as basis for deferred commits; another option would be to traverse
    // the bridge's internal cache handles, but a handle may map to multiple objects (with the same name)
    // so that's not 1-1 with the effects of a non-deferred commit order.
    using CommitListType = std::pair<helium::IntrusivePtr<UsdBaseObject>,bool>;
    std::vector<CommitListType> commitList;
    std::vector<UsdVolume*> volumeList; // Tracks all volumes to auto-commit when child fields have been committed
    bool lockCommitList = false;

    std::vector<helium::IntrusivePtr<UsdSharedString>> sharedStringList;

    ANARIStatusCallback statusFunc = nullptr;
    const void* statusUserData = nullptr;
    ANARIStatusCallback userSetStatusFunc = nullptr;
    const void* userSetStatusUserData = nullptr;
    std::vector<char> lastStatusMessage;
};

