// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdInstance.h"
#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdGroup.h"

#define GroupType ANARI_GROUP
using GroupUsdType = AnariToUsdBridgedObject<GroupType>::Type;

DEFINE_PARAMETER_MAP(UsdInstance,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timeVarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("group", GroupType, group)
  REGISTER_PARAMETER_MACRO("transform", ANARI_FLOAT32_MAT4, transform)
)

UsdInstance::UsdInstance(const char* name,
  UsdDevice* device)
  : BridgedBaseObjectType(ANARI_INSTANCE, name, device)
{
}

UsdInstance::~UsdInstance()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(cachedBridge)
    cachedBridge->DeleteInstance(usdHandle);
#endif
}

bool UsdInstance::deferCommit(UsdDevice* device)
{
  const UsdInstanceData& paramData = getReadParams();

  if(UsdObjectNotInitialized<GroupUsdType>(paramData.group))
  {
    return true;
  }
  return false;
}

bool UsdInstance::doCommitData(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();

  const char* instanceName = getName();

  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateInstance(instanceName, usdHandle);

  if (paramChanged || isNew)
  {
    doCommitRefs(device); // Perform immediate commit of refs - no params from children required

    paramChanged = false;
  }

  return false;
}

void UsdInstance::doCommitRefs(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdInstanceData& paramData = getReadParams();

  double timeStep = device->getReadParams().timeStep;
  bool groupTimeVarying = paramData.timeVarying & 1;
  bool transformTimeVarying = paramData.timeVarying & (1 << 1);

  if (paramData.group)
  {
    usdBridge->SetGroupRef(usdHandle, paramData.group->getUsdHandle(), groupTimeVarying, timeStep);
  }
  else
  {
    usdBridge->DeleteGroupRef(usdHandle, groupTimeVarying, timeStep);
  }

  usdBridge->SetInstanceTransform(usdHandle, paramData.transform.Data, transformTimeVarying, timeStep);
}