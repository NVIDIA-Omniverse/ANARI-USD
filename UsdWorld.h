// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdDataArray;

enum class UsdWorldComponents
{
  INSTANCES = 0,
  SURFACES,
  VOLUMES
};

struct UsdWorldData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying.
  UsdDataArray* instances = nullptr;
  UsdDataArray* surfaces = nullptr;
  UsdDataArray* volumes = nullptr;
};

class UsdWorld : public UsdBridgedBaseObject<UsdWorld, UsdWorldData, UsdWorldHandle, UsdWorldComponents>
{
  public:
    UsdWorld(const char* name, UsdDevice* device);
    ~UsdWorld();

    void remove(UsdDevice* device) override;

    static constexpr ComponentPair componentParamNames[] = {
      ComponentPair(UsdWorldComponents::INSTANCES, "instance"),
      ComponentPair(UsdWorldComponents::SURFACES, "surface"),
      ComponentPair(UsdWorldComponents::VOLUMES, "volume")};

  protected:

    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override;

    std::vector<UsdInstanceHandle> instanceHandles; // for convenience
    std::vector<UsdSurfaceHandle> surfaceHandles; // for convenience
    std::vector<UsdVolumeHandle> volumeHandles; // for convenience
};