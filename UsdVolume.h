// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdSpatialField;
class UsdDevice;
class UsdDataArray;

struct UsdVolumeData
{
  const char* name = nullptr;
  const char* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying. 0:color, 1:opacity, 2:valueRange (field reference always set over all timesteps)

  UsdSpatialField* field = nullptr;

  bool preClassified = false;
  
  //TF params
  const UsdDataArray* color = nullptr; 
  const UsdDataArray* opacity = nullptr;
  float valueRange[2] = { 0.0f, 1.0f };
  float densityScale = 1.0f;
};

class UsdVolume : public UsdBridgedBaseObject<UsdVolume, UsdVolumeData, UsdVolumeHandle>
{
  public:
    UsdVolume(const char* name, UsdBridge* bridge, UsdDevice* device);
    ~UsdVolume();
  
    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    void filterResetParam(
      const char *name) override;
  
    virtual void commit(UsdDevice* device);

  protected:

    bool CheckTfParams(UsdDevice* device);
    bool UpdateVolume(UsdDevice* device, const char* debugName);

    UsdSpatialField* prevField = nullptr;
};