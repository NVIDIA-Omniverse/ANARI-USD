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

    void saveUsd(UsdDevice* device);

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    UsdBridge* usdBridge;
};
