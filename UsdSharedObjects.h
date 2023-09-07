// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "helium/utility/IntrusivePtr.h"
#include "UsdCommonMacros.h"

#include <string>

template<typename BaseType, typename InitType>
class UsdRefCountWrapped : public helium::RefCounted
{
  public:
    UsdRefCountWrapped(InitType i)
        : data(i)
    {}

    BaseType data;
};

class UsdSharedString : public UsdRefCountWrapped<std::string, const char*>
{
  public:
    UsdSharedString(const char* cStr)
      : UsdRefCountWrapped<std::string, const char*>(cStr)
    {}

    static const char* c_str(const UsdSharedString* string) { return string ? string->c_str() : nullptr; }
    const char* c_str() const { return data.c_str(); }
};