// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"

class UsdRenderer;

struct UsdFrameData
{
  UsdRenderer* renderer = nullptr;
  int size[2];
  ANARIDataType color;
  ANARIDataType depth;
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

    void commit(UsdDevice* device) override;

    const void* mapBuffer(const char* channel);
    void unmapBuffer(const char* channel);

    UsdRenderer* getRenderer();

  protected:

    char* ReserveBuffer(ANARIDataType format);

    char* mappedColorMem = nullptr;
    char* mappedDepthMem = nullptr;
};
