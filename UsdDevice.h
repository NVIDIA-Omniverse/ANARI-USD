// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "anari/detail/Device.h"
#include "anari/detail/Library.h"
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

struct UsdDeviceData
{
  const char* hostName = nullptr;
  const char* outputPath = nullptr;
  bool createNewSession = true;
  bool outputBinary = false;

  double timeStep = 0.0;

  bool noMaterialOutput = false;
};

class UsdDevice : public anari::Device, anari::RefCounted, public UsdParameterizedObject<UsdDevice, UsdDeviceData>
{
  public:

    UsdDevice();
    ~UsdDevice();

    /////////////////////////////////////////////////////////////////////////////
    // Main virtual interface to accepting API calls
    /////////////////////////////////////////////////////////////////////////////

    // Device API ///////////////////////////////////////////////////////////////

    int deviceImplements(const char *extension) override;

    void deviceCommit() override;

    void deviceRetain() override;

    void deviceRelease() override;

    void deviceSetParameter(const char *id, ANARIDataType type, const void *mem) override;

    void deviceUnsetParameter(const char *id) override;

    // Data Arrays //////////////////////////////////////////////////////////////
    ANARIArray1D newArray1D(void *appMemory,
      ANARIMemoryDeleter deleter,
      void *userdata,
      ANARIDataType,
      uint64_t numItems1,
      uint64_t byteStride1) override;

    ANARIArray2D newArray2D(void *appMemory,
      ANARIMemoryDeleter deleter,
      void *userdata,
      ANARIDataType,
      uint64_t numItems1,
      uint64_t numItems2,
      uint64_t byteStride1,
      uint64_t byteStride2) override;

    ANARIArray3D newArray3D(void *appMemory,
      ANARIMemoryDeleter deleter,
      void *userdata,
      ANARIDataType,
      uint64_t numItems1,
      uint64_t numItems2,
      uint64_t numItems3,
      uint64_t byteStride1,
      uint64_t byteStride2,
      uint64_t byteStride3) override;

    void* mapArray(ANARIArray) override;
    void unmapArray(ANARIArray) override;

    // Renderable Objects ///////////////////////////////////////////////////////

    ANARILight newLight(const char *type) override { return nullptr; }

    ANARICamera newCamera(const char *type) override { return nullptr; }

    ANARIGeometry newGeometry(const char *type);
    ANARISpatialField newSpatialField(const char *type) override;

    ANARISurface newSurface() override;
    ANARIVolume newVolume(const char *type) override;

    // Model Meta-Data //////////////////////////////////////////////////////////

    ANARIMaterial newMaterial(const char *material_type) override;

    ANARISampler newSampler(const char *type) override;

    // Instancing ///////////////////////////////////////////////////////////////

    ANARIGroup newGroup() override;

    ANARIInstance newInstance() override;

    // Top-level Worlds /////////////////////////////////////////////////////////

    ANARIWorld newWorld() override;

    // Object + Parameter Lifetime Management ///////////////////////////////////

    void setParameter(ANARIObject object,
      const char *name,
      ANARIDataType type,
      const void *mem) override;

    void unsetParameter(ANARIObject object, const char *name) override;

    void commit(ANARIObject object) override;

    void release(ANARIObject _obj) override;
    void retain(ANARIObject _obj) override;

    // Object Query Interface ///////////////////////////////////////////////////

    int getProperty(ANARIObject object,
      const char *name,
      ANARIDataType type,
      void *mem,
      uint64_t size,
      uint32_t mask) override;

    // FrameBuffer Manipulation /////////////////////////////////////////////////

    ANARIFrame newFrame() override;

    const void *frameBufferMap(
      ANARIFrame fb, const char *channel) override;

    void frameBufferUnmap(ANARIFrame fb, const char *channel) override;

    // Frame Rendering //////////////////////////////////////////////////////////

    ANARIRenderer newRenderer(const char *type) override;

    void renderFrame(ANARIFrame frame) override;
    int frameReady(ANARIFrame, ANARIWaitMask) override { return 1; }
    void discardFrame(ANARIFrame) override {}

    // USD Specific /////////////////////////////////////////////////////////////

    bool nameExists(const char* name);

#ifdef CHECK_MEMLEAKS
    // Memleak checking
    void LogAllocation(const UsdBaseObject* ptr);
    void LogDeallocation(const UsdBaseObject* ptr);
    std::vector<const UsdBaseObject*> allocatedObjects;
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
    const char* makeUniqueName(const char* name);

    ANARIArray CreateDataArray(void *appMemory,
      ANARIMemoryDeleter deleter,
      void *userData,
      ANARIDataType dataType,
      uint64_t numItems1,
      int64_t byteStride1,
      uint64_t numItems2,
      int64_t byteStride2,
      uint64_t numItems3,
      int64_t byteStride3);

    std::unique_ptr<UsdDeviceInternals> internals;

    ANARIStatusCallback statusFunc = nullptr;
    void* statusUserData = nullptr;
    ANARIStatusCallback userSetStatusFunc = nullptr;
    void* userSetStatusUserData = nullptr;
    std::vector<char> lastStatusMessage;
};

