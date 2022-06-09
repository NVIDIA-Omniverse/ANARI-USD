// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdLight.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDevice.h"

DEFINE_PARAMETER_MAP(UsdLight,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
)

UsdLight::UsdLight(const char* name, UsdBridge* bridge, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_LIGHT, name, bridge)
{}

UsdLight::~UsdLight()
{}

void UsdLight::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device) 
{}

void UsdLight::filterResetParam(
  const char *name) 
{}

bool UsdLight::deferCommit(UsdDevice* device) 
{ 
  return false; 
}

void UsdLight::doCommitWork(UsdDevice* device) 
{}