// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdBridge;

enum class UsdLightComponents
{
  COLOR = 0,
  INTENSITY,
  IRRADIANCE,
  POWER,
  RADIANCE,
  POSITION,
  DIRECTION,
  ANGULAR_DIAMETER,
  RADIUS
};

struct UsdLightData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // TimeVarying bits

  UsdFloat3 color = {1.0f, 1.0f, 1.0f};
  UsdFloat3 position = {0.0f, 0.0f, 0.0f};
  UsdFloat3 direction = {0.0f, 0.0f, -1.0f};
  float irradiance = -1.0f;
  float intensity = -1.0f;
  float power = -1.0f;
  float radiance = -1.0f;
  // ANARI directional light parameter, in radians. Default 0 = perfectly parallel.
  float angularDiameter = 0.0f;
  // ANARI point light parameter, sphere radius. Default 0 = true point source.
  float radius = 0.0f;
};

class UsdLight : public UsdBridgedBaseObject<UsdLight, UsdLightData, UsdLightHandle, UsdLightComponents>
{
  public:
    UsdLight(
      const char* name,
      const char* type,
      UsdDevice* device);
    ~UsdLight();

    void remove(UsdDevice* device) override;

    // Lights are not instanceable.
    bool isInstanceable() const { return false; }

    static constexpr ComponentPair componentParamNames[] = {
      ComponentPair(UsdLightComponents::COLOR, "color"),
      ComponentPair(UsdLightComponents::INTENSITY, "intensity"),
      ComponentPair(UsdLightComponents::IRRADIANCE, "irradiance"),
      ComponentPair(UsdLightComponents::POWER, "power"),
      ComponentPair(UsdLightComponents::RADIANCE, "radiance"),
      ComponentPair(UsdLightComponents::POSITION, "position"),
      ComponentPair(UsdLightComponents::DIRECTION, "direction"),
      ComponentPair(UsdLightComponents::ANGULAR_DIAMETER, "angularDiameter"),
      ComponentPair(UsdLightComponents::RADIUS, "radius")};

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    void CopyParameters(const UsdLightData& paramData, UsdBridgeDirectionalLightData& lightData);
    void CopyParameters(const UsdLightData& paramData,UsdBridgePointLightData& lightData);
    void CopyParameters(const UsdLightData& paramData, UsdBridgeDomeLightData& lightData);

    template<typename LightDataType>
    void SetLightData(UsdBridge* usdBridge, UsdLightHandle usdHandle, const UsdLightData& paramData, double timeStep);

    UsdBridgeLightType lightType = UsdBridgeLightType::DIRECTIONAL;
};
