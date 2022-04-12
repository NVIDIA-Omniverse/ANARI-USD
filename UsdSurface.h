// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdGeometry;
class UsdMaterial;
class UsdDevice;

struct UsdSurfaceData
{
  const char* name = nullptr;
  const char* usdName = nullptr;

  // No timevarying data: geometry and material reference always set over all timesteps

  const UsdGeometry* geometry = nullptr;
  const UsdMaterial* material = nullptr;
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
  
    virtual void commit(UsdDevice* device);

  protected:
};