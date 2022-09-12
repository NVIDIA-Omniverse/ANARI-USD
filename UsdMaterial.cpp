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
  REGISTER_PARAMETER_MACRO("usd::timestep::color", ANARI_FLOAT64, colorSamplerTimeStep)
  REGISTER_PARAMETER_MACRO("usd::timestep::opacity", ANARI_FLOAT64, opacitySamplerTimeStep)
  REGISTER_PARAMETER_MULTITYPE_MACRO("color", ANARI_FLOAT32_VEC3, SamplerType, ANARI_STRING, color)
  REGISTER_PARAMETER_MULTITYPE_MACRO("opacity", ANARI_FLOAT32, SamplerType, ANARI_STRING, opacity)
  REGISTER_PARAMETER_MACRO("specular", ANARI_FLOAT32_VEC3, specular)
  REGISTER_PARAMETER_MACRO("emissive", ANARI_FLOAT32_VEC3, emissive)
  REGISTER_PARAMETER_MACRO("shininess", ANARI_FLOAT32, shininess)
  REGISTER_PARAMETER_MACRO("emissiveintensity", ANARI_FLOAT32, emissiveIntensity)
  REGISTER_PARAMETER_MACRO("metallic", ANARI_FLOAT32, metallic)
  REGISTER_PARAMETER_MACRO("ior", ANARI_FLOAT32, ior) 
)

namespace
{
  template<typename T0, typename T1, typename T2>
  void SetMultiTypeMatParam(UsdMaterialData& materialData, const UsdMultiTypeParameter<T0, T1, T2>& param)
  {
    
  }
}

UsdMaterial::UsdMaterial(const char* name, const char* type, UsdBridge* bridge, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_MATERIAL, name, bridge)
{
  if (!std::strcmp(type, "matte"))
  {
    IsPbr = false;
    IsTranslucent = false;
  }
  else if (!std::strcmp(type, "transparentMatte"))
  {
    IsPbr = false;
    IsTranslucent = true;
  }
  if (!std::strcmp(type, "pbr"))
  {
    IsPbr = true;
    IsTranslucent = false;
  }
  else if (!std::strcmp(type, "transparentPbr"))
  {
    IsPbr = true;
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
  //const UsdMaterialData& paramData = getReadParams();

  //if(UsdObjectNotInitialized<SamplerUsdType>(paramData.color.type == SamplerType))
  //{
  //  return true;
  //}
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

    double worldTimeStep = device->getReadParams().timeStep;
    double dataTimeStep = selectObjTime(paramData.timeStep, worldTimeStep);

    // Make sure to decouple before setting vertexcolors.
    if (!paramData.color.type == SamplerType)
    {
      usdBridge->DeleteSamplerRef(usdHandle, worldTimeStep);
    }

    UsdBridgeMaterialData matData;
    matData.HasTranslucency = IsTranslucent;
    matData.IsPbr = IsPbr;

    paramData.color.Get(matData.Diffuse);
    UsdSharedString* vertexColorSource = nullptr;
    matData.VertexColorSource = paramData.color.Get(vertexColorSource) ? vertexColorSource->c_str() : nullptr;

    paramData.opacity.Get(matData.Opacity);
    UsdSharedString* vertexOpacitySource = nullptr;
    matData.VertexOpacitySource = paramData.opacity.Get(vertexOpacitySource) ? vertexOpacitySource->c_str() : nullptr;
    
    memcpy(matData.Specular, paramData.specular, sizeof(matData.Specular));
    memcpy(matData.Emissive, paramData.emissive, sizeof(matData.Emissive));
    matData.EmissiveIntensity = paramData.emissiveIntensity;
    matData.Roughness = 1.0f - paramData.shininess;
    matData.Metallic = paramData.metallic;
    matData.Ior = paramData.ior;

    matData.TimeVarying = (UsdBridgeMaterialData::DataMemberId) paramData.timeVarying;

    usdBridge->SetMaterialData(usdHandle, matData, dataTimeStep);

    paramChanged = false;

    return paramData.color.type == SamplerType; // Only commit refs when material actually contains a texture (filename param from diffusemap is required)
  }

  return false;
}

void UsdMaterial::doCommitRefs(UsdDevice* device)
{
  const UsdMaterialData& paramData = getReadParams();

  // Diffuse sampler overrides vertex colors (and must always do so at paramchange, due to colors being rebound blindly)
  UsdSampler* colorSampler = nullptr;
  if (paramData.color.Get(colorSampler))
  {
    double worldTimeStep = device->getReadParams().timeStep;
    double samplerObjTimeStep = colorSampler->getReadParams().timeStep;
    double samplerRefTime = selectRefTime(paramData.colorSamplerTimeStep, samplerObjTimeStep, worldTimeStep);
    
    // Reading child (sampler) data in the material has the consequence that the sampler's parameters as they were last committed are in effect "part of" the material parameter set, at this point of commit. 
    // So in case a sampler at a particular timestep is referenced from a material at two different world timesteps 
    // - ie. for this world timestep, a particular timestep of an image already committed and subsequently referenced at some other previous world timestep is reused - 
    // the user needs to make sure that not only the timestep is set correctly on the sampler for the commit (which is by itself lightweight, as it does not trigger a full commit), 
    // but also that the parameters read here have been re-set on the sampler to the values belonging to the referenced timestep, as if there is no USD representation of a sampler object. 
    // Setting those parameters will in turn trigger a full commit of the sampler object, which is in theory inefficient.
    // However, in case of a sampler this is not a problem in practice; data transfer is only a concern when the filename is *not* set, at which point a relative file corresponding
    // to the sampler timestep will be automatically chosen and set for the material, without the sampler object requiring any updates. 
    // In case a filename *is* set, only the filename is used and no data transfer/file io operations are performed.
    UsdSharedString* imageUrl = colorSampler->getReadParams().imageUrl;
    bool fNameTimeVarying = (colorSampler->getReadParams().timeVarying & 1);

    usdBridge->SetSamplerRef(usdHandle, colorSampler->getUsdHandle(), UsdSharedString::c_str(imageUrl), fNameTimeVarying, worldTimeStep, samplerRefTime);
  }
}