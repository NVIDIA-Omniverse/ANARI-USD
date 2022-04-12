// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

struct UsdSamplerData
{
  const char* name = nullptr;
  const char* usdName = nullptr;

  double timeStep = 0.0;
  int timeVarying = 0; // Bitmask indicating which attributes are time-varying. 0:fileName, 1:wrapS, 2:wrapT

  const char* fileName = nullptr;
  const char* wrapS = nullptr;
  const char* wrapT = nullptr;
};

class UsdSampler : public UsdBridgedBaseObject<UsdSampler, UsdSamplerData, UsdSamplerHandle>
{
  public:
    UsdSampler(const char* name, UsdBridge* bridge);
    ~UsdSampler();

    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    void filterResetParam(
      const char *name) override;

    void commit(UsdDevice* device) override;

  protected:
};