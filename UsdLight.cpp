// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdLight.h"
#include "UsdAnari.h"
#include "UsdDevice.h"

DEFINE_PARAMETER_MAP(UsdLight,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timeVarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("color", ANARI_FLOAT32_VEC3, color)
  REGISTER_PARAMETER_MACRO("position", ANARI_FLOAT32_VEC3, position)
  REGISTER_PARAMETER_MACRO("direction", ANARI_FLOAT32_VEC3, direction)
  REGISTER_PARAMETER_MACRO("irradiance", ANARI_FLOAT32, irradiance)
  REGISTER_PARAMETER_MACRO("intensity", ANARI_FLOAT32, intensity)
  REGISTER_PARAMETER_MACRO("power", ANARI_FLOAT32, power)
)

namespace
{
  UsdBridgeLightType GetLightType(const char* type)
  {
    UsdBridgeLightType lightType;

    if (strEquals(type, "directional"))
      lightType = UsdBridgeLightType::DIRECTIONAL;
    else if (strEquals(type, "point"))
      lightType = UsdBridgeLightType::POINT;
    else
      lightType = UsdBridgeLightType::DOME;

    return lightType;
  }
}

UsdLight::UsdLight(const char* name, const char* type, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_LIGHT, name, device)
{
  lightType = GetLightType(type);

}

UsdLight::~UsdLight()
{}

void UsdLight::remove(UsdDevice* device)
{
  applyRemoveFunc(device, &UsdBridge::DeleteLight);
}

void UsdLight::CopyParameters(const UsdLightData& paramData, UsdBridgeDirectionalLightData& lightData)
{
  typedef UsdBridgeDirectionalLightData::DataMemberId DMI;

  lightData.Color = paramData.color;
  lightData.Intensity = (paramData.irradiance == -1.0f) ? 1.0f : paramData.irradiance;
  lightData.Direction = paramData.direction;

  lightData.TimeVarying = DMI::ALL
    & (isTimeVarying(CType::COLOR) ? DMI::ALL : ~DMI::COLOR)
    & (isTimeVarying(CType::INTENSITY) ? DMI::ALL : ~DMI::INTENSITY)
    & (isTimeVarying(CType::DIRECTION) ? DMI::ALL : ~DMI::DIRECTION);
}

void UsdLight::CopyParameters(const UsdLightData& paramData,UsdBridgePointLightData& lightData)
{
  typedef UsdBridgePointLightData::DataMemberId DMI;

  lightData.Color = paramData.color;
  lightData.Intensity = (paramData.intensity == -1.0f) ?
    ((paramData.power == -1.0f) ? paramData.irradiance : paramData.power/(4*M_PI))
    : paramData.intensity;
  lightData.Position = paramData.position;

  lightData.TimeVarying = DMI::ALL
    & (isTimeVarying(CType::COLOR) ? DMI::ALL : ~DMI::COLOR)
    & (isTimeVarying(CType::INTENSITY) ? DMI::ALL : ~DMI::INTENSITY)
    & (isTimeVarying(CType::POWER) ? DMI::ALL : ~DMI::INTENSITY)
    & (isTimeVarying(CType::IRRADIANCE) ? DMI::ALL : ~DMI::INTENSITY)
    & (isTimeVarying(CType::POSITION) ? DMI::ALL : ~DMI::POSITION);
}

void UsdLight::CopyParameters(const UsdLightData& paramData, UsdBridgeDomeLightData& lightData)
{
  typedef UsdBridgeDomeLightData::DataMemberId DMI;

  lightData.Color = paramData.color;
  lightData.Intensity = (paramData.intensity == -1.0f) ? 1.0f : paramData.intensity;

  lightData.TimeVarying = DMI::ALL
    & (isTimeVarying(CType::COLOR) ? DMI::ALL : ~DMI::COLOR)
    & (isTimeVarying(CType::INTENSITY) ? DMI::ALL : ~DMI::INTENSITY);
}

template<typename LightDataType>
void UsdLight::SetLightData(UsdBridge* usdBridge, UsdLightHandle usdHandle, const UsdLightData& paramData, double timeStep)
{
  LightDataType lightData;
  CopyParameters(paramData, lightData);

  usdBridge->SetLightData(usdHandle, lightData, timeStep);
}

bool UsdLight::deferCommit(UsdDevice* device) 
{ 
  return false; 
}

bool UsdLight::doCommitData(UsdDevice* device) 
{
  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdLightData& paramData = getReadParams();
  double timeStep = device->getReadParams().timeStep;

  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateLight(getName(), lightType, usdHandle);

  if (paramChanged || isNew)
  {
    switch(lightType)
    {
      case UsdBridgeLightType::DIRECTIONAL: SetLightData<UsdBridgeDirectionalLightData>(usdBridge, usdHandle, paramData, timeStep); break;
      case UsdBridgeLightType::POINT: SetLightData<UsdBridgePointLightData>(usdBridge, usdHandle, paramData, timeStep); break;
      case UsdBridgeLightType::DOME: SetLightData<UsdBridgeDomeLightData>(usdBridge, usdHandle, paramData, timeStep); break;
      default: break;
    }

    paramChanged = false;
  }

  return false;
}