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
  float transform[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
};

class UsdInstance : public UsdBridgedBaseObject<UsdInstance, UsdInstanceData, UsdInstanceHandle>
{
  public:
    UsdInstance(const char* name,
      UsdDevice* device);
    ~UsdInstance();

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
};