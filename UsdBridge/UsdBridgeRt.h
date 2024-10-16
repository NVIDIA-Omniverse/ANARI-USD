// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeRt_h
#define UsdBridgeRt_h

#include "UsdBridgeData.h"

class UsdBridgeRtInternals;

class UsdBridgeRt
{
  public:
    UsdBridgeRt(const ::PXR_NS::UsdStagePtr& sceneStage, const ::PXR_NS::SdfPath& primPath);
    ~UsdBridgeRt();

    void ReCalc();
    bool ValidPrim();

    // Update attrib (directly or via its UsdRt/Fabric equivalents) with contents of arrayData and optionally return a span.
    // If arrayData is a nullptr, the attribute backing memory will be allocated, but not written to (for which the user can employ the returned span).
    template<typename ReturnEltType = UsdBridgeNoneType>
    UsdBridgeSpanI<ReturnEltType>* UpdateUsdAttribute(const UsdBridgeLogObject& logObj, const void* arrayData, UsdBridgeType arrayDataType, size_t arrayNumElements,
      const ::PXR_NS::UsdAttribute& attrib, const ::PXR_NS::UsdTimeCode& timeCode, bool writeToAttrib);

  protected:

    UsdBridgeRtInternals* Internals = nullptr;
};

#endif