// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "anari/detail/IntrusivePtr.h"
#include "UsdCommonMacros.h"
#include "UsdAnari.h"

#include <algorithm>

class UsdDevice;

class UsdBaseObject : public anari::RefCounted
{
  public:
    UsdBaseObject(ANARIDataType t)
      : type(t)
    {}

    virtual void filterSetParam(
      const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) = 0;

    virtual void filterResetParam(
      const char *name) = 0;

    virtual int getProperty(const char *name,
      ANARIDataType type,
      void *mem,
      uint64_t size,
      UsdDevice* device) = 0;

    virtual void commit(UsdDevice* device) = 0;

    ANARIDataType getType() const { return type; }

  protected:
    ANARIDataType type;
};