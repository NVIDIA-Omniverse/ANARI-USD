// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdRenderer.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDevice.h"

DEFINE_PARAMETER_MAP(UsdRenderer,
)

UsdRenderer::UsdRenderer(UsdBridge* bridge)
  : UsdBaseObject(ANARI_RENDERER)
  , usdBridge(bridge)
{
}

UsdRenderer::~UsdRenderer()
{

}

void UsdRenderer::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
}

void UsdRenderer::filterResetParam(const char *name)
{
}

int UsdRenderer::getProperty(const char * name, ANARIDataType type, void * mem, uint64_t size, UsdDevice * device)
{
  return 0;
}

void UsdRenderer::doCommitWork(UsdDevice* device)
{
  
}

void UsdRenderer::saveUsd()
{
  if(!usdBridge)
    return;

  usdBridge->SaveScene();
}
