// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdMaterial.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdSampler.h"

#define SamplerType ANARI_SAMPLER
using SamplerUsdType = AnariToUsdBridgedObject<SamplerType>::Type;

DEFINE_PARAMETER_MAP(UsdMaterial, 
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timestep", ANARI_FLOAT64, timeStep)
  REGISTER_PARAMETER_MACRO("usd::timevarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("color", ANARI_FLOAT32_VEC3, diffuse) 
  REGISTER_PARAMETER_MACRO("specular", ANARI_FLOAT32_VEC3, specular)
  REGISTER_PARAMETER_MACRO("emissive", ANARI_FLOAT32_VEC3, emissive)
  REGISTER_PARAMETER_MACRO("shininess", ANARI_FLOAT32, shininess)
  REGISTER_PARAMETER_MACRO("opacity", ANARI_FLOAT32, opacity)
  REGISTER_PARAMETER_MACRO("emissiveintensity", ANARI_FLOAT32, emissiveIntensity)
  REGISTER_PARAMETER_MACRO("metallic", ANARI_FLOAT32, metallic)
  REGISTER_PARAMETER_MACRO("ior", ANARI_FLOAT32, ior)
  REGISTER_PARAMETER_MACRO("usevertexcolors", ANARI_BOOL, useVertexColors)
  REGISTER_PARAMETER_MACRO("map_kd", SamplerType, diffuseMap)
)

UsdMaterial::UsdMaterial(const char* name, const char* type, UsdBridge* bridge, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_MATERIAL, name, bridge)
{
  if (!std::strcmp(type, "matte"))
  {
    IsTranslucent = false;
  }
  else if (!std::strcmp(type, "transparentMatte"))
  {
    IsTranslucent = true;
  }
  else
  {
    device->reportStatus(this, ANARI_MATERIAL, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdMaterial '%s' intialization error: unknown material type", getName());
  }
}

UsdMaterial::~UsdMaterial()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(usdBridge)
    usdBridge->DeleteMaterial(usdHandle);
#endif
}

void UsdMaterial::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  if (filterNameParam(name, type, mem, device))
    setParam(name, type, mem, device);
}

void UsdMaterial::filterResetParam(const char *name)
{
  resetParam(name);
}

bool UsdMaterial::deferCommit(UsdDevice* device)
{
  const UsdMaterialData& paramData = getReadParams();

  if(UsdObjectNotInitialized<SamplerUsdType>(paramData.diffuseMap))
  {
    return true;
  }
  return false;
}

bool UsdMaterial::doCommitData(UsdDevice* device)
{
  if(!usdBridge || !device->getReadParams().outputMaterial)
    return false;

  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateMaterial(getName(), usdHandle);

  if (paramChanged || isNew)
  {
    const UsdMaterialData& paramData = getReadParams();

    double timeStep = paramData.timeStep;

    // Make sure to decouple before setting vertexcolors.
    if (!paramData.diffuseMap)
    {
      usdBridge->DeleteSamplerRef(usdHandle, timeStep);
    }

    UsdBridgeMaterialData matData;
    memcpy(matData.Diffuse, paramData.diffuse, sizeof(matData.Diffuse));
    memcpy(matData.Specular, paramData.specular, sizeof(matData.Specular));
    memcpy(matData.Emissive, paramData.emissive, sizeof(matData.Emissive));
    matData.Opacity = paramData.opacity;
    matData.EmissiveIntensity = paramData.emissiveIntensity;
    matData.Roughness = 1.0f - paramData.shininess;
    matData.Metallic = paramData.metallic;
    matData.Ior = paramData.ior;
    matData.UseVertexColors = paramData.useVertexColors;
    matData.HasTranslucency = IsTranslucent;

    matData.TimeVarying = (UsdBridgeMaterialData::DataMemberId) paramData.timeVarying;

    usdBridge->SetMaterialData(usdHandle, matData, timeStep);

    paramChanged = false;

    return paramData.diffuseMap;
  }

  return false;
}

void UsdMaterial::doCommitRefs(UsdDevice* device)
{
  const UsdMaterialData& paramData = getReadParams();

  // Diffuse sampler overrides vertex colors (and must always do so at paramchange, due to colors being rebound blindly)
  if (paramData.diffuseMap)
  {
    double timeStep = paramData.timeStep;
    UsdSharedString* fName = paramData.diffuseMap->getReadParams().fileName;
    usdBridge->SetSamplerRef(usdHandle, paramData.diffuseMap->getUsdHandle(), UsdSharedString::c_str(fName), timeStep);
  }
}