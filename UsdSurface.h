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

    void remove(UsdDevice* device) override;

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override;

    void onParamRefChanged(UsdBaseObject* paramObject, bool incRef, bool onWriteParams) override;
    void observe(UsdBaseObject* caller, UsdDevice* device) override;

    // When geometry notifies surface, the bound parameters have to be updated.
    // The parameters are not time-varying, so they can be represented by a universal reference.
    bool updateBoundParameters = false;
};