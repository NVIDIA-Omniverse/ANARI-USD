// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "helium/utility/IntrusivePtr.h"
#include "UsdCommonMacros.h"
#include "UsdParameterizedObject.h"

class UsdDevice;

// Base parameterized class without being derived as such - nontemplated to allow for polymorphic use
class UsdBaseObject : public helium::RefCounted
{
  public:
    // If device != 0, the object is added to the commit list
    UsdBaseObject(ANARIDataType t, UsdDevice* device = nullptr);

    virtual void filterSetParam(
      const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) = 0;

    virtual void filterResetParam(
      const char *name) = 0;

    virtual void resetAllParams() = 0;

    virtual void* getParameter(const char* name, ANARIDataType& returnType) = 0;

    virtual int getProperty(const char *name,
      ANARIDataType type,
      void *mem,
      uint64_t size,
      UsdDevice* device) = 0;

    virtual void commit(UsdDevice* device) = 0;

    virtual void remove(UsdDevice* device) = 0; // Remove any committed data and refs

    ANARIDataType getType() const { return type; }

  protected:
    virtual bool deferCommit(UsdDevice* device) = 0;  // Returns whether data commit has to be deferred
    virtual bool doCommitData(UsdDevice* device) = 0; // Data commit, execution can be immediate, returns whether doCommitRefs has to be performed
    virtual void doCommitRefs(UsdDevice* device) = 0; // For updates with dependencies on referenced object's data, is always executed deferred

    ANARIDataType type;

    friend class UsdDevice;
};

// Templated base implementation of parameterized object
template<typename T, typename D>
class UsdParameterizedBaseObject : public UsdBaseObject, public UsdParameterizedObject<T, D>
{
  public:
    typedef UsdParameterizedObject<T, D> ParamClass;

    UsdParameterizedBaseObject(ANARIDataType t, UsdDevice* device = nullptr)
      : UsdBaseObject(t, device)
    {}

    void filterSetParam(
      const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override
    {
      ParamClass::setParam(name, type, mem, device);
    }

    void filterResetParam(
      const char *name) override
    {
      ParamClass::resetParam(name);
    }

    void resetAllParams() override
    {
      ParamClass::resetParams();
    }

    void* getParameter(const char* name, ANARIDataType& returnType) override
    {
      return ParamClass::getParam(name, returnType);
    }

    int getProperty(const char *name,
      ANARIDataType type,
      void *mem,
      uint64_t size,
      UsdDevice* device) override
    {
      return 0;
    }

    void commit(UsdDevice* device) override
    {
      ParamClass::transferWriteToReadParams();
      UsdBaseObject::commit(device);
    }

    // Convenience functions for commonly used name property
    virtual const char* getName() const { return ""; }

  protected:
    // Convenience functions for commonly used name property
    bool setNameParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device)
    {
      const char* objectName = static_cast<const char*>(mem);

      if (type == ANARI_STRING)
      {
        if (strEquals(name, "name"))
        {
          if (!objectName || strEquals(objectName, ""))
          {
            reportStatusThroughDevice(UsdLogInfo(device, this, ANARI_OBJECT, nullptr), ANARI_SEVERITY_WARNING, ANARI_STATUS_NO_ERROR,
              "%s: ANARI object %s cannot be an empty string, using auto-generated name instead.", getName(), "name");
          }
          else
          {
            ParamClass::setParam(name, type, mem, device);
            ParamClass::setParam("usd::name", type, mem, device);
            this->formatUsdName(this->getWriteParams().usdName);
          }
          return true;
        }
        else if (strEquals(name, "usd::name"))
        {
          reportStatusThroughDevice(UsdLogInfo(device, this, ANARI_OBJECT, nullptr), ANARI_SEVERITY_WARNING, ANARI_STATUS_NO_ERROR,
            "%s parameter '%s' cannot be set, only read with getProperty().", getName(), "usd::name");
          return true;
        }
      }
      return false;
    }

    int getNameProperty(const char *name,
      ANARIDataType type,
      void *mem,
      uint64_t size,
      UsdDevice* device)
    {
      if (type == ANARI_STRING && strEquals(name, "usd::name"))
      {
        snprintf((char*)mem, size, "%s", UsdSharedString::c_str(this->getReadParams().usdName));
        return 1;
      }
      else if (type == ANARI_UINT64 && strEquals(name, "usd::name.size"))
      {
        if (Assert64bitStringLengthProperty(size, UsdLogInfo(device, this, ANARI_ARRAY, this->getName()), "usd::name.size"))
        {
          uint64_t nameLen = this->getReadParams().usdName ? strlen(this->getReadParams().usdName->c_str())+1 : 0;
          memcpy(mem, &nameLen, size);
        }
        return 1;
      }
      return 0;
    }
};