// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdSampler.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDevice.h"

DEFINE_PARAMETER_MAP(UsdSampler, 
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timestep", ANARI_FLOAT64, timeStep)
  REGISTER_PARAMETER_MACRO("usd::timevarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("usd::imageurl", ANARI_STRING, imageUrl)
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
      if (!std::strcmp(anariWrapMode, "clampToEdge"))
      {
        usdWrapMode = UsdBridgeSamplerData::WrapMode::CLAMP;
      }
      else if (!std::strcmp(anariWrapMode, "repeat"))
      {
        usdWrapMode = UsdBridgeSamplerData::WrapMode::REPEAT;
      }
      else if (!std::strcmp(anariWrapMode, "mirrorRepeat"))
      {
        usdWrapMode = UsdBridgeSamplerData::WrapMode::MIRROR;
      }
    }
    return usdWrapMode;
  }
}

UsdSampler::UsdSampler(const char* name, const char* type, UsdBridge* bridge, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_SAMPLER, name, bridge)
{
  if (strcmp(type, "sphere") == 0)
    samplerType = SAMPLER_1D;
  else if (strcmp(type, "cylinder") == 0)
    samplerType = SAMPLER_2D;
  else if (strcmp(type, "cone") == 0)
    samplerType = SAMPLER_3D;
  else
    device->reportStatus(this, ANARI_SAMPLER, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdSampler '%s' construction failed: type %s not supported", getName(), name);
}

UsdSampler::~UsdSampler()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(usdBridge)
    usdBridge->DeleteSampler(usdHandle);
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

bool UsdSampler::deferCommit(UsdDevice* device)
{
  return false;
}

bool UsdSampler::doCommitData(UsdDevice* device)
{
  if(!usdBridge || 
    !device->getReadParams().outputMaterial ||
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
    UsdBridgeSamplerData samplerData;
    samplerData.Type = type;
  
    if(paramData.imageUrl)
    {
      samplerData.ImageUrl = UsdSharedString::c_str(paramData.imageUrl);
    }

    samplerData.WrapS = ANARIToUsdBridgeWrapMode(UsdSharedString::c_str(paramData.wrapS));
    samplerData.WrapT = ANARIToUsdBridgeWrapMode(UsdSharedString::c_str(paramData.wrapT));
    samplerData.WrapR = ANARIToUsdBridgeWrapMode(UsdSharedString::c_str(paramData.wrapR));

    samplerData.TimeVarying = (UsdBridgeSamplerData::DataMemberId)paramData.timeVarying;

    double worldTimeStep = device->getReadParams().timeStep;
    double dataTimeStep = selectObjTime(paramData.timeStep, worldTimeStep);
    usdBridge->SetSamplerData(usdHandle, samplerData, dataTimeStep);

    paramChanged = false;
  }

  return false;
}
