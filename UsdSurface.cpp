// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdSurface.h"
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
  REGISTER_PARAMETER_MACRO("usd::time::geometry", ANARI_FLOAT64, geometryRefTimeStep)
  REGISTER_PARAMETER_MACRO("usd::time::material", ANARI_FLOAT64, materialRefTimeStep)
  REGISTER_PARAMETER_MACRO("geometry", GeometryType, geometry)
  REGISTER_PARAMETER_MACRO("material", MaterialType, material)
)

UsdSurface::UsdSurface(const char* name, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_SURFACE, name, device)
{
}

UsdSurface::~UsdSurface()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  // Given that the object is destroyed, none of its references to other objects
  // has to be updated anymore.
  if(cachedBridge)
    cachedBridge->DeleteSurface(usdHandle);
#endif
}

bool UsdSurface::deferCommit(UsdDevice* device)
{
  // Given that all handles/data are used in doCommitRefs, which is always executed deferred, we don't need to check for initialization

  //const UsdSurfaceData& paramData = getReadParams();
  //if(UsdObjectNotInitialized<GeometryUsdType>(paramData.geometry) || UsdObjectNotInitialized<MaterialUsdType>(paramData.material))
  //{
  //  return true;
  //}
  return false;
}

bool UsdSurface::doCommitData(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();
    
  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateSurface(getName(), usdHandle);

  if (paramChanged || isNew)
  {
    paramChanged = false;

    return true; // In this case a doCommitRefs is required, with data (timesteps, handles) from children
  }

  return false;
}

void UsdSurface::doCommitRefs(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdSurfaceData& paramData = getReadParams();

  double worldTimeStep = device->getReadParams().timeStep;

  // Make sure the references are updated on the Bridge side.
  if (paramData.geometry)
  {
    double geomObjTimeStep = paramData.geometry->getReadParams().timeStep;

    if(device->getReadParams().outputMaterial && paramData.material)
    {
      double matObjTimeStep = paramData.material->getReadParams().timeStep;

      // The geometry to which a material binds has an effect on attribute reader (geom primvar) names, and output types
      paramData.material->updateBoundParameters(paramData.geometry->isInstanced(), device);

      usdBridge->SetGeometryMaterialRef(usdHandle, 
        paramData.geometry->getUsdHandle(), 
        paramData.material->getUsdHandle(), 
        worldTimeStep,
        selectRefTime(paramData.geometryRefTimeStep, geomObjTimeStep, worldTimeStep),
        selectRefTime(paramData.materialRefTimeStep, matObjTimeStep, worldTimeStep)
        );
    }
    else
    {
      usdBridge->SetGeometryRef(usdHandle,
        paramData.geometry->getUsdHandle(),
        worldTimeStep,
        selectRefTime(paramData.geometryRefTimeStep, geomObjTimeStep, worldTimeStep)
      );
      usdBridge->DeleteMaterialRef(usdHandle, worldTimeStep);
    }
  }
  else
  {
    usdBridge->DeleteGeometryRef(usdHandle, worldTimeStep);

    if (!paramData.material && device->getReadParams().outputMaterial)
    {
      usdBridge->DeleteMaterialRef(usdHandle, worldTimeStep);
    }
  }
}