// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeUsdRenderer_h
#define UsdBridgeUsdRenderer_h

#include "usd.h"
#include "UsdBridgeData.h"

class UsdBridgeUsdWriter;
class UsdBridgeRendererInternals;

// Using hydra to render to a pixel buffer
class UsdBridgeRenderer
{
public:
    UsdBridgeRenderer(UsdBridgeUsdWriter& usdWriter);
    ~UsdBridgeRenderer();

    // Sets up the scene and render delegates
    void Initialize(const char* rendererName = "HdStormRenderer");

    void SetCameraPath(const pxr::SdfPath& cameraPath);

    void Render(uint32_t width, uint32_t height, double timeStep);

    UsdBridgeUsdWriter& UsdWriter;
    UsdBridgeRendererInternals* Internals = nullptr;
};

#endif