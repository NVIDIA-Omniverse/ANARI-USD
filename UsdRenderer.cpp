// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdRenderer.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdDevice_queries.h"

DEFINE_PARAMETER_MAP(UsdRenderer,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::hydraRendererName", ANARI_STRING, hydraRendererName)
)

UsdRenderer::UsdRenderer()
  : UsdParameterizedBaseObject<UsdRenderer, UsdRendererData>(ANARI_RENDERER)
{
}

UsdRenderer::~UsdRenderer()
{
}

int UsdRenderer::getProperty(const char * name, ANARIDataType type, void * mem, uint64_t size, UsdDevice* device)
{
  if (strEquals(name, "extension") && type == ANARI_STRING_LIST)
  {
    writeToVoidP(mem, anari::usd::query_extensions());
    return 1;
  }

  return 0;
}

const char* UsdRenderer::getHydraRendererName() const
{
  const UsdRendererData& paramData = getReadParams();
  return paramData.hydraRendererName ? paramData.hydraRendererName->c_str() : nullptr;
}

bool UsdRenderer::deferCommit(UsdDevice* device)
{
  return false;
}

bool UsdRenderer::doCommitData(UsdDevice* device)
{
  return false;
}
