// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgeData.h"
#include "anari/anari_enums.h"
#include "UsdCommonMacros.h"

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
struct UsdDataLayout;

struct LogInfo
{
  LogInfo(UsdDevice* dev, void* src, ANARIDataType srcType, const char* srcName)
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

UsdBridgeType AnariToUsdBridgeType(ANARIDataType anariType);
UsdBridgeType AnariToUsdBridgeType_Flattened(ANARIDataType anariType);
size_t AnariTypeSize(ANARIDataType anariType);
const char* AnariTypeToString(ANARIDataType anariType);
ANARIStatusSeverity UsdBridgeLogLevelToAnariSeverity(UsdBridgeLogLevel level);

void reportStatusThroughDevice(const LogInfo& logInfo, ANARIStatusSeverity severity, ANARIStatusCode statusCode,
  const char *format, const char* firstArg, const char* secondArg); // In case #include <UsdDevice.h> is undesired

bool Assert64bitStringLengthProperty(uint64_t size, const LogInfo& logInfo, const char* propName);
bool AssertOneDimensional(const UsdDataLayout& layout, const LogInfo& logInfo, const char* arrayName);
bool AssertNoStride(const UsdDataLayout& layout, const LogInfo& logInfo, const char* arrayName);
bool AssertArrayType(UsdDataArray* dataArray, ANARIDataType dataType, const LogInfo& logInfo, const char* errorMessage);


// Template definitions

template<int AnariType>
class AnariToUsdBridgedObject {};

template<int AnariType>
class AnariToUsdBaseObject {};

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

USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_ARRAY, UsdDataArray)
USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_ARRAY1D, UsdDataArray)
USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_ARRAY2D, UsdDataArray)
USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_ARRAY3D, UsdDataArray)
USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_FRAME, UsdFrame)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_GEOMETRY, UsdGeometry)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_GROUP, UsdGroup)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_INSTANCE, UsdInstance)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_LIGHT, UsdLight)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_MATERIAL, UsdMaterial)
USDBRIDGE_DEFINE_BASE_OBJECT_MAPPING(ANARI_RENDERER, UsdRenderer)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_SURFACE, UsdSurface)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_SAMPLER, UsdSampler)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_SPATIAL_FIELD, UsdSpatialField)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_VOLUME, UsdVolume)
USDBRIDGE_DEFINE_BRIDGED_OBJECT_MAPPING(ANARI_WORLD, UsdWorld)
