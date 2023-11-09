// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeUtils.h"

#include <cmath>

namespace ubutils
{

  const char* UsdBridgeTypeToString(UsdBridgeType type)
  {
    const char* typeStr = nullptr;
    switch(type)
    {
      case UsdBridgeType::BOOL: typeStr = "BOOL"; break;
      case UsdBridgeType::UCHAR: typeStr = "UCHAR"; break;
      case UsdBridgeType::UCHAR_SRGB_R: typeStr = "UCHAR_SRGB_R"; break;
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
      case UsdBridgeType::UCHAR_SRGB_RA: typeStr = "UCHAR_SRGB_RA"; break;
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
      case UsdBridgeType::UCHAR_SRGB_RGB: typeStr = "UCHAR_SRGB_RGB"; break;
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
      case UsdBridgeType::UCHAR_SRGB_RGBA: typeStr = "UCHAR_SRGB_RGBA"; break;
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
      case UsdBridgeType::INT_PAIR: typeStr = "INT_PAIR"; break;
      case UsdBridgeType::INT_PAIR2: typeStr = "INT_PAIR2"; break;
      case UsdBridgeType::INT_PAIR3: typeStr = "INT_PAIR3"; break;
      case UsdBridgeType::INT_PAIR4: typeStr = "INT_PAIR4"; break;
      case UsdBridgeType::FLOAT_PAIR: typeStr = "FLOAT_PAIR"; break;
      case UsdBridgeType::FLOAT_PAIR2: typeStr = "FLOAT_PAIR2"; break;
      case UsdBridgeType::FLOAT_PAIR3: typeStr = "FLOAT_PAIR3"; break;
      case UsdBridgeType::FLOAT_PAIR4: typeStr = "FLOAT_PAIR4"; break;
      case UsdBridgeType::DOUBLE_PAIR: typeStr = "DOUBLE_PAIR"; break;
      case UsdBridgeType::DOUBLE_PAIR2: typeStr = "DOUBLE_PAIR2"; break;
      case UsdBridgeType::DOUBLE_PAIR3: typeStr = "DOUBLE_PAIR3"; break;
      case UsdBridgeType::DOUBLE_PAIR4: typeStr = "DOUBLE_PAIR4"; break;
      case UsdBridgeType::ULONG_PAIR: typeStr = "ULONG_PAIR"; break;
      case UsdBridgeType::ULONG_PAIR2: typeStr = "ULONG_PAIR2"; break;
      case UsdBridgeType::ULONG_PAIR3: typeStr = "ULONG_PAIR3"; break;
      case UsdBridgeType::ULONG_PAIR4: typeStr = "ULONG_PAIR4"; break;
      case UsdBridgeType::FLOAT_MAT2: typeStr = "FLOAT_MAT2"; break;
      case UsdBridgeType::FLOAT_MAT3: typeStr = "FLOAT_MAT3"; break;
      case UsdBridgeType::FLOAT_MAT4: typeStr = "FLOAT_MAT4"; break;
      case UsdBridgeType::FLOAT_MAT2x3: typeStr = "FLOAT_MAT2x3"; break;
      case UsdBridgeType::FLOAT_MAT3x4: typeStr = "FLOAT_MAT3x4"; break;
      case UsdBridgeType::UNDEFINED: default: typeStr = "UNDEFINED"; break;
    }
    return typeStr;
  }

  UsdBridgeType UsdBridgeTypeFlatten(UsdBridgeType type)
  {
    UsdBridgeType flatType = UsdBridgeType::UNDEFINED;
    switch(type)
    {
      case UsdBridgeType::BOOL:
      case UsdBridgeType::UCHAR:
      case UsdBridgeType::UCHAR_SRGB_R:
      case UsdBridgeType::CHAR:
      case UsdBridgeType::USHORT:
      case UsdBridgeType::SHORT:
      case UsdBridgeType::UINT:
      case UsdBridgeType::INT:
      case UsdBridgeType::ULONG:
      case UsdBridgeType::LONG:
      case UsdBridgeType::HALF:
      case UsdBridgeType::FLOAT:
      case UsdBridgeType::DOUBLE: flatType = type; break;
      case UsdBridgeType::UCHAR2: flatType = UsdBridgeType::UCHAR; break;
      case UsdBridgeType::UCHAR_SRGB_RA: flatType = UsdBridgeType::UCHAR_SRGB_R; break;
      case UsdBridgeType::CHAR2: flatType = UsdBridgeType::CHAR; break;
      case UsdBridgeType::USHORT2: flatType = UsdBridgeType::USHORT; break;
      case UsdBridgeType::SHORT2: flatType = UsdBridgeType::SHORT; break;
      case UsdBridgeType::UINT2: flatType = UsdBridgeType::UINT; break;
      case UsdBridgeType::INT2: flatType = UsdBridgeType::INT; break;
      case UsdBridgeType::ULONG2: flatType = UsdBridgeType::ULONG; break;
      case UsdBridgeType::LONG2: flatType = UsdBridgeType::LONG; break;
      case UsdBridgeType::HALF2: flatType = UsdBridgeType::HALF; break;
      case UsdBridgeType::FLOAT2: flatType = UsdBridgeType::FLOAT; break;
      case UsdBridgeType::DOUBLE2: flatType = UsdBridgeType::DOUBLE; break;
      case UsdBridgeType::UCHAR3: flatType = UsdBridgeType::UCHAR; break;
      case UsdBridgeType::UCHAR_SRGB_RGB: flatType = UsdBridgeType::UCHAR_SRGB_R; break;
      case UsdBridgeType::CHAR3: flatType = UsdBridgeType::CHAR; break;
      case UsdBridgeType::USHORT3:flatType = UsdBridgeType::USHORT; break;
      case UsdBridgeType::SHORT3: flatType = UsdBridgeType::SHORT; break;
      case UsdBridgeType::UINT3: flatType = UsdBridgeType::UINT; break;
      case UsdBridgeType::INT3: flatType = UsdBridgeType::INT; break;
      case UsdBridgeType::ULONG3: flatType = UsdBridgeType::ULONG; break;
      case UsdBridgeType::LONG3: flatType = UsdBridgeType::LONG; break;
      case UsdBridgeType::HALF3: flatType = UsdBridgeType::HALF; break;
      case UsdBridgeType::FLOAT3: flatType = UsdBridgeType::FLOAT; break;
      case UsdBridgeType::DOUBLE3: flatType = UsdBridgeType::DOUBLE; break;
      case UsdBridgeType::UCHAR4: flatType = UsdBridgeType::UCHAR; break;
      case UsdBridgeType::UCHAR_SRGB_RGBA: flatType = UsdBridgeType::UCHAR_SRGB_R; break;
      case UsdBridgeType::CHAR4: flatType = UsdBridgeType::CHAR; break;
      case UsdBridgeType::USHORT4: flatType = UsdBridgeType::USHORT; break;
      case UsdBridgeType::SHORT4: flatType = UsdBridgeType::SHORT; break;
      case UsdBridgeType::UINT4: flatType = UsdBridgeType::UINT; break;
      case UsdBridgeType::INT4: flatType = UsdBridgeType::INT; break;
      case UsdBridgeType::ULONG4: flatType = UsdBridgeType::ULONG; break;
      case UsdBridgeType::LONG4: flatType = UsdBridgeType::LONG; break;
      case UsdBridgeType::HALF4: flatType = UsdBridgeType::HALF; break;
      case UsdBridgeType::FLOAT4: flatType = UsdBridgeType::FLOAT; break;
      case UsdBridgeType::DOUBLE4: flatType = UsdBridgeType::DOUBLE; break;
      case UsdBridgeType::INT_PAIR:
      case UsdBridgeType::INT_PAIR2:
      case UsdBridgeType::INT_PAIR3:
      case UsdBridgeType::INT_PAIR4: flatType = UsdBridgeType::INT; break;
      case UsdBridgeType::FLOAT_PAIR:
      case UsdBridgeType::FLOAT_PAIR2:
      case UsdBridgeType::FLOAT_PAIR3:
      case UsdBridgeType::FLOAT_PAIR4: flatType = UsdBridgeType::FLOAT; break;
      case UsdBridgeType::DOUBLE_PAIR:
      case UsdBridgeType::DOUBLE_PAIR2:
      case UsdBridgeType::DOUBLE_PAIR3:
      case UsdBridgeType::DOUBLE_PAIR4: flatType = UsdBridgeType::DOUBLE; break;
      case UsdBridgeType::ULONG_PAIR:
      case UsdBridgeType::ULONG_PAIR2:
      case UsdBridgeType::ULONG_PAIR3:
      case UsdBridgeType::ULONG_PAIR4: flatType = UsdBridgeType::ULONG; break;
      case UsdBridgeType::FLOAT_MAT2:
      case UsdBridgeType::FLOAT_MAT3:
      case UsdBridgeType::FLOAT_MAT4:
      case UsdBridgeType::FLOAT_MAT2x3:
      case UsdBridgeType::FLOAT_MAT3x4: flatType = UsdBridgeType::FLOAT; break;
      case UsdBridgeType::UNDEFINED: default: flatType = UsdBridgeType::UNDEFINED; break;
    }
    return flatType;
  }

  const unsigned int SRGBTable[] =
  {
      0x00000000,0x399f22b4,0x3a1f22b4,0x3a6eb40e,0x3a9f22b4,0x3ac6eb61,0x3aeeb40e,0x3b0b3e5d,
      0x3b1f22b4,0x3b33070b,0x3b46eb61,0x3b5b518d,0x3b70f18d,0x3b83e1c6,0x3b8fe616,0x3b9c87fd,
      0x3ba9c9b7,0x3bb7ad6f,0x3bc63549,0x3bd56361,0x3be539c1,0x3bf5ba70,0x3c0373b5,0x3c0c6152,
      0x3c15a703,0x3c1f45be,0x3c293e6b,0x3c3391f7,0x3c3e4149,0x3c494d43,0x3c54b6c7,0x3c607eb1,
      0x3c6ca5df,0x3c792d22,0x3c830aa8,0x3c89af9f,0x3c9085db,0x3c978dc5,0x3c9ec7c2,0x3ca63433,
      0x3cadd37d,0x3cb5a601,0x3cbdac20,0x3cc5e639,0x3cce54ab,0x3cd6f7d5,0x3cdfd010,0x3ce8ddb9,
      0x3cf2212c,0x3cfb9ac1,0x3d02a569,0x3d0798dc,0x3d0ca7e6,0x3d11d2af,0x3d171963,0x3d1c7c2e,
      0x3d21fb3c,0x3d2796b2,0x3d2d4ebb,0x3d332380,0x3d39152b,0x3d3f23e3,0x3d454fd1,0x3d4b991c,
      0x3d51ffef,0x3d58846a,0x3d5f26b7,0x3d65e6fe,0x3d6cc564,0x3d73c20f,0x3d7add29,0x3d810b67,
      0x3d84b795,0x3d887330,0x3d8c3e4a,0x3d9018f6,0x3d940345,0x3d97fd4a,0x3d9c0716,0x3da020bb,
      0x3da44a4b,0x3da883d7,0x3daccd70,0x3db12728,0x3db59112,0x3dba0b3b,0x3dbe95b5,0x3dc33092,
      0x3dc7dbe2,0x3dcc97b6,0x3dd1641f,0x3dd6412c,0x3ddb2eef,0x3de02d77,0x3de53cd5,0x3dea5d19,
      0x3def8e52,0x3df4d091,0x3dfa23e8,0x3dff8861,0x3e027f07,0x3e054280,0x3e080ea3,0x3e0ae378,
      0x3e0dc105,0x3e10a754,0x3e13966b,0x3e168e52,0x3e198f10,0x3e1c98ad,0x3e1fab30,0x3e22c6a3,
      0x3e25eb09,0x3e29186c,0x3e2c4ed0,0x3e2f8e41,0x3e32d6c4,0x3e362861,0x3e39831e,0x3e3ce703,
      0x3e405416,0x3e43ca5f,0x3e4749e4,0x3e4ad2ae,0x3e4e64c2,0x3e520027,0x3e55a4e6,0x3e595303,
      0x3e5d0a8b,0x3e60cb7c,0x3e6495e0,0x3e6869bf,0x3e6c4720,0x3e702e0c,0x3e741e84,0x3e781890,
      0x3e7c1c38,0x3e8014c2,0x3e82203c,0x3e84308d,0x3e8645ba,0x3e885fc5,0x3e8a7eb2,0x3e8ca283,
      0x3e8ecb3d,0x3e90f8e1,0x3e932b74,0x3e9562f8,0x3e979f71,0x3e99e0e2,0x3e9c274e,0x3e9e72b7,
      0x3ea0c322,0x3ea31892,0x3ea57308,0x3ea7d289,0x3eaa3718,0x3eaca0b7,0x3eaf0f69,0x3eb18333,
      0x3eb3fc18,0x3eb67a18,0x3eb8fd37,0x3ebb8579,0x3ebe12e1,0x3ec0a571,0x3ec33d2d,0x3ec5da17,
      0x3ec87c33,0x3ecb2383,0x3ecdd00b,0x3ed081cd,0x3ed338cc,0x3ed5f50b,0x3ed8b68d,0x3edb7d54,
      0x3ede4965,0x3ee11ac1,0x3ee3f16b,0x3ee6cd67,0x3ee9aeb6,0x3eec955d,0x3eef815d,0x3ef272ba,
      0x3ef56976,0x3ef86594,0x3efb6717,0x3efe6e02,0x3f00bd2d,0x3f02460e,0x3f03d1a7,0x3f055ff9,
      0x3f06f106,0x3f0884cf,0x3f0a1b56,0x3f0bb49b,0x3f0d50a0,0x3f0eef67,0x3f1090f1,0x3f12353e,
      0x3f13dc51,0x3f15862b,0x3f1732cd,0x3f18e239,0x3f1a946f,0x3f1c4971,0x3f1e0141,0x3f1fbbdf,
      0x3f21794e,0x3f23398e,0x3f24fca0,0x3f26c286,0x3f288b41,0x3f2a56d3,0x3f2c253d,0x3f2df680,
      0x3f2fca9e,0x3f31a197,0x3f337b6c,0x3f355820,0x3f3737b3,0x3f391a26,0x3f3aff7c,0x3f3ce7b5,
      0x3f3ed2d2,0x3f40c0d4,0x3f42b1be,0x3f44a590,0x3f469c4b,0x3f4895f1,0x3f4a9282,0x3f4c9201,
      0x3f4e946e,0x3f5099cb,0x3f52a218,0x3f54ad57,0x3f56bb8a,0x3f58ccb0,0x3f5ae0cd,0x3f5cf7e0,
      0x3f5f11ec,0x3f612eee,0x3f634eef,0x3f6571e9,0x3f6797e3,0x3f69c0d6,0x3f6beccd,0x3f6e1bbf,
      0x3f704db8,0x3f7282af,0x3f74baae,0x3f76f5ae,0x3f7933b9,0x3f7b74c6,0x3f7db8e0,0x3f800000
  };

  const float* SrgbToLinearTable()
  {
    return reinterpret_cast<const float*>(SRGBTable);
  }

  float SrgbToLinear(float val)
  {
    if( val < 0.04045f )
        val /= 12.92f;
    else
        val = pow((val + 0.055f)/1.055f,2.4f);
    return val;
  }

  void SrgbToLinear3(float* color)
  {
    color[0] = SrgbToLinear(color[0]);
    color[1] = SrgbToLinear(color[1]);
    color[2] = SrgbToLinear(color[2]);
  }

}