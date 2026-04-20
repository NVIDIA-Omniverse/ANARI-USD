// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdGroup.h"
#include "UsdAnari.h"
#include "UsdDataArray.h"
#include "UsdDevice.h"
#include "UsdLight.h"
#include "UsdSurface.h"
#include "UsdVolume.h"

#define SurfaceType ANARI_SURFACE
#define VolumeType ANARI_VOLUME
#define LightType ANARI_LIGHT
using SurfaceUsdType = AnariToUsdBridgedObject<SurfaceType>::Type;
using VolumeUsdType = AnariToUsdBridgedObject<VolumeType>::Type;
using LightUsdType = AnariToUsdBridgedObject<LightType>::Type;

DEFINE_PARAMETER_MAP(UsdGroup,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timeVarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("surface", ANARI_ARRAY, surfaces)
  REGISTER_PARAMETER_MACRO("volume", ANARI_ARRAY, volumes)
  REGISTER_PARAMETER_MACRO("light", ANARI_ARRAY, lights)
)

constexpr UsdGroup::ComponentPair UsdGroup::componentParamNames[]; // Workaround for C++14's lack of inlining constexpr arrays

UsdGroup::UsdGroup(const char* name,
  UsdDevice* device)
  : BridgedBaseObjectType(ANARI_GROUP, name, device)
{
}

UsdGroup::~UsdGroup()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(cachedBridge)
    cachedBridge->DeleteGroup(usdHandle);
#endif
}

void UsdGroup::remove(UsdDevice* device)
{
  applyRemoveFunc(device, &UsdBridge::DeleteGroup);
}

bool UsdGroup::deferCommit(UsdDevice* device)
{
  const UsdGroupData& paramData = getReadParams();

  if(UsdObjectNotInitialized<SurfaceUsdType>(paramData.surfaces) ||
    UsdObjectNotInitialized<VolumeUsdType>(paramData.volumes) ||
    UsdObjectNotInitialized<LightUsdType>(paramData.lights))
  {
    return true;
  }
  return false;
}

bool UsdGroup::doCommitData(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();

  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateGroup(getName(), usdHandle);

  if (paramChanged || isNew)
  {
    doCommitRefs(device); // Perform immediate commit of refs - no params from children required

    paramChanged = false;
  }
  return false;
}


void UsdGroup::doCommitRefs(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdGroupData& paramData = getReadParams();
  double timeStep = device->getReadParams().timeStep;

  UsdLogInfo logInfo(device, this, ANARI_GROUP, this->getName());

  bool surfacesTimeVarying = isTimeVarying(UsdGroupComponents::SURFACES);
  bool volumesTimeVarying = isTimeVarying(UsdGroupComponents::VOLUMES);
  bool lightsTimeVarying = isTimeVarying(UsdGroupComponents::LIGHTS);

  ManageRefArray<SurfaceType, ANARISurface, UsdSurface>(usdHandle, paramData.surfaces, surfacesTimeVarying, timeStep,
    surfaceHandles, instanceableValues, &UsdBridge::SetSurfaceRefs, &UsdBridge::DeleteSurfaceRefs,
    usdBridge, logInfo, "UsdGroup commit failed: 'surface' array elements should be of type ANARI_SURFACE");

  ManageRefArray<VolumeType, ANARIVolume, UsdVolume>(usdHandle, paramData.volumes, volumesTimeVarying, timeStep,
    volumeHandles, instanceableValues, &UsdBridge::SetVolumeRefs, &UsdBridge::DeleteVolumeRefs,
    usdBridge, logInfo, "UsdGroup commit failed: 'volume' array elements should be of type ANARI_VOLUME");

  ManageRefArray<LightType, ANARILight, UsdLight>(usdHandle, paramData.lights, lightsTimeVarying, timeStep,
    lightHandles, instanceableValues, &UsdBridge::SetLightRefs, &UsdBridge::DeleteLightRefs,
    usdBridge, logInfo, "UsdGroup commit failed: 'light' array elements should be of type ANARI_LIGHT");
}