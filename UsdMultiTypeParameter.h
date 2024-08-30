// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdAnari.h"
#include "anari/anari_cpp/Traits.h"

template<typename T0, typename T1, typename T2>
struct UsdMultiTypeParameter
{
  using CDataType0 = T0;
  using CDataType1 = T1;
  using CDataType2 = T2;

  union DataUnion
  {
    T0 type0;
    T1 type1;
    T2 type2;
  };

  DataUnion data;
  ANARIDataType type;

  // Helper functions
  T0& Get(T0& arg) const 
  { 
    if(AnariTypeMatchesCType<CDataType0>(type)) { arg = data.type0; }
    return arg; 
  }

  T1& Get(T1& arg) const 
  { 
    if(AnariTypeMatchesCType<CDataType1>(type)) { arg = data.type1; }
    return arg; 
  }

  T2& Get(T2& arg) const 
  { 
    if(AnariTypeMatchesCType<CDataType2>(type)) { arg = data.type2; }
    return arg; 
  }
};


