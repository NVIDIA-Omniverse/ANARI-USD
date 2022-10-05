// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdGroup.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDataArray.h"
#include "UsdDevice.h"
#include "UsdSurface.h"
#include "UsdVolume.h"

#define SurfaceType ANARI_SURFACE
#define VolumeType ANARI_VOLUME
using SurfaceUsdType = AnariToUsdBridgedObject<SurfaceType>::Type;
using VolumeUsdType = AnariToUsdBridgedObject<VolumeType>::Type;

DEFINE_PARAMETER_MAP(UsdGroup,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timeVarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("surface", ANARI_ARRAY, surfaces)
  REGISTER_PARAMETER_MACRO("volume", ANARI_ARRAY, volumes)
)

UsdGroup::UsdGroup(const char* name, UsdBridge* bridge,
  UsdDevice* device)
  : BridgedBaseObjectType(ANARI_GROUP, name, bridge)
{
}

UsdGroup::~UsdGroup()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(usdBridge)
    usdBridge->DeleteGroup(usdHandle);
#endif
}

void UsdGroup::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  if (filterNameParam(name, type, mem, device))
    setParam(name, type, mem, device);
}

void UsdGroup::filterResetParam(const char *name)
{
  resetParam(name);
}

bool UsdGroup::deferCommit(UsdDevice* device)
{
  const UsdGroupData& paramData = getReadParams();

  if(UsdObjectNotInitialized<SurfaceUsdType>(paramData.surfaces) || 
    UsdObjectNotInitialized<VolumeUsdType>(paramData.volumes))
  {
    return true;
  }
  return false;
}

bool UsdGroup::doCommitData(UsdDevice* device)
{
  if(!usdBridge)
    return false;

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
  const UsdGroupData& paramData = getReadParams();

  UsdLogInfo logInfo(device, this, ANARI_GROUP, this->getName());
  bool validRefs = 
    AssertArrayType(paramData.surfaces, SurfaceType, logInfo, "UsdGroup commit failed: 'surface' array elements should be of type ANARI_SURFACE") &&
    AssertArrayType(paramData.volumes, VolumeType, logInfo, "UsdGroup commit failed: 'volume' array elements should be of type ANARI_VOLUME");

  if(validRefs)
  {
    double timeStep = device->getReadParams().timeStep;
    bool surfacesTimeVarying = paramData.timeVarying & 1;
    bool volumesTimeVarying = paramData.timeVarying & (1 << 1);

    if (paramData.surfaces)
    {
      const ANARISurface* surfaces = reinterpret_cast<const ANARISurface*>(paramData.surfaces->getData());

      uint64_t numModels = paramData.surfaces->getLayout().numItems1;
      surfaceHandles.resize(numModels);
      for (uint64_t i = 0; i < numModels; ++i)
      {
        const UsdSurface* usdSurface = reinterpret_cast<const UsdSurface*>(surfaces[i]);
        surfaceHandles[i] = usdSurface->getUsdHandle();
      }

      usdBridge->SetSurfaceRefs(usdHandle, &surfaceHandles[0], numModels, surfacesTimeVarying, timeStep);
    }
    else
    {
      usdBridge->DeleteSurfaceRefs(usdHandle, surfacesTimeVarying, timeStep);
    }

    if (paramData.volumes)
    {
      const ANARIVolume* volumes = reinterpret_cast<const ANARIVolume*>(paramData.volumes->getData());

      uint64_t numModels = paramData.volumes->getLayout().numItems1;
      volumeHandles.resize(numModels);
      for (uint64_t i = 0; i < numModels; ++i)
      {
        const UsdVolume* usdVolume = reinterpret_cast<const UsdVolume*>(volumes[i]);
        volumeHandles[i] = usdVolume->getUsdHandle();
      }

      usdBridge->SetVolumeRefs(usdHandle, &volumeHandles[0], numModels, volumesTimeVarying, timeStep);
    }
    else
    {
      usdBridge->DeleteVolumeRefs(usdHandle, volumesTimeVarying, timeStep);
    }
  }
}