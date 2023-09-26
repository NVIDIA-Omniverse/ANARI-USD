// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdGroup;

struct UsdInstanceData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying. 0:group, 1:transform
  UsdGroup* group = nullptr;
  UsdFloatMat4 transform;
};

class UsdInstance : public UsdBridgedBaseObject<UsdInstance, UsdInstanceData, UsdInstanceHandle>
{
  public:
    UsdInstance(const char* name,
      UsdDevice* device);
    ~UsdInstance();

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override;
};