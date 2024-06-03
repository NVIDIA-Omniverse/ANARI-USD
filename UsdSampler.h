// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

enum class UsdSamplerComponents
{
  DATA = 0,
  WRAPS,
  WRAPT,
  WRAPR
};

struct UsdSamplerData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  double timeStep = 0.0;
  int timeVarying = 0; // Bitmask indicating which attributes are time-varying.

  const UsdDataArray* imageData = nullptr;

  UsdSharedString* inAttribute = nullptr; 
  UsdSharedString* imageUrl = nullptr;
  UsdSharedString* wrapS = nullptr;
  UsdSharedString* wrapT = nullptr;
  UsdSharedString* wrapR = nullptr;
};

class UsdSampler : public UsdBridgedBaseObject<UsdSampler, UsdSamplerData, UsdSamplerHandle, UsdSamplerComponents>
{
  protected:
    enum SamplerType
    {
      SAMPLER_UNKNOWN = 0,
      SAMPLER_1D,
      SAMPLER_2D,
      SAMPLER_3D
    };

  public:
    UsdSampler(const char* name, const char* type, UsdDevice* device);
    ~UsdSampler();

    void remove(UsdDevice* device) override;

    bool isPerInstance() const { return perInstance; }
    void updateBoundParameters(bool boundToInstance, const UsdSharedString*const* attribNames, size_t numAttribNames, UsdDevice* device);

    static constexpr ComponentPair componentParamNames[] = {
      ComponentPair(CType::DATA, "image"),
      ComponentPair(CType::DATA, "imageUrl"),
      ComponentPair(CType::WRAPS, "wrapMode"),
      ComponentPair(CType::WRAPS, "wrapMode1"),
      ComponentPair(CType::WRAPT, "wrapMode2"),
      ComponentPair(CType::WRAPR, "wrapMode3")};

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    void setSamplerTimeVarying(UsdBridgeSamplerData::DataMemberId& timeVarying);

    SamplerType samplerType = SAMPLER_UNKNOWN;

    bool perInstance = false; // Whether sampler is attached to a point instancer
    bool instanceAttributeAttached = false; // Whether a value to inAttribute has been set which in USD is different between per-instance and regular geometries
};