// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeTimeEvaluator_h
#define UsdBridgeTimeEvaluator_h

#include "usd.h"
PXR_NAMESPACE_USING_DIRECTIVE

template<class T>
class UsdBridgeTimeEvaluator
{
public:
  UsdBridgeTimeEvaluator(const T& data, double timeStep)
    : Data(data)
    , TimeCode(timeStep)
  {
  }

  UsdBridgeTimeEvaluator(const T& data)
    : Data(data)
  {
  }

  const UsdTimeCode& Eval(typename T::DataMemberId member) const
  {
#ifdef TIME_BASED_CACHING
    if ((Data.TimeVarying & member) != T::DataMemberId::NONE)
      return TimeCode;
    else
#endif
    return DefaultTime;
  }

  bool IsTimeVarying(typename T::DataMemberId member) const
  {
#ifdef TIME_BASED_CACHING
    return ((Data.TimeVarying & member) != T::DataMemberId::NONE);
#else
    return false;
#endif
  }

  UsdTimeCode Default() const { return DefaultTime; }

  const T& Data;
  const UsdTimeCode TimeCode;
  static const UsdTimeCode DefaultTime;
};

template<class T>
const UsdTimeCode UsdBridgeTimeEvaluator<T>::DefaultTime = UsdTimeCode::Default();

template<>
class UsdBridgeTimeEvaluator<bool>
{
public:
  UsdBridgeTimeEvaluator(bool timeVarying, double timeStep)
    : TimeVarying(timeVarying)
    , TimeCode(timeStep)
  {
  }

  const UsdTimeCode& Eval() const
  {
#ifdef TIME_BASED_CACHING
    if (TimeVarying)
      return TimeCode;
    else
#endif
    return DefaultTime;
  }

  UsdTimeCode Default() const { return DefaultTime; }

  const bool TimeVarying;
  const UsdTimeCode TimeCode;
  static const UsdTimeCode DefaultTime;
};

const UsdTimeCode UsdBridgeTimeEvaluator<bool>::DefaultTime = UsdTimeCode::Default();

#endif