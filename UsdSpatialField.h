// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"

class UsdDataArray;

struct UsdSpatialFieldData
{
  const char* name = nullptr;
  const char* usdName = nullptr;

  double timeStep = 0.0;
  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying. 0:data, 1:gridSpacing, 2:gridOrigin

  const UsdDataArray* data = nullptr;
  
  float gridSpacing[3] = {1.0f, 1.0f, 1.0f};
  float gridOrigin[3] = {1.0f, 1.0f, 1.0f};
  
  //int filter = 0;
  //int gradientFilter = 0;
};

class UsdSpatialField : public UsdBridgedBaseObject<UsdSpatialField, UsdSpatialFieldData, UsdSpatialFieldHandle>
{
  protected:

  public:
    UsdSpatialField(const char* name, const char* type, UsdBridge* bridge);
    ~UsdSpatialField();

    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    void filterResetParam(
      const char *name) override;

    void commit(UsdDevice* device) override;

    friend class UsdVolume;

  protected:

    void toBridge(UsdDevice* device, const char* debugName);
};