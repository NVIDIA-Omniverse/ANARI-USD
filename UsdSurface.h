// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdGeometry;
class UsdMaterial;

struct UsdSurfaceData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  // No timevarying data: geometry and material reference always set over all timesteps

  UsdGeometry* geometry = nullptr;
  UsdMaterial* material = nullptr;
};

class UsdSurface : public UsdBridgedBaseObject<UsdSurface, UsdSurfaceData, UsdSurfaceHandle>
{
  public:
    UsdSurface(const char* name, UsdBridge* bridge, UsdDevice* device);
    ~UsdSurface();
  
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
};