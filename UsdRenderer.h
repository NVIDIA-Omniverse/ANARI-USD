// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"

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

    void commit(UsdDevice* device) override;

    void saveUsd();

  protected:
    UsdBridge* usdBridge;
};
