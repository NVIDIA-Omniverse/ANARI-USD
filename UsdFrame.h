// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"
#include "UsdParameterizedObject.h"

class UsdRenderer;
class UsdWorld;
class UsdCamera;

struct UsdFrameData
{
  UsdUint2 size = {0, 0};
  ANARIDataType color = ANARI_UNKNOWN;
  ANARIDataType depth = ANARI_UNKNOWN;
  UsdCamera* camera = nullptr;
  UsdRenderer* renderer = nullptr;
  UsdWorld* world = nullptr;

  double time = 0.0;
};

class UsdFrame : public UsdParameterizedBaseObject<UsdFrame, UsdFrameData>
{
  public:
    UsdFrame(UsdBridge* bridge);
    ~UsdFrame();

    void remove(UsdDevice* device) override {}

    const void* mapBuffer(
      const char* channel,
      uint32_t *width,
      uint32_t *height,
      ANARIDataType *pixelType,
      UsdDevice* device);
    void unmapBuffer(const char* channel, UsdDevice* device);

    void saveUsd(UsdDevice* device);
    void renderFrame(UsdDevice* device);
    bool frameReady(ANARIWaitMask mask, UsdDevice* device);

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    char* ReserveBuffer(ANARIDataType format);

    UsdUint2 renderBufferSize = {0, 0};
    ANARIDataType renderBufferColorFormat = ANARI_UNKNOWN;
    ANARIDataType renderBufferDepthFormat = ANARI_UNKNOWN;

    char* mappedColorMem = nullptr;
    char* mappedDepthMem = nullptr;
};
