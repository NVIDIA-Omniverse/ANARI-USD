// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdFrame.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdCamera.h"
#include "anari/frontend/type_utility.h"

#define CameraType ANARI_CAMERA
using CameraUsdType = AnariToUsdBridgedObject<CameraType>::Type;

DEFINE_PARAMETER_MAP(UsdFrame,
  REGISTER_PARAMETER_MACRO("size", ANARI_UINT32_VEC2, size)
  REGISTER_PARAMETER_MACRO("channel.color", ANARI_DATA_TYPE, color)
  REGISTER_PARAMETER_MACRO("channel.depth", ANARI_DATA_TYPE, depth)
  REGISTER_PARAMETER_MACRO("camera", CameraType, camera)
  REGISTER_PARAMETER_MACRO("usd::time", ANARI_FLOAT64, time)
)

UsdFrame::UsdFrame(UsdBridge* bridge)
  : UsdParameterizedBaseObject<UsdFrame, UsdFrameData>(ANARI_FRAME)
{
}

UsdFrame::~UsdFrame()
{
  delete[] mappedColorMem;
  delete[] mappedDepthMem;
}

bool UsdFrame::deferCommit(UsdDevice* device)
{
  return false;
}

bool UsdFrame::doCommitData(UsdDevice* device)
{
  return false;
}

const void* UsdFrame::mapBuffer(const char *channel,
  uint32_t *width,
  uint32_t *height,
  ANARIDataType *pixelType)
{
  const UsdFrameData& paramData = getReadParams();

  *width = paramData.size.Data[0];
  *height = paramData.size.Data[1];
  *pixelType = ANARI_UNKNOWN;

  if (strEquals(channel, "channel.color"))
  {
    mappedColorMem = ReserveBuffer(paramData.color);
    *pixelType = paramData.color;
    return mappedColorMem;
  }
  else if (strEquals(channel, "channel.depth"))
  {
    mappedDepthMem = ReserveBuffer(paramData.depth);
    *pixelType = paramData.depth;
    return mappedDepthMem;
  }

  *width = 0;
  *height = 0;

  return nullptr;
}

void UsdFrame::unmapBuffer(const char *channel)
{
  if (strEquals(channel, "channel.color"))
  {
    delete[] mappedColorMem;
    mappedColorMem = nullptr;
  }
  else if (strEquals(channel, "channel.depth"))
  {
    delete[] mappedDepthMem;
    mappedDepthMem = nullptr;
  }
}

char* UsdFrame::ReserveBuffer(ANARIDataType format)
{
  const UsdFrameData& paramData = getReadParams();
  size_t formatSize = anari::sizeOf(format);
  size_t memSize = formatSize * paramData.size.Data[0] * paramData.size.Data[1];
  return new char[memSize];
}

void UsdFrame::saveUsd(UsdDevice* device)
{
  device->getUsdBridge()->SaveScene();
}

void UsdFrame::renderFrame(UsdDevice* device)
{
  const UsdFrameData& paramData = getReadParams();

  if(paramData.camera)
  {
    UsdCameraHandle cameraHandle = paramData.camera->getUsdHandle();
    device->getUsdBridge()->SetRenderCamera(cameraHandle);

    device->getUsdBridge()->RenderFrame(paramData.size.Data[0], paramData.size.Data[1], paramData.time);
  }
}
