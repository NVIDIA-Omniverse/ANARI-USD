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
#include "UsdSharedObjects.h"
#include "UsdMultiTypeParameter.h"
#include "helium/utility/IntrusivePtr.h"
#include "anari/frontend/type_utility.h"

class UsdDevice;
class UsdBridge;
class UsdBaseObject;

// When deriving from UsdParameterizedObject<T>, define a a struct T::Data and
// a static void T::registerParams() that registers any member of T::Data using REGISTER_PARAMETER_MACRO()
template<typename T, typename D>
class UsdParameterizedObject
{
public:
  struct UsdAnariDataTypeStore
  {
    ANARIDataType type0 = ANARI_UNKNOWN;
    ANARIDataType type1 = ANARI_UNKNOWN;
    ANARIDataType type2 = ANARI_UNKNOWN;

    ANARIDataType singleType() const { return type0; }
    bool isMultiType() const { return type1 != ANARI_UNKNOWN; }
    bool typeMatches(ANARIDataType inType)  const
    {
      return inType == type0 ||
        (isMultiType() && (inType == type1 || inType == type2));
    }
  };

  struct ParamTypeInfo
  {
    size_t dataOffset = 0;  // offset of data, within paramDataSet D
    size_t typeOffset = 0;  // offset of type, from data
    size_t size = 0;        // Total size of data+type
    UsdAnariDataTypeStore types;
  };

  using ParameterizedClassType = UsdParameterizedObject<T, D>;
  using ParamContainer = std::map<std::string, ParamTypeInfo>;

  void* getParam(const char* name, ANARIDataType& returnType)
  {
    // Check if name registered
    typename ParamContainer::iterator it = registeredParams->find(name);
    if (it != registeredParams->end())
    {
      const ParamTypeInfo& typeInfo = it->second;

      void* destAddress = nullptr;
      getParamTypeAndAddress(paramDataSets[paramWriteIdx], typeInfo,
        returnType, destAddress);

      return destAddress;
    }

    return nullptr;
  }

protected:
  helium::RefCounted** ptrToRefCountedPtr(void* address) { return reinterpret_cast<helium::RefCounted**>(address); }
  UsdBaseObject* toBaseObjectPtr(void* address) { return *reinterpret_cast<UsdBaseObject**>(address); }
  ANARIDataType* toAnariDataTypePtr(void* address) { return reinterpret_cast<ANARIDataType*>(address); }

  bool isRefCounted(ANARIDataType type) const { return anari::isObject(type) || type == ANARI_STRING; }

  void safeRefInc(void* paramPtr, ANARIDataType paramType, bool onWriteParams) // Pointer to the parameter address which holds a helium::RefCounted*
  {
    helium::RefCounted** refCountedPP = ptrToRefCountedPtr(paramPtr);
    if (*refCountedPP)
    {
      (*refCountedPP)->refInc(helium::RefType::INTERNAL);

      if(anari::isObject(paramType))
        onParamRefChanged(toBaseObjectPtr(paramPtr), true, onWriteParams);
    }
  }

  void safeRefDec(void* paramPtr, ANARIDataType paramType, bool onWriteParams) // Pointer to the parameter address which holds a helium::RefCounted*
  {
    helium::RefCounted** refCountedPP = ptrToRefCountedPtr(paramPtr);
    if (*refCountedPP)
    {
      if(anari::isObject(paramType))
        onParamRefChanged(toBaseObjectPtr(paramPtr), false, onWriteParams);

      helium::RefCounted*& refCountedP = *refCountedPP;
#ifdef CHECK_MEMLEAKS
      logDeallocationThroughDevice(allocDevice, refCountedP, paramType);
#endif
      assert(refCountedP->useCount(helium::RefType::INTERNAL) > 0);
      refCountedP->refDec(helium::RefType::INTERNAL);
      refCountedP = nullptr; // Explicitly clear the pointer (see destructor)
    }
  }

  virtual void onParamRefChanged(UsdBaseObject* paramObject, bool incRef, bool onWriteParams) {}

  void* paramAddress(D& paramData, const ParamTypeInfo& typeInfo)
  {
    return reinterpret_cast<char*>(&paramData) + typeInfo.dataOffset;
  }

  ANARIDataType paramType(void* paramAddress, const ParamTypeInfo& typeInfo)
  {
    if(typeInfo.types.isMultiType())
      return *toAnariDataTypePtr(static_cast<char*>(paramAddress) + typeInfo.typeOffset);
    else
      return typeInfo.types.singleType();
  }

  void setMultiParamType(void* paramAddress, const ParamTypeInfo& typeInfo, ANARIDataType newType)
  {
    if(typeInfo.types.isMultiType())
      *toAnariDataTypePtr(static_cast<char*>(paramAddress) + typeInfo.typeOffset) = newType;
  }

  void getParamTypeAndAddress(D& paramData, const ParamTypeInfo& typeInfo,
        ANARIDataType& returnType, void*& returnAddress)
  {
    returnAddress = paramAddress(paramData, typeInfo);
    returnType = paramType(returnAddress, typeInfo);
  }

  // Convenience function for usd-compatible parameters
  void formatUsdName(UsdSharedString* nameStr)
  {
    char* name = const_cast<char*>(UsdSharedString::c_str(nameStr));
    assert(strlen(name) > 0);

    auto letter = [](unsigned c) { return ((c - 'A') < 26) || ((c - 'a') < 26); };
    auto number = [](unsigned c) { return (c - '0') < 10; };
    auto under = [](unsigned c) { return c == '_'; };

    unsigned x = *name;
    if (!letter(x) && !under(x)) { *name = '_'; }
    x = *(++name);
    while (x != '\0')
    {
      if(!letter(x) && !number(x) && !under(x))
        *name = '_';
      x = *(++name);
    };
  }

public:
  UsdParameterizedObject()
  {
    static ParamContainer* reg = ParameterizedClassType::registerParams();
    registeredParams = reg;
  }

  ~UsdParameterizedObject()
  {
    // Manually decrease the references on all objects in the read and writeparam datasets
    // (since the pointers are relinquished)
    auto it = registeredParams->begin();
    while (it != registeredParams->end())
    {
      const ParamTypeInfo& typeInfo = it->second;

      ANARIDataType readParamType, writeParamType;
      void* readParamAddress = nullptr;
      void* writeParamAddress = nullptr;

      getParamTypeAndAddress(paramDataSets[paramReadIdx], typeInfo,
        readParamType, readParamAddress);
      getParamTypeAndAddress(paramDataSets[paramWriteIdx], typeInfo,
        writeParamType, writeParamAddress);

      // Works even if two parameters point to the same paramdata address, as the content at that address (object pointers) are set to null
      if(isRefCounted(readParamType))
        safeRefDec(readParamAddress, readParamType, false);
      if(isRefCounted(writeParamType))
        safeRefDec(writeParamAddress, writeParamType, true);

      ++it;
    }
  }

  const D& getReadParams() const { return paramDataSets[paramReadIdx]; }
  D& getWriteParams() { return paramDataSets[paramWriteIdx]; }

protected:

  void setParam(const char* name, ANARIDataType srcType, const void* rawSrc, UsdDevice* device)
  {
#ifdef CHECK_MEMLEAKS
    allocDevice = device;
#endif

    if(srcType == ANARI_UNKNOWN)
    {
      reportStatusThroughDevice(UsdLogInfo(device, this, ANARI_OBJECT, nullptr), ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
          "Attempting to set param %s with type %s", name, AnariTypeToString(srcType));
      return;
    }
    else if(anari::isArray(srcType))
    {
      // Flatten the source type in case of array
      srcType = ANARI_ARRAY;
    }

    // Check if name registered
    typename ParamContainer::iterator it = registeredParams->find(name);
    if (it != registeredParams->end())
    {
      const ParamTypeInfo& typeInfo = it->second;

      // Check if type matches
      if (typeInfo.types.typeMatches(srcType))
      {
        ANARIDataType destType;
        void* destAddress = nullptr;
        getParamTypeAndAddress(paramDataSets[paramWriteIdx], typeInfo,
          destType, destAddress);

        const void* srcAddress = rawSrc; //temporary src
        size_t numBytes = anari::sizeOf(srcType); // Size is determined purely by source data

        bool contentUpdate = srcType != destType; // Always do a content update if types differ (in case of multitype params)

        // Update data for all the different types
        UsdSharedString* sharedStr = nullptr;
        if (srcType == ANARI_STRING)
        {
          // Wrap strings to make them refcounted,
          // from that point they are considered normal RefCounteds.
          UsdSharedString* destStr = reinterpret_cast<UsdSharedString*>(*ptrToRefCountedPtr(destAddress));
          const char* srcCstr = reinterpret_cast<const char*>(srcAddress);

          contentUpdate = contentUpdate || !destStr || !strEquals(destStr->c_str(), srcCstr); // Note that execution of strEquals => (srcType == destType)

          if(contentUpdate)
          {
            sharedStr = new UsdSharedString(srcCstr); // Remember to refdec
            numBytes = sizeof(void*);
            srcAddress = &sharedStr;
#ifdef CHECK_MEMLEAKS
            logAllocationThroughDevice(allocDevice, sharedStr, ANARI_STRING);
#endif
          }
        }
        else
          contentUpdate = contentUpdate || bool(std::memcmp(destAddress, srcAddress, numBytes));

        if(contentUpdate)
        {
          if(isRefCounted(destType))
            safeRefDec(destAddress, destType, true);

          std::memcpy(destAddress, srcAddress, numBytes);

          if(isRefCounted(srcType))
            safeRefInc(destAddress, srcType, true);
        }

        // If a string object has been created, decrease its public refcount (1 at creation)
        if (sharedStr)
        {
          assert(sharedStr->useCount() == 2); // Single public and internal reference
          sharedStr->refDec(helium::RefType::PUBLIC);
        }

        // Update the type for multitype params (so far only data has been updated)
        if(contentUpdate)
          setMultiParamType(destAddress, typeInfo, srcType);

        if(!strEquals(name, "usd::time")) // Allow for re-use of object as reference at different timestep, without triggering a full re-commit of the referenced object
        {
#ifdef TIME_BASED_CACHING
          paramChanged = true; //For time-varying parameters, comparisons between content of potentially different timesteps is meaningless
#else
          paramChanged = paramChanged || contentUpdate;
#endif
        }
      }
      else
        reportStatusThroughDevice(UsdLogInfo(device, this, ANARI_OBJECT, nullptr), ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
          "Param %s is not of an accepted type. For example, use %s instead.", name, AnariTypeToString(typeInfo.types.type0));
    }
  }

  void resetParam(const ParamTypeInfo& typeInfo)
  {
    size_t paramSize = typeInfo.size;

    // Copy to existing write param location
    ANARIDataType destType;
    void* destAddress = nullptr;
    getParamTypeAndAddress(paramDataSets[paramWriteIdx], typeInfo,
      destType, destAddress);

    // Create temporary default-constructed parameter set and find source param address ((multi-)type is of no concern)
    D defaultParamData;
    void* srcAddress = paramAddress(defaultParamData, typeInfo);

    // Make sure to dec existing ptr, as it will be relinquished
    if(isRefCounted(destType))
      safeRefDec(destAddress, destType, true);

    // Just replace contents of the whole parameter structure, single or multiparam
    std::memcpy(destAddress, srcAddress, paramSize);
  }

  void resetParam(const char* name)
  {
    typename ParamContainer::iterator it = registeredParams->find(name);
    if (it != registeredParams->end())
    {
      const ParamTypeInfo& typeInfo = it->second;
      resetParam(typeInfo);

      if(!strEquals(name, "usd::time"))
      {
        paramChanged = true;
      }
    }
  }

  void resetParams()
  {
    auto it = registeredParams->begin();
    while (it != registeredParams->end())
    {
      resetParam(it->second);
      ++it;
    }
    paramChanged = true;
  }

  void transferWriteToReadParams()
  {
    // Make sure object references are removed for
    // the overwritten readparams, and increased for the source writeparams
    auto it = registeredParams->begin();
    while (it != registeredParams->end())
    {
      const ParamTypeInfo& typeInfo = it->second;

      ANARIDataType srcType, destType;
      void* srcAddress = nullptr;
      void* destAddress = nullptr;

      getParamTypeAndAddress(paramDataSets[paramWriteIdx], typeInfo,
        srcType, srcAddress);
      getParamTypeAndAddress(paramDataSets[paramReadIdx], typeInfo,
        destType, destAddress);

      // SrcAddress and destAddress are not the same, but they may specifically contain the same object pointers.
      // Branch out on that situation (may also be disabled, which results in a superfluous inc/dec)
      if(std::memcmp(destAddress, srcAddress, typeInfo.size))
      {
        // First inc, then dec (in case branch is taken out and the pointed to object is the same)
        if (isRefCounted(srcType))
          safeRefInc(srcAddress, srcType, false); // count as increase of readparams ref due to subsequent copy
        if (isRefCounted(destType))
          safeRefDec(destAddress, destType, false);

        // Perform assignment immediately; there may be multiple parameters with the same object target,
        // which will be branched out at the compare the second time around
        std::memcpy(destAddress, srcAddress, typeInfo.size);
      }

      ++it;
    }
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
  registeredParams.emplace( std::make_pair<std::string, ParamTypeInfo>( \
    std::string(ParamName), \
    {offsetof(DataType, ParamData), 0, sizeof(DataType::ParamData), {ParamType, ANARI_UNKNOWN, ANARI_UNKNOWN}} \
  )); \
  static_assert(AssertParamDataType<decltype(DataType::ParamData), ParamType>::value, "ANARI type " #ParamType " of member '" #ParamData "' does not correspond to member type");

#define REGISTER_PARAMETER_MULTITYPE_MACRO(ParamName, ParamType0, ParamType1, ParamType2, ParamData) \
  { \
    using multitype_t = decltype(DataType::ParamData); \
    static_assert(AssertParamDataType<multitype_t::CDataType0, ParamType0>::value, "MultiTypeParams registration: ParamType0 " #ParamType0 " of member '" #ParamData "' doesn't match AnariType0"); \
    static_assert(AssertParamDataType<multitype_t::CDataType1, ParamType1>::value, "MultiTypeParams registration: ParamType1 " #ParamType1 " of member '" #ParamData "' doesn't match AnariType1"); \
    static_assert(AssertParamDataType<multitype_t::CDataType2, ParamType2>::value, "MultiTypeParams registration: ParamType2 " #ParamType2 " of member '" #ParamData "' doesn't match AnariType2"); \
    size_t dataOffset = offsetof(DataType, ParamData); \
    size_t typeOffset = offsetof(DataType, ParamData.type); \
    registeredParams.emplace( std::make_pair<std::string, ParamTypeInfo>( \
      std::string(ParamName), \
      {dataOffset, typeOffset - dataOffset, sizeof(DataType::ParamData), {ParamType0, ParamType1, ParamType2}} \
    )); \
  }

// Static assert explainer: gets the element type of the array via the decltype of *std::begin(), which in turn accepts an array
#define REGISTER_PARAMETER_ARRAY_MACRO(ParamName, ParamNameSuffix, ParamType, ParamData, NumEntries) \
  { \
    using element_type_t = std::remove_reference_t<decltype(*std::begin(std::declval<decltype(DataType::ParamData)&>()))>; \
    static_assert(AssertParamDataType<element_type_t, ParamType>::value, "ANARI type " #ParamType " of member '" #ParamData "' does not correspond to member type"); \
    static_assert(sizeof(decltype(DataType::ParamData)) == sizeof(element_type_t)*NumEntries, "Number of elements of member '" #ParamData "' does not correspond with member declaration."); \
    size_t offset0 = offsetof(DataType, ParamData[0]); \
    size_t offset1 = offsetof(DataType, ParamData[1]); \
    size_t paramSize = offset1-offset0; \
    for(int i = 0; i < NumEntries; ++i) \
    { \
      registeredParams.emplace( std::make_pair<std::string, ParamTypeInfo>( \
        ParamName + std::to_string(i) + ParamNameSuffix, \
        {offset0+paramSize*i, 0, paramSize, {ParamType, ANARI_UNKNOWN, ANARI_UNKNOWN}} \
      )); \
    } \
  }
