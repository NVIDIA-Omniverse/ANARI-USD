// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeNumerics_h
#define UsdBridgeNumerics_h

#include "UsdBridgeMacros.h"

#include <math.h>

struct UsdUint2
{
  using DataType = unsigned int;
  DataType Data[2] = { 0, 0 };
};

struct UsdFloat2
{
  using DataType = float;
  DataType Data[2] = { 1.0, 1.0 };
};

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

  inline void DirectionToQuaternionZ(float* dir, float dirLength, float* quat)
  {
    // Use Z axis of glyph to orient along.

    // (dot(|segDir|, zAxis), cross(|segDir|, zAxis)) gives (cos(th), axis*sin(th)),
    // but rotation is represented by cos(th/2), axis*sin(th/2), ie. half the amount of rotation.
    // So calculate (dot(|halfVec|, zAxis), cross(|halfVec|, zAxis)) instead.
    float invDirLength = 1.0f / dirLength;
    float halfVec[3] = {
      dir[0] * invDirLength,
      dir[1] * invDirLength,
      dir[2] * invDirLength + 1.0f
    };
    float halfNorm = sqrtf(halfVec[0] * halfVec[0] + halfVec[1] * halfVec[1] + halfVec[2] * halfVec[2]);
    if (halfNorm != 0.0f)
    {
      float invHalfNorm = 1.0f / halfNorm;
      halfVec[0] *= invHalfNorm;
      halfVec[1] *= invHalfNorm;
      halfVec[2] *= invHalfNorm;
    }

    // Cross zAxis (0,0,1) with segment direction (new Z axis) to get rotation axis * sin(angle)
    float sinAxis[3] = { -halfVec[1], halfVec[0], 0.0f };
    // Dot for cos(angle)
    float cosAngle = halfVec[2];

    if (halfNorm == 0.0f) // In this case there is a 180 degree rotation
    {
      sinAxis[1] = 1.0f; //sinAxis*sin(pi/2) = (0,1,0)*sin(pi/2) = (0,1,0)
      // cosAngle = cos(pi/2) = 0.0f;
    }

    quat[0] = cosAngle;
    quat[1] = sinAxis[0];
    quat[2] = sinAxis[1];
    quat[3] = sinAxis[2];
  }
}

#endif

