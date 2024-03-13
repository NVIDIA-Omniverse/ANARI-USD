// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"
#include "UsdParameterizedObject.h"

class UsdRenderer;
class UsdWorld;

struct UsdFrameData
{
  UsdWorld* world = nullptr;
  UsdRenderer* renderer = nullptr;
  UsdUint2 size = {0, 0};
  ANARIDataType color = ANARI_UNKNOWN;
  ANARIDataType depth = ANARI_UNKNOWN;
};

class UsdFrame : public UsdParameterizedBaseObject<UsdFrame, UsdFrameData>
{
  public:
    UsdFrame(UsdBridge* bridge);
    ~UsdFrame();

    void remove(UsdDevice* device) override {}

    const void* mapBuffer(const char* channel,
      uint32_t *width,
      uint32_t *height,
      ANARIDataType *pixelType);
    void unmapBuffer(const char* channel);

    void saveUsd(UsdDevice* device);

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    char* ReserveBuffer(ANARIDataType format);

    char* mappedColorMem = nullptr;
    char* mappedDepthMem = nullptr;
};
