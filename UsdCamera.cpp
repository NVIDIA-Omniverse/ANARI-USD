// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdCamera.h"
#include "UsdAnari.h"
#include "UsdDataArray.h"
#include "UsdDevice.h"

DEFINE_PARAMETER_MAP(UsdCamera,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timeVarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("position", ANARI_FLOAT32_VEC3, position)
  REGISTER_PARAMETER_MACRO("direction", ANARI_FLOAT32_VEC3, direction)
  REGISTER_PARAMETER_MACRO("up", ANARI_FLOAT32_VEC3, up)
  REGISTER_PARAMETER_MACRO("imageRegion", ANARI_FLOAT32_BOX2, imageRegion)
  REGISTER_PARAMETER_MACRO("aspect", ANARI_FLOAT32, aspect)
  REGISTER_PARAMETER_MACRO("near", ANARI_FLOAT32, near)
  REGISTER_PARAMETER_MACRO("far", ANARI_FLOAT32, far)
  REGISTER_PARAMETER_MACRO("fovy", ANARI_FLOAT32, fovy)
  REGISTER_PARAMETER_MACRO("height", ANARI_FLOAT32, height)
)

constexpr UsdCamera::ComponentPair UsdCamera::componentParamNames[]; // Workaround for C++14's lack of inlining constexpr arrays

UsdCamera::UsdCamera(const char* name, const char* type, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_CAMERA, name, device)
{
  if(strEquals(type, "perspective"))
    cameraType = CAMERA_PERSPECTIVE;
  else if(strEquals(type, "orthographic"))
    cameraType = CAMERA_ORTHOGRAPHIC;
}

UsdCamera::~UsdCamera()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(cachedBridge)
    cachedBridge->DeleteCamera(usdHandle);
#endif
}

void UsdCamera::remove(UsdDevice* device)
{
  applyRemoveFunc(device, &UsdBridge::DeleteCamera);
}

bool UsdCamera::deferCommit(UsdDevice* device)
{
  return false;
}

void UsdCamera::copyParameters(UsdBridgeCameraData& camData)
{
  typedef UsdBridgeCameraData::DataMemberId DMI;

  const UsdCameraData& paramData = getReadParams();

  camData.Position = paramData.position;
  camData.Direction = paramData.direction;
  camData.Up = paramData.up;
  camData.ImageRegion = paramData.imageRegion;
  camData.Aspect = paramData.aspect;
  camData.Near = paramData.near;
  camData.Far = paramData.far;
  camData.Fovy = paramData.fovy;
  camData.Height = paramData.height;

  camData.TimeVarying = DMI::ALL
    & (isTimeVarying(CType::VIEW) ? DMI::ALL : ~DMI::VIEW)
    & (isTimeVarying(CType::PROJECTION) ? DMI::ALL : ~DMI::PROJECTION);
}

bool UsdCamera::doCommitData(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdCameraData& paramData = getReadParams();
  double timeStep = device->getReadParams().timeStep;

  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateCamera(getName(), usdHandle);

  if (paramChanged || isNew)
  {
    UsdBridgeCameraData camData;
    copyParameters(camData);

    usdBridge->SetCameraData(usdHandle, camData, timeStep);

    paramChanged = false;
  }
  return false;
}