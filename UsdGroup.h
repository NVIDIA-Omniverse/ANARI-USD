// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"

class UsdDataArray;
class UsdBridge;

struct UsdGroupData
{
  const char* name = nullptr;
  const char* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying. 0:surfaces, 1:volumes
  const UsdDataArray* surfaces = nullptr;
  const UsdDataArray* volumes = nullptr;
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

    void commit(UsdDevice* device) override;

  protected:
    std::vector<UsdSurfaceHandle> surfaceHandles; // for convenience
    std::vector<UsdVolumeHandle> volumeHandles; // for convenience
};