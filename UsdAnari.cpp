#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdDataArray.h"
#include "anari/type_utility.h"

UsdBridgeType AnariToUsdBridgeType(ANARIDataType anariType)
{
  switch (anariType)
  {
  case ANARI_UINT8: return UsdBridgeType::UCHAR;
  case ANARI_UINT8_VEC2: return UsdBridgeType::UCHAR2;
  case ANARI_UINT8_VEC3: return UsdBridgeType::UCHAR3;
  case ANARI_UINT8_VEC4: return UsdBridgeType::UCHAR4;
  case ANARI_INT8: return UsdBridgeType::CHAR;
  case ANARI_INT8_VEC2: return UsdBridgeType::CHAR2;
  case ANARI_INT8_VEC3: return UsdBridgeType::CHAR3;
  case ANARI_INT8_VEC4: return UsdBridgeType::CHAR4;

  case ANARI_UFIXED8: return UsdBridgeType::UCHAR;
  case ANARI_UFIXED8_VEC2: return UsdBridgeType::UCHAR2;
  case ANARI_UFIXED8_VEC3: return UsdBridgeType::UCHAR3;
  case ANARI_UFIXED8_VEC4: return UsdBridgeType::UCHAR4;
  case ANARI_FIXED8: return UsdBridgeType::CHAR;
  case ANARI_FIXED8_VEC2: return UsdBridgeType::CHAR2;
  case ANARI_FIXED8_VEC3: return UsdBridgeType::CHAR3;
  case ANARI_FIXED8_VEC4: return UsdBridgeType::CHAR4;

  case ANARI_UFIXED8_R_SRGB: return UsdBridgeType::UCHAR_SRGB_R;
  case ANARI_UFIXED8_RA_SRGB: return UsdBridgeType::UCHAR_SRGB_RA;
  case ANARI_UFIXED8_RGB_SRGB: return UsdBridgeType::UCHAR_SRGB_RGB;
  case ANARI_UFIXED8_RGBA_SRGB: return UsdBridgeType::UCHAR_SRGB_RGBA;

  case ANARI_UINT16: return UsdBridgeType::USHORT;
  case ANARI_UINT16_VEC2: return UsdBridgeType::USHORT2;
  case ANARI_UINT16_VEC3: return UsdBridgeType::USHORT3;
  case ANARI_UINT16_VEC4: return UsdBridgeType::USHORT4;
  case ANARI_INT16: return UsdBridgeType::SHORT;
  case ANARI_INT16_VEC2: return UsdBridgeType::SHORT2;
  case ANARI_INT16_VEC3: return UsdBridgeType::SHORT3;
  case ANARI_INT16_VEC4: return UsdBridgeType::SHORT4;

  case ANARI_UFIXED16: return UsdBridgeType::USHORT;
  case ANARI_UFIXED16_VEC2: return UsdBridgeType::USHORT2;
  case ANARI_UFIXED16_VEC3: return UsdBridgeType::USHORT3;
  case ANARI_UFIXED16_VEC4: return UsdBridgeType::USHORT4;
  case ANARI_FIXED16: return UsdBridgeType::SHORT;
  case ANARI_FIXED16_VEC2: return UsdBridgeType::SHORT2;
  case ANARI_FIXED16_VEC3: return UsdBridgeType::SHORT3;
  case ANARI_FIXED16_VEC4: return UsdBridgeType::SHORT4;

  case ANARI_UINT32: return UsdBridgeType::UINT;
  case ANARI_UINT32_VEC2: return UsdBridgeType::UINT2;
  case ANARI_UINT32_VEC3: return UsdBridgeType::UINT3;
  case ANARI_UINT32_VEC4: return UsdBridgeType::UINT4;
  case ANARI_INT32: return UsdBridgeType::INT;
  case ANARI_INT32_VEC2: return UsdBridgeType::INT2;
  case ANARI_INT32_VEC3: return UsdBridgeType::INT3;
  case ANARI_INT32_VEC4: return UsdBridgeType::INT4;

  case ANARI_UFIXED32: return UsdBridgeType::UINT;
  case ANARI_UFIXED32_VEC2: return UsdBridgeType::UINT2;
  case ANARI_UFIXED32_VEC3: return UsdBridgeType::UINT3;
  case ANARI_UFIXED32_VEC4: return UsdBridgeType::UINT4;
  case ANARI_FIXED32: return UsdBridgeType::INT;
  case ANARI_FIXED32_VEC2: return UsdBridgeType::INT2;
  case ANARI_FIXED32_VEC3: return UsdBridgeType::INT3;
  case ANARI_FIXED32_VEC4: return UsdBridgeType::INT4;

  case ANARI_UINT64: return UsdBridgeType::ULONG;
  case ANARI_UINT64_VEC2: return UsdBridgeType::ULONG2;
  case ANARI_UINT64_VEC3: return UsdBridgeType::ULONG3;
  case ANARI_UINT64_VEC4: return UsdBridgeType::ULONG4;
  case ANARI_INT64: return UsdBridgeType::LONG;
  case ANARI_INT64_VEC2: return UsdBridgeType::LONG2;
  case ANARI_INT64_VEC3: return UsdBridgeType::LONG3;
  case ANARI_INT64_VEC4: return UsdBridgeType::LONG4;

  case ANARI_FLOAT32: return UsdBridgeType::FLOAT;
  case ANARI_FLOAT32_VEC2: return UsdBridgeType::FLOAT2;
  case ANARI_FLOAT32_VEC3: return UsdBridgeType::FLOAT3;
  case ANARI_FLOAT32_VEC4: return UsdBridgeType::FLOAT4;

  case ANARI_FLOAT64: return UsdBridgeType::DOUBLE;
  case ANARI_FLOAT64_VEC2: return UsdBridgeType::DOUBLE2;
  case ANARI_FLOAT64_VEC3: return UsdBridgeType::DOUBLE3;
  case ANARI_FLOAT64_VEC4: return UsdBridgeType::DOUBLE4;

  default: return UsdBridgeType::UNDEFINED;
  }
}

UsdBridgeType AnariToUsdBridgeType_Flattened(ANARIDataType anariType)
{
  switch (anariType)
  {
  case ANARI_UINT8: return UsdBridgeType::UCHAR;
  case ANARI_UINT8_VEC2: return UsdBridgeType::UCHAR;
  case ANARI_UINT8_VEC3: return UsdBridgeType::UCHAR;
  case ANARI_UINT8_VEC4: return UsdBridgeType::UCHAR;
  case ANARI_INT8: return UsdBridgeType::CHAR;
  case ANARI_INT8_VEC2: return UsdBridgeType::CHAR;
  case ANARI_INT8_VEC3: return UsdBridgeType::CHAR;
  case ANARI_INT8_VEC4: return UsdBridgeType::CHAR;

  case ANARI_UFIXED8: return UsdBridgeType::UCHAR;
  case ANARI_UFIXED8_VEC2: return UsdBridgeType::UCHAR;
  case ANARI_UFIXED8_VEC3: return UsdBridgeType::UCHAR;
  case ANARI_UFIXED8_VEC4: return UsdBridgeType::UCHAR;
  case ANARI_FIXED8: return UsdBridgeType::CHAR;
  case ANARI_FIXED8_VEC2: return UsdBridgeType::CHAR;
  case ANARI_FIXED8_VEC3: return UsdBridgeType::CHAR;
  case ANARI_FIXED8_VEC4: return UsdBridgeType::CHAR;

  case ANARI_UFIXED8_R_SRGB: return UsdBridgeType::UCHAR_SRGB_R;
  case ANARI_UFIXED8_RA_SRGB: return UsdBridgeType::UCHAR_SRGB_R;
  case ANARI_UFIXED8_RGB_SRGB: return UsdBridgeType::UCHAR_SRGB_R;
  case ANARI_UFIXED8_RGBA_SRGB: return UsdBridgeType::UCHAR_SRGB_R;

  case ANARI_UINT16: return UsdBridgeType::USHORT;
  case ANARI_UINT16_VEC2: return UsdBridgeType::USHORT;
  case ANARI_UINT16_VEC3: return UsdBridgeType::USHORT;
  case ANARI_UINT16_VEC4: return UsdBridgeType::USHORT;
  case ANARI_INT16: return UsdBridgeType::SHORT;
  case ANARI_INT16_VEC2: return UsdBridgeType::SHORT;
  case ANARI_INT16_VEC3: return UsdBridgeType::SHORT;
  case ANARI_INT16_VEC4: return UsdBridgeType::SHORT;

  case ANARI_UFIXED16: return UsdBridgeType::USHORT;
  case ANARI_UFIXED16_VEC2: return UsdBridgeType::USHORT;
  case ANARI_UFIXED16_VEC3: return UsdBridgeType::USHORT;
  case ANARI_UFIXED16_VEC4: return UsdBridgeType::USHORT;
  case ANARI_FIXED16: return UsdBridgeType::SHORT;
  case ANARI_FIXED16_VEC2: return UsdBridgeType::SHORT;
  case ANARI_FIXED16_VEC3: return UsdBridgeType::SHORT;
  case ANARI_FIXED16_VEC4: return UsdBridgeType::SHORT;

  case ANARI_UINT32: return UsdBridgeType::UINT;
  case ANARI_UINT32_VEC2: return UsdBridgeType::UINT;
  case ANARI_UINT32_VEC3: return UsdBridgeType::UINT;
  case ANARI_UINT32_VEC4: return UsdBridgeType::UINT;
  case ANARI_INT32: return UsdBridgeType::INT;
  case ANARI_INT32_VEC2: return UsdBridgeType::INT;
  case ANARI_INT32_VEC3: return UsdBridgeType::INT;
  case ANARI_INT32_VEC4: return UsdBridgeType::INT;

  case ANARI_UFIXED32: return UsdBridgeType::UINT;
  case ANARI_UFIXED32_VEC2: return UsdBridgeType::UINT;
  case ANARI_UFIXED32_VEC3: return UsdBridgeType::UINT;
  case ANARI_UFIXED32_VEC4: return UsdBridgeType::UINT;
  case ANARI_FIXED32: return UsdBridgeType::INT;
  case ANARI_FIXED32_VEC2: return UsdBridgeType::INT;
  case ANARI_FIXED32_VEC3: return UsdBridgeType::INT;
  case ANARI_FIXED32_VEC4: return UsdBridgeType::INT;

  case ANARI_UINT64: return UsdBridgeType::ULONG;
  case ANARI_UINT64_VEC2: return UsdBridgeType::ULONG;
  case ANARI_UINT64_VEC3: return UsdBridgeType::ULONG;
  case ANARI_UINT64_VEC4: return UsdBridgeType::ULONG;
  case ANARI_INT64: return UsdBridgeType::LONG;
  case ANARI_INT64_VEC2: return UsdBridgeType::LONG;
  case ANARI_INT64_VEC3: return UsdBridgeType::LONG;
  case ANARI_INT64_VEC4: return UsdBridgeType::LONG;
  
  case ANARI_FLOAT32: return UsdBridgeType::FLOAT;
  case ANARI_FLOAT32_VEC2: return UsdBridgeType::FLOAT;
  case ANARI_FLOAT32_VEC3: return UsdBridgeType::FLOAT;
  case ANARI_FLOAT32_VEC4: return UsdBridgeType::FLOAT;

  case ANARI_FLOAT64: return UsdBridgeType::DOUBLE;
  case ANARI_FLOAT64_VEC2: return UsdBridgeType::DOUBLE;
  case ANARI_FLOAT64_VEC3: return UsdBridgeType::DOUBLE;
  case ANARI_FLOAT64_VEC4: return UsdBridgeType::DOUBLE;

  default: return UsdBridgeType::UNDEFINED;
  }
}

size_t AnariTypeSize(ANARIDataType anariType)
{
  if(anari::isObject(anariType))
    anariType = ANARI_OBJECT;

  size_t typeSize = 0;
  switch (anariType)
  {
  case ANARI_DATA_TYPE:
    typeSize = sizeof(ANARIDataType);
    break;
  case ANARI_STRING:
    typeSize = sizeof(const char*);
    break;
  case ANARI_FUNCTION_POINTER:
  case ANARI_MEMORY_DELETER:
  case ANARI_STATUS_CALLBACK:
  case ANARI_FRAME_COMPLETION_CALLBACK:
    typeSize = sizeof(ANARIStatusCallback);
    break;
  case ANARI_VOID_POINTER:
  case ANARI_DEVICE:
  case ANARI_LIBRARY:
  case ANARI_OBJECT:
    typeSize = sizeof(void*);
    break;
  case ANARI_INT8:
  case ANARI_UINT8:
  case ANARI_FIXED8:
  case ANARI_UFIXED8:
  case ANARI_UFIXED8_R_SRGB:
    typeSize = sizeof(uint8_t);
    break;
  case ANARI_INT8_VEC2:
  case ANARI_UINT8_VEC2:
  case ANARI_FIXED8_VEC2:
  case ANARI_UFIXED8_VEC2:
  case ANARI_UFIXED8_RA_SRGB:
    typeSize = 2 *sizeof(uint8_t);
    break;
  case ANARI_INT8_VEC3:
  case ANARI_UINT8_VEC3:
  case ANARI_FIXED8_VEC3:
  case ANARI_UFIXED8_VEC3:
  case ANARI_UFIXED8_RGB_SRGB:
    typeSize = 3 * sizeof(uint8_t);
    break;
  case ANARI_INT8_VEC4:
  case ANARI_UINT8_VEC4:
  case ANARI_FIXED8_VEC4:
  case ANARI_UFIXED8_VEC4:
  case ANARI_UFIXED8_RGBA_SRGB:
    typeSize = 4 * sizeof(uint8_t);
    break;
  case ANARI_INT16:
  case ANARI_UINT16:
  case ANARI_FIXED16:
  case ANARI_UFIXED16:
    typeSize = sizeof(uint16_t);
    break;
  case ANARI_INT16_VEC2:
  case ANARI_UINT16_VEC2:
  case ANARI_FIXED16_VEC2:
  case ANARI_UFIXED16_VEC2:
    typeSize = 2 * sizeof(uint16_t);
    break;
  case ANARI_INT16_VEC3:
  case ANARI_UINT16_VEC3:
  case ANARI_FIXED16_VEC3:
  case ANARI_UFIXED16_VEC3:
    typeSize = 3 * sizeof(uint16_t);
    break;
  case ANARI_INT16_VEC4:
  case ANARI_UINT16_VEC4:
  case ANARI_FIXED16_VEC4:
  case ANARI_UFIXED16_VEC4:
    typeSize = 4 * sizeof(uint16_t);
    break;
  case ANARI_INT32:
  case ANARI_UINT32:
  case ANARI_FIXED32:
  case ANARI_UFIXED32:
  case ANARI_BOOL:
    typeSize = sizeof(uint32_t);
    break;
  case ANARI_INT32_VEC2:
  case ANARI_UINT32_VEC2:
  case ANARI_FIXED32_VEC2:
  case ANARI_UFIXED32_VEC2:
    typeSize = 2 * sizeof(uint32_t);
    break;
  case ANARI_INT32_VEC3:
  case ANARI_UINT32_VEC3:
  case ANARI_FIXED32_VEC3:
  case ANARI_UFIXED32_VEC3:
    typeSize = 3 * sizeof(uint32_t);
    break;
  case ANARI_INT32_VEC4:
  case ANARI_UINT32_VEC4:
  case ANARI_FIXED32_VEC4:
  case ANARI_UFIXED32_VEC4:
    typeSize = 4 * sizeof(uint32_t);
    break;
  case ANARI_INT64:
  case ANARI_UINT64:
  case ANARI_FIXED64:
  case ANARI_UFIXED64:
    typeSize = sizeof(uint64_t);
    break;
  case ANARI_INT64_VEC2:
  case ANARI_UINT64_VEC2:
  case ANARI_FIXED64_VEC2:
  case ANARI_UFIXED64_VEC2:
    typeSize = 2 * sizeof(uint64_t);
    break;
  case ANARI_INT64_VEC3:
  case ANARI_UINT64_VEC3:
  case ANARI_FIXED64_VEC3:
  case ANARI_UFIXED64_VEC3:
    typeSize = 3 * sizeof(uint64_t);
    break;
  case ANARI_INT64_VEC4:
  case ANARI_UINT64_VEC4:
  case ANARI_FIXED64_VEC4:
  case ANARI_UFIXED64_VEC4:
    typeSize = 4 * sizeof(uint64_t);
    break;
  case ANARI_FLOAT32:
    typeSize = sizeof(float);
    break;
  case ANARI_FLOAT32_VEC2:
    typeSize = 2 * sizeof(float);
    break;
  case ANARI_FLOAT32_VEC3:
    typeSize = 3 * sizeof(float);
    break;
  case ANARI_FLOAT32_VEC4:
    typeSize = 4 * sizeof(float);
    break;
  case ANARI_FLOAT64:
    typeSize = sizeof(double);
    break;
  case ANARI_FLOAT64_VEC2:
    typeSize = 2 * sizeof(double);
    break;
  case ANARI_FLOAT64_VEC3:
    typeSize = 3 * sizeof(double);
    break;
  case ANARI_FLOAT64_VEC4:
    typeSize = 4 * sizeof(double);
    break;
  case ANARI_INT32_BOX1:
    typeSize = 2 * sizeof(uint32_t);
    break;
  case ANARI_INT32_BOX2:
    typeSize = 4 * sizeof(uint32_t);
    break;
  case ANARI_INT32_BOX3:
    typeSize = 6 * sizeof(uint32_t);
    break;
  case ANARI_INT32_BOX4:
    typeSize = 8 * sizeof(uint32_t);
    break;
  case ANARI_FLOAT32_BOX1:
    typeSize = 2 * sizeof(float);
    break;
  case ANARI_FLOAT32_BOX2:
    typeSize = 4 * sizeof(float);
    break;
  case ANARI_FLOAT32_BOX3:
    typeSize = 6 * sizeof(float);
    break;
  case ANARI_FLOAT32_BOX4:
    typeSize = 8 * sizeof(float);
    break;
  case ANARI_FLOAT32_MAT2:
    typeSize = 4 * sizeof(float);
    break;
  case ANARI_FLOAT32_MAT3:
    typeSize = 9 * sizeof(float);
    break;
  case ANARI_FLOAT32_MAT4:
    typeSize = 16 * sizeof(float);
    break;
  case ANARI_FLOAT32_MAT2x3:
    typeSize = 6 * sizeof(float);
    break;
  case ANARI_FLOAT32_MAT3x4:
    typeSize = 12 * sizeof(float);
    break;
  case ANARI_FLOAT32_QUAT_IJKW:
    typeSize = 4 * sizeof(float);
    break;
  default:
    assert(false); //Implementation incomplete
    break;
  }

  return typeSize;
}

#define TYPE_STRING_CASE(x) \
  case (x): \
  typeString = #x; break;

const char* AnariTypeToString(ANARIDataType anariType)
{
  const char* typeString = "Unknown type";

  switch(anariType)
  {
    TYPE_STRING_CASE(ANARI_DATA_TYPE)
    TYPE_STRING_CASE(ANARI_VOID_POINTER)
    TYPE_STRING_CASE(ANARI_STRING)
    TYPE_STRING_CASE(ANARI_FUNCTION_POINTER)
    TYPE_STRING_CASE(ANARI_MEMORY_DELETER)
    TYPE_STRING_CASE(ANARI_STATUS_CALLBACK)
    TYPE_STRING_CASE(ANARI_FRAME_COMPLETION_CALLBACK)
    TYPE_STRING_CASE(ANARI_LIBRARY)
    TYPE_STRING_CASE(ANARI_DEVICE)
    TYPE_STRING_CASE(ANARI_OBJECT)
    TYPE_STRING_CASE(ANARI_ARRAY)
    TYPE_STRING_CASE(ANARI_ARRAY1D)
    TYPE_STRING_CASE(ANARI_ARRAY2D)
    TYPE_STRING_CASE(ANARI_ARRAY3D)
    TYPE_STRING_CASE(ANARI_CAMERA)
    TYPE_STRING_CASE(ANARI_FRAME)
    TYPE_STRING_CASE(ANARI_GEOMETRY)
    TYPE_STRING_CASE(ANARI_GROUP)
    TYPE_STRING_CASE(ANARI_INSTANCE)
    TYPE_STRING_CASE(ANARI_LIGHT)
    TYPE_STRING_CASE(ANARI_MATERIAL)
    TYPE_STRING_CASE(ANARI_RENDERER)
    TYPE_STRING_CASE(ANARI_SURFACE)
    TYPE_STRING_CASE(ANARI_SAMPLER)
    TYPE_STRING_CASE(ANARI_SPATIAL_FIELD)
    TYPE_STRING_CASE(ANARI_VOLUME)
    TYPE_STRING_CASE(ANARI_WORLD)
    TYPE_STRING_CASE(ANARI_INT8)
    TYPE_STRING_CASE(ANARI_INT8_VEC2)
    TYPE_STRING_CASE(ANARI_INT8_VEC3)
    TYPE_STRING_CASE(ANARI_INT8_VEC4)
    TYPE_STRING_CASE(ANARI_UINT8)
    TYPE_STRING_CASE(ANARI_UINT8_VEC2)
    TYPE_STRING_CASE(ANARI_UINT8_VEC3)
    TYPE_STRING_CASE(ANARI_UINT8_VEC4)
    TYPE_STRING_CASE(ANARI_INT16)
    TYPE_STRING_CASE(ANARI_INT16_VEC2)
    TYPE_STRING_CASE(ANARI_INT16_VEC3)
    TYPE_STRING_CASE(ANARI_INT16_VEC4)
    TYPE_STRING_CASE(ANARI_UINT16)
    TYPE_STRING_CASE(ANARI_UINT16_VEC2)
    TYPE_STRING_CASE(ANARI_UINT16_VEC3)
    TYPE_STRING_CASE(ANARI_UINT16_VEC4)
    TYPE_STRING_CASE(ANARI_INT32)
    TYPE_STRING_CASE(ANARI_UINT32)
    TYPE_STRING_CASE(ANARI_BOOL)
    TYPE_STRING_CASE(ANARI_INT32_VEC2)
    TYPE_STRING_CASE(ANARI_UINT32_VEC2)
    TYPE_STRING_CASE(ANARI_INT32_VEC3)
    TYPE_STRING_CASE(ANARI_UINT32_VEC3)
    TYPE_STRING_CASE(ANARI_INT32_VEC4)
    TYPE_STRING_CASE(ANARI_UINT32_VEC4)
    TYPE_STRING_CASE(ANARI_INT64)
    TYPE_STRING_CASE(ANARI_UINT64)
    TYPE_STRING_CASE(ANARI_INT64_VEC2)
    TYPE_STRING_CASE(ANARI_UINT64_VEC2)
    TYPE_STRING_CASE(ANARI_INT64_VEC3)
    TYPE_STRING_CASE(ANARI_UINT64_VEC3)
    TYPE_STRING_CASE(ANARI_INT64_VEC4)
    TYPE_STRING_CASE(ANARI_UINT64_VEC4)
    TYPE_STRING_CASE(ANARI_FLOAT32)
    TYPE_STRING_CASE(ANARI_FLOAT32_VEC2)
    TYPE_STRING_CASE(ANARI_FLOAT32_VEC3)
    TYPE_STRING_CASE(ANARI_FLOAT32_VEC4)
    TYPE_STRING_CASE(ANARI_FLOAT64)
    TYPE_STRING_CASE(ANARI_FLOAT64_VEC2)
    TYPE_STRING_CASE(ANARI_FLOAT64_VEC3)
    TYPE_STRING_CASE(ANARI_FLOAT64_VEC4)
    TYPE_STRING_CASE(ANARI_INT32_BOX1)
    TYPE_STRING_CASE(ANARI_INT32_BOX2)
    TYPE_STRING_CASE(ANARI_INT32_BOX3)
    TYPE_STRING_CASE(ANARI_INT32_BOX4)
    TYPE_STRING_CASE(ANARI_FLOAT32_BOX1)
    TYPE_STRING_CASE(ANARI_FLOAT32_BOX2)
    TYPE_STRING_CASE(ANARI_FLOAT32_BOX3)
    TYPE_STRING_CASE(ANARI_FLOAT32_BOX4)
    TYPE_STRING_CASE(ANARI_FLOAT32_MAT2)
    TYPE_STRING_CASE(ANARI_FLOAT32_MAT3)
    TYPE_STRING_CASE(ANARI_FLOAT32_MAT4)
    TYPE_STRING_CASE(ANARI_FLOAT32_MAT2x3)
    TYPE_STRING_CASE(ANARI_FLOAT32_MAT3x4)
    TYPE_STRING_CASE(ANARI_FIXED8)
    TYPE_STRING_CASE(ANARI_FIXED8_VEC2)
    TYPE_STRING_CASE(ANARI_FIXED8_VEC3)
    TYPE_STRING_CASE(ANARI_FIXED8_VEC4)
    TYPE_STRING_CASE(ANARI_UFIXED8)
    TYPE_STRING_CASE(ANARI_UFIXED8_VEC2)
    TYPE_STRING_CASE(ANARI_UFIXED8_VEC3)
    TYPE_STRING_CASE(ANARI_UFIXED8_VEC4)
    TYPE_STRING_CASE(ANARI_FIXED16)
    TYPE_STRING_CASE(ANARI_FIXED16_VEC2)
    TYPE_STRING_CASE(ANARI_FIXED16_VEC3)
    TYPE_STRING_CASE(ANARI_FIXED16_VEC4)
    TYPE_STRING_CASE(ANARI_UFIXED16)
    TYPE_STRING_CASE(ANARI_UFIXED16_VEC2)
    TYPE_STRING_CASE(ANARI_UFIXED16_VEC3)
    TYPE_STRING_CASE(ANARI_UFIXED16_VEC4)
    TYPE_STRING_CASE(ANARI_FIXED32)
    TYPE_STRING_CASE(ANARI_FIXED32_VEC2)
    TYPE_STRING_CASE(ANARI_FIXED32_VEC3)
    TYPE_STRING_CASE(ANARI_FIXED32_VEC4)
    TYPE_STRING_CASE(ANARI_UFIXED32)
    TYPE_STRING_CASE(ANARI_UFIXED32_VEC2)
    TYPE_STRING_CASE(ANARI_UFIXED32_VEC3)
    TYPE_STRING_CASE(ANARI_UFIXED32_VEC4)
    TYPE_STRING_CASE(ANARI_FIXED64)
    TYPE_STRING_CASE(ANARI_FIXED64_VEC2)
    TYPE_STRING_CASE(ANARI_FIXED64_VEC3)
    TYPE_STRING_CASE(ANARI_FIXED64_VEC4)
    TYPE_STRING_CASE(ANARI_UFIXED64)
    TYPE_STRING_CASE(ANARI_UFIXED64_VEC2)
    TYPE_STRING_CASE(ANARI_UFIXED64_VEC3)
    TYPE_STRING_CASE(ANARI_UFIXED64_VEC4)
    TYPE_STRING_CASE(ANARI_UFIXED8_R_SRGB)
    TYPE_STRING_CASE(ANARI_UFIXED8_RA_SRGB)
    TYPE_STRING_CASE(ANARI_UFIXED8_RGB_SRGB)
    TYPE_STRING_CASE(ANARI_UFIXED8_RGBA_SRGB)
    TYPE_STRING_CASE(ANARI_FLOAT32_QUAT_IJKW)
    default:
      break;
  }

  return typeString;
}

const char* AnariAttributeToUsdName(const char* param, bool perInstance, const UsdLogInfo& logInfo)
{
  if(strEquals(param, "worldPosition")
  || strEquals(param, "worldNormal"))
  {
    reportStatusThroughDevice(logInfo, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdSampler '%s' inAttribute %s not supported, use inTransform parameter on object-space attribute instead.", logInfo.sourceName, param);
  }
  if(strEquals(param, "objectPosition"))
  {
    if(perInstance)
      return "positions";
    else
      return "points";
  }
  else if(strEquals(param, "objectNormal"))
  {
    return "normals";
  }
  //else if(!strncmp(param, "attribute", 9))
  //{
  //  return param;
  //}
  return param; // The generic case just returns the param itself
}

ANARIStatusSeverity UsdBridgeLogLevelToAnariSeverity(UsdBridgeLogLevel level)
{
  ANARIStatusSeverity severity = ANARI_SEVERITY_INFO;
  switch (level)
  {
    case UsdBridgeLogLevel::STATUS: severity = ANARI_SEVERITY_INFO; break;
    case UsdBridgeLogLevel::WARNING: severity = ANARI_SEVERITY_WARNING; break;
    case UsdBridgeLogLevel::ERR: severity = ANARI_SEVERITY_ERROR; break;
    default: severity = ANARI_SEVERITY_INFO; break;
  }
  return severity;
}

void reportStatusThroughDevice(const UsdLogInfo& logInfo, ANARIStatusSeverity severity, ANARIStatusCode statusCode,
  const char *format, const char* firstArg, const char* secondArg)
{
  if(logInfo.device)
    logInfo.device->reportStatus(logInfo.source, logInfo.sourceType, severity, statusCode, format, firstArg, secondArg);
}

#ifdef CHECK_MEMLEAKS
void logAllocationThroughDevice(UsdDevice* device, const UsdBaseObject* obj)
{
  device->LogAllocation(obj);
}

void logDeallocationThroughDevice(UsdDevice* device, const UsdBaseObject* obj)
{
  device->LogDeallocation(obj);
}
#endif

bool Assert64bitStringLengthProperty(uint64_t size, const UsdLogInfo& logInfo, const char* name)
{
  if (size != sizeof(uint64_t) && logInfo.device)
  {
    logInfo.device->reportStatus(logInfo.source, ANARI_OBJECT, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "On object '%s', getProperty() on %s, size parameter differs from sizeof(uint64_t)", logInfo.sourceName, name);
    return false;
  }
  return true;
}

bool AssertOneDimensional(const UsdDataLayout& layout, const UsdLogInfo& logInfo, const char* arrayName)
{
  if (!layout.isOneDimensional() && logInfo.device)
  {
    logInfo.device->reportStatus(logInfo.source, logInfo.sourceType, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "On object '%s', '%s' array has to be 1-dimensional.", logInfo.sourceName, arrayName);
    return false;
  }
  return true;
}

bool AssertNoStride(const UsdDataLayout& layout, const UsdLogInfo& logInfo, const char* arrayName)
{
  if (!layout.isDense() && logInfo.device)
  {
    logInfo.device->reportStatus(logInfo.source, logInfo.sourceType, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "On object '%s', '%s' layout strides should all be 0.", logInfo.sourceName, arrayName);
    return false;
  }
  return true;
}

bool AssertArrayType(UsdDataArray* dataArray, ANARIDataType dataType, const UsdLogInfo& logInfo, const char* errorMessage)
{
  if (dataArray && dataArray->getType() != dataType && logInfo.device)
  {
    logInfo.device->reportStatus(logInfo.source, logInfo.sourceType, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "On object '%s', '%s'", logInfo.sourceName, errorMessage);
    return false;
  }
  return true;
}