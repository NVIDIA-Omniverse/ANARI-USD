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

void UsdWorld::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  if (filterNameParam(name, type, mem, device))
    setParam(name, type, mem, device);
}

void UsdWorld::filterResetParam(const char *name)
{
  resetParam(name);
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

  bool instancesTimeVarying = paramData.timeVarying & 1;
  bool surfacesTimeVarying = paramData.timeVarying & (1 << 1);
  bool volumesTimeVarying = paramData.timeVarying & (1 << 2);

  UsdLogInfo logInfo(device, this, ANARI_WORLD, this->getName());

  ManageRefArray<InstanceType, ANARIInstance, UsdInstance>(usdHandle, paramData.instances, instancesTimeVarying, timeStep,
    instanceHandles, &UsdBridge::SetInstanceRefs, &UsdBridge::DeleteInstanceRefs,
    usdBridge, logInfo, "UsdWorld commit failed: 'instance' array elements should be of type ANARI_INSTANCE");

  ManageRefArray<SurfaceType, ANARISurface, UsdSurface>(usdHandle, paramData.surfaces, surfacesTimeVarying, timeStep,
    surfaceHandles, &UsdBridge::SetSurfaceRefs, &UsdBridge::DeleteSurfaceRefs,
    usdBridge, logInfo, "UsdGroup commit failed: 'surface' array elements should be of type ANARI_SURFACE");

  ManageRefArray<VolumeType, ANARIVolume, UsdVolume>(usdHandle, paramData.volumes, volumesTimeVarying, timeStep,
    volumeHandles, &UsdBridge::SetVolumeRefs, &UsdBridge::DeleteVolumeRefs,
    usdBridge, logInfo, "UsdGroup commit failed: 'volume' array elements should be of type ANARI_VOLUME");
}