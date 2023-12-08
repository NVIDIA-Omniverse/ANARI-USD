// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdGroup;

enum class UsdInstanceComponents
{
  GROUP = 0,
  TRANSFORM
};

struct UsdInstanceData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying.
  UsdGroup* group = nullptr;
  UsdFloatMat4 transform;
};

class UsdInstance : public UsdBridgedBaseObject<UsdInstance, UsdInstanceData, UsdInstanceHandle, UsdInstanceComponents>
{
  public:
    UsdInstance(const char* name,
      UsdDevice* device);
    ~UsdInstance();

    static constexpr ComponentPair componentParamNames[] = {
      ComponentPair(UsdInstanceComponents::GROUP, "group"),
      ComponentPair(UsdInstanceComponents::TRANSFORM, "transform")};

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override;
};