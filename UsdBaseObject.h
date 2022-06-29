// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "anari/detail/IntrusivePtr.h"
#include "UsdCommonMacros.h"
#include "UsdAnari.h"

#include <algorithm>
#include <string>

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
    virtual bool deferCommit(UsdDevice* device) = 0;  // Returns whether data commit has to be deferred
    virtual bool doCommitData(UsdDevice* device) = 0; // Data commit, execution can be immediate, returns whether doCommitRefs has to be performed
    virtual void doCommitRefs(UsdDevice* device) = 0; // For updates with dependencies on referenced object's data, is always executed deferred 

    ANARIDataType type;

    friend class UsdDevice;
};

template<typename BaseType, typename InitType>
class UsdRefCountWrapped : public UsdBaseObject
{
  public:
    UsdRefCountWrapped(ANARIDataType t, InitType i)
        : UsdBaseObject(t)
        , Data(i)
    {}

    // We should split the refcounted+type into its own class instead of overriding
    // functions that are meaningless
    virtual void filterSetParam(
      const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override {}

    virtual void filterResetParam(
      const char *name) override {}

    virtual int getProperty(const char *name,
      ANARIDataType type,
      void *mem,
      uint64_t size,
      UsdDevice* device) override { return 0; }

    void commit(UsdDevice* device) override {}  

    BaseType Data;

  protected:
    virtual bool deferCommit(UsdDevice* device) { return false; }
    virtual bool doCommitData(UsdDevice* device) { return false; } 
    virtual void doCommitRefs(UsdDevice* device) {}
};

class UsdSharedString : public UsdRefCountWrapped<std::string, const char*>
{
  public:
    UsdSharedString(const char* cStr)
      : UsdRefCountWrapped<std::string, const char*>(ANARI_STRING, cStr)
    {}

    static const char* c_str(const UsdSharedString* string) { return string ? string->c_str() : nullptr; }
    const char* c_str() const { return Data.c_str(); }
};