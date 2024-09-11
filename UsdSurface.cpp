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
  REGISTER_PARAMETER_MACRO("usd::time.geometry", ANARI_FLOAT64, geometryRefTimeStep)
  REGISTER_PARAMETER_MACRO("usd::time.material", ANARI_FLOAT64, materialRefTimeStep)
  REGISTER_PARAMETER_MACRO("geometry", GeometryType, geometry)
  REGISTER_PARAMETER_MACRO("material", MaterialType, material)
  REGISTER_PARAMETER_MACRO("usd::isInstanceable", ANARI_BOOL, isInstanceable)
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

void UsdSurface::remove(UsdDevice* device)
{
  applyRemoveFunc(device, &UsdBridge::DeleteSurface);
}

bool UsdSurface::isInstanceable() const
{
  const UsdSurfaceData& paramData = getReadParams();
  return paramData.isInstanceable;
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
  const UsdSurfaceData& paramData = getReadParams();
    
  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateSurface(getName(), usdHandle);

  if (paramChanged || isNew)
  {
    paramChanged = false;
    updateBoundParameters = true;

    return true; // In this case a doCommitRefs is required, with data (timesteps, handles) from children
  }

  return updateBoundParameters; // Update the bound parameters after they have been committed on geometry/material
}

void UsdSurface::doCommitRefs(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdSurfaceData& paramData = getReadParams();

  double worldTimeStep = device->getReadParams().timeStep;

  bool hasGeometry = paramData.geometry;
  bool hasMaterial = device->getReadParams().outputMaterial && paramData.material;
  if(hasGeometry && hasMaterial && updateBoundParameters)
  {
    const UsdGeometryData& geomParamData = paramData.geometry->getReadParams();

    // The geometry to which a material binds has an effect on attribute reader (geom primvar) names, and output types
    paramData.material->updateBoundParameters(paramData.geometry->isInstanced(), geomParamData, device);

    updateBoundParameters = false;
  }

  // Make sure the references are updated on the Bridge side.
  if (hasGeometry)
  {
    const UsdGeometryData& geomParamData = paramData.geometry->getReadParams();
    double geomObjTimeStep = geomParamData.timeStep;

    if(hasMaterial)
    {
      double matObjTimeStep = paramData.material->getReadParams().timeStep;

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

void UsdSurface::onParamRefChanged(UsdBaseObject* paramObject, bool incRef, bool onWriteParams)
{
  if(!onWriteParams && paramObject->getType() == ANARI_GEOMETRY)
  {
    if(incRef)
      paramObject->addObserver(this);
    else
      paramObject->removeObserver(this);
  }

  BridgedBaseObjectType::onParamRefChanged(paramObject, incRef, onWriteParams);
}

void UsdSurface::observe(UsdBaseObject* caller, UsdDevice* device)
{
  if(caller->getType() == ANARI_GEOMETRY)
  {
    updateBoundParameters = true;
    device->addToCommitList(this, true); // No write to read params; just write to USD
  }

  BridgedBaseObjectType::observe(caller, device);
}