// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdWorld.h"
#include "UsdAnari.h"
#include "UsdInstance.h"
#include "UsdSurface.h"
#include "UsdVolume.h"
#include "UsdDevice.h"
#include "UsdDataArray.h"
#include "UsdGroup.h"

#define InstanceType ANARI_INSTANCE
#define SurfaceType ANARI_SURFACE
#define VolumeType ANARI_VOLUME
using InstanceUsdType = AnariToUsdBridgedObject<InstanceType>::Type;
using SurfaceUsdType = AnariToUsdBridgedObject<SurfaceType>::Type;
using VolumeUsdType = AnariToUsdBridgedObject<VolumeType>::Type;

DEFINE_PARAMETER_MAP(UsdWorld,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timeVarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("instance", ANARI_ARRAY, instances)
  REGISTER_PARAMETER_MACRO("surface", ANARI_ARRAY, surfaces)
  REGISTER_PARAMETER_MACRO("volume", ANARI_ARRAY, volumes)
)

constexpr UsdWorld::ComponentPair UsdWorld::componentParamNames[]; // Workaround for C++14's lack of inlining constexpr arrays

UsdWorld::UsdWorld(const char* name, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_WORLD, name, device)
{
}

UsdWorld::~UsdWorld()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(cachedBridge)
    cachedBridge->DeleteWorld(usdHandle);
#endif
}

void UsdWorld::remove(UsdDevice* device)
{
  applyRemoveFunc(device, &UsdBridge::DeleteWorld);
}

bool UsdWorld::deferCommit(UsdDevice* device)
{
  const UsdWorldData& paramData = getReadParams();

  if(UsdObjectNotInitialized<InstanceUsdType>(paramData.instances) ||
    UsdObjectNotInitialized<SurfaceUsdType>(paramData.surfaces) || 
    UsdObjectNotInitialized<VolumeUsdType>(paramData.volumes))
  {
    return true;
  }
  return false;
}

bool UsdWorld::doCommitData(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();

  const char* objName = getName();

  bool isNew = false;
  if(!usdHandle.value)
    isNew = usdBridge->CreateWorld(objName, usdHandle);

  if (paramChanged || isNew)
  {
    doCommitRefs(device); // Perform immediate commit of refs - no params from children required

    paramChanged = false;
  }

  return false;
}

void UsdWorld::doCommitRefs(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdWorldData& paramData = getReadParams();
  double timeStep = device->getReadParams().timeStep;

  const char* objName = getName();

  bool instancesTimeVarying = isTimeVarying(UsdWorldComponents::INSTANCES);
  bool surfacesTimeVarying = isTimeVarying(UsdWorldComponents::SURFACES);
  bool volumesTimeVarying = isTimeVarying(UsdWorldComponents::VOLUMES);

  UsdLogInfo logInfo(device, this, ANARI_WORLD, this->getName());

  ManageRefArray<InstanceType, ANARIInstance, UsdInstance>(usdHandle, paramData.instances, instancesTimeVarying, timeStep,
    instanceHandles, instanceableValues, &UsdBridge::SetInstanceRefs, &UsdBridge::DeleteInstanceRefs,
    usdBridge, logInfo, "UsdWorld commit failed: 'instance' array elements should be of type ANARI_INSTANCE");

  ManageRefArray<SurfaceType, ANARISurface, UsdSurface>(usdHandle, paramData.surfaces, surfacesTimeVarying, timeStep,
    surfaceHandles, instanceableValues, &UsdBridge::SetSurfaceRefs, &UsdBridge::DeleteSurfaceRefs,
    usdBridge, logInfo, "UsdGroup commit failed: 'surface' array elements should be of type ANARI_SURFACE");

  ManageRefArray<VolumeType, ANARIVolume, UsdVolume>(usdHandle, paramData.volumes, volumesTimeVarying, timeStep,
    volumeHandles, instanceableValues, &UsdBridge::SetVolumeRefs, &UsdBridge::DeleteVolumeRefs,
    usdBridge, logInfo, "UsdGroup commit failed: 'volume' array elements should be of type ANARI_VOLUME");
}