// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdDataArray;

struct UsdWorldData
{
  const char* name = nullptr;
  const char* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying. 0:instances
  UsdDataArray* instances = nullptr;
};

class UsdWorld : public UsdBridgedBaseObject<UsdWorld, UsdWorldData, UsdWorldHandle>
{
  public:
    UsdWorld(const char* name, UsdBridge* bridge);
    ~UsdWorld();

    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    void filterResetParam(
      const char *name) override;

  protected:
    bool deferCommit(UsdDevice* device) override;
    void doCommitWork(UsdDevice* device) override;

    std::vector<UsdInstanceHandle> instanceHandles; // for convenience
};