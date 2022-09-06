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
#include "anari/backend/utilities/IntrusivePtr.h"
#include "anari/type_utility.h"
#include "anari/anari_cpp/Traits.h"

class UsdDevice;
class UsdBridge;

// When deriving from UsdParameterizedObject<T>, define a a struct T::Data and
// a static void T::registerParams() that registers any member of T::Data using REGISTER_PARAMETER_MACRO()
template<class T, class D>
class UsdParameterizedObject
{
protected:
  bool isBaseObject(ANARIDataType type) const { return anari::isObject(type) || type == ANARI_STRING; }

  void safeRefInc(char* paramPtr) // Pointer to the parameter address which holds a UsdBaseObject*
  {
    UsdBaseObject** baseObj = reinterpret_cast<UsdBaseObject**>(paramPtr);
    if (*baseObj)
      (*baseObj)->refInc(anari::RefType::INTERNAL);
  }

  void safeRefDec(char* paramPtr) // Pointer to the parameter address which holds a UsdBaseObject*
  {
    UsdBaseObject** baseObj = reinterpret_cast<UsdBaseObject**>(paramPtr);
    if (*baseObj)
    {
#ifdef CHECK_MEMLEAKS
      allocDevice->LogDeallocation(*baseObj);
#endif
      (*baseObj)->refDec(anari::RefType::INTERNAL);
      *baseObj = nullptr; // Explicitly clear the pointer
    }
  }

  bool objectMatches(char* src, char* dest)
  {
    UsdBaseObject** srcObj = reinterpret_cast<UsdBaseObject**>(src);
    UsdBaseObject** destObj = reinterpret_cast<UsdBaseObject**>(dest);
    return *srcObj == *destObj;
  }

  char* paramAddress(D& paramData, size_t offset)
  {
    return reinterpret_cast<char*>(&paramData) + offset;
  }

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
    // which have been set in setParam (and copied over in transferWriteToReadParams)
    auto it = registeredParams->begin();
    while (it != registeredParams->end())
    {
      ANARIDataType type = it->second.second;

      if (isBaseObject(type))
      {
        char* readParam = paramAddress(paramDataSets[paramReadIdx], it->second.first);
        char* writeParam = paramAddress(paramDataSets[paramWriteIdx], it->second.first);

        safeRefDec(readParam);
        safeRefDec(writeParam);
      }

      ++it;
    }
  }

  const D& getReadParams() const { return paramDataSets[paramReadIdx]; }
  D& getWriteParams() { return paramDataSets[paramWriteIdx]; }

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
        char* dest = paramAddress(paramDataSets[paramWriteIdx], it->second.first);
        UsdBaseObject** baseObj = reinterpret_cast<UsdBaseObject**>(dest);

        const void* src = rawSrc; //temporary src
        size_t numBytes = AnariTypeSize(type);

        bool contentUpdate = true;

        if (type == ANARI_BOOL)
        {
          bool* destBool_p = reinterpret_cast<bool*>(dest);
          bool srcBool = *(reinterpret_cast<const uint32_t*>(src));

          contentUpdate =  (*destBool_p != srcBool);

          *destBool_p = srcBool;
        }
        else
        {
          UsdSharedString* sharedStr = nullptr;
          if (type == ANARI_STRING)
          {
            // Wrap strings to make them refcounted,
            // from that point they are considered normal UsdBaseObjects.
            UsdSharedString* destStr = reinterpret_cast<UsdSharedString*>(*baseObj);
            const char* srcCstr = reinterpret_cast<const char*>(src);

            contentUpdate = !destStr || std::strcmp(destStr->c_str(), srcCstr) != 0;

            if(contentUpdate)
            {
              sharedStr = new UsdSharedString(srcCstr);
              numBytes = sizeof(void*);
              src = &sharedStr;

#ifdef CHECK_MEMLEAKS
              allocDevice->LogAllocation(sharedStr);
#endif
            }
          }
          else
            contentUpdate = bool(memcmp(dest, src, numBytes));

          if(contentUpdate)
          {
            if(isBaseObject(type))
              safeRefDec(dest);

            std::memcpy(dest, src, numBytes);

            if(isBaseObject(type))
              safeRefInc(dest);
          }

          // If a string object has been created, decrease its public refcount (1 at creation)
          if (sharedStr)
          {
            assert(sharedStr->useCount() == 2); // Single public and internal reference
            sharedStr->refDec();
          }
        }

        if(!strcmp(name, "usd::timestep")) // Allow for re-use of object as reference at different timestep, without triggering a full re-commit of the referenced object
        {
#ifdef TIME_BASED_CACHING
          paramChanged = true; //For time-varying parameters, comparisons between content of potentially different timesteps is meaningless
#else
          paramChanged = paramChanged || contentUpdate;
#endif
        }
      }
      else
        reportStatusThroughDevice(LogInfo(device, this, ANARI_OBJECT, nullptr), ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
          "Param %s should be of type %s", name, AnariTypeToString(it->second.second));
    }
  }

  void resetParam(const char* name)
  {
    ParamContainer::iterator it = registeredParams->find(name);
    if (it != registeredParams->end())
    {
      ANARIDataType type = it->second.second;
      char* dest = paramAddress(paramDataSets[paramWriteIdx], it->second.first);

      D defaultParamData;
      char* src = paramAddress(defaultParamData, it->second.first);

      if (type == ANARI_BOOL)
      {
        *(reinterpret_cast<bool*>(dest)) = *(reinterpret_cast<const bool*>(src));
      }
      else
      {
        if(isBaseObject(type))
          safeRefDec(dest);

        std::memcpy(dest, src, AnariTypeSize(type));
      }

      if(!strcmp(name, "usd::timestep")) 
      {
        paramChanged = true;
      }
    }
  }

  void TransferWriteToReadParams()
  {
    // Make sure object references are removed for
    // the overwritten readparams, and increased for the source writeparams (only if different)
    auto it = registeredParams->begin();
    while (it != registeredParams->end())
    {
      ANARIDataType type = it->second.second;

      char* src = paramAddress(paramDataSets[paramWriteIdx], it->second.first);
      char* dest = paramAddress(paramDataSets[paramReadIdx], it->second.first);

      if (isBaseObject(type) && !objectMatches(src, dest))
      {
        safeRefDec(dest);
        safeRefInc(src);
      }

      ++it;
    }

    // Pod datatype, so directly assign the structs
    paramDataSets[paramReadIdx] = paramDataSets[paramWriteIdx];
  }

  static ParamContainer* registerParams();

  ParamContainer* registeredParams;

  typedef T DerivedClassType;
  typedef D DataType;

  D paramDataSets[2];
  constexpr static unsigned int paramReadIdx = 0;
  constexpr static unsigned int paramWriteIdx = 1;
  bool paramChanged = false;

#ifdef CHECK_MEMLEAKS
  UsdDevice* allocDevice = nullptr;
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
