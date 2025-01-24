// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeUsdWriter.h"

#include "UsdBridgeUsdWriter_Common.h"

namespace
{
  template<typename UsdLightPrimType>
  void CreateLightCommon(UsdStageRefPtr lightStage, const SdfPath& lightPrimPath)
  {
    UsdLightPrimType lightPrim = GetOrDefinePrim<UsdLightPrimType>(lightStage, lightPrimPath);
    assert(lightPrim);

    lightPrim.CreateColorAttr();
    lightPrim.CreateIntensityAttr();
  }

  void CreateDirectionalLight(UsdStageRefPtr lightStage, const SdfPath& lightPrimPath)
  {
    CreateLightCommon<UsdLuxDistantLight>(lightStage, lightPrimPath);
  }

  void CreatePointLight(UsdStageRefPtr lightStage, const SdfPath& lightPrimPath)
  {
    CreateLightCommon<UsdLuxSphereLight>(lightStage, lightPrimPath);
  }

  void CreateDomeLight(UsdStageRefPtr lightStage, const SdfPath& lightPrimPath)
  {
    CreateLightCommon<UsdLuxDomeLight>(lightStage, lightPrimPath);
  }
}

void UsdBridgeUsdWriter::InitializeUsdLight(UsdStageRefPtr lightStage, const SdfPath& lightPrimPath, UsdBridgeLightType lightType)
{
  switch(lightType)
  {
    case UsdBridgeLightType::DIRECTIONAL: CreateDirectionalLight(lightStage, lightPrimPath); break;
    case UsdBridgeLightType::POINT: CreatePointLight(lightStage, lightPrimPath); break;
    default: CreateDomeLight(lightStage, lightPrimPath); break;
  };
}

namespace
{
  template<typename LightDataType, typename UsdLightPrimType>
  UsdLightPrimType UpdateUsdLightCommon(
    UsdStageRefPtr timeVarStage, const SdfPath& lightPrimPath,
    const LightDataType& lightData, double timeStep, bool timeVarHasChanged,
    const TimeEvaluator<LightDataType>& timeEval)
  {
    typedef LightDataType::DataMemberId DMI;

    UsdLightPrimType lightPrim = UsdLightPrimType::Get(timeVarStage, lightPrimPath);
    assert(lightPrim);

    GfVec3f colorValue(lightData.Color.Data);
    ClearAndSetUsdAttribute(lightPrim.GetColorAttr(), colorValue,
      timeEval.Eval(DMI::COLOR), timeVarHasChanged && !timeEval.IsTimeVarying(DMI::COLOR));
    
    ClearAndSetUsdAttribute(lightPrim.GetIntensityAttr(), lightData.Intensity,
      timeEval.Eval(DMI::INTENSITY), timeVarHasChanged && !timeEval.IsTimeVarying(DMI::INTENSITY));
  
    return lightPrim;
  }
}

void UsdBridgeUsdWriter::UpdateUsdLight(UsdStageRefPtr timeVarStage, const SdfPath& lightPrimPath, 
    const UsdBridgeDirectionalLightData& lightData, double timeStep, bool timeVarHasChanged)
{
  typedef UsdBridgeDirectionalLightData::DataMemberId DMI;

  const TimeEvaluator<UsdBridgeDirectionalLightData> timeEval(lightData, timeStep);
  UsdLuxDistantLight lightPrim = UpdateUsdLightCommon<UsdBridgeDirectionalLightData, UsdLuxDistantLight>(
    timeVarStage, lightPrimPath, lightData, timeStep, timeVarHasChanged, timeEval);

  // Set the direction transform via a viewmatrix
  GfVec3d eyePoint(0.0, 0.0, 0.0);
  GfVec3d fwdDir(lightData.Direction.Data);
  GfVec3d approxUpDir((lightData.Direction.Data[1] > lightData.Direction.Data[0]) ?
    GfVec3d(1.0, 0.0, 0.0) : GfVec3d(0.0, 1.0, 0.0)); // SetLookAt will orthogonalize
  GfVec3d lookAtPoint = eyePoint+fwdDir;

  GfMatrix4d viewMatrix;
  viewMatrix.SetLookAt(eyePoint, lookAtPoint, approxUpDir);

  lightPrim.ClearXformOpOrder();
  UsdGeomXformOp xformOp = lightPrim.AddTransformOp(UsdGeomXformOp::PrecisionFloat);
  ClearAndSetUsdAttribute(xformOp.GetAttr(), viewMatrix, timeEval.Eval(DMI::DIRECTION),
    timeVarHasChanged && !timeEval.IsTimeVarying(DMI::DIRECTION));
}

void UsdBridgeUsdWriter::UpdateUsdLight(UsdStageRefPtr timeVarStage, const SdfPath& lightPrimPath, 
  const UsdBridgePointLightData& lightData, double timeStep, bool timeVarHasChanged)
{
  typedef UsdBridgePointLightData::DataMemberId DMI;

  const TimeEvaluator<UsdBridgePointLightData> timeEval(lightData, timeStep);
  UsdLuxSphereLight lightPrim = UpdateUsdLightCommon<UsdBridgePointLightData, UsdLuxSphereLight>(
    timeVarStage, lightPrimPath, lightData, timeStep, timeVarHasChanged, timeEval);
  
  GfVec3f position(lightData.Position.Data);

  lightPrim.ClearXformOpOrder();
  UsdGeomXformOp translOp = lightPrim.AddTranslateOp(UsdGeomXformOp::PrecisionFloat);
  ClearAndSetUsdAttribute(translOp.GetAttr(), position, timeEval.Eval(DMI::POSITION),
    timeVarHasChanged && !timeEval.IsTimeVarying(DMI::POSITION));
}

void UsdBridgeUsdWriter::UpdateUsdLight(UsdStageRefPtr timeVarStage, const SdfPath& lightPrimPath, 
  const UsdBridgeDomeLightData& lightData, double timeStep, bool timeVarHasChanged)
{
  typedef UsdBridgeDomeLightData::DataMemberId DMI;

  const TimeEvaluator<UsdBridgeDomeLightData> timeEval(lightData, timeStep);
  UsdLuxDomeLight lightPrim = UpdateUsdLightCommon<UsdBridgeDomeLightData, UsdLuxDomeLight>(
    timeVarStage, lightPrimPath, lightData, timeStep, timeVarHasChanged, timeEval);
}