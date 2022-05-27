// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <map>
#include <string>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <vector>
#include "UsdAnari.h"
#include "UsdBaseObject.h"
#include "anari/detail/IntrusivePtr.h"
#include "anari/type_utility.h"
#include "anari/anari_cpp/Traits.h"

class UsdDevice;
class UsdBridge;

// When deriving from UsdParameterizedObject<T>, define a a struct T::Data and
// a static void T::registerParams() that registers any member of T::Data using REGISTER_PARAMETER_MACRO()
template<class T, class D>
class UsdParameterizedObject
{
public:

  typedef UsdParameterizedObject<T, D> ParameterizedClassType;
  typedef std::map<std::string, std::pair<size_t, ANARIDataType>> ParamContainer;

  UsdParameterizedObject()
  {
    static ParamContainer* reg = ParameterizedClassType::registerParams();
    registeredParams = reg;
  }

  ~UsdParameterizedObject()
  {
    // Make sure the data is automatically decreasing the references
    // which have been set in setParam
    auto it = registeredParams->begin();
    while (it != registeredParams->end())
    {
      ANARIDataType type = it->second.second;

      char* dest = (reinterpret_cast<char*>(&paramData) + it->second.first);

      if (type == ANARI_STRING)
      {
        char** destStr = reinterpret_cast<char**>(dest);
#ifdef CHECK_MEMLEAKS
        LogDeallocation(*destStr);
#endif
        delete[] *destStr;
      }
      else if (anari::isObject(type))
      {
        UsdBaseObject** baseObj = reinterpret_cast<UsdBaseObject**>(dest);
        if (*baseObj)
        {
#ifdef CHECK_MEMLEAKS
          allocDevice->LogDeallocation(*baseObj);
#endif
          (*baseObj)->refDec(anari::RefType::INTERNAL);
          *baseObj = nullptr;
        }
      }

      ++it;
    }

#ifdef CHECK_MEMLEAKS
    assert(allocatedStrings.empty());
#endif
  }

  const D& getParams() const { return paramData; }

protected:

  void setParam(const char* name, ANARIDataType type, const void* rawSrc, UsdDevice* device)
  {
#ifdef CHECK_MEMLEAKS
    allocDevice = device;
#endif

    // Check if name registered
    ParamContainer::iterator it = registeredParams->find(name);
    if (it != registeredParams->end())
    {
      // Check if type matches
      if (type == it->second.second ||
        (it->second.second == ANARI_ARRAY
        && ( type == ANARI_ARRAY1D
          || type == ANARI_ARRAY2D
          || type == ANARI_ARRAY3D)))
      {
        char* dest = (reinterpret_cast<char*>(&paramData) + it->second.first);
        UsdBaseObject** baseObj = reinterpret_cast<UsdBaseObject**>(dest);

        const char* src = static_cast<const char*>(rawSrc);
        size_t numBytes = AnariTypeSize(type);

        if (type == ANARI_BOOL)
        {
          *(reinterpret_cast<bool*>(dest)) = *(reinterpret_cast<const uint32_t*>(src));
        }
        else
        {
          if (type == ANARI_STRING)
          {
            char** destStr = reinterpret_cast<char**>(dest);
            numBytes = strlen(src) + 1;

#ifdef CHECK_MEMLEAKS
            LogDeallocation(*destStr);
#endif
            delete[] * destStr;
            *destStr = new char[numBytes];
            dest = *destStr;

#ifdef CHECK_MEMLEAKS
            LogAllocation(dest);
#endif
          }
          else if (anari::isObject(type) && *baseObj)
          {
#ifdef CHECK_MEMLEAKS
            allocDevice->LogDeallocation(*baseObj);
#endif
            (*baseObj)->refDec(anari::RefType::INTERNAL);
          }

#ifdef TIME_BASED_CACHING
          paramChanged = true; //For time-varying parameters, comparisons between content of potentially different timesteps is meaningless
#else
          paramChanged = paramChanged || bool(memcmp(dest, src, numBytes));
#endif
          std::memcpy(dest, src, numBytes);

          if (anari::isObject(type) && *baseObj)
            (*baseObj)->refInc(anari::RefType::INTERNAL);
        }
      }
      else
        reportStatusThroughDevice(device, this, ANARI_OBJECT, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
          "Param %s should be of type %s", name, AnariTypeToString(it->second.second));
    }
  }

  void resetParam(const char* name)
  {
    ParamContainer::iterator it = registeredParams->find(name);
    if (it != registeredParams->end())
    {
      ANARIDataType type = it->second.second;
      char* dest = (reinterpret_cast<char*>(&paramData) + it->second.first);

      if (type == ANARI_STRING)
      {
        char** destStr = reinterpret_cast<char**>(dest);
#ifdef CHECK_MEMLEAKS
        LogDeallocation(*destStr);
#endif
        delete[] *destStr;
        *destStr = nullptr;
      }
      else
      {
        D defaultParamData;
        char* src = (reinterpret_cast<char*>(&defaultParamData) + it->second.first);

        if (type == ANARI_BOOL)
        {
          *(reinterpret_cast<bool*>(dest)) = *(reinterpret_cast<const bool*>(src));
        }
        else
        {
          if (anari::isObject(type))
          {
            UsdBaseObject** baseObj = reinterpret_cast<UsdBaseObject**>(dest);
            if (*baseObj)
            {
#ifdef CHECK_MEMLEAKS
              allocDevice->LogDeallocation(*baseObj);
#endif
              (*baseObj)->refDec(anari::RefType::INTERNAL);
            }
          }

          std::memcpy(dest, src, AnariTypeSize(type));
        }
      }

      paramChanged = true;
    }
  }

  static ParamContainer* registerParams();

  ParamContainer* registeredParams;

  typedef T DerivedClassType;
  typedef D DataType;

  D paramData;
  bool paramChanged = false;

#ifdef CHECK_MEMLEAKS
  // Memleak checking
  void LogAllocation(const char* ptr) { allocatedStrings.push_back(ptr); }
  void LogDeallocation(const char* ptr)
  {
    if (ptr)
    {
      auto it = std::find(allocatedStrings.begin(), allocatedStrings.end(), ptr);
      assert(it != allocatedStrings.end());
      allocatedStrings.erase(it);
    }
  }
  std::vector<const char*> allocatedStrings;
  UsdDevice* allocDevice;
#endif
};

#define DEFINE_PARAMETER_MAP(DefClass, Params) template<> UsdParameterizedObject<DefClass,DefClass::DataType>::ParamContainer* UsdParameterizedObject<DefClass,DefClass::DataType>::registerParams() { static ParamContainer registeredParams; Params return &registeredParams; }

#define REGISTER_PARAMETER_MACRO(ParamName, ParamType, ParamData) \
  registeredParams.emplace( \
    ParamName, \
    std::make_pair<size_t, ANARIDataType>(offsetof(DataType, ParamData), ParamType) \
  );

#define REGISTER_PARAMETER_ARRAY_MACRO(ParamName, ParamType, ParamData, NumEntries) \
  { size_t offset0 = offsetof(DataType, ParamData[0]); \
    size_t offset1 = offsetof(DataType, ParamData[1]); \
    for(int i = 0; i < NumEntries; ++i) \
    { \
      std::string ParamNameWithIndex = ParamName + std::to_string(i); \
      registeredParams.emplace( \
        ParamNameWithIndex, \
        std::make_pair<size_t, ANARIDataType>(offset0+(offset1-offset0)*i, ParamType) \
      ); \
    } \
  }
