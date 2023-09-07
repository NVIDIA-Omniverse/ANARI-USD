// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

#include <limits>

class UsdGeometry;
class UsdMaterial;

struct UsdSurfaceData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  // No timevarying data: geometry and material reference always set over all timesteps

  UsdGeometry* geometry = nullptr;
  UsdMaterial* material = nullptr;

  double geometryRefTimeStep = std::numeric_limits<float>::quiet_NaN();
  double materialRefTimeStep = std::numeric_limits<float>::quiet_NaN();
};

class UsdSurface : public UsdBridgedBaseObject<UsdSurface, UsdSurfaceData, UsdSurfaceHandle>
{
  public:
    UsdSurface(const char* name, UsdDevice* device);
    ~UsdSurface();

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override;
};