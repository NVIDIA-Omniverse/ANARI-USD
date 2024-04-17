// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdLight.h"
#include "UsdAnari.h"
#include "UsdDevice.h"

DEFINE_PARAMETER_MAP(UsdLight,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
)

UsdLight::UsdLight(const char* type, const char* name, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_LIGHT, name, device)
{}

UsdLight::~UsdLight()
{}

void UsdLight::remove(UsdDevice* device)
{
  //applyRemoveFunc(device, &UsdBridge::DeleteLight);
}

bool UsdLight::deferCommit(UsdDevice* device) 
{ 
  return false; 
}

bool UsdLight::doCommitData(UsdDevice* device) 
{
  return false;
}