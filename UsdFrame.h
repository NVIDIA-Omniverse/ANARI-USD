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
  int size[2] = {0, 0};
  ANARIDataType color = ANARI_UNKNOWN;
  ANARIDataType depth = ANARI_UNKNOWN;
};

class UsdFrame : public UsdBaseObject, public UsdParameterizedObject<UsdFrame, UsdFrameData>
{
  public:
    UsdFrame(UsdBridge* bridge);
    ~UsdFrame();

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

    virtual void commit(UsdDevice* device) override
    {
      TransferWriteToReadParams();
      UsdBaseObject::commit(device);
    }

    const void* mapBuffer(const char* channel,
      uint32_t *width,
      uint32_t *height,
      ANARIDataType *pixelType);
    void unmapBuffer(const char* channel);

    UsdRenderer* getRenderer();

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    char* ReserveBuffer(ANARIDataType format);

    char* mappedColorMem = nullptr;
    char* mappedDepthMem = nullptr;
};
