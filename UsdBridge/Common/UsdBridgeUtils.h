// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeUtils_h
#define UsdBridgeUtils_h

#include "UsdBridgeData.h"

// USD-independent utils for the UsdBridge

namespace ubutils
{
  const char* UsdBridgeTypeToString(UsdBridgeType type);
  UsdBridgeType UsdBridgeTypeFlatten(UsdBridgeType type);

  const float* SrgbToLinearTable(); // returns a float[256] array
  float SrgbToLinear(float val);
  void SrgbToLinear3(float* color); // expects a float[3]
}

#endif