// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"
#include "UsdParameterizedObject.h"

struct UsdRendererData
{

};


class UsdRenderer : public UsdBaseObject, public UsdParameterizedObject<UsdRenderer, UsdRendererData>
{
  public:
    UsdRenderer(UsdBridge* bridge);
    ~UsdRenderer();

    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    void filterResetParam(
      const char *name) override;

    int getProperty(const char *name,
      ANARIDataType type,
      void *mem,
      uint64_t size,
      UsdDevice* device) override;

    void saveUsd();

  protected:
    bool deferCommit(UsdDevice* device) override;
    void doCommitWork(UsdDevice* device) override;

    UsdBridge* usdBridge;
};
