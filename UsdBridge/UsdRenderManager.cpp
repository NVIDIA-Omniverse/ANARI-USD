// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdRenderManager.h"
#include "UsdBridgeRenderContext.h"
#include "UsdBridgeUsdWriter.h"

#ifdef USD_DEVICE_RENDERING_ENABLED

UsdRenderManager::UsdRenderManager(UsdBridgeUsdWriter& usdWriter)
    : UsdWriter(usdWriter)
{
}

UsdRenderManager::~UsdRenderManager()
{
    // Clear frames first (contexts depend on cores)
    Frames.clear();
    // Then clear cores
    Cores.clear();
}

void UsdRenderManager::RegisterFrame(const char* frameName)
{
    if (!frameName)
        return;

    std::string name(frameName);
    if (Frames.find(name) == Frames.end())
    {
        Frames[name] = FrameState();
    }
}

void UsdRenderManager::UnregisterFrame(const char* frameName)
{
    if (!frameName)
        return;

    Frames.erase(std::string(frameName));
}

void UsdRenderManager::UnregisterFrameByState(void* frameState)
{
    if (!frameState)
        return;

    for (auto it = Frames.begin(); it != Frames.end(); ++it)
    {
        if (&it->second == frameState)
        {
            Frames.erase(it);
            return;
        }
    }
}

void* UsdRenderManager::GetFrameState(const char* frameName)
{
    return GetFrameStateInternal(frameName);
}

UsdRenderManager::FrameState* UsdRenderManager::GetFrameStateInternal(const char* frameName)
{
    if (!frameName)
        return nullptr;

    auto it = Frames.find(std::string(frameName));
    if (it == Frames.end())
        return nullptr;

    return &it->second;
}

void UsdRenderManager::SetFrameRenderer(const char* frameName, const char* hydraRendererName)
{
    FrameState* state = GetFrameStateInternal(frameName);
    if (!state)
        return;

    const char* rendererName = hydraRendererName ? hydraRendererName : DefaultRendererName;

    // Check if renderer changed
    if (state->RendererName == rendererName && state->Context)
        return;

    state->RendererName = rendererName;

    // Get or create the core for this renderer
    state->Core = GetOrCreateCore(rendererName);

    if (state->Core && state->Core->IsInitialized())
    {
        // Create a new context for this frame using the frame name
        pxr::SdfPath contextId("/RenderContext_" + std::string(frameName));
        state->Context = CreateRenderContext(
            RenderContextMode::Shared,
            state->Core,
            UsdWriter,
            rendererName,
            contextId);

        // Restore camera and world paths if they were set
        if (!state->CameraPath.IsEmpty() && state->Context)
        {
            state->Context->SetCameraPath(state->CameraPath);
        }
        if (!state->WorldPath.IsEmpty() && state->Context)
        {
            state->Context->SetWorldPath(state->WorldPath);
        }
    }
}

void UsdRenderManager::SetFrameCamera(const char* frameName, const pxr::SdfPath& cameraPath)
{
    FrameState* state = GetFrameStateInternal(frameName);
    if (!state)
        return;

    state->CameraPath = cameraPath;

    if (state->Context)
    {
        state->Context->SetCameraPath(cameraPath);
    }
}

void UsdRenderManager::SetFrameWorld(const char* frameName, const pxr::SdfPath& worldPath)
{
    FrameState* state = GetFrameStateInternal(frameName);
    if (!state)
        return;

    state->WorldPath = worldPath;

    if (state->Context)
    {
        state->Context->SetWorldPath(worldPath);
    }
}

void UsdRenderManager::Render(const char* frameName, uint32_t width, uint32_t height, double timeStep)
{
    FrameState* state = GetFrameStateInternal(frameName);
    if (!state)
        return;

    if (state->Context)
    {
        state->Context->Render(width, height, timeStep);
    }
}

bool UsdRenderManager::FrameReady(const char* frameName, bool wait)
{
    FrameState* state = GetFrameStateInternal(frameName);
    if (!state)
        return true;

    if (state->Context)
    {
        return state->Context->FrameReady(wait);
    }
    return true;
}

void* UsdRenderManager::MapFrame(const char* frameName, UsdBridgeType& returnFormat)
{
    FrameState* state = GetFrameStateInternal(frameName);
    if (!state)
        return nullptr;

    if (state->Context)
    {
        return state->Context->MapFrame(returnFormat);
    }
    return nullptr;
}

void UsdRenderManager::UnmapFrame(const char* frameName)
{
    FrameState* state = GetFrameStateInternal(frameName);
    if (!state)
        return;

    if (state->Context)
    {
        state->Context->UnmapFrame();
    }
}

bool UsdRenderManager::IsRenderingEnabled() const
{
    return true;
}

UsdBridgeRendererCore* UsdRenderManager::GetOrCreateCore(const char* hydraRendererName)
{
    std::string name = hydraRendererName ? hydraRendererName : DefaultRendererName;

    auto it = Cores.find(name);
    if (it != Cores.end())
    {
        return it->second.get();
    }

    // Create a new core for this renderer
    auto core = std::make_unique<UsdBridgeRendererCore>(UsdWriter);
    core->Initialize(hydraRendererName);

    UsdBridgeRendererCore* corePtr = core.get();
    Cores[name] = std::move(core);

    return corePtr;
}

#else // USD_DEVICE_RENDERING_ENABLED

// Stub implementations when rendering is disabled

UsdRenderManager::UsdRenderManager(UsdBridgeUsdWriter& usdWriter)
    : UsdWriter(usdWriter)
{
}

UsdRenderManager::~UsdRenderManager()
{
}

void UsdRenderManager::RegisterFrame(const char* frameName)
{
}

void UsdRenderManager::UnregisterFrame(const char* frameName)
{
}

void UsdRenderManager::UnregisterFrameByState(void* frameState)
{
}

void* UsdRenderManager::GetFrameState(const char* frameName)
{
    return nullptr;
}

UsdRenderManager::FrameState* UsdRenderManager::GetFrameStateInternal(const char* frameName)
{
    return nullptr;
}

void UsdRenderManager::SetFrameRenderer(const char* frameName, const char* hydraRendererName)
{
}

void UsdRenderManager::SetFrameCamera(const char* frameName, const pxr::SdfPath& cameraPath)
{
}

void UsdRenderManager::SetFrameWorld(const char* frameName, const pxr::SdfPath& worldPath)
{
}

void UsdRenderManager::Render(const char* frameName, uint32_t width, uint32_t height, double timeStep)
{
}

bool UsdRenderManager::FrameReady(const char* frameName, bool wait)
{
    return true;
}

void* UsdRenderManager::MapFrame(const char* frameName, UsdBridgeType& returnFormat)
{
    return nullptr;
}

void UsdRenderManager::UnmapFrame(const char* frameName)
{
}

bool UsdRenderManager::IsRenderingEnabled() const
{
    return false;
}

UsdBridgeRendererCore* UsdRenderManager::GetOrCreateCore(const char* hydraRendererName)
{
    return nullptr;
}

#endif // USD_DEVICE_RENDERING_ENABLED
