// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdDataArray;

struct UsdWorldData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying. 0:instances, 1:surfaces, 2:volumes
  UsdDataArray* instances = nullptr;
  UsdDataArray* surfaces = nullptr;
  UsdDataArray* volumes = nullptr;
};

class UsdWorld : public UsdBridgedBaseObject<UsdWorld, UsdWorldData, UsdWorldHandle>
{
  public:
    UsdWorld(const char* name);
    ~UsdWorld();

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

    std::vector<UsdInstanceHandle> instanceHandles; // for convenience
    std::vector<UsdSurfaceHandle> surfaceHandles; // for convenience
    std::vector<UsdVolumeHandle> volumeHandles; // for convenience
};