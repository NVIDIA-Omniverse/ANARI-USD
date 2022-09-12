// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"
#include "UsdBridgeNumerics.h"
#include <limits>

class UsdSampler;

struct UsdMaterialData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  double timeStep = std::numeric_limits<float>::quiet_NaN();
  int timeVarying = 0; // Bitmask indicating which attributes are time-varying. 0: color, 1: opacity, 2: specular, 
    // 3: emissive, 4: emissiveintensity, 5: roughness, 6: metallic, 7: ior

  UsdMultiTypeParameter<UsdFloat3, UsdSampler*, UsdSharedString*> color = {{ 1.0f, 1.0f, 1.0f }, ANARI_FLOAT32_VEC3};
  UsdMultiTypeParameter<float, UsdSampler*, UsdSharedString*> opacity = {0.0f, ANARI_FLOAT32};

  float specular[3] = { 1.0f, 1.0f, 1.0f };
  float emissive[3] = { 1.0f, 1.0f, 1.0f };
  float emissiveIntensity = 0.0f;
  float shininess = 0.5f;
  float metallic = -1.0f;
  float ior = 1.0f;

  double colorSamplerTimeStep = std::numeric_limits<float>::quiet_NaN();
  double opacitySamplerTimeStep = std::numeric_limits<float>::quiet_NaN();
};

class UsdMaterial : public UsdBridgedBaseObject<UsdMaterial, UsdMaterialData, UsdMaterialHandle>
{
  public:
    UsdMaterial(const char* name, const char* type, UsdBridge* bridge, UsdDevice* device);
    ~UsdMaterial();

    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    void filterResetParam(
      const char *name) override;

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override;

    bool IsTranslucent = false;
    bool IsPbr = false;
};