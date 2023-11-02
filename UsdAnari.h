// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgeData.h"
#include "UsdCommonMacros.h"
#include "anari/frontend/anari_enums.h"
#include "anari/anari_cpp/Traits.h"

#include <cstring>

class UsdDevice;
class UsdDataArray;
class UsdFrame;
class UsdGeometry;
class UsdGroup;
class UsdInstance;
class UsdLight;
class UsdMaterial;
class UsdRenderer;
class UsdSurface;
class UsdSampler;
class UsdSpatialField;
class UsdVolume;
class UsdWorld;
class UsdSharedString;
class UsdBaseObject;
struct UsdDataLayout;

namespace anari
{
  ANARI_TYPEFOR_SPECIALIZATION(UsdFloat3, ANARI_FLOAT32_VEC3);
  ANARI_TYPEFOR_SPECIALIZATION(UsdSampler*, ANARI_SAMPLER);
  ANARI_TYPEFOR_SPECIALIZATION(UsdSharedString*, ANARI_STRING);
}

// Shared convenience functions
namespace
{
  inline bool strEquals(const char* arg0, const char* arg1)
  {
    return strcmp(arg0, arg1) == 0;
  }

  template <typename T>
  inline void writeToVoidP(void *_p, T v)
  {
    T *p = (T *)_p;
    *p = v;
  }
}

// Standard log info
struct UsdLogInfo
{
  UsdLogInfo(UsdDevice* dev, void* src, ANARIDataType srcType, const char* srcName)
    : device(dev)
    , source(src)
    , sourceType(srcType)
    , sourceName(srcName)
  {}

  UsdDevice* device = nullptr; 
  void* source = nullptr;
  ANARIDataType sourceType = ANARI_VOID_POINTER;
  const char* sourceName = nullptr;
};

void reportStatusThroughDevice(const UsdLogInfo& logInfo, ANARIStatusSeverity severity, ANARIStatusCode statusCode,
  const char *format, const char* firstArg, const char* secondArg); // In case #include <UsdDevice.h> is undesired

#ifdef CHECK_MEMLEAKS  
void logAllocationThroughDevice(UsdDevice* device, const void* ptr, ANARIDataType ptrType);
void logDeallocationThroughDevice(UsdDevice* device, const void* ptr, ANARIDataType ptrType);
#endif

// Anari <=> USD conversions
UsdBridgeType AnariToUsdBridgeType(ANARIDataType anariType);
UsdBridgeType AnariToUsdBridgeType_Flattened(ANARIDataType anariType);
const char* AnariTypeToString(ANARIDataType anariType);
const char* AnariAttributeToUsdName(const char* param, bool perInstance, const UsdLogInfo& logInfo);
UsdBridgeMaterialData::AlphaModes AnariToUsdAlphaMode(const char* alphaMode);
ANARIStatusSeverity UsdBridgeLogLevelToAnariSeverity(UsdBridgeLogLevel level);

bool Assert64bitStringLengthProperty(uint64_t size, const UsdLogInfo& logInfo, const char* propName);
bool AssertOneDimensional(const UsdDataLayout& layout, const UsdLogInfo& logInfo, const char* arrayName);
bool AssertNoStride(const UsdDataLayout& layout, const UsdLogInfo& logInfo, const char* arrayName);
bool AssertArrayType(UsdDataArray* dataArray, ANARIDataType dataType, const UsdLogInfo& logInfo, const char* errorMessage);

// Template definitions
template<typename AnariType>
class AnariToUsdObject {};

template<int AnariType>
class AnariToUsdBridgedObject {};

template<int AnariType>
class AnariToUsdBaseObject {};

#define USDBRIDGE_DEFINE_OBJECT_MAPPING(AnariType, UsdType) \
template<>\
class AnariToUsdObject<AnariType>\
{\
  public:\
    using Type = UsdType;\
};

#define USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(AnariType, UsdType) \
template<>\
class AnariToUsdBaseObject<(int)AnariType>\
{\
  public:\
    using Type = UsdType;\
};

#define USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(AnariType, UsdType)\
template<>\
class AnariToUsdBridgedObject<(int)AnariType>\
{\
  public:\
    using Type = UsdType;\
};\
template<>\
class AnariToUsdBaseObject<(int)AnariType>\
{\
  public:\
    using Type = UsdType;\
};

USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIObject, UsdBaseObject)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIDevice, UsdDevice)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIArray, UsdDataArray)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIArray1D, UsdDataArray)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIArray2D, UsdDataArray)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIArray3D, UsdDataArray)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIFrame, UsdFrame)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIGeometry, UsdGeometry)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIGroup, UsdGroup)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIInstance, UsdInstance)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARILight, UsdLight)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIMaterial, UsdMaterial)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARISampler, UsdSampler)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARISurface, UsdSurface)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIRenderer, UsdRenderer)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARISpatialField, UsdSpatialField)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIVolume, UsdVolume)
USDBRIDGE_DEFINE_OBJECT_MAPPING(ANARIWorld, UsdWorld)

USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_DEVICE, UsdDevice)
USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_ARRAY, UsdDataArray)
USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_ARRAY1D, UsdDataArray)
USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_ARRAY2D, UsdDataArray)
USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_ARRAY3D, UsdDataArray)
USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_FRAME, UsdFrame)
USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_RENDERER, UsdRenderer)

USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_GEOMETRY, UsdGeometry)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_GROUP, UsdGroup)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_INSTANCE, UsdInstance)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_LIGHT, UsdLight)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_MATERIAL, UsdMaterial)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_SURFACE, UsdSurface)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_SAMPLER, UsdSampler)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_SPATIAL_FIELD, UsdSpatialField)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_VOLUME, UsdVolume)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_WORLD, UsdWorld)

template<typename AnariType>
typename AnariToUsdObject<AnariType>::Type* AnariToUsdObjectPtr(AnariType object) { return (typename AnariToUsdObject<AnariType>::Type*) object; }
