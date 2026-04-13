// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeRenderSettingsState.h"

#include <pxr/base/vt/value.h>

#include <cassert>

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{
    template<typename T>
    T GetOrDefinePrim(const UsdStageRefPtr& stage, const SdfPath& path)
    {
        T prim = T::Get(stage, path);
        if (!prim)
        {
            prim = T::Define(stage, path);
        }
        assert(prim);
        return prim;
    }
}

void UsdBridgeRenderSettingsState::InitializePaths(const SdfPath& contextId)
{
    std::string contextName = contextId.GetName();
    SettingsPath = "/Render/" + contextName + "/Settings";
    ProductPath = "/Render/" + contextName + "/Product";
    VarPath = "/Render/" + contextName + "/Vars/LdrColor";
}

void UsdBridgeRenderSettingsState::CreatePrims(UsdStageRefPtr stage)
{
    if (!stage)
    {
        return;
    }

    Settings = GetOrDefinePrim<UsdRenderSettings>(stage, SdfPath(SettingsPath));
    Settings.CreateResolutionAttr();
    Settings.CreateCameraRel();

    Product = GetOrDefinePrim<UsdRenderProduct>(stage, SdfPath(ProductPath));
    Product.CreateResolutionAttr();
    Product.CreateCameraRel();
    Product.CreateOrderedVarsRel();

    SdfPath renderVarSdf(VarPath);
    UsdRenderVar renderVarPrim = GetOrDefinePrim<UsdRenderVar>(stage, renderVarSdf);
    renderVarPrim.CreateSourceNameAttr(VtValue(std::string("LdrColor")));
    Product.GetOrderedVarsRel().AddTarget(renderVarSdf);
}

bool UsdBridgeRenderSettingsState::SetResolution(uint32_t width, uint32_t height)
{
    if (width == CachedWidth && height == CachedHeight)
        return false;

    CachedWidth = width;
    CachedHeight = height;

    if (Settings)
        Settings.GetResolutionAttr().Set(GfVec2i((int)width, (int)height));
    if (Product)
        Product.GetResolutionAttr().Set(GfVec2i((int)width, (int)height));

    return true;
}

bool UsdBridgeRenderSettingsState::SetCameraPath(const SdfPath& cameraPath)
{
    if (cameraPath == CachedCameraPath)
        return false;

    CachedCameraPath = cameraPath;

    if (Settings)
    {
        Settings.GetCameraRel().ClearTargets(false);
        Settings.GetCameraRel().AddTarget(cameraPath);
    }
    if (Product)
    {
        Product.GetCameraRel().ClearTargets(false);
        Product.GetCameraRel().AddTarget(cameraPath);
    }

    return true;
}
