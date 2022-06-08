// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdInstance.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdGroup.h"

#define GroupType ANARI_GROUP
using GroupUsdType = AnariToUsdBridgedObject<GroupType>::Type;

DEFINE_PARAMETER_MAP(UsdInstance,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timevarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("group", GroupType, group)
  REGISTER_PARAMETER_MACRO("transform", ANARI_FLOAT32_MAT3x4, transform)
)

UsdInstance::UsdInstance(const char* name, UsdBridge* bridge, 
  UsdDevice* device)
  : BridgedBaseObjectType(ANARI_INSTANCE, name, bridge)
{
}

UsdInstance::~UsdInstance()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(usdBridge)
    usdBridge->DeleteInstance(usdHandle);
#endif
}

void UsdInstance::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  if (filterNameParam(name, type, mem, device))
    setParam(name, type, mem, device);
}

void UsdInstance::filterResetParam(const char *name)
{
  resetParam(name);
}

bool UsdInstance::deferCommit(UsdDevice* device)
{
  if(UsdObjectNotInitialized<GroupUsdType>(paramData.group))
  {
    return true;
  }
  return false;
}

void UsdInstance::doCommitWork(UsdDevice* device)
{
  if(!usdBridge)
    return;

  const char* instanceName = getName();

  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateInstance(instanceName, usdHandle);

  if (paramChanged || isNew)
  {
    double timeStep = device->getParams().timeStep;
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

    {
      float bridgeTransform[16] = { 0.0f };
      memcpy(&bridgeTransform[0], &paramData.transform[0], 3 * sizeof(float));
      memcpy(&bridgeTransform[4], &paramData.transform[3], 3 * sizeof(float));
      memcpy(&bridgeTransform[8], &paramData.transform[6], 3 * sizeof(float));
      memcpy(&bridgeTransform[12], &paramData.transform[9], 3 * sizeof(float));
      bridgeTransform[15] = 1.0f;

      usdBridge->SetInstanceTransform(usdHandle, bridgeTransform, transformTimeVarying, timeStep);
    }

    paramChanged = false;
  }
}