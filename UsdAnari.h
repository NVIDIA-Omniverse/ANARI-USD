// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridge/UsdBridgeData.h"
#include "anari/anari_enums.h"
#include "UsdCommonMacros.h"

class UsdDevice;
struct UsdDataLayout;

UsdBridgeType AnariToUsdBridgeType(ANARIDataType anariType);
UsdBridgeType AnariToUsdBridgeType_Flattened(ANARIDataType anariType);
size_t AnariTypeSize(ANARIDataType anariType);
const char* AnariTypeToString(ANARIDataType anariType);
ANARIStatusSeverity UsdBridgeLogLevelToAnariSeverity(UsdBridgeLogLevel level);

void reportStatusThroughDevice(UsdDevice* device,
  void* source, ANARIDataType sourceType, ANARIStatusSeverity severity, ANARIStatusCode statusCode,
  const char *format, const char* firstArg, const char* secondArg); // In case #include <UsdDevice.h> is undesired
bool checkSizeOnStringLengthProperty(UsdDevice* device, void* source, uint64_t size, const char* name);

bool AssertOneDimensional(const UsdDataLayout& layout, UsdDevice* device, const char* objName, const char* arrayName);
bool AssertNoStride(const UsdDataLayout& layout, UsdDevice* device, const char* objName, const char* arrayName);