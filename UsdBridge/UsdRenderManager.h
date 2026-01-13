// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdRenderManager_h
#define UsdRenderManager_h

#include "UsdBridgeData.h"
#include "usd.h"
#include <memory>
#include <unordered_map>
#include <string>

class UsdBridgeUsdWriter;
class UsdBridgeRendererCore;
class UsdBridgeRenderContext;

//
// UsdRenderManager - Manages multiple renderer cores and render contexts
//
// - One core per Hydra renderer plugin (e.g., HdStorm, HdCycles)
// - One context per frame (each frame can have its own camera/world)
//
class UsdRenderManager
{
public:
    UsdRenderManager(UsdBridgeUsdWriter& usdWriter);
    ~UsdRenderManager();

    // Frame lifecycle
    void RegisterFrame(const char* frameName);
    void UnregisterFrame(const char* frameName);
    void UnregisterFrameByState(void* frameState); // For unregistering when name changed

    // Configure a frame's rendering setup
    void SetFrameRenderer(const char* frameName, const char* hydraRendererName);
    void SetFrameCamera(const char* frameName, const pxr::SdfPath& cameraPath);
    void SetFrameWorld(const char* frameName, const pxr::SdfPath& worldPath);

    // Render operations
    void Render(const char* frameName, uint32_t width, uint32_t height, double timeStep);
    bool FrameReady(const char* frameName, bool wait);
    void* MapFrame(const char* frameName, UsdBridgeType& returnFormat);
    void UnmapFrame(const char* frameName);

    // Check if rendering is supported
    bool IsRenderingEnabled() const;

    // Get frame state pointer for identity comparison (returns nullptr if not registered)
    void* GetFrameState(const char* frameName);

private:
    // Get or create a core for the given renderer name
    UsdBridgeRendererCore* GetOrCreateCore(const char* hydraRendererName);

    // Internal frame state access
    struct FrameState;
    FrameState* GetFrameStateInternal(const char* frameName);

    struct FrameState
    {
        std::string RendererName;
        pxr::SdfPath CameraPath;
        pxr::SdfPath WorldPath;
        UsdBridgeRendererCore* Core = nullptr;
        std::unique_ptr<UsdBridgeRenderContext> Context;
    };

    UsdBridgeUsdWriter& UsdWriter;

    // Map of renderer name -> core
    std::unordered_map<std::string, std::unique_ptr<UsdBridgeRendererCore>> Cores;

    // Map of frame name -> frame state
    std::unordered_map<std::string, FrameState> Frames;

    // Default renderer name
    static constexpr const char* DefaultRendererName = "HdStormRendererPlugin";
};

#endif
