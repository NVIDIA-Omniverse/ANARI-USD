// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdVolume.h"
#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdSpatialField.h"
#include "UsdDataArray.h"

DEFINE_PARAMETER_MAP(UsdVolume,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timeVarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("usd::preClassified", ANARI_BOOL, preClassified)
  REGISTER_PARAMETER_MACRO("usd::time::value", ANARI_FLOAT64, fieldRefTimeStep)
  REGISTER_PARAMETER_MACRO("value", ANARI_SPATIAL_FIELD, field)
  REGISTER_PARAMETER_MACRO("color", ANARI_ARRAY, color)
  REGISTER_PARAMETER_MACRO("opacity", ANARI_ARRAY, opacity)
  REGISTER_PARAMETER_MACRO("valueRange", ANARI_FLOAT32_BOX1, valueRange)
  REGISTER_PARAMETER_MACRO("unitDistance", ANARI_FLOAT32, unitDistance)
)

namespace
{
  void GatherTfData(const UsdVolumeData& paramData, UsdBridgeTfData& tfData)
  {
    // Get transfer function data
    const UsdDataArray* tfColor = paramData.color;
    const UsdDataArray* tfOpacity = paramData.opacity;

    UsdBridgeType tfColorType = AnariToUsdBridgeType(tfColor->getType());
    UsdBridgeType tfOpacityType = AnariToUsdBridgeType(tfOpacity->getType());

    // Write to struct
    tfData.TfColors = tfColor->getData();
    tfData.TfColorsType = tfColorType;
    tfData.TfNumColors = (int)(tfColor->getLayout().numItems1);
    tfData.TfOpacities = tfOpacity->getData();
    tfData.TfOpacitiesType = tfOpacityType;
    tfData.TfNumOpacities = (int)(tfOpacity->getLayout().numItems1);
    tfData.TfValueRange[0] = paramData.valueRange.Data[0];
    tfData.TfValueRange[1] = paramData.valueRange.Data[1];
  }
}

UsdVolume::UsdVolume(const char* name, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_VOLUME, name, device)
  , usdDevice(device)
{
  usdDevice->addToVolumeList(this);
}

UsdVolume::~UsdVolume()
{
  usdDevice->removeFromVolumeList(this);

#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  // Given that the object is destroyed, none of its references to other objects
  // has to be updated anymore.
  if(cachedBridge)
    cachedBridge->DeleteVolume(usdHandle);
#endif
}

bool UsdVolume::CheckTfParams(UsdDevice* device)
{
  const UsdVolumeData& paramData = getReadParams();

  const char* debugName = getName();

  UsdLogInfo logInfo(device, this, ANARI_VOLUME, debugName);

  // Only perform data(type) checks, data upload along with field in UsdVolume::commit()
  const UsdDataArray* tfColor = paramData.color;
  if (paramData.color == nullptr)
  {
    device->reportStatus(this, ANARI_VOLUME, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction color array not set.", debugName);
    return false;
  }

  const UsdDataArray* tfOpacity = paramData.opacity;
  if (tfOpacity == nullptr)
  {
    device->reportStatus(this, ANARI_VOLUME, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction opacity not set.", debugName);
    return false;
  }

  if (!AssertOneDimensional(tfColor->getLayout(), logInfo, "tfColor"))
  {
    device->reportStatus(this, ANARI_VOLUME, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction color array not one-dimensional.", debugName);
    return false;
  }

  if (!AssertOneDimensional(tfOpacity->getLayout(), logInfo, "tfOpacity"))
  {
    device->reportStatus(this, ANARI_VOLUME, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction opacity array not one-dimensional.", debugName);
    return false;
  }

  if (paramData.preClassified && tfColor->getType() != ANARI_FLOAT32_VEC3)
  {
    device->reportStatus(this, ANARI_VOLUME, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction color array needs to be of type ANARI_FLOAT32_VEC3 when preClassified is set.", debugName);
    return false;
  }

  if (tfOpacity->getType() != ANARI_FLOAT32)
  {
    device->reportStatus(this, ANARI_VOLUME, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction opacity array needs to be of type ANARI_FLOAT32.", debugName);
    return false;
  }

  if (tfColor->getLayout().numItems1 != tfOpacity->getLayout().numItems1)
  {
    device->reportStatus(this, ANARI_VOLUME, ANARI_SEVERITY_WARNING, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit warning: transferfunction output merges colors and opacities into one array, so they should contain the same number of elements.", debugName);
  }

  return true;
}

bool UsdVolume::UpdateVolume(UsdDevice* device, const char* debugName)
{
  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdVolumeData& paramData = getReadParams();

  UsdSpatialField* field = paramData.field;

  if (!CheckTfParams(device))
    return false;

  // Get field data
  const UsdSpatialFieldData& fieldParams = field->getReadParams();
  const UsdDataArray* fieldDataArray = fieldParams.data;
  if(!fieldDataArray) return false; // Enforced in field commit()
  const UsdDataLayout& posLayout = fieldDataArray->getLayout();

  UsdBridgeType fieldDataType = AnariToUsdBridgeType(fieldDataArray->getType());

  //Set bridge volumedata
  UsdBridgeVolumeData volumeData;
  volumeData.Data = fieldDataArray->getData();
  volumeData.DataType = fieldDataType;

  size_t* elts = volumeData.NumElements;
  float* ori = volumeData.Origin;
  float* celldims = volumeData.CellDimensions;
  elts[0] = posLayout.numItems1; elts[1] = posLayout.numItems2; elts[2] = posLayout.numItems3;
  ori[0] = fieldParams.gridOrigin[0]; ori[1] = fieldParams.gridOrigin[1]; ori[2] = fieldParams.gridOrigin[2];
  celldims[0] = fieldParams.gridSpacing[0]; celldims[1] = fieldParams.gridSpacing[1]; celldims[2] = fieldParams.gridSpacing[2];

  GatherTfData(paramData, volumeData.TfData);
  
  // Set whether we want to output source data or preclassified colored volumes
  volumeData.preClassified = paramData.preClassified;

  typedef UsdBridgeVolumeData::DataMemberId DMI;
  volumeData.TimeVarying = DMI::ALL
    & (field->isTimeVarying(UsdSpatialFieldComponents::DATA) ? DMI::ALL : ~DMI::DATA)
    & (isTimeVarying(CType::COLOR) ? DMI::ALL : ~DMI::TFCOLORS)
    & (isTimeVarying(CType::OPACITY) ? DMI::ALL : ~DMI::TFOPACITIES)
    & (isTimeVarying(CType::VALUERANGE) ? DMI::ALL : ~DMI::TFVALUERANGE);

  double worldTimeStep = device->getReadParams().timeStep;
  double fieldTimeStep = selectRefTime(paramData.fieldRefTimeStep, fieldParams.timeStep, worldTimeStep); // use the link time, as there is no such thing as separate field data
  
  usdBridge->SetSpatialFieldData(field->getUsdHandle(), volumeData, fieldTimeStep);

  return true;
}

bool UsdVolume::deferCommit(UsdDevice* device)
{
  // The spatial field may not yet have been committed, but the volume reads data from its params during commit. So always defer until flushing of commit list.
  return !device->isFlushingCommitList();
}

bool UsdVolume::doCommitData(UsdDevice* device)
{
  const UsdVolumeData& paramData = getReadParams();

  UsdBridge* usdBridge = device->getUsdBridge();

  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateVolume(getName(), usdHandle);

  const char* debugName = getName();

  if (prevField != paramData.field)
  {
    double worldTimeStep = device->getReadParams().timeStep;

    // Make sure the references are updated on the Bridge side.
    if (paramData.field)
    {
      const UsdSpatialFieldData& fieldParams = paramData.field->getReadParams();

      usdBridge->SetSpatialFieldRef(usdHandle,
        paramData.field->getUsdHandle(),
        worldTimeStep,
        selectRefTime(paramData.fieldRefTimeStep, fieldParams.timeStep, worldTimeStep)
        );
    }
    else
    {
      usdBridge->DeleteSpatialFieldRef(usdHandle, worldTimeStep);
    }

    prevField = paramData.field;
  }

  // Regardless of whether tf param changes, field params or the vol reference itself, UpdateVolume is required.
  if (paramChanged || paramData.field->paramChanged)
  {
    if(paramData.field)
    {
      UpdateVolume(device, debugName);

      paramChanged = false;
      paramData.field->paramChanged = false;
    }
    else
    {
      device->reportStatus(this, ANARI_VOLUME, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
        "UsdVolume '%s' commit failed: field reference missing.", debugName);
    }
  }

  return false;
}