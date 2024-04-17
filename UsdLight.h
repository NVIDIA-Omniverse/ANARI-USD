// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

struct UsdLightData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;
};

class UsdLight : public UsdBridgedBaseObject<UsdLight, UsdLightData, UsdLightHandle>
{
  public:
    UsdLight(
      const char* type,
      const char* name,
      UsdDevice* device);
    ~UsdLight();

    void remove(UsdDevice* device) override;

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}
};
