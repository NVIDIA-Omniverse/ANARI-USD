// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeUtils.h"


const char* UsdBridgeTypeToString(UsdBridgeType type)
{
  const char* typeStr = nullptr;
  switch(type)  
  {
    case UsdBridgeType::BOOL: typeStr = "BOOL"; break;
    case UsdBridgeType::UCHAR: typeStr = "UCHAR"; break;
    case UsdBridgeType::CHAR: typeStr = "CHAR"; break;
    case UsdBridgeType::USHORT: typeStr = "USHORT"; break;
    case UsdBridgeType::SHORT: typeStr = "SHORT"; break;
    case UsdBridgeType::UINT: typeStr = "UINT"; break;
    case UsdBridgeType::INT: typeStr = "INT"; break;
    case UsdBridgeType::ULONG: typeStr = "ULONG"; break;
    case UsdBridgeType::LONG: typeStr = "LONG"; break;
    case UsdBridgeType::HALF: typeStr = "HALF"; break;
    case UsdBridgeType::FLOAT: typeStr = "FLOAT"; break;
    case UsdBridgeType::DOUBLE: typeStr = "DOUBLE"; break;
    case UsdBridgeType::UCHAR2: typeStr = "UCHAR2"; break;
    case UsdBridgeType::CHAR2: typeStr = "CHAR2"; break;
    case UsdBridgeType::USHORT2: typeStr = "USHORT2"; break;
    case UsdBridgeType::SHORT2: typeStr = "SHORT2"; break;
    case UsdBridgeType::UINT2: typeStr = "UINT2"; break;
    case UsdBridgeType::INT2: typeStr = "INT2"; break;
    case UsdBridgeType::ULONG2: typeStr = "ULONG2"; break;
    case UsdBridgeType::LONG2: typeStr = "LONG2"; break;
    case UsdBridgeType::HALF2: typeStr = "HALF2"; break;
    case UsdBridgeType::FLOAT2: typeStr = "FLOAT2"; break;
    case UsdBridgeType::DOUBLE2: typeStr = "DOUBLE2"; break;
    case UsdBridgeType::UCHAR3: typeStr = "UCHAR3"; break;
    case UsdBridgeType::CHAR3: typeStr = "CHAR3"; break;
    case UsdBridgeType::USHORT3: typeStr = "USHORT3"; break;
    case UsdBridgeType::SHORT3: typeStr = "SHORT3"; break;
    case UsdBridgeType::UINT3: typeStr = "UINT3"; break;
    case UsdBridgeType::INT3: typeStr = "INT3"; break;
    case UsdBridgeType::ULONG3: typeStr = "ULONG3"; break;
    case UsdBridgeType::LONG3: typeStr = "LONG3"; break;
    case UsdBridgeType::HALF3: typeStr = "HALF3"; break;
    case UsdBridgeType::FLOAT3: typeStr = "FLOAT3"; break;
    case UsdBridgeType::DOUBLE3: typeStr = "DOUBLE3"; break;
    case UsdBridgeType::UCHAR4: typeStr = "UCHAR4"; break;
    case UsdBridgeType::CHAR4: typeStr = "CHAR4"; break;
    case UsdBridgeType::USHORT4: typeStr = "USHORT4"; break;
    case UsdBridgeType::SHORT4: typeStr = "SHORT4"; break;
    case UsdBridgeType::UINT4: typeStr = "UINT4"; break;
    case UsdBridgeType::INT4: typeStr = "INT4"; break;
    case UsdBridgeType::ULONG4: typeStr = "ULONG4"; break;
    case UsdBridgeType::LONG4: typeStr = "LONG4"; break;
    case UsdBridgeType::HALF4: typeStr = "HALF4"; break;
    case UsdBridgeType::FLOAT4: typeStr = "FLOAT4"; break;
    case UsdBridgeType::DOUBLE4: typeStr = "DOUBLE4"; break;
    case UsdBridgeType::UNDEFINED: 
    default: typeStr = "UNDEFINED"; break;
  }
  return typeStr;
}