#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdDataArray.h"
#include "anari/frontend/type_utility.h"

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

  case ANARI_INT32_BOX1: return UsdBridgeType::INT_PAIR;
  case ANARI_INT32_BOX2: return UsdBridgeType::INT_PAIR2;
  case ANARI_INT32_BOX3: return UsdBridgeType::INT_PAIR3;
  case ANARI_INT32_BOX4: return UsdBridgeType::INT_PAIR4;

  case ANARI_FLOAT32_BOX1: return UsdBridgeType::FLOAT_PAIR;
  case ANARI_FLOAT32_BOX2: return UsdBridgeType::FLOAT_PAIR2;
  case ANARI_FLOAT32_BOX3: return UsdBridgeType::FLOAT_PAIR3;
  case ANARI_FLOAT32_BOX4: return UsdBridgeType::FLOAT_PAIR4;

  case ANARI_UINT64_REGION1: return UsdBridgeType::ULONG_PAIR;
  case ANARI_UINT64_REGION2: return UsdBridgeType::ULONG_PAIR2;
  case ANARI_UINT64_REGION3: return UsdBridgeType::ULONG_PAIR3;
  case ANARI_UINT64_REGION4: return UsdBridgeType::ULONG_PAIR4;

  case ANARI_FLOAT32_MAT2: return UsdBridgeType::FLOAT_MAT2;
  case ANARI_FLOAT32_MAT3: return UsdBridgeType::FLOAT_MAT3;
  case ANARI_FLOAT32_MAT4: return UsdBridgeType::FLOAT_MAT4;
  case ANARI_FLOAT32_MAT2x3: return UsdBridgeType::FLOAT_MAT2x3;
  case ANARI_FLOAT32_MAT3x4: return UsdBridgeType::FLOAT_MAT3x4;

  case ANARI_FLOAT32_QUAT_IJKW:  return UsdBridgeType::FLOAT4;

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

  case ANARI_INT32_BOX1: return UsdBridgeType::INT;
  case ANARI_INT32_BOX2: return UsdBridgeType::INT;
  case ANARI_INT32_BOX3: return UsdBridgeType::INT;
  case ANARI_INT32_BOX4: return UsdBridgeType::INT;

  case ANARI_FLOAT32_BOX1: return UsdBridgeType::FLOAT;
  case ANARI_FLOAT32_BOX2: return UsdBridgeType::FLOAT;
  case ANARI_FLOAT32_BOX3: return UsdBridgeType::FLOAT;
  case ANARI_FLOAT32_BOX4: return UsdBridgeType::FLOAT;

  case ANARI_UINT64_REGION1: return UsdBridgeType::ULONG;
  case ANARI_UINT64_REGION2: return UsdBridgeType::ULONG;
  case ANARI_UINT64_REGION3: return UsdBridgeType::ULONG;
  case ANARI_UINT64_REGION4: return UsdBridgeType::ULONG;

  case ANARI_FLOAT32_MAT2: return UsdBridgeType::FLOAT;
  case ANARI_FLOAT32_MAT3: return UsdBridgeType::FLOAT;
  case ANARI_FLOAT32_MAT4: return UsdBridgeType::FLOAT;
  case ANARI_FLOAT32_MAT2x3: return UsdBridgeType::FLOAT;
  case ANARI_FLOAT32_MAT3x4: return UsdBridgeType::FLOAT;

  default: return UsdBridgeType::UNDEFINED;
  }
}

template<int T>
struct AnariTypeStringConverter : public anari::ANARITypeProperties<T>
{
  const char* operator()(){ return anari::ANARITypeProperties<T>::enum_name; }
};

const char* AnariTypeToString(ANARIDataType anariType)
{
  return anari::anariTypeInvoke<const char*, AnariTypeStringConverter>(anariType);
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

UsdBridgeMaterialData::AlphaModes AnariToUsdAlphaMode(const char* alphaMode)
{
  if(alphaMode)
  {
    if(strEquals(alphaMode, "blend"))
    {
      return UsdBridgeMaterialData::AlphaModes::BLEND;
    }
    else if(strEquals(alphaMode, "mask"))
    {
      return UsdBridgeMaterialData::AlphaModes::MASK;
    }
  }

  return UsdBridgeMaterialData::AlphaModes::NONE;
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
void logAllocationThroughDevice(UsdDevice* device, const void* ptr, ANARIDataType ptrType)
{
  if(anari::isObject(ptrType))
    device->LogObjAllocation((const UsdBaseObject*)ptr);
  else if(ptrType == ANARI_STRING)
    device->LogStrAllocation((const UsdSharedString*)ptr);
  else
    device->LogRawAllocation(ptr);
}

void logDeallocationThroughDevice(UsdDevice* device, const void* ptr, ANARIDataType ptrType)
{
  if(anari::isObject(ptrType))
    device->LogObjDeallocation((const UsdBaseObject*)ptr);
  else if(ptrType == ANARI_STRING)
    device->LogStrDeallocation((const UsdSharedString*)ptr);
  else
    device->LogRawDeallocation(ptr);
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