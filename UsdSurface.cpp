// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdSurface.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdMaterial.h"
#include "UsdGeometry.h"

#define GeometryType ANARI_GEOMETRY
#define MaterialType ANARI_MATERIAL
using GeometryUsdType = AnariToUsdBridgedObject<GeometryType>::Type;
using MaterialUsdType = AnariToUsdBridgedObject<MaterialType>::Type;

DEFINE_PARAMETER_MAP(UsdSurface,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("geometry", GeometryType, geometry)
  REGISTER_PARAMETER_MACRO("material", MaterialType, material)
)

UsdSurface::UsdSurface(const char* name, UsdBridge* bridge, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_SURFACE, name, bridge)
{
}

UsdSurface::~UsdSurface()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  // Given that the object is destroyed, none of its references to other objects
  // has to be updated anymore.
  if (usdBridge)
    usdBridge->DeleteSurface(usdHandle);
#endif
}

void UsdSurface::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  if (filterNameParam(name, type, mem, device))
    setParam(name, type, mem, device);
}

void UsdSurface::filterResetParam(const char *name)
{
  resetParam(name);
}

bool UsdSurface::deferCommit(UsdDevice* device)
{
  const UsdSurfaceData& paramData = getReadParams();

  if(UsdObjectNotInitialized<GeometryUsdType>(paramData.geometry) || UsdObjectNotInitialized<MaterialUsdType>(paramData.material))
  {
    return true;
  }
  return false;
}

bool UsdSurface::doCommitData(UsdDevice* device)
{
  if(!usdBridge)
    return false;
    
  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateSurface(getName(), usdHandle);

  if (paramChanged || isNew)
  {
    paramChanged = false;

    return true;
  }

  return false;
}

void UsdSurface::doCommitRefs(UsdDevice* device)
{
  double worldTimeStep = device->getReadParams().timeStep;

  const UsdSurfaceData& paramData = getReadParams();

  // Make sure the references are updated on the Bridge side.
  if (paramData.geometry && (!device->getReadParams().outputMaterial || paramData.material))
  {
    if(device->getReadParams().outputMaterial)
      usdBridge->SetGeometryMaterialRef(usdHandle, 
        paramData.geometry->getUsdHandle(), 
        paramData.material->getUsdHandle(), 
        worldTimeStep,
        paramData.geometry->getReadParams().timeStep,
        paramData.material->getReadParams().timeStep);
    else
      usdBridge->SetGeometryRef(usdHandle, 
        paramData.geometry->getUsdHandle(), 
        worldTimeStep,
        paramData.geometry->getReadParams().timeStep); 
  }
  else
  {
    if (!paramData.geometry)
    {
      usdBridge->DeleteGeometryRef(usdHandle, worldTimeStep);
    }
    if (!paramData.material && device->getReadParams().outputMaterial)
    {
      usdBridge->DeleteMaterialRef(usdHandle, worldTimeStep);
    }
  }
}