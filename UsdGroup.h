// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdDataArray;
class UsdBridge;

struct UsdGroupData
{
  const char* name = nullptr;
  const char* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying. 0:surfaces, 1:volumes
  UsdDataArray* surfaces = nullptr;
  UsdDataArray* volumes = nullptr;
};

class UsdGroup : public UsdBridgedBaseObject<UsdGroup, UsdGroupData, UsdGroupHandle>
{
  public:
    UsdGroup(const char* name, UsdBridge* bridge, UsdDevice* device);
    ~UsdGroup();

    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    void filterResetParam(
      const char *name) override;

  protected:
    bool deferCommit(UsdDevice* device) override;
    void doCommitWork(UsdDevice* device) override;

    std::vector<UsdSurfaceHandle> surfaceHandles; // for convenience
    std::vector<UsdVolumeHandle> volumeHandles; // for convenience
};