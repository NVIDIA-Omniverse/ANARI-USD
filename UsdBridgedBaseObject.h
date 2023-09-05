// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdParameterizedObject.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdDataArray.h"

#include <cmath>

class UsdDevice;

template<class T, class D, class H>
class UsdBridgedBaseObject : public UsdBaseObject, public UsdParameterizedObject<T, D>
{
  public:
    typedef UsdParameterizedObject<T, D> ParamClass;

    UsdBridgedBaseObject(ANARIDataType t, const char* name, UsdDevice* device)
      : UsdBaseObject(t, device)
      , uniqueName(name)
    {
    }

    H getUsdHandle() const { return usdHandle; }

    const char* getName() const { return this->getReadParams().usdName ? this->getReadParams().usdName->c_str() : uniqueName; }

    bool filterNameParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device)
    {
      const char* objectName = static_cast<const char*>(mem);

      if (type == ANARI_STRING)
      {
        if (strEquals(name, "name"))
        {
          if (strEquals(objectName, ""))
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
          return false;
        }
        else if (strEquals(name, "usd::name"))
        {
          reportStatusThroughDevice(UsdLogInfo(device, this, ANARI_OBJECT, nullptr), ANARI_SEVERITY_WARNING, ANARI_STATUS_NO_ERROR,
            "%s parameter '%s' cannot be set, only read with getProperty().", getName(), "usd::name");
          return false;
        }
      }
      return true;
    }

    void resetAllParams() override
    {
      resetParams();
    }

    void* tempGetParam(const char* name, ANARIDataType& returnType) override
    {
      return getParam(name, returnType);
    }

    int getProperty(const char *name,
      ANARIDataType type,
      void *mem,
      uint64_t size,
      UsdDevice* device) override
    {
      if (type == ANARI_STRING && strEquals(name, "usd::name"))
      {
        snprintf((char*)mem, size, "%s", UsdSharedString::c_str(this->getReadParams().usdName));
        return 1;
      }
      else if (type == ANARI_UINT64 && strEquals(name, "usd::name.size"))
      {
        if (Assert64bitStringLengthProperty(size, UsdLogInfo(device, this, ANARI_OBJECT, this->getName()), "usd::name.size"))
        {
          uint64_t nameLen = this->getReadParams().usdName ? strlen(this->getReadParams().usdName->c_str())+1 : 0;
          memcpy(mem, &nameLen, size);
        }
        return 1;
      }
      else
      {
        UsdBridge* usdBridge = device->getUsdBridge();
        if(!usdBridge)
        {
          reportStatusThroughDevice(UsdLogInfo(device, this, ANARI_OBJECT, nullptr), ANARI_SEVERITY_WARNING, ANARI_STATUS_NO_ERROR,
            "%s parameter '%s' cannot be read with getProperty(); it requires a succesful device parameter commit.", getName(), name);
        }

        if (type == ANARI_STRING && strEquals(name, "usd::primPath"))
        {
          const char* primPath = usdBridge->GetPrimPath(&usdHandle);
          snprintf((char*)mem, size, "%s", primPath);
          return 1;
        }
        else if (type == ANARI_UINT64 && strEquals(name, "usd::primPath.size"))
        {
          if (Assert64bitStringLengthProperty(size, UsdLogInfo(device, this, ANARI_OBJECT, this->getName()), "usd::primPath.size"))
          {
            const char* primPath = usdBridge->GetPrimPath(&usdHandle);
            uint64_t nameLen = strlen(primPath)+1;
            memcpy(mem, &nameLen, size);
          }
          return 1;
        }
      }
      return 0;
    }

    virtual void commit(UsdDevice* device) override
    {
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
      cachedBridge = device->getUsdBridge();
#endif
      this->transferWriteToReadParams();
      UsdBaseObject::commit(device);
    }

    double selectObjTime(double objTimeStep, double worldTimeStep)
    {
      return
#ifdef VALUE_CLIP_RETIMING
        !std::isnan(objTimeStep) ? objTimeStep :
#endif
        worldTimeStep;
    }

    double selectRefTime(double refTimeStep, double objTimeStep, double worldTimeStep)
    {
      return
  #ifdef VALUE_CLIP_RETIMING
        !std::isnan(refTimeStep) ? refTimeStep :
          (!std::isnan(objTimeStep) ? objTimeStep : worldTimeStep);
  #else
        worldTimeStep;
  #endif
    }

  protected:
    typedef UsdBridgedBaseObject<T,D,H> BridgedBaseObjectType;

    const char* uniqueName;
    H usdHandle;
    
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
    UsdBridge* cachedBridge = nullptr;
#endif
};

template<class T, class D, class H>
inline bool UsdObjectNotInitialized(const UsdBridgedBaseObject<T,D,H>* obj)
{
  return obj && !obj->getUsdHandle().value;
}

template<class T>
inline bool UsdObjectNotInitialized(UsdDataArray* objects)
{
  if (!objects)
    return false;

  bool notInitialized = false;
  if(anari::isObject(objects->getType()))
  {
    const T* const * object = reinterpret_cast<const T* const *>(objects->getData());
    uint64_t numObjects = objects->getLayout().numItems1;
    for(int i = 0; i < numObjects; ++i)
    {
      notInitialized = notInitialized || UsdObjectNotInitialized(object[i]);
    }
  }

  return notInitialized;
}