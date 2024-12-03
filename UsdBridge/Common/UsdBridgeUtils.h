// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeUtils_h
#define UsdBridgeUtils_h

#include "UsdBridgeData.h"

// USD-independent utils for the UsdBridge

#define UsdBridgeLogMacro(obj, level, message) \
  { std::stringstream logStream; \
    logStream << message; \
    std::string logString = logStream.str(); \
    obj.LogCallback(level, obj.LogUserData, logString.c_str()); }

namespace ubutils
{
  const char* UsdBridgeTypeToString(UsdBridgeType type);
  UsdBridgeType UsdBridgeTypeFlatten(UsdBridgeType type);

  const float* SrgbToLinearTable(); // returns a float[256] array
  float SrgbToLinear(float val);
  void SrgbToLinear3(float* color); // expects a float[3]

  template<typename DMI>
  DMI GetAttribBit(int attribIndex)
  {
    return (static_cast<DMI>(static_cast<int>(DMI::ATTRIBUTE0) << attribIndex));
  }
}

#endif