// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdDataArray;
class UsdVolume;

enum class UsdSpatialFieldComponents
{
  DATA = 0 // includes spacing and origin
};

struct UsdSpatialFieldData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  double timeStep = std::numeric_limits<float>::quiet_NaN();
  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying.

  const UsdDataArray* data = nullptr;
  
  float gridSpacing[3] = {1.0f, 1.0f, 1.0f};
  float gridOrigin[3] = {1.0f, 1.0f, 1.0f};
  
  //int filter = 0;
  //int gradientFilter = 0;
};

class UsdSpatialField : public UsdBridgedBaseObject<UsdSpatialField, UsdSpatialFieldData, UsdSpatialFieldHandle, UsdSpatialFieldComponents>
{
  public:
    UsdSpatialField(const char* name, const char* type, UsdDevice* device);
    ~UsdSpatialField();

    void commit(UsdDevice* device) override;

    void remove(UsdDevice* device) override;

    friend class UsdVolume;

    static constexpr ComponentPair componentParamNames[] = {
      ComponentPair(UsdSpatialFieldComponents::DATA, "data")};

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    void toBridge(UsdDevice* device, const char* debugName);
};