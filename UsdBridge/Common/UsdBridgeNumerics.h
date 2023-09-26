// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeNumerics_h
#define UsdBridgeNumerics_h

#include "UsdBridgeMacros.h"


struct UsdFloat3
{
  using DataType = float;
  DataType Data[3] = { 1.0, 1.0, 1.0 };
};

struct UsdFloatMat4
{
  using DataType = float;
  DataType Data[16] = {
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      0, 0, 0, 1 };
};

struct UsdQuaternion
{
  using DataType = float;
  DataType Data[4] = {1.0, 0.0, 0.0, 0.0};
};

namespace usdbridgenumerics
{
  template<typename T>
  bool isIdentity(const T& val)
  {
    static T identity;
    return !memcmp(val.Data, identity.Data, sizeof(T));
  }
}

#endif

