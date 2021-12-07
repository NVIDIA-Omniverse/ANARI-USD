// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdTransferFunction.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdDevice.h"
#include "UsdDataArray.h"

DEFINE_PARAMETER_MAP(UsdTransferFunction, 
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("name.usd", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("timestep", ANARI_FLOAT64, timeStep)
  REGISTER_PARAMETER_MACRO("timevarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("color", ANARI_ARRAY, color)
  REGISTER_PARAMETER_MACRO("opacity", ANARI_ARRAY, opacity)
  REGISTER_PARAMETER_MACRO("valueRange", ANARI_FLOAT32_VEC2, valueRange)
)

UsdTransferFunction::UsdTransferFunction(const char* name, UsdBridge* bridge)
  : BridgedBaseObjectType(name, bridge)
{
}

UsdTransferFunction::~UsdTransferFunction()
{
}

void UsdTransferFunction::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  if (filterNameParam(name, type, mem, device))
    setParam(name, type, mem, device);
}

void UsdTransferFunction::filterResetParam(const char *name)
{
  resetParam(name);
}

void UsdTransferFunction::commit(UsdDevice* device)
{
  if(!usdBridge)
    return;

  const char* debugName = getName();

  // Only perform data(type) checks, data upload along with volume in UsdVolumetricModel::commit()
  const UsdDataArray* tfColor = this->paramData.color;
  if (this->paramData.color == nullptr)
  {
    device->reportStatus(this, ANARI_TRANSFER_FUNCTION, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, 
      "UsdTransferFunction '%s' commit failed: color array not set.", debugName);
    return;
  }

  const UsdDataArray* tfOpacity = this->paramData.opacity;
  if (tfOpacity == nullptr)
  {
    device->reportStatus(this, ANARI_TRANSFER_FUNCTION, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, 
      "UsdTransferFunction '%s' commit failed: opacity not set.", debugName);
    return;
  }

  if (AssertOneDimensional(tfColor->getLayout(), device, debugName, "tfColor"))
    return;

  if (AssertOneDimensional(tfOpacity->getLayout(), device, debugName, "tfOpacity"))
    return;

  if (tfColor->getType() != ANARI_FLOAT32_VEC3)
  {
    device->reportStatus(this, ANARI_TRANSFER_FUNCTION, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdTransferFunction '%s' commit failed: color array needs to be of type vec3f.", debugName);
    return;
  }

  if (tfOpacity->getType() != ANARI_FLOAT32)
  {
    device->reportStatus(this, ANARI_TRANSFER_FUNCTION, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, 
      "UsdTransferFunction '%s' commit failed: opacity array needs to be of type float.", debugName);
    return;
  }

  // Handled in UsdVolumetricModel
  //if (paramChanged)
  //{
  //
  //
  //  paramChanged = false;
  //}
}
