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
    lightPrim.CreateExposureAttr();
    lightPrim.CreateNormalizeAttr();
  }

  void CreateDirectionalLight(UsdStageRefPtr lightStage, const SdfPath& lightPrimPath)
  {
    CreateLightCommon<UsdLuxDistantLight>(lightStage, lightPrimPath);
    UsdLuxDistantLight lightPrim = UsdLuxDistantLight::Get(lightStage, lightPrimPath);
    assert(lightPrim);
    lightPrim.CreateAngleAttr();
  }

  void CreatePointLight(UsdStageRefPtr lightStage, const SdfPath& lightPrimPath)
  {
    CreateLightCommon<UsdLuxSphereLight>(lightStage, lightPrimPath);
    UsdLuxSphereLight lightPrim = UsdLuxSphereLight::Get(lightStage, lightPrimPath);
    assert(lightPrim);
    lightPrim.CreateRadiusAttr();
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
    typedef typename LightDataType::DataMemberId DMI;

    UsdLightPrimType lightPrim = UsdLightPrimType::Get(timeVarStage, lightPrimPath);
    assert(lightPrim);

    GfVec3f colorValue(lightData.Color.Data);
    ClearAndSetUsdAttribute(lightPrim.GetColorAttr(), colorValue,
      timeEval.Eval(DMI::COLOR), timeVarHasChanged && !timeEval.IsTimeVarying(DMI::COLOR));

    ClearAndSetUsdAttribute(lightPrim.GetIntensityAttr(), lightData.Intensity,
      timeEval.Eval(DMI::INTENSITY), timeVarHasChanged && !timeEval.IsTimeVarying(DMI::INTENSITY));

    // Exposure shares the time-varying behavior of intensity since it is
    // derived from the same source magnitude.
    ClearAndSetUsdAttribute(lightPrim.GetExposureAttr(), lightData.Exposure,
      timeEval.Eval(DMI::INTENSITY), timeVarHasChanged && !timeEval.IsTimeVarying(DMI::INTENSITY));

    // Author normalize=true so 'intensity' has a renderer-independent
    // physical meaning across light types: lux for DistantLight,
    // candela for SphereLight, nits for DomeLight.
    lightPrim.GetNormalizeAttr().Set(true);

    return lightPrim;
  }
}

void UsdBridgeUsdWriter::UpdateUsdLight(UsdStageRefPtr timeVarStage, const SdfPath& lightPrimPath, 
    const UsdBridgeDirectionalLightData& lightData, double timeStep, bool timeVarHasChanged)
{
  typedef typename UsdBridgeDirectionalLightData::DataMemberId DMI;

  const TimeEvaluator<UsdBridgeDirectionalLightData> timeEval(lightData, timeStep);
  UsdLuxDistantLight lightPrim = UpdateUsdLightCommon<UsdBridgeDirectionalLightData, UsdLuxDistantLight>(
    timeVarStage, lightPrimPath, lightData, timeStep, timeVarHasChanged, timeEval);

  // ANARI angularDiameter is in radians; UsdLuxDistantLight inputs:angle is in degrees.
  const float angleDeg = lightData.AngularDiameter * (180.0f / float(M_PI));
  ClearAndSetUsdAttribute(lightPrim.GetAngleAttr(), angleDeg,
    timeEval.Eval(DMI::ANGULAR_DIAMETER),
    timeVarHasChanged && !timeEval.IsTimeVarying(DMI::ANGULAR_DIAMETER));

  // Set the direction transform via a viewmatrix
  GfVec3d eyePoint(0.0, 0.0, 0.0);
  GfVec3d fwdDir(lightData.Direction.Data);
  const double absX = std::fabs(lightData.Direction.Data[0]);
  const double absY = std::fabs(lightData.Direction.Data[1]);
  GfVec3d approxUpDir = (absX < absY) ? GfVec3d(1.0, 0.0, 0.0) : GfVec3d(0.0, 1.0, 0.0);// SetLookAt will orthogonalize
  GfVec3d lookAtPoint = eyePoint+fwdDir;

  GfMatrix4d viewMatrix;
  viewMatrix.SetLookAt(eyePoint, lookAtPoint, approxUpDir);

  GfMatrix4d xform = viewMatrix.GetInverse(); // local-to-world

  lightPrim.ClearXformOpOrder();
  UsdGeomXformOp xformOp = lightPrim.AddTransformOp();

  ClearAndSetUsdAttribute(xformOp.GetAttr(), xform, timeEval.Eval(DMI::DIRECTION),
    timeVarHasChanged && !timeEval.IsTimeVarying(DMI::DIRECTION));
}

void UsdBridgeUsdWriter::UpdateUsdLight(UsdStageRefPtr timeVarStage, const SdfPath& lightPrimPath, 
  const UsdBridgePointLightData& lightData, double timeStep, bool timeVarHasChanged)
{
  typedef typename UsdBridgePointLightData::DataMemberId DMI;

  const TimeEvaluator<UsdBridgePointLightData> timeEval(lightData, timeStep);
  UsdLuxSphereLight lightPrim = UpdateUsdLightCommon<UsdBridgePointLightData, UsdLuxSphereLight>(
    timeVarStage, lightPrimPath, lightData, timeStep, timeVarHasChanged, timeEval);

  ClearAndSetUsdAttribute(lightPrim.GetRadiusAttr(), lightData.Radius,
    timeEval.Eval(DMI::RADIUS),
    timeVarHasChanged && !timeEval.IsTimeVarying(DMI::RADIUS));

  GfVec3f position(lightData.Position.Data);

  lightPrim.ClearXformOpOrder();
  UsdGeomXformOp translOp = lightPrim.AddTranslateOp(UsdGeomXformOp::PrecisionFloat);

  ClearAndSetUsdAttribute(translOp.GetAttr(), position, timeEval.Eval(DMI::POSITION),
    timeVarHasChanged && !timeEval.IsTimeVarying(DMI::POSITION));
}

void UsdBridgeUsdWriter::UpdateUsdLight(UsdStageRefPtr timeVarStage, const SdfPath& lightPrimPath, 
  const UsdBridgeDomeLightData& lightData, double timeStep, bool timeVarHasChanged)
{
  typedef typename UsdBridgeDomeLightData::DataMemberId DMI;

  const TimeEvaluator<UsdBridgeDomeLightData> timeEval(lightData, timeStep);
  UsdLuxDomeLight lightPrim = UpdateUsdLightCommon<UsdBridgeDomeLightData, UsdLuxDomeLight>(
    timeVarStage, lightPrimPath, lightData, timeStep, timeVarHasChanged, timeEval);
}