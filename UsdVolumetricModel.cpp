// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdVolumetricModel.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdDevice.h"
#include "UsdSpatialField.h"
#include "UsdDataArray.h"

DEFINE_PARAMETER_MAP(UsdVolumetricModel,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timevarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("field", ANARI_SPATIAL_FIELD, volume)
  REGISTER_PARAMETER_MACRO("usd::preclassified", ANARI_BOOL, preClassified)
  REGISTER_PARAMETER_MACRO("color", ANARI_ARRAY, color)
  REGISTER_PARAMETER_MACRO("opacity", ANARI_ARRAY, opacity)
  REGISTER_PARAMETER_MACRO("valueRange", ANARI_FLOAT32_VEC2, valueRange)
  REGISTER_PARAMETER_MACRO("densityScale", ANARI_FLOAT32, densityScale)
)

UsdVolumetricModel::UsdVolumetricModel(const char* name, UsdBridge* bridge, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_VOLUME, name, bridge)
{
}

UsdVolumetricModel::~UsdVolumetricModel()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  // Given that the object is destroyed, none of its references to other objects
  // has to be updated anymore.
  if (usdBridge)
    usdBridge->DeleteVolumetricModel(usdHandle);
#endif
}

void UsdVolumetricModel::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  if (filterNameParam(name, type, mem, device))
    setParam(name, type, mem, device);
}

void UsdVolumetricModel::filterResetParam(const char *name)
{
  resetParam(name);
}

bool UsdVolumetricModel::CheckTfParams(UsdDevice* device)
{
  const char* debugName = getName();

  // Only perform data(type) checks, data upload along with volume in UsdVolumetricModel::commit()
  const UsdDataArray* tfColor = this->paramData.color;
  if (this->paramData.color == nullptr)
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolumetricModel '%s' commit failed: transferfunction color array not set.", debugName);
    return false;
  }

  const UsdDataArray* tfOpacity = this->paramData.opacity;
  if (tfOpacity == nullptr)
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolumetricModel '%s' commit failed: transferfunction opacity not set.", debugName);
    return false;
  }

  if (AssertOneDimensional(tfColor->getLayout(), device, debugName, "tfColor"))
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolumetricModel '%s' commit failed: transferfunction color array not one-dimensional.", debugName);
    return false;
  }

  if (AssertOneDimensional(tfOpacity->getLayout(), device, debugName, "tfOpacity"))
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolumetricModel '%s' commit failed: transferfunction opacity array not one-dimensional.", debugName);
    return false;
  }

  if (tfColor->getType() != ANARI_FLOAT32_VEC3)
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolumetricModel '%s' commit failed: transferfunction color array needs to be of type vec3f.", debugName);
    return false;
  }

  if (tfOpacity->getType() != ANARI_FLOAT32)
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolumetricModel '%s' commit failed: transferfunction opacity array needs to be of type float.", debugName);
    return false;
  }

  return true;
}

bool UsdVolumetricModel::UpdateVolume(UsdDevice* device, const char* debugName)
{
  UsdVolume* volume = paramData.volume;

  if (!CheckTfParams(device))
    return false;

  // Get volume data
  UsdVolumeData& volParams = volume->paramData;
  const UsdDataArray* volumeDataArray = volParams.data;
  if(!volumeDataArray) return false; // Enforced in volume commit()
  const UsdDataLayout& posLayout = volumeDataArray->getLayout();

  UsdBridgeType volDataType = AnariToUsdBridgeType(volumeDataArray->getType());

  // Get transfer function data
  const UsdDataArray* tfColor = paramData.color;
  if(!tfColor) return false; // Enforced in transferfunction commit()
  const UsdDataArray* tfOpacity = paramData.opacity;
  if(!tfOpacity) return false;

  UsdBridgeType tfColorType = AnariToUsdBridgeType(tfColor->getType());
  UsdBridgeType tfOpacityType = AnariToUsdBridgeType(tfOpacity->getType());

  UsdBridgeVolumeData volumeData;
  volumeData.Data = volumeDataArray->getData();
  volumeData.DataType = volDataType;
  volumeData.TfColors = tfColor->getData();
  volumeData.TfColorsType = tfColorType;
  volumeData.TfNumColors = (int)(tfColor->getLayout().numItems1);
  volumeData.TfOpacities = tfOpacity->getData();
  volumeData.TfOpacitiesType = tfOpacityType;
  volumeData.TfNumOpacities = (int)(tfOpacity->getLayout().numItems1);

  size_t* elts = volumeData.NumElements;
  float* ori = volumeData.Origin;
  float* celldims = volumeData.CellDimensions;
  elts[0] = posLayout.numItems1; elts[1] = posLayout.numItems2; elts[2] = posLayout.numItems3;
  ori[0] = volParams.gridOrigin[0]; ori[1] = volParams.gridOrigin[1]; ori[2] = volParams.gridOrigin[2];
  celldims[0] = volParams.gridSpacing[0]; celldims[1] = volParams.gridSpacing[1]; celldims[2] = volParams.gridSpacing[2];

  volumeData.TfValueRange[0] = paramData.valueRange[0]; volumeData.TfValueRange[1] = paramData.valueRange[1];

  // Set whether we want to output source data or preclassified colored volumes
  volumeData.preClassified = paramData.preClassified;

  typedef UsdBridgeVolumeData::DataMemberId DMI;
  volumeData.TimeVarying = (DMI)(volParams.timeVarying | (paramData.timeVarying << UsdBridgeVolumeData::TFDataStart));

  double volTimeStep = volume->paramData.timeStep;
  usdBridge->SetVolumeData(volume->getUsdHandle(), volumeData, volTimeStep);

  return true;
}


void UsdVolumetricModel::commit(UsdDevice* device)
{
  if(!usdBridge)
    return;

  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateVolumetricModel(getName(), usdHandle);

  const char* debugName = getName();

  if (prevVolume != paramData.volume)
  {
    double worldTimeStep = device->getParams().timeStep;

    // Make sure the references are updated on the Bridge side.
    if (paramData.volume)
    {
      usdBridge->SetVolumeRef(usdHandle,
        paramData.volume->getUsdHandle(),
        worldTimeStep,
        paramData.volume->getParams().timeStep);
    }
    else
    {
      usdBridge->DeleteVolumeRef(usdHandle, worldTimeStep);
    }

    prevVolume = paramData.volume;
  }

  // Regardless of whether tf param changes, volume params or the vol reference itself, UpdateVolume is required.
  if (paramChanged || paramData.volume->paramChanged)
  {
    UpdateVolume(device, debugName);

    paramChanged = false;
    paramData.volume->paramChanged = false;
  }
  else
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolumetricModel '%s' commit failed: volume or transferfunction reference missing.", debugName);
  }
}