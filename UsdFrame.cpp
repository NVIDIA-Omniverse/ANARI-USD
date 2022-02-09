// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdFrame.h"
#include "UsdDevice.h"
#include "UsdAnari.h"
#include "UsdBridge/UsdBridge.h"

DEFINE_PARAMETER_MAP(UsdFrame,
  REGISTER_PARAMETER_MACRO("world", ANARI_WORLD, world)
  REGISTER_PARAMETER_MACRO("renderer", ANARI_RENDERER, renderer)
  REGISTER_PARAMETER_MACRO("size", ANARI_UINT32_VEC2, size)
  REGISTER_PARAMETER_MACRO("color", ANARI_DATA_TYPE, color)
  REGISTER_PARAMETER_MACRO("depth", ANARI_DATA_TYPE, depth)
)

UsdFrame::UsdFrame(UsdBridge* bridge)
  : UsdBaseObject(ANARI_FRAME)
{
}

UsdFrame::~UsdFrame()
{
  delete[] mappedColorMem;
  delete[] mappedDepthMem;
}

void UsdFrame::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  // No name parameter, so no filterNameParam check
  setParam(name, type, mem, device);
}

void UsdFrame::filterResetParam(const char *name)
{
  resetParam(name);
}

int UsdFrame::getProperty(const char * name, ANARIDataType type, void * mem, uint64_t size, UsdDevice * device)
{
  return 0;
}

void UsdFrame::commit(UsdDevice* device)
{
  
}

const void* UsdFrame::mapBuffer(const char *channel)
{
  if (strcmp(channel, "color") == 0)
  {
    mappedColorMem = ReserveBuffer(paramData.color);
    return mappedColorMem;
  }
  else if (strcmp(channel, "depth") == 0)
  {
    mappedDepthMem = ReserveBuffer(paramData.depth);
    return mappedDepthMem;
  }
  return nullptr;
}

void UsdFrame::unmapBuffer(const char *channel)
{
  if (strcmp(channel, "color") == 0)
  {
    delete[] mappedColorMem;
    mappedColorMem = nullptr;
  }
  else if (strcmp(channel, "depth") == 0)
  {
    delete[] mappedDepthMem;
    mappedDepthMem = nullptr;
  }
}

UsdRenderer* UsdFrame::getRenderer()
{
  return paramData.renderer;
}

char* UsdFrame::ReserveBuffer(ANARIDataType format)
{
  size_t formatSize = AnariTypeSize(format);
  size_t memSize = formatSize * paramData.size[0] * paramData.size[1];
  return new char[memSize];
}
