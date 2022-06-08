// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdVolume.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdAnari.h"
#include "UsdDevice.h"
#include "UsdSpatialField.h"
#include "UsdDataArray.h"

DEFINE_PARAMETER_MAP(UsdVolume,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timevarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("usd::preclassified", ANARI_BOOL, preClassified)
  REGISTER_PARAMETER_MACRO("field", ANARI_SPATIAL_FIELD, field)
  REGISTER_PARAMETER_MACRO("color", ANARI_ARRAY, color)
  REGISTER_PARAMETER_MACRO("opacity", ANARI_ARRAY, opacity)
  REGISTER_PARAMETER_MACRO("valueRange", ANARI_FLOAT32_VEC2, valueRange)
  REGISTER_PARAMETER_MACRO("densityScale", ANARI_FLOAT32, densityScale)
)

UsdVolume::UsdVolume(const char* name, UsdBridge* bridge, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_VOLUME, name, bridge)
{
}

UsdVolume::~UsdVolume()
{
  if(this->paramData.field)
    this->paramData.field->removeParent(this);

#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  // Given that the object is destroyed, none of its references to other objects
  // has to be updated anymore.
  if (usdBridge)
    usdBridge->DeleteVolume(usdHandle);
#endif
}

void UsdVolume::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  bool isField = std::strcmp(name, "field") == 0;

  if(isField && this->paramData.field)
    this->paramData.field->removeParent(this);

  if (filterNameParam(name, type, mem, device))
    setParam(name, type, mem, device);

  if(isField && this->paramData.field)
    this->paramData.field->addParent(this);
}

void UsdVolume::filterResetParam(const char *name)
{
  bool isField = std::strcmp(name, "field") == 0;
  if(isField && this->paramData.field)
    this->paramData.field->removeParent(this);

  resetParam(name);
}

bool UsdVolume::CheckTfParams(UsdDevice* device)
{
  const char* debugName = getName();

  LogInfo logInfo(device, this, ANARI_VOLUME, debugName);

  // Only perform data(type) checks, data upload along with field in UsdVolume::commit()
  const UsdDataArray* tfColor = this->paramData.color;
  if (this->paramData.color == nullptr)
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction color array not set.", debugName);
    return false;
  }

  const UsdDataArray* tfOpacity = this->paramData.opacity;
  if (tfOpacity == nullptr)
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction opacity not set.", debugName);
    return false;
  }

  if (!AssertOneDimensional(tfColor->getLayout(), logInfo, "tfColor"))
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction color array not one-dimensional.", debugName);
    return false;
  }

  if (!AssertOneDimensional(tfOpacity->getLayout(), logInfo, "tfOpacity"))
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction opacity array not one-dimensional.", debugName);
    return false;
  }

  if (tfColor->getType() != ANARI_FLOAT32_VEC3)
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction color array needs to be of type vec3f.", debugName);
    return false;
  }

  if (tfOpacity->getType() != ANARI_FLOAT32)
  {
    device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdVolume '%s' commit failed: transferfunction opacity array needs to be of type float.", debugName);
    return false;
  }

  return true;
}

bool UsdVolume::UpdateVolume(UsdDevice* device, const char* debugName)
{
  UsdSpatialField* field = paramData.field;

  if (!CheckTfParams(device))
    return false;

  // Get field data
  UsdSpatialFieldData& fieldParams = field->paramData;
  const UsdDataArray* fieldDataArray = fieldParams.data;
  if(!fieldDataArray) return false; // Enforced in field commit()
  const UsdDataLayout& posLayout = fieldDataArray->getLayout();

  UsdBridgeType fieldDataType = AnariToUsdBridgeType(fieldDataArray->getType());

  // Get transfer function data
  const UsdDataArray* tfColor = paramData.color;
  if(!tfColor) return false; // Enforced in transferfunction commit()
  const UsdDataArray* tfOpacity = paramData.opacity;
  if(!tfOpacity) return false;

  UsdBridgeType tfColorType = AnariToUsdBridgeType(tfColor->getType());
  UsdBridgeType tfOpacityType = AnariToUsdBridgeType(tfOpacity->getType());

  //Set bridge volumedata
  UsdBridgeVolumeData volumeData;
  volumeData.Data = fieldDataArray->getData();
  volumeData.DataType = fieldDataType;
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
  ori[0] = fieldParams.gridOrigin[0]; ori[1] = fieldParams.gridOrigin[1]; ori[2] = fieldParams.gridOrigin[2];
  celldims[0] = fieldParams.gridSpacing[0]; celldims[1] = fieldParams.gridSpacing[1]; celldims[2] = fieldParams.gridSpacing[2];

  volumeData.TfValueRange[0] = paramData.valueRange[0]; volumeData.TfValueRange[1] = paramData.valueRange[1];

  // Set whether we want to output source data or preclassified colored volumes
  volumeData.preClassified = paramData.preClassified;

  typedef UsdBridgeVolumeData::DataMemberId DMI;
  volumeData.TimeVarying = (DMI)(fieldParams.timeVarying | (paramData.timeVarying << UsdBridgeVolumeData::TFDataStart));

  double fieldTimeStep = field->paramData.timeStep;
  usdBridge->SetVolumeData(field->getUsdHandle(), volumeData, fieldTimeStep);

  return true;
}

bool UsdVolume::deferCommit(UsdDevice* device)
{
  // The spatial field may not yet be committed, but the volume reads data from its params during commit. So always defer.
  return true; 
}

void UsdVolume::doCommitWork(UsdDevice* device)
{
  if(!usdBridge)
    return;

  if(UsdObjectNotInitialized(paramData.field))
  {
    device->addToCommitList(paramData.field);
    device->addToCommitList(this);
    return;
  }

  bool isNew = false;
  if (!usdHandle.value)
    isNew = usdBridge->CreateVolume(getName(), usdHandle);

  const char* debugName = getName();

  if (prevField != paramData.field)
  {
    double worldTimeStep = device->getParams().timeStep;

    // Make sure the references are updated on the Bridge side.
    if (paramData.field)
    {
      usdBridge->SetSpatialFieldRef(usdHandle,
        paramData.field->getUsdHandle(),
        worldTimeStep,
        paramData.field->getParams().timeStep);
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
      device->reportStatus(this, ANARI_SPATIAL_FIELD, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
        "UsdVolume '%s' commit failed: field reference missing.", debugName);
    }
  }
}