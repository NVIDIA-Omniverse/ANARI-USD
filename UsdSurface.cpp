// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdSurface.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdDevice.h"
#include "UsdMaterial.h"
#include "UsdGeometry.h"

DEFINE_PARAMETER_MAP(UsdSurface,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("geometry", ANARI_GEOMETRY, geometry)
  REGISTER_PARAMETER_MACRO("material", ANARI_MATERIAL, material)
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

void UsdSurface::commit(UsdDevice* device)
{
  if(!usdBridge)
    return;
    
  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateSurface(getName(), usdHandle);

  if (paramChanged || isNew)
  {
    double worldTimeStep = device->getParams().timeStep;

    // Make sure the references are updated on the Bridge side.
    if (paramData.geometry && paramData.material)
    {
      if(device->getParams().outputMaterial)
        usdBridge->SetGeometryMaterialRef(usdHandle, 
          paramData.geometry->getUsdHandle(), 
          paramData.material->getUsdHandle(), 
          worldTimeStep,
          paramData.geometry->getParams().timeStep,
          paramData.material->getParams().timeStep);
      else
        usdBridge->SetGeometryRef(usdHandle, 
          paramData.geometry->getUsdHandle(), 
          worldTimeStep,
          paramData.geometry->getParams().timeStep); 
    }
    else
    {
      if (!paramData.geometry)
      {
        usdBridge->DeleteGeometryRef(usdHandle, worldTimeStep);
      }
      if (!paramData.material && device->getParams().outputMaterial)
      {
        usdBridge->DeleteMaterialRef(usdHandle, worldTimeStep);
      }
    }

    paramChanged = false;
  }
}