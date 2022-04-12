// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

class UsdDataArray;

struct UsdTransferFunctionData
{
  const char* name = nullptr;
  const char* usdName = nullptr;

  double timeStep = 0.0;
  int timeVarying = 0xFFFFFFFF; // Bitmask indicating which attributes are time-varying. 0:color, 1:opacity, 2:valueRange
  
  
};

class UsdTransferFunction : public UsdBridgedBaseObject<UsdTransferFunction, UsdTransferFunctionData, UsdTransferFunctionHandle>
{
  public:
    UsdTransferFunction(const char* name, UsdBridge* bridge);
    ~UsdTransferFunction();

    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    void filterResetParam(
      const char *name) override;

    void commit(UsdDevice* device) override;

    friend class UsdVolumetricModel;
  protected:
};