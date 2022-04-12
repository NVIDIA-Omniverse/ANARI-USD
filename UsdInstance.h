// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdGroup;

struct UsdInstanceData
{
  const char* name = nullptr;
  const char* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying. 0:group, 1:transform
  const UsdGroup* group = nullptr;
  float transform[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
};

class UsdInstance : public UsdBridgedBaseObject<UsdInstance, UsdInstanceData, UsdInstanceHandle>
{
  public:
    UsdInstance(const char* name, UsdBridge* bridge, 
      UsdDevice* device);
    ~UsdInstance();

    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    void filterResetParam(
      const char *name) override;

    void commit(UsdDevice* device) override;

  protected:
};