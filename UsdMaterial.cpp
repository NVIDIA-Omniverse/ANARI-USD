// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdMaterial.h"
#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdSampler.h"
#include "UsdDataArray.h"
#include "UsdGeometry.h"

#define SamplerType ANARI_SAMPLER
using SamplerUsdType = AnariToUsdBridgedObject<SamplerType>::Type;

#define REGISTER_PARAMETER_MATERIAL_MULTITYPE_MACRO(ParamName, ParamType0, ParamData) \
  REGISTER_PARAMETER_MULTITYPE_MACRO(ParamName, ParamType0, SamplerType, ANARI_STRING, ParamData)

DEFINE_PARAMETER_MAP(UsdMaterial, 
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::time", ANARI_FLOAT64, timeStep)
  REGISTER_PARAMETER_MACRO("usd::timeVarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("usd::time.sampler.color", ANARI_FLOAT64, colorSamplerTimeStep)
  REGISTER_PARAMETER_MACRO("usd::time.sampler.baseColor", ANARI_FLOAT64, colorSamplerTimeStep)
  REGISTER_PARAMETER_MACRO("usd::time.sampler.opacity", ANARI_FLOAT64, opacitySamplerTimeStep)
  REGISTER_PARAMETER_MACRO("usd::time.sampler.emissive", ANARI_FLOAT64, emissiveSamplerTimeStep)
  REGISTER_PARAMETER_MACRO("usd::time.sampler.emissiveIntensity", ANARI_FLOAT64, emissiveIntensitySamplerTimeStep)
  REGISTER_PARAMETER_MACRO("usd::time.sampler.roughness", ANARI_FLOAT64, roughnessSamplerTimeStep)
  REGISTER_PARAMETER_MACRO("usd::time.sampler.metallic", ANARI_FLOAT64, metallicSamplerTimeStep)
  REGISTER_PARAMETER_MACRO("usd::time.sampler.ior", ANARI_FLOAT64, iorSamplerTimeStep)
  REGISTER_PARAMETER_MATERIAL_MULTITYPE_MACRO("color", ANARI_FLOAT32_VEC3, color)
  REGISTER_PARAMETER_MATERIAL_MULTITYPE_MACRO("baseColor", ANARI_FLOAT32_VEC3, color)
  REGISTER_PARAMETER_MATERIAL_MULTITYPE_MACRO("opacity", ANARI_FLOAT32, opacity)
  REGISTER_PARAMETER_MACRO("alphaMode", ANARI_STRING, alphaMode)
  REGISTER_PARAMETER_MACRO("alphaCutoff", ANARI_FLOAT32, alphaCutoff)
  REGISTER_PARAMETER_MATERIAL_MULTITYPE_MACRO("emissive", ANARI_FLOAT32_VEC3, emissiveColor)
  REGISTER_PARAMETER_MATERIAL_MULTITYPE_MACRO("emissiveIntensity", ANARI_FLOAT32, emissiveIntensity)
  REGISTER_PARAMETER_MATERIAL_MULTITYPE_MACRO("roughness", ANARI_FLOAT32, roughness)
  REGISTER_PARAMETER_MATERIAL_MULTITYPE_MACRO("metallic", ANARI_FLOAT32, metallic)
  REGISTER_PARAMETER_MATERIAL_MULTITYPE_MACRO("ior", ANARI_FLOAT32, ior) 
)

constexpr UsdMaterial::ComponentPair UsdMaterial::componentParamNames[]; // Workaround for C++14's lack of inlining constexpr arrays

using DMI = UsdMaterial::MaterialDMI;

UsdMaterial::UsdMaterial(const char* name, const char* type, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_MATERIAL, name, device)
{
  if (strEquals(type, "matte"))
  {
    isPbr = false;
  }
  else if (strEquals(type, "physicallyBased"))
  {
    isPbr = true;
  }
  else
  {
    device->reportStatus(this, ANARI_MATERIAL, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdMaterial '%s' intialization error: unknown material type", getName());
  }
}

UsdMaterial::~UsdMaterial()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(cachedBridge)
    cachedBridge->DeleteMaterial(usdHandle);
#endif
}

void UsdMaterial::remove(UsdDevice* device)
{
  applyRemoveFunc(device, &UsdBridge::DeleteMaterial);
}

template<typename ValueType>
void UsdMaterial::changeMaterialInputSource(const UsdMaterialMultiTypeParameter<ValueType>& param,
  MaterialDMI dataMemberId, const UsdGeometryData& geomParamData, bool forceUpdate,
  UsdDevice* device, const UsdLogInfo& logInfo)
{
  UsdSharedString* anariAttribStr = nullptr;
  param.Get(anariAttribStr); // If an input parameter is of string type, returns the pointer of the string (otherwise untouched)
  const char* anariAttrib = UsdSharedString::c_str(anariAttribStr);

  if(anariAttrib)
  {
    auto [hasMeshDependentAttribs, meshDependentAttribName] =
      GetGeomDependentAttributeName(anariAttrib, perInstance, device->getReadParams().useDisplayColorOpacity, geomParamData.attributeNames, MAX_ATTRIBS, logInfo);

    bool hasFixedAttributeType = HasFixedAttributeType(anariAttrib);

    if(forceUpdate || hasMeshDependentAttribs || !hasFixedAttributeType)
      materialInputAttributes.push_back(UsdBridge::MaterialInputAttribName(dataMemberId, meshDependentAttribName));
  }
  else if(param.type == SamplerType)
  {
    UsdSampler* sampler = nullptr;
    if (param.Get(sampler))
    {
      sampler->updateBoundParameters(perInstance, geomParamData.attributeNames, MAX_ATTRIBS, device);
    }
  }
}

template<typename ValueType>
bool UsdMaterial::getSamplerRefData(const UsdMaterialMultiTypeParameter<ValueType>& param, double refTimeStep, MaterialDMI dataMemberId, UsdDevice* device, const UsdLogInfo& logInfo)
{
  UsdSampler* sampler = nullptr;
  param.Get(sampler);

  if(sampler)
  {
    const UsdSamplerData& samplerParamData = sampler->getReadParams();

    double worldTimeStep = device->getReadParams().timeStep;
    double samplerObjTimeStep = samplerParamData.timeStep;
    double samplerRefTime = selectRefTime(refTimeStep, samplerObjTimeStep, worldTimeStep);
    
    // Reading child (sampler) data in the material has the consequence that the sampler's parameters as they were last committed are in effect "part of" the material parameter set, at this point of commit. 
    // So in case a sampler at a particular timestep is referenced from a material at two different world timesteps 
    // - ie. for this world timestep, a particular timestep of an image already committed and subsequently referenced at some other previous world timestep is reused - 
    // the user needs to make sure that not only the timestep is set correctly on the sampler for the commit (which is by itself lightweight, as it does not trigger a full commit), 
    // but also that the parameters read here have been re-set on the sampler to the values belonging to the referenced timestep, as if there is no USD representation of a sampler object. 
    // Setting those parameters will in turn trigger a full commit of the sampler object, which is in theory inefficient.
    // However, in case of a sampler this is not a problem in practice; data transfer is only a concern when the filename is *not* set, at which point a relative file corresponding
    // to the sampler timestep will be automatically chosen and set for the material, without the sampler object requiring any updates. 
    // In case a filename *is* set, only the filename is used and no data transfer/file io operations are performed.
    //const char* imageUrl = UsdSharedString::c_str(samplerParamData.imageUrl); // not required anymore since all materials are a graph

    int imageNumComponents = 4;
    if(samplerParamData.imageData)
    {
      imageNumComponents = static_cast<int>(anari::componentsOf(samplerParamData.imageData->getType()));
    }
    UsdSamplerRefData samplerRefData = {imageNumComponents, samplerRefTime, dataMemberId};

    samplerHandles.push_back(sampler->getUsdHandle());
    samplerRefDatas.push_back(samplerRefData);
  }

  return false;
}

template<typename ValueType>
void UsdMaterial::assignParameterToMaterialInput(const UsdMaterialMultiTypeParameter<ValueType>& param, 
  UsdBridgeMaterialData::MaterialInput<ValueType>& matInput, bool useDisplayColorOpacity, const UsdLogInfo& logInfo)
{
  param.Get(matInput.Value);

  UsdSharedString* anariAttribStr = nullptr;
  matInput.SrcAttrib = param.Get(anariAttribStr) ? 
    AnariAttributeToUsdName(anariAttribStr->c_str(), perInstance, useDisplayColorOpacity, logInfo) :
    nullptr; 

  UsdSampler* sampler = nullptr;
  matInput.Sampler = param.Get(sampler);
}

void UsdMaterial::updateBoundParameters(bool boundToInstance, const UsdGeometryData& geomParamData, UsdDevice* device)
{ 
  // The geometry to which a material binds has an effect on:
  // - attribute reader (geom primvar) names,
  // - attribute reader output types
  // These will have been setup initially in commit() with UsdBridge::SetMaterialData using UpdateUsdMaterial, which is correct for:
  // - the attribute reader names except those that are geometry type dependent
  // - the types of those attributes that are predetermined (points, normals)
  // For anything else or in case no geometry has been bound before,
  // the material's attribute reader needs to be updated to use the correct type given the new geometry.
  // This is done here with UsdMaterial::changeMaterialInputSource().

  perInstance = boundToInstance;

  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdMaterialData& paramData = getReadParams();
  
  UsdLogInfo logInfo = {device, this, ANARI_MATERIAL, getName()};

  double worldTimeStep = device->getReadParams().timeStep;
  double dataTimeStep = selectObjTime(paramData.timeStep, worldTimeStep);

  // Fix up any parameters that have a geometry-type-dependent name set as source attribute
  materialInputAttributes.clear();

  bool forceUpdate = !isBound;
  changeMaterialInputSource(paramData.color, DMI::DIFFUSE, geomParamData, forceUpdate, device, logInfo);
  changeMaterialInputSource(paramData.opacity, DMI::OPACITY, geomParamData, forceUpdate, device, logInfo);
  changeMaterialInputSource(paramData.emissiveColor, DMI::EMISSIVECOLOR, geomParamData, forceUpdate, device, logInfo);
  changeMaterialInputSource(paramData.emissiveIntensity, DMI::EMISSIVEINTENSITY, geomParamData, forceUpdate, device, logInfo);
  changeMaterialInputSource(paramData.roughness, DMI::ROUGHNESS, geomParamData, forceUpdate, device, logInfo);
  changeMaterialInputSource(paramData.metallic, DMI::METALLIC, geomParamData, forceUpdate, device, logInfo);
  changeMaterialInputSource(paramData.ior, DMI::IOR, geomParamData, forceUpdate, device, logInfo);

  DMI timeVarying;
  setMaterialTimeVarying(timeVarying);

  // Fixup attribute name and type depending on the newly bound geometry
  if(materialInputAttributes.size())
    usdBridge->ChangeMaterialInputAttributes(usdHandle, materialInputAttributes.data(), materialInputAttributes.size(), dataTimeStep, timeVarying);

  isBound = true;
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
  UsdBridge* usdBridge = device->getUsdBridge();

  if(!device->getReadParams().outputMaterial)
    return false;

  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateMaterial(getName(), usdHandle);

  if (paramChanged || isNew)
  {
    const UsdMaterialData& paramData = getReadParams();

    double worldTimeStep = device->getReadParams().timeStep;
    double dataTimeStep = selectObjTime(paramData.timeStep, worldTimeStep);

    UsdBridgeMaterialData matData;
    matData.IsPbr = isPbr;
    matData.AlphaMode = AnariToUsdAlphaMode(UsdSharedString::c_str(paramData.alphaMode));
    matData.AlphaCutoff = paramData.alphaCutoff;

    bool useDisplayColorOpacity = device->getReadParams().useDisplayColorOpacity;

    UsdLogInfo logInfo = {device, this, ANARI_MATERIAL, getName()};
    
    assignParameterToMaterialInput(paramData.color, matData.Diffuse, useDisplayColorOpacity, logInfo);
    assignParameterToMaterialInput(paramData.opacity, matData.Opacity, useDisplayColorOpacity, logInfo);
    assignParameterToMaterialInput(paramData.emissiveColor, matData.Emissive, useDisplayColorOpacity, logInfo);
    assignParameterToMaterialInput(paramData.emissiveIntensity, matData.EmissiveIntensity, useDisplayColorOpacity, logInfo);
    assignParameterToMaterialInput(paramData.roughness, matData.Roughness, useDisplayColorOpacity, logInfo);
    assignParameterToMaterialInput(paramData.metallic, matData.Metallic, useDisplayColorOpacity, logInfo);
    assignParameterToMaterialInput(paramData.ior, matData.Ior, useDisplayColorOpacity, logInfo);

    setMaterialTimeVarying(matData.TimeVarying);

    usdBridge->SetMaterialData(usdHandle, matData, dataTimeStep);

    paramChanged = false;

    return paramData.color.type == SamplerType; // Only commit refs when material actually contains a texture (filename param from diffusemap is required)
  }

  return false;
}

void UsdMaterial::doCommitRefs(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdMaterialData& paramData = getReadParams();

  double worldTimeStep = device->getReadParams().timeStep;

  samplerHandles.clear();
  samplerRefDatas.clear();

  UsdLogInfo logInfo = {device, this, ANARI_MATERIAL, getName()};

  getSamplerRefData(paramData.color, paramData.colorSamplerTimeStep, DMI::DIFFUSE, device, logInfo);
  getSamplerRefData(paramData.opacity, paramData.opacitySamplerTimeStep, DMI::OPACITY, device, logInfo);
  getSamplerRefData(paramData.emissiveColor, paramData.emissiveSamplerTimeStep, DMI::EMISSIVECOLOR, device, logInfo);
  getSamplerRefData(paramData.emissiveIntensity, paramData.emissiveIntensitySamplerTimeStep, DMI::EMISSIVEINTENSITY, device, logInfo);
  getSamplerRefData(paramData.roughness, paramData.roughnessSamplerTimeStep, DMI::ROUGHNESS, device, logInfo);
  getSamplerRefData(paramData.metallic, paramData.metallicSamplerTimeStep, DMI::METALLIC, device, logInfo);
  getSamplerRefData(paramData.ior, paramData.iorSamplerTimeStep, DMI::IOR, device, logInfo);

  if(samplerHandles.size())
    usdBridge->SetSamplerRefs(usdHandle, samplerHandles.data(), samplerHandles.size(), worldTimeStep, samplerRefDatas.data());
  else
    usdBridge->DeleteSamplerRefs(usdHandle, worldTimeStep);
}

void UsdMaterial::setMaterialTimeVarying(UsdBridgeMaterialData::DataMemberId& timeVarying)
{
  timeVarying = DMI::ALL
    & (isTimeVarying(CType::COLOR) ? DMI::ALL : ~DMI::DIFFUSE)
    & (isTimeVarying(CType::OPACITY) ? DMI::ALL : ~DMI::OPACITY)
    & (isTimeVarying(CType::EMISSIVE) ? DMI::ALL : ~DMI::EMISSIVECOLOR)
    & (isTimeVarying(CType::EMISSIVEFACTOR) ? DMI::ALL : ~DMI::EMISSIVEINTENSITY)
    & (isTimeVarying(CType::ROUGHNESS) ? DMI::ALL : ~DMI::ROUGHNESS)
    & (isTimeVarying(CType::METALLIC) ? DMI::ALL : ~DMI::METALLIC)
    & (isTimeVarying(CType::IOR) ? DMI::ALL : ~DMI::IOR);
}