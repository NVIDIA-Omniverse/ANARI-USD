// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeUtils_Internal_h
#define UsdBridgeUtils_Internal_h

#include "UsdBridgeData.h"

namespace
{
  inline bool strEquals(const char* arg0, const char* arg1)
  {
    return std::strcmp(arg0, arg1) == 0;
  }
}

#endif