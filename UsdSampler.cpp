// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdSampler.h"
#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdDataArray.h"

DEFINE_PARAMETER_MAP(UsdSampler, 
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::time", ANARI_FLOAT64, timeStep)
  REGISTER_PARAMETER_MACRO("usd::timeVarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("usd::imageUrl", ANARI_STRING, imageUrl)
  REGISTER_PARAMETER_MACRO("inAttribute", ANARI_STRING, inAttribute)
  REGISTER_PARAMETER_MACRO("image", ANARI_ARRAY, imageData)
  REGISTER_PARAMETER_MACRO("wrapMode", ANARI_STRING, wrapS)
  REGISTER_PARAMETER_MACRO("wrapMode1", ANARI_STRING, wrapS)
  REGISTER_PARAMETER_MACRO("wrapMode2", ANARI_STRING, wrapT)
  REGISTER_PARAMETER_MACRO("wrapMode3", ANARI_STRING, wrapR)
)

namespace
{ 
  UsdBridgeSamplerData::WrapMode ANARIToUsdBridgeWrapMode(const char* anariWrapMode)
  {
    UsdBridgeSamplerData::WrapMode usdWrapMode = UsdBridgeSamplerData::WrapMode::BLACK;
    if(anariWrapMode)
    {
      if (strEquals(anariWrapMode, "clampToEdge"))
      {
        usdWrapMode = UsdBridgeSamplerData::WrapMode::CLAMP;
      }
      else if (strEquals(anariWrapMode, "repeat"))
      {
        usdWrapMode = UsdBridgeSamplerData::WrapMode::REPEAT;
      }
      else if (strEquals(anariWrapMode, "mirrorRepeat"))
      {
        usdWrapMode = UsdBridgeSamplerData::WrapMode::MIRROR;
      }
    }
    return usdWrapMode;
  }
}

UsdSampler::UsdSampler(const char* name, const char* type, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_SAMPLER, name, device)
{
  if (strEquals(type, "image1D"))
    samplerType = SAMPLER_1D;
  else if (strEquals(type, "image2D"))
    samplerType = SAMPLER_2D;
  else if (strEquals(type, "image3D"))
    samplerType = SAMPLER_3D;
  else
    device->reportStatus(this, ANARI_SAMPLER, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdSampler '%s' construction failed: type %s not supported", getName(), name);
}

UsdSampler::~UsdSampler()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(cachedBridge)
    cachedBridge->DeleteSampler(usdHandle);
#endif
}

void UsdSampler::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  if (filterNameParam(name, type, mem, device))
    setParam(name, type, mem, device);
}

void UsdSampler::filterResetParam(const char *name)
{
  resetParam(name);
}

void UsdSampler::setPerInstance(bool enable, UsdDevice* device) 
{ 
  UsdBridge* usdBridge = device->getUsdBridge();

  if(!usdHandle.value)
    return;
  
  if(perInstance != enable)
  {
    // Fix up the position attribute
    const UsdSamplerData& paramData = getReadParams();
    const char* inAttribName = UsdSharedString::c_str(paramData.inAttribute);
    if(inAttribName && strEquals(inAttribName, "objectPosition"))
    {
      // In case of a per-instance specific attribute name, there can be only one change of the attribute name.
      UsdLogInfo logInfo(device, this, ANARI_SAMPLER, getName());
      if(instanceAttributeAttached)
      {
        reportStatusThroughDevice(logInfo, ANARI_SEVERITY_WARNING, ANARI_STATUS_INVALID_ARGUMENT,
          "UsdSampler '%s' binds its inAttribute parameter to %s, but is transitively bound to both an instanced geometry (cones, spheres, cylinders) and regular geometry. \
          This is incompatible with USD, which demands a differently bound name for those categories. \
          Please create two different samplers and bind each to only one of both categories of geometry. \
          The inAttribute value will be updated, but may therefore invalidate previous bindings to the objectPosition attribute.", logInfo.sourceName, "'objectPosition'");
      }

      instanceAttributeAttached = true;

      const char* usdAttribName = AnariAttributeToUsdName(inAttribName, perInstance, logInfo);

      double worldTimeStep = device->getReadParams().timeStep;
      double dataTimeStep = selectObjTime(paramData.timeStep, worldTimeStep);
      UsdBridgeSamplerData::DataMemberId timeVarying = (UsdBridgeSamplerData::DataMemberId)paramData.timeVarying;
      
      usdBridge->ChangeInAttribute(usdHandle, usdAttribName, dataTimeStep, timeVarying);
    }

    perInstance = enable;
  } 
}

bool UsdSampler::deferCommit(UsdDevice* device)
{
  return false;
}

bool UsdSampler::doCommitData(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();

  if(!device->getReadParams().outputMaterial ||
    samplerType == SAMPLER_UNKNOWN)
    return false;

  const UsdSamplerData& paramData = getReadParams();

  UsdBridgeSamplerData::SamplerType type = 
    (samplerType == SAMPLER_1D ? UsdBridgeSamplerData::SamplerType::SAMPLER_1D : 
      (samplerType == SAMPLER_2D ? UsdBridgeSamplerData::SamplerType::SAMPLER_2D :
        UsdBridgeSamplerData::SamplerType::SAMPLER_3D
        )
      );

  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateSampler(getName(), usdHandle, type);

  if (paramChanged || isNew)
  {
    if (paramData.inAttribute && (std::strlen(UsdSharedString::c_str(paramData.inAttribute)) > 0) 
      && (paramData.imageUrl || paramData.imageData))
    {
      bool supportedImage = true;
      int numComponents = 0;
      if(paramData.imageData)
      {
        numComponents = static_cast<int>(anari::componentsOf(paramData.imageData->getType()));

        if(numComponents > 4)
          device->reportStatus(this, ANARI_SAMPLER, ANARI_SEVERITY_WARNING, ANARI_STATUS_INVALID_ARGUMENT, 
            "UsdSampler '%s' image data has more than 4 components. Anything above the 4th component will be ignored.", UsdSharedString::c_str(paramData.imageData->getName()));

        if(anari::sizeOf(paramData.imageData->getType()) != sizeof(int8_t) * numComponents)
        {
          device->reportStatus(this, ANARI_SAMPLER, ANARI_SEVERITY_WARNING, ANARI_STATUS_INVALID_ARGUMENT, 
            "UsdSampler '%s' commit failed: image data color channels are not 8 bit.", UsdSharedString::c_str(paramData.imageData->getName()));  
          supportedImage = false;
        }
      }

      if(supportedImage)
      {
        UsdLogInfo logInfo(device, this, ANARI_SAMPLER, getName());

        UsdBridgeSamplerData samplerData;
        samplerData.Type = type;

        double worldTimeStep = device->getReadParams().timeStep;
        double dataTimeStep = selectObjTime(paramData.timeStep, worldTimeStep);

        samplerData.InAttribute = AnariAttributeToUsdName(UsdSharedString::c_str(paramData.inAttribute), perInstance, logInfo);
      
        if(paramData.imageUrl)
        {
          samplerData.ImageUrl = UsdSharedString::c_str(paramData.imageUrl);
        }

        if(paramData.imageData)
        {
          samplerData.Data = paramData.imageData->getData();
          samplerData.ImageName = UsdSharedString::c_str(paramData.imageData->getName());
          samplerData.ImageNumComponents = numComponents;
          samplerData.DataType = AnariToUsdBridgeType(paramData.imageData->getType());
          paramData.imageData->getLayout().copyDims(samplerData.ImageDims);
          paramData.imageData->getLayout().copyStride(samplerData.ImageStride);
        }

        samplerData.WrapS = ANARIToUsdBridgeWrapMode(UsdSharedString::c_str(paramData.wrapS));
        samplerData.WrapT = ANARIToUsdBridgeWrapMode(UsdSharedString::c_str(paramData.wrapT));
        samplerData.WrapR = ANARIToUsdBridgeWrapMode(UsdSharedString::c_str(paramData.wrapR));

        samplerData.TimeVarying = (UsdBridgeSamplerData::DataMemberId)paramData.timeVarying;

        usdBridge->SetSamplerData(usdHandle, samplerData, dataTimeStep);
      }
    }
    else
    {
      device->reportStatus(this, ANARI_SAMPLER, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, 
        "UsdSampler '%s' commit failed: missing either the 'inAttribute', or both the 'image' and 'usd::imageUrl' parameter", UsdSharedString::c_str(paramData.imageData->getName()));
    }

    paramChanged = false;
  }

  return false;
}
