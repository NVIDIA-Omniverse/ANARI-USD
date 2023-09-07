// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

#include <limits>

class UsdSpatialField;
class UsdDevice;
class UsdDataArray;

struct UsdVolumeData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying. 0:color, 1:opacity, 2:valueRange (field reference always set over all timesteps)

  UsdSpatialField* field = nullptr;
  double fieldRefTimeStep = std::numeric_limits<float>::quiet_NaN();

  bool preClassified = false;

  //TF params
  const UsdDataArray* color = nullptr;
  const UsdDataArray* opacity = nullptr;
  float valueRange[2] = { 0.0f, 1.0f };
  float unitDistance = 1.0f;
};

class UsdVolume : public UsdBridgedBaseObject<UsdVolume, UsdVolumeData, UsdVolumeHandle>
{
  public:
    UsdVolume(const char* name, UsdDevice* device);
    ~UsdVolume();

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    bool CheckTfParams(UsdDevice* device);
    bool UpdateVolume(UsdDevice* device, const char* debugName);

    UsdSpatialField* prevField = nullptr;
    UsdDevice* usdDevice = nullptr;
};