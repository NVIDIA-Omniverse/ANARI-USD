// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdFrame.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdCamera.h"
#include "UsdRenderer.h"
#include "UsdWorld.h"
#include "anari/frontend/type_utility.h"

#define CameraType ANARI_CAMERA
#define RendererType ANARI_RENDERER
#define WorldType ANARI_WORLD
using CameraUsdType = AnariToUsdBridgedObject<CameraType>::Type;
using WorldUsdType = AnariToUsdBridgedObject<WorldType>::Type;

DEFINE_PARAMETER_MAP(UsdFrame,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("size", ANARI_UINT32_VEC2, size)
  REGISTER_PARAMETER_MACRO("channel.color", ANARI_DATA_TYPE, color)
  REGISTER_PARAMETER_MACRO("channel.depth", ANARI_DATA_TYPE, depth)
  REGISTER_PARAMETER_MACRO("camera", CameraType, camera)
  REGISTER_PARAMETER_MACRO("renderer", RendererType, renderer)
  REGISTER_PARAMETER_MACRO("world", WorldType, world)
  REGISTER_PARAMETER_MACRO("usd::time", ANARI_FLOAT64, time)
)

UsdFrame::UsdFrame(const char* name, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_FRAME, name, device)
{
  cachedBridge = device->getUsdBridge();
}

UsdFrame::~UsdFrame()
{
  if (registeredFrameState)
  {
    cachedBridge->UnregisterFrame(getName());
  }
  delete[] mappedColorMem;
  delete[] mappedDepthMem;
}

void UsdFrame::remove(UsdDevice* device)
{
  if (registeredFrameState)
  {
    cachedBridge->UnregisterFrame(getName());
    registeredFrameState = nullptr;
  }
}

bool UsdFrame::deferCommit(UsdDevice* device)
{
  return false;
}

bool UsdFrame::doCommitData(UsdDevice* device)
{
  if (!cachedBridge)
    return false;

  // The name of this frame determines the renderproduct prim registration in USD.
  // Check if name changed or first registration
  const char* frameName = getName();
  void* currentFrameState = cachedBridge->GetFrameState(frameName);

  if (registeredFrameState == nullptr || registeredFrameState != currentFrameState)
  {
    // Name changed or first registration - unregister old and register new
    if (registeredFrameState)
    {
      // Name changed - unregister old frame by its state pointer
      cachedBridge->UnregisterFrameByState(registeredFrameState);
    }

    cachedBridge->RegisterFrame(frameName);
    registeredFrameState = cachedBridge->GetFrameState(frameName);
  }

  return false;
}

const void* UsdFrame::mapBuffer(
  const char *channel,
  uint32_t *width,
  uint32_t *height,
  ANARIDataType *pixelType,
  UsdDevice* device)
{
  *width = renderBufferSize.Data[0];
  *height = renderBufferSize.Data[1];
  *pixelType = ANARI_UNKNOWN;

  if (strEquals(channel, "channel.color"))
  {
    // Only attempt to map from bridge if frame is registered
    if (registeredFrameState)
    {
      UsdBridgeType usdBridgeFormat;
      if(void* deviceBuffer = cachedBridge->MapFrame(getName(), usdBridgeFormat))
      {
        *pixelType = (usdBridgeFormat == UsdBridgeType::UCHAR4) ?
          ANARI_UFIXED8_VEC4 : UsdBridgeToAnariType(usdBridgeFormat);
        return deviceBuffer;
      }
    }

    mappedColorMem = ReserveBuffer(renderBufferColorFormat);
    *pixelType = renderBufferColorFormat;
    return mappedColorMem;
  }
  else if (strEquals(channel, "channel.depth"))
  {
    mappedDepthMem = ReserveBuffer(renderBufferDepthFormat);
    *pixelType = renderBufferDepthFormat;
    return mappedDepthMem;
  }

  *width = 0;
  *height = 0;

  return nullptr;
}

void UsdFrame::unmapBuffer(const char *channel, UsdDevice* device)
{
  if (registeredFrameState)
  {
    cachedBridge->UnmapFrame(getName());
  }

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
  size_t formatSize = anari::sizeOf(format);
  size_t memSize = formatSize * renderBufferSize.Data[0] * renderBufferSize.Data[1];
  return new char[memSize];
}

void UsdFrame::saveUsd(UsdDevice* device)
{
  if (cachedBridge)
  {
    cachedBridge->SaveScene();
  }
}

void UsdFrame::renderFrame(UsdDevice* device)
{
  const UsdFrameData& paramData = getReadParams();

  // Ensure camera has a valid handle before rendering
  if(UsdObjectNotInitialized<CameraUsdType>(paramData.camera))
    return;

  if (!cachedBridge || !registeredFrameState)
    return;

  const char* frameName = getName();

  // Set up the renderer for this frame
  const char* hydraRendererName = nullptr;
  if(paramData.renderer)
  {
    hydraRendererName = paramData.renderer->getHydraRendererName();
  }
  cachedBridge->SetFrameRenderer(frameName, hydraRendererName);

  // Set world path if world is provided and has a valid handle
  if(!UsdObjectNotInitialized<WorldUsdType>(paramData.world))
  {
    UsdWorldHandle worldHandle = paramData.world->getUsdHandle();
    cachedBridge->SetFrameWorld(frameName, worldHandle);
  }

  // Set camera
  UsdCameraHandle cameraHandle = paramData.camera->getUsdHandle();
  cachedBridge->SetFrameCamera(frameName, cameraHandle);

  // Cache the renderbuffer properties belonging to this frame
  renderBufferSize = paramData.size;
  renderBufferColorFormat = paramData.color;
  renderBufferDepthFormat = paramData.depth;

  // Render (to a renderbuffer)
  cachedBridge->RenderFrame(frameName, paramData.size.Data[0], paramData.size.Data[1], paramData.time);
}

bool UsdFrame::frameReady(ANARIWaitMask mask, UsdDevice* device)
{
  if (!registeredFrameState)
    return true;

  return cachedBridge->FrameReady(getName(), mask == ANARI_WAIT);
}
