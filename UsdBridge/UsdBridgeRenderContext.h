// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeRenderContext_h
#define UsdBridgeRenderContext_h

#include "usd.h"
#include "UsdBridgeData.h"

#include <memory>

class UsdBridgeUsdWriter;

#ifdef USD_DEVICE_RENDERING_ENABLED
PXR_NAMESPACE_OPEN_SCOPE
class HdRenderIndex;
class HdEngine;
class Hgi;
class UsdImagingDelegate;
PXR_NAMESPACE_CLOSE_SCOPE
#endif

//
// Factory mode for creating render contexts
//
enum class RenderContextMode
{
    Shared,     // Use shared core (efficient)
    Standalone  // Debug fallback (each context has own engine)
};

//
// Abstract interface for a render context
// A render context represents a single "viewport" that can render from a specific camera
//
class UsdBridgeRenderContext
{
public:
    virtual ~UsdBridgeRenderContext() = default;

    virtual void SetCameraPath(const pxr::SdfPath& cameraPath) = 0;
    virtual void SetWorldPath(const pxr::SdfPath& worldPath) = 0;
    virtual void Render(uint32_t width, uint32_t height, double timeStep) = 0;
    virtual bool FrameReady(bool wait) = 0;
    virtual void* MapFrame(UsdBridgeType& returnFormat) = 0;
    virtual void UnmapFrame() = 0;

    virtual bool IsInitialized() const = 0;
};

//
// Shared renderer core - holds the heavy resources that can be shared between contexts
// One core per renderer plugin type (e.g., one for HdStorm, one for HdCycles)
//
class UsdBridgeRendererCore
{
public:
    UsdBridgeRendererCore(UsdBridgeUsdWriter& usdWriter);
    ~UsdBridgeRendererCore();

    // Initialize with a specific Hydra renderer plugin
    void Initialize(const char* rendererPluginName);

    bool IsInitialized() const;

#ifdef USD_DEVICE_RENDERING_ENABLED
    // Access shared resources (for UsdBridgeRenderContextShared)
    pxr::HdRenderIndex* GetRenderIndex();
    pxr::HdEngine* GetEngine();
    pxr::Hgi* GetHgi();
    pxr::UsdImagingDelegate* GetSceneDelegate();
#endif

    pxr::UsdStageRefPtr GetStage();
    UsdBridgeUsdWriter& GetUsdWriter();

protected:
    class Internals;
    std::unique_ptr<Internals> Data;
    UsdBridgeUsdWriter& UsdWriter;
};

//
// Factory function to create the appropriate context type
//
std::unique_ptr<UsdBridgeRenderContext> CreateRenderContext(
    RenderContextMode mode,
    UsdBridgeRendererCore* core,  // Required for Shared mode, can be null for Standalone
    UsdBridgeUsdWriter& usdWriter,
    const char* rendererPluginName,
    const pxr::SdfPath& contextId
);

#endif
