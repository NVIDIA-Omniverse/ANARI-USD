// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"
#include "UsdBridgedBaseObject.h"

enum class UsdCameraComponents
{
  VIEW = 0,
  PROJECTION
};

struct UsdCameraData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  int timeVarying = 0xFFFFFFFF; // TimeVarying bits
  
  UsdFloat3 position = {0.0f, 0.0f, 0.0f};
  UsdFloat3 direction = {0.0f, 0.0f, -1.0f};
  UsdFloat3 up = {0.0f, 1.0f, 0.0f};
  UsdFloatBox2 imageRegion = {0.0f, 0.0f, 1.0f, 1.0f};

  // perspective/orthographic
  float aspect = 1.0f;
  float near = 1.0f;
  float far = 10000;
  float fovy = M_PI/3.0f;
  float height = 1.0f;
};

class UsdCamera : public UsdBridgedBaseObject<UsdCamera, UsdCameraData, UsdCameraHandle, UsdCameraComponents>
{
  public:
    UsdCamera(const char* name, const char* type, UsdDevice* device);
    ~UsdCamera();

    void remove(UsdDevice* device) override;

    enum CameraType
    {
      CAMERA_UNKNOWN = 0,
      CAMERA_PERSPECTIVE,
      CAMERA_ORTHOGRAPHIC
    };

    static constexpr ComponentPair componentParamNames[] = {
      ComponentPair(UsdCameraComponents::VIEW, "view"),
      ComponentPair(UsdCameraComponents::PROJECTION, "projection")};

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    void copyParameters(UsdBridgeCameraData& camData);

    CameraType cameraType = CAMERA_UNKNOWN;
};