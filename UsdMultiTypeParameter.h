// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdAnari.h"
#include "anari/anari_cpp/Traits.h"

template<typename T0, typename T1, typename T2>
struct UsdMultiTypeParameter
{
  static constexpr int AnariType0 = anari::ANARITypeFor<T0>::value;
  static constexpr int AnariType1 = anari::ANARITypeFor<T1>::value;
  static constexpr int AnariType2 = anari::ANARITypeFor<T2>::value;

  union DataUnion
  {
    T0 type0;
    T1 type1;
    T2 type2;
  };

  DataUnion data;
  ANARIDataType type;

  // Helper functions
  template<typename ReturnType>
  ReturnType& Get(ReturnType&) const { static_assert(always_false<T>::value , "Get() for UsdMultiTypeParameter not matching any parameter type"); return nullptr; }

  template<>
  T0& Get<T0>(T0& arg) const 
  { 
    if(AnariType0 == type) { arg = data.type0; }
    return arg; 
  }

  template<>
  T1& Get<T1>(T1& arg) const 
  { 
    if(AnariType1 == type) { arg = data.type1; }
    return arg; 
  }

  template<>
  T2& Get<T2>(T2& arg) const 
  { 
    if(AnariType2 == type) { arg = data.type2; }
    return arg; 
  }
};


