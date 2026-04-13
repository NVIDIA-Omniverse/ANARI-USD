// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeRenderSettingsState_h
#define UsdBridgeRenderSettingsState_h

#include "usd.h"

#include <pxr/usd/usdRender/product.h>
#include <pxr/usd/usdRender/settings.h>
#include <pxr/usd/usdRender/var.h>

#include <string>

//
// Owns UsdRenderSettings / UsdRenderProduct / RenderVar prims for one frame.
// Managed by UsdRenderManager; render contexts do not interact with this class.
//
class UsdBridgeRenderSettingsState
{
public:
    void InitializePaths(const pxr::SdfPath& contextId);
    void CreatePrims(pxr::UsdStageRefPtr stage);
    bool SetResolution(uint32_t width, uint32_t height);
    bool SetCameraPath(const pxr::SdfPath& cameraPath);

private:
    std::string SettingsPath;
    std::string ProductPath;
    std::string VarPath;

    pxr::UsdRenderSettings Settings;
    pxr::UsdRenderProduct Product;

    uint32_t CachedWidth = 0;
    uint32_t CachedHeight = 0;
    pxr::SdfPath CachedCameraPath;
};

#endif
