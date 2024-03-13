// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"
#include "UsdParameterizedObject.h"

struct UsdRendererData
{

};

class UsdRenderer : public UsdParameterizedBaseObject<UsdRenderer, UsdRendererData>
{
  public:
    UsdRenderer();
    ~UsdRenderer();

    void remove(UsdDevice* device) override {}

    int getProperty(const char * name, ANARIDataType type, void * mem, uint64_t size, UsdDevice* device) override;

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    UsdBridge* usdBridge;
};
