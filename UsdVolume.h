// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

#include <limits>

class UsdSpatialField;
class UsdDevice;
class UsdDataArray;

enum class UsdVolumeComponents
{
  COLOR = 0,
  OPACITY,
  VALUERANGE
};

struct UsdVolumeData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying. (field reference always set over all timesteps)

  bool isInstanceable = false;

  UsdSpatialField* field = nullptr;
  double fieldRefTimeStep = std::numeric_limits<float>::quiet_NaN();

  bool preClassified = false;

  //TF params
  const UsdDataArray* color = nullptr;
  const UsdDataArray* opacity = nullptr;
  UsdFloatBox1 valueRange = { 0.0f, 1.0f };
  float unitDistance = 1.0f;
};

class UsdVolume : public UsdBridgedBaseObject<UsdVolume, UsdVolumeData, UsdVolumeHandle, UsdVolumeComponents>
{
  public:
    UsdVolume(const char* name, UsdDevice* device);
    ~UsdVolume();

    void remove(UsdDevice* device) override;

    bool isInstanceable() const;

    static constexpr ComponentPair componentParamNames[] = {
      ComponentPair(UsdVolumeComponents::COLOR, "color"),
      ComponentPair(UsdVolumeComponents::OPACITY, "opacity"),
      ComponentPair(UsdVolumeComponents::VALUERANGE, "valueRange")};

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    void onParamRefChanged(UsdBaseObject* paramObject, bool incRef, bool onWriteParams) override;
    void observe(UsdBaseObject* caller, UsdDevice* device) override;

    bool CheckTfParams(UsdDevice* device);
    bool UpdateVolume(UsdDevice* device, const char* debugName);

    UsdSpatialField* prevField = nullptr;
    UsdDevice* usdDevice = nullptr;
};