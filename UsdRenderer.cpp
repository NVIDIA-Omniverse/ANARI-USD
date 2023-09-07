// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdRenderer.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDevice.h"

DEFINE_PARAMETER_MAP(UsdRenderer,
)

UsdRenderer::UsdRenderer()
  : UsdParameterizedBaseObject<UsdRenderer, UsdRendererData>(ANARI_RENDERER)
{
}

UsdRenderer::~UsdRenderer()
{

}

bool UsdRenderer::deferCommit(UsdDevice* device)
{
  return false;
}

bool UsdRenderer::doCommitData(UsdDevice* device)
{
  return false;
}

void UsdRenderer::saveUsd(UsdDevice* device)
{
  device->getUsdBridge()->SaveScene();
}
