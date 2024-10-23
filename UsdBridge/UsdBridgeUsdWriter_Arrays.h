// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeUsdWriterArrays_h
#define UsdBridgeUsdWriterArrays_h

#include "UsdBridgeData.h"
#include "UsdBridgeUtils.h"

#include <string>
#include <sstream>
#include <algorithm>
#include <functional>
#include <type_traits>

#if defined(USE_USDRT) && defined(USE_USDRT_ELTTYPE)
#define USDBRIDGE_ARRAYTYPE_ELEMENTTYPE using ElementType = typename ArrayType::element_type;
#else
#define USDBRIDGE_ARRAYTYPE_ELEMENTTYPE using ElementType = typename ArrayType::ElementType;
#endif

namespace UsdBridgeTypeTraits
{

  DEFINE_USDBRIDGETYPE_SCALARTYPE(HALF, GfHalf)
  DEFINE_USDBRIDGETYPE_SCALARTYPE(HALF2, GfHalf)
  DEFINE_USDBRIDGETYPE_SCALARTYPE(HALF3, GfHalf)
  DEFINE_USDBRIDGETYPE_SCALARTYPE(HALF4, GfHalf)

  template<typename Type>
  struct UsdBaseToBridgeType
  {};
  #define USDBASE_TO_USDBRIDGE_TYPE(TypeUsdBase, TypeBridge)\
    template<>\
    struct UsdBaseToBridgeType<TypeUsdBase>\
    {\
      static constexpr UsdBridgeType Type = TypeBridge;\
    };
  USDBASE_TO_USDBRIDGE_TYPE(bool, UsdBridgeType::BOOL);
  USDBASE_TO_USDBRIDGE_TYPE(uint8_t, UsdBridgeType::UCHAR);
  USDBASE_TO_USDBRIDGE_TYPE(int32_t, UsdBridgeType::INT);
  USDBASE_TO_USDBRIDGE_TYPE(uint32_t, UsdBridgeType::UINT);
  USDBASE_TO_USDBRIDGE_TYPE(int64_t, UsdBridgeType::LONG);
  USDBASE_TO_USDBRIDGE_TYPE(uint64_t, UsdBridgeType::ULONG);
  USDBASE_TO_USDBRIDGE_TYPE(GfHalf, UsdBridgeType::HALF);
  USDBASE_TO_USDBRIDGE_TYPE(float, UsdBridgeType::FLOAT);
  USDBASE_TO_USDBRIDGE_TYPE(double, UsdBridgeType::DOUBLE);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec2i, UsdBridgeType::INT2);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec2h, UsdBridgeType::HALF2);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec2f, UsdBridgeType::FLOAT2);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec2d, UsdBridgeType::DOUBLE2);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec3i, UsdBridgeType::INT3);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec3h, UsdBridgeType::HALF3);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec3f, UsdBridgeType::FLOAT3);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec3d, UsdBridgeType::DOUBLE3);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec4i, UsdBridgeType::INT4);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec4h, UsdBridgeType::HALF4);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec4f, UsdBridgeType::FLOAT4);
  USDBASE_TO_USDBRIDGE_TYPE(GfVec4d, UsdBridgeType::DOUBLE4);
  USDBASE_TO_USDBRIDGE_TYPE(GfQuath, UsdBridgeType::HALF4);
  USDBASE_TO_USDBRIDGE_TYPE(GfQuatf, UsdBridgeType::FLOAT4);
  USDBASE_TO_USDBRIDGE_TYPE(GfQuatd, UsdBridgeType::DOUBLE4);

  template<typename PxrEltType>
  struct PxrToRtEltType
  {
    using Type = std::enable_if_t<std::is_scalar_v<PxrEltType>, PxrEltType>;
  };
  #define PXR_ELTTYPE_TO_RT_ELTTYPE(TypePxrElt, TypeRtElt)\
    template<>\
    struct PxrToRtEltType<TypePxrElt>\
    {\
      using Type = TypeRtElt;\
    };
  PXR_ELTTYPE_TO_RT_ELTTYPE(UsdBridgeNoneType, UsdBridgeNoneType)
  PXR_ELTTYPE_TO_RT_ELTTYPE(::PXR_NS::GfVec3f, GfVec3f)
  PXR_ELTTYPE_TO_RT_ELTTYPE(::PXR_NS::GfVec4f, GfVec4f)
  PXR_ELTTYPE_TO_RT_ELTTYPE(::PXR_NS::GfQuath, GfQuath)
}

namespace
{ //internal linkage

namespace UsdBridgeArrays
{
  template<typename T0, typename T1>
  struct CanCopyType
  {
    static constexpr bool Value = std::is_same<T0, T1>::value;
  };
  #define DEFINE_CANCOPY_SIGNED_UNSIGNED(Type0, Type1)\
    template<>\
    struct CanCopyType<Type0, Type1>\
    {\
      static constexpr bool Value = true;\
    };\
    template<>\
    struct CanCopyType<Type1, Type0>\
    {\
      static constexpr bool Value = true;\
    };
  DEFINE_CANCOPY_SIGNED_UNSIGNED(char, unsigned char)
  DEFINE_CANCOPY_SIGNED_UNSIGNED(short, unsigned short)
  DEFINE_CANCOPY_SIGNED_UNSIGNED(int, unsigned int)
  DEFINE_CANCOPY_SIGNED_UNSIGNED(int8_t, uint8_t)
  DEFINE_CANCOPY_SIGNED_UNSIGNED(int64_t, uint64_t)

  template<typename ArrayType>
  ArrayType& GetStaticTempArray(size_t numElements)
  {
    thread_local static ArrayType array;
    array.resize(numElements);
    return array;
  }

  template<typename SpanInitType, template <typename T> class SpanI, typename AttributeCType>
  SpanI<AttributeCType>& GetStaticSpan(SpanInitType& destSpanInit)
  {
    thread_local static SpanI<AttributeCType> destSpan(destSpanInit);
    destSpan = SpanI<AttributeCType>(destSpanInit);
    return destSpan;
  }

  template<typename ElementType>
  void WriteToSpanCopy(const void* data, UsdBridgeSpanI<ElementType>& destSpan)
  {
    ElementType* typedData = (ElementType*)data;
    if(destSpan.size() > 0)
      std::copy(typedData, typedData+destSpan.size(), destSpan.begin());
  }

  template<typename DestType, typename SourceType>
  void WriteToSpanConvert(const void* data, UsdBridgeSpanI<DestType>& destSpan)
  {
    SourceType* typedData = (SourceType*)data;
    DestType* destArray = destSpan.begin();
    for (int i = 0; i < destSpan.size(); ++i)
    {
      destArray[i] = DestType(typedData[i]);
    }
  }

  template<typename DestType, typename SourceType>
  void WriteToSpanExpand(const void* data, UsdBridgeSpanI<DestType>& destSpan)
  {
    constexpr size_t DestComponents = DestType::dimension;
    SourceType* typedData = (SourceType*)data;
    DestType* destArray = destSpan.begin();
    for (int i = 0; i < destSpan.size(); ++i)
    {
      if constexpr(DestComponents == 2)
        destArray[i] = DestType(typedData[i], typedData[i]);
      if constexpr(DestComponents == 3)
        destArray[i] = DestType(typedData[i], typedData[i], typedData[i]);
      if constexpr(DestComponents == 4)
        destArray[i] = DestType(typedData[i], typedData[i], typedData[i], typedData[i]);
    }
  }

  template<typename DestType, typename SourceType>
  void WriteToSpanConvertQuat(const void* data, UsdBridgeSpanI<DestType>& destSpan)
  {
    using DestScalarType = typename DestType::ScalarType;
    SourceType* typedSrcData = (SourceType*)data;
    DestType* destArray = destSpan.begin();
    for (int i = 0; i < destSpan.size(); ++i)
    {
      destArray[i] = DestType(
        DestScalarType(typedSrcData[i*4+3]), // note that the real component comes first in the quat's constructor
        DestScalarType(typedSrcData[i*4]),
        DestScalarType(typedSrcData[i*4+1]),
        DestScalarType(typedSrcData[i*4+2]));
    }
  }

  template<typename NormalsType>
  void WriteToSpanNormalsToQuaternions(const void* normals, UsdBridgeSpanI<GfQuath>& quaternions)
  {
    GfVec3f from(0.0f, 0.0f, 1.0f);
    NormalsType* norms = (NormalsType*)(normals);
    size_t i = 0;
    for (GfQuath& destQuat : quaternions)
    {
      GfVec3f to((float)(norms[i * 3]), (float)(norms[i * 3 + 1]), (float)(norms[i * 3 + 2]));
      GfRotation rot(from, to);
      const GfQuaternion& quat = rot.GetQuaternion();
      destQuat = GfQuath((float)(quat.GetReal()), GfVec3h(quat.GetImaginary()));
      ++i;
    }
  }

  template<typename DestType, typename SourceScalarType, int NumComponents>
  void WriteToSpanConvertVector(const void* data, UsdBridgeSpanI<DestType>& destSpan)
  {
    using DestScalarType = typename DestType::ScalarType;
    const SourceScalarType* typedSrcData = (SourceScalarType*)data;
    DestType* destArray = destSpan.begin();
    for (int i = 0; i < destSpan.size(); ++i)
    {
      for(int j = 0; j < NumComponents; ++j)
        destArray[i][j] = DestScalarType(*(typedSrcData++));
    }
  }

  template<typename InputEltType, int numComponents>
  void WriteToSpanExpandToColor(const void* data, UsdBridgeSpanI<GfVec4f>& destSpan)
  {
    const InputEltType* typedInput = reinterpret_cast<const InputEltType*>(data);
    size_t i = 0;
    // No memcopies, as input is not guaranteed to be of float type
    if(numComponents == 1)
      for (auto& destElt : destSpan)
      { destElt = GfVec4f(typedInput[i], 0.0f, 0.0f, 1.0f); ++i; }
    if(numComponents == 2)
      for (auto& destElt : destSpan)
      { destElt = GfVec4f(typedInput[i*2], typedInput[i*2+1], 0.0f, 1.0f); ++i; }
    if(numComponents == 3)
      for (auto& destElt : destSpan)
      { destElt = GfVec4f(typedInput[i*3], typedInput[i*3+1], typedInput[i*3+2], 1.0f); ++i; }
  }

  template<typename InputEltType, int numComponents>
  void WriteToSpanExpandToColorNormalize(const void* data, UsdBridgeSpanI<GfVec4f>& destSpan)
  {
    const InputEltType* typedInput = reinterpret_cast<const InputEltType*>(data);
    double normFactor = 1.0 / (double)std::numeric_limits<InputEltType>::max(); // float may not be enough for uint32_t
    // No memcopies, as input is not guaranteed to be of float type
    size_t i = 0;
    if(numComponents == 1)
      for (auto& destElt : destSpan)
      { destElt = GfVec4f(typedInput[i]*normFactor, 0.0f, 0.0f, 1.0f); ++i; }
    if(numComponents == 2)
      for (auto& destElt : destSpan)
      { destElt = GfVec4f(typedInput[i*2]*normFactor, typedInput[i*2+1]*normFactor, 0.0f, 1.0f); ++i; }
    if(numComponents == 3)
      for (auto& destElt : destSpan)
      { destElt = GfVec4f(typedInput[i*3]*normFactor, typedInput[i*3+1]*normFactor, typedInput[i*3+2]*normFactor, 1.0f); ++i; }
    if(numComponents == 4)
      for (auto& destElt : destSpan)
      { destElt = GfVec4f(typedInput[i*4]*normFactor, typedInput[i*4+1]*normFactor, typedInput[i*4+2]*normFactor, typedInput[i*4+3]*normFactor); ++i; }
  }

  template<int numComponents>
  void WriteToSpanExpandSRGBToColor(const void* data, UsdBridgeSpanI<GfVec4f>& destSpan)
  {
    const unsigned char* typedInput = reinterpret_cast<const unsigned char*>(data);
    float normFactor = 1.0f / 255.0f;
    const float* srgbTable = ubutils::SrgbToLinearTable();
    // No memcopies, as input is not guaranteed to be of float type
    size_t i = 0;
    if(numComponents == 1)
      for (auto& destElt : destSpan)
      { destElt = GfVec4f(srgbTable[typedInput[i]], 0.0f, 0.0f, 1.0f); ++i; }
    if(numComponents == 2)
      for (auto& destElt : destSpan)
      { destElt = GfVec4f(srgbTable[typedInput[i*2]], 0.0f, 0.0f, typedInput[i*2+1]*normFactor); ++i; } // Alpha is linear
    if(numComponents == 3)
      for (auto& destElt : destSpan)
      { destElt = GfVec4f(srgbTable[typedInput[i*3]], srgbTable[typedInput[i*3+1]], srgbTable[typedInput[i*3+2]], 1.0f); ++i; }
    if(numComponents == 4)
      for (auto& destElt : destSpan)
      { destElt = GfVec4f(srgbTable[typedInput[i*4]], srgbTable[typedInput[i*4+1]], srgbTable[typedInput[i*4+2]], typedInput[i*4+3]*normFactor); ++i; }
  }

  #define WRITE_SPAN_MACRO(EltType) \
    WriteToSpanCopy<EltType>(arrayData, destSpan)

  #define WRITE_SPAN_MACRO_CONVERT(DestType, SrcType) \
    WriteToSpanConvert<DestType, SrcType>(arrayData, destSpan)

  #define WRITE_SPAN_MACRO_EXPAND_COL(EltType, NumComponents) \
    WriteToSpanExpandToColor<EltType, NumComponents>(arrayData, destSpan)

  #define WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(EltType, NumComponents) \
    WriteToSpanExpandToColorNormalize<EltType, NumComponents>(arrayData, destSpan)

  #define WRITE_SPAN_MACRO_EXPAND_SGRB(NumComponents) \
    WriteToSpanExpandSRGBToColor<NumComponents>(arrayData, destSpan)

  // Assigns color data array to GfVec4f span
  void WriteToSpanColor(const UsdBridgeLogObject& logObj, UsdBridgeSpanI<GfVec4f>& destSpan,
    const void* arrayData, size_t arrayNumElements, UsdBridgeType arrayType)
  {
    size_t destSpanSize = destSpan.size();

    if(destSpanSize != arrayNumElements)
    {
      UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "Usd Attribute span size (" << destSpanSize << ") does not correspond to number of array elements to write (" << arrayNumElements << "), so copy is aborted");
      return;
    }

    switch (arrayType)
    {
      case UsdBridgeType::UCHAR: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint8_t, 1); break; }
      case UsdBridgeType::UCHAR2: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint8_t, 2); break; }
      case UsdBridgeType::UCHAR3: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint8_t, 3); break; }
      case UsdBridgeType::UCHAR4: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint8_t, 4); break; }
      case UsdBridgeType::UCHAR_SRGB_R: {WRITE_SPAN_MACRO_EXPAND_SGRB(1); break; }
      case UsdBridgeType::UCHAR_SRGB_RA: {WRITE_SPAN_MACRO_EXPAND_SGRB(2); break; }
      case UsdBridgeType::UCHAR_SRGB_RGB: {WRITE_SPAN_MACRO_EXPAND_SGRB(3); break; }
      case UsdBridgeType::UCHAR_SRGB_RGBA: {WRITE_SPAN_MACRO_EXPAND_SGRB(4); break; }
      case UsdBridgeType::USHORT: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint16_t, 1); break; }
      case UsdBridgeType::USHORT2: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint16_t, 2); break; }
      case UsdBridgeType::USHORT3: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint16_t, 3); break; }
      case UsdBridgeType::USHORT4: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint16_t, 4); break; }
      case UsdBridgeType::UINT: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint32_t, 1); break; }
      case UsdBridgeType::UINT2: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint32_t, 2); break; }
      case UsdBridgeType::UINT3: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint32_t, 3); break; }
      case UsdBridgeType::UINT4: {WRITE_SPAN_MACRO_EXPAND_NORMALIZE_COL(uint32_t, 4); break; }
      case UsdBridgeType::FLOAT: {WRITE_SPAN_MACRO_EXPAND_COL(float, 1); break; }
      case UsdBridgeType::FLOAT2: {WRITE_SPAN_MACRO_EXPAND_COL(float,2); break; }
      case UsdBridgeType::FLOAT3: {WRITE_SPAN_MACRO_EXPAND_COL(float, 3); break; }
      case UsdBridgeType::FLOAT4: {WRITE_SPAN_MACRO(GfVec4f); break; }
      case UsdBridgeType::DOUBLE: {WRITE_SPAN_MACRO_EXPAND_COL(double, 1); break; }
      case UsdBridgeType::DOUBLE2: {WRITE_SPAN_MACRO_EXPAND_COL(double, 2); break; }
      case UsdBridgeType::DOUBLE3: {WRITE_SPAN_MACRO_EXPAND_COL(double, 3); break; }
      case UsdBridgeType::DOUBLE4: {WRITE_SPAN_MACRO_CONVERT(GfVec4f, GfVec4d); break; }
      default: { UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "UsdGeom color primvar is not of type (UCHAR/USHORT/UINT/FLOAT/DOUBLE)(1/2/3/4) or UCHAR_SRGB_<X>."); break; }
    }
  }
}

namespace UsdBridgeArrays
{
  struct AttribSpanInit
  {
    // Standard 4-argument constructor, template only required for conditional compilation of the next constructor
    template<typename U = SdfValueTypeName>
    AttribSpanInit(size_t numElements,
      ::PXR_NS::SdfValueTypeName attribValueType,
      UsdAttribute& attrib,
      UsdTimeCode& timeCode)
      : NumElements(numElements)
      , AttribValueType(attribValueType)
      , Attrib(attrib)
      , TimeCode(timeCode)
    {
    }

    // Only compile when we are in a PXR_NS namespace for easier construction.
    template<typename U = SdfValueTypeName,
      std::enable_if_t<std::is_same_v<::PXR_NS::SdfValueTypeName, U>, bool> = true>
    AttribSpanInit(size_t numElements,
      UsdAttribute& attrib,
      UsdTimeCode& timeCode)
      : NumElements(numElements)
      , AttribValueType(attrib.GetTypeName())
      , Attrib(attrib)
      , TimeCode(timeCode)
    {
    }

    const char* GetAttribName()
    {
      return Attrib.GetName().GetText();
    }

    ::PXR_NS::SdfValueTypeName GetAttribValueType()
    {
      return AttribValueType;
    }

    void SetAutoAssignToAttrib(bool assign)
    {
      AutoAssignToAttrib = assign;
    }

    bool GetAutoAssignToAttrib() const
    {
      return AutoAssignToAttrib;
    }

    size_t NumElements = 0;
    ::PXR_NS::SdfValueTypeName AttribValueType;
    UsdAttribute Attrib;
    UsdTimeCode TimeCode;
    bool AutoAssignToAttrib = true;
  };

  template<typename EltType>
  class AttribSpan : public UsdBridgeSpanI<EltType>
  {
    public:
      AttribSpan(AttribSpanInit& spanInit)
        : SpanInit(spanInit)
        , AttribArray(&GetStaticTempArray<VtArray<EltType>>(spanInit.NumElements))
      {}

      EltType* begin() override
      {
        if(AttribArray->size())
          return &(*AttribArray)[0];
        return nullptr;
      }

      const EltType* begin() const override
      {
        if(AttribArray->size())
          return &(*AttribArray)[0];
        return nullptr;
      }

      EltType* end() override
      {
        if(AttribArray->size())
          return &(*AttribArray)[0]+SpanInit.NumElements;
        return nullptr;
      }

      const EltType* end() const override
      {
        if(AttribArray->size())
          return &(*AttribArray)[0]+SpanInit.NumElements;
        return nullptr;
      }

      size_t size() const override
      {
        return AttribArray->size();
      }

      void AssignToAttrib() override
      {
        SpanInit.Attrib.Set(*AttribArray, SpanInit.TimeCode);
      }

      AttribSpanInit SpanInit;
      VtArray<EltType>* AttribArray;
  };

    // Make sure types correspond to AssignArrayToAttribute()
  SdfValueTypeName GetAttribArrayType(UsdBridgeType eltType)
  {
    assert(eltType != UsdBridgeType::UNDEFINED);

    SdfValueTypeName result = SdfValueTypeNames->BoolArray;

    switch (eltType)
    {
      case UsdBridgeType::UCHAR: 
      case UsdBridgeType::CHAR: { result = SdfValueTypeNames->UCharArray; break;}
      case UsdBridgeType::USHORT: { result = SdfValueTypeNames->UIntArray; break; }
      case UsdBridgeType::SHORT: { result = SdfValueTypeNames->IntArray; break; }
      case UsdBridgeType::UINT: { result = SdfValueTypeNames->UIntArray; break; }
      case UsdBridgeType::INT: { result = SdfValueTypeNames->IntArray; break; }
      case UsdBridgeType::LONG: { result = SdfValueTypeNames->Int64Array; break; }
      case UsdBridgeType::ULONG: { result = SdfValueTypeNames->UInt64Array; break; }
      case UsdBridgeType::HALF: { result = SdfValueTypeNames->HalfArray; break; }
      case UsdBridgeType::FLOAT: { result = SdfValueTypeNames->FloatArray; break; }
      case UsdBridgeType::DOUBLE: { result = SdfValueTypeNames->DoubleArray; break; }

      case UsdBridgeType::UCHAR2:
      case UsdBridgeType::CHAR2:
      case UsdBridgeType::USHORT2:
      case UsdBridgeType::SHORT2:
      case UsdBridgeType::UINT2:
      case UsdBridgeType::INT2: { result = SdfValueTypeNames->Int2Array; break; }
      case UsdBridgeType::HALF2: { result = SdfValueTypeNames->Half2Array; break; }
      case UsdBridgeType::FLOAT2: { result = SdfValueTypeNames->Float2Array; break; }
      case UsdBridgeType::DOUBLE2: { result = SdfValueTypeNames->Double2Array; break; }

      case UsdBridgeType::UCHAR3:
      case UsdBridgeType::CHAR3:
      case UsdBridgeType::USHORT3:
      case UsdBridgeType::SHORT3:
      case UsdBridgeType::UINT3:
      case UsdBridgeType::INT3: { result = SdfValueTypeNames->Int3Array; break; }
      case UsdBridgeType::HALF3: { result = SdfValueTypeNames->Half3Array; break; }
      case UsdBridgeType::FLOAT3: { result = SdfValueTypeNames->Float3Array; break; }
      case UsdBridgeType::DOUBLE3: { result = SdfValueTypeNames->Double3Array; break; }

      case UsdBridgeType::UCHAR4:
      case UsdBridgeType::CHAR4:
      case UsdBridgeType::USHORT4:
      case UsdBridgeType::SHORT4:
      case UsdBridgeType::UINT4:
      case UsdBridgeType::INT4: { result = SdfValueTypeNames->Int4Array; break; }
      case UsdBridgeType::HALF4: { result = SdfValueTypeNames->Half4Array; break; }
      case UsdBridgeType::FLOAT4: { result = SdfValueTypeNames->Float4Array; break; }
      case UsdBridgeType::DOUBLE4: { result = SdfValueTypeNames->Double4Array; break; }

      default: break; //unsupported defaults to bool
    };

    return result;
  }

  // Creates the span using AttributeCType (the type to write to), since at this point both the span and proper type are known.
  template<typename SpanInitType, template <typename T> class SpanI, typename AttributeCType, UsdBridgeType ArrayEltBridgeType>
  UsdBridgeSpanI<AttributeCType>* WriteSpanToAttrib(const UsdBridgeLogObject& logObj, SpanInitType& destSpanInit, const void* arrayData, size_t arrayNumElements)
  {
    // Also uses a bunch of type traits to figure out whether to copy/convert/expand
    constexpr UsdBridgeType AttributeEltType = UsdBridgeTypeTraits::UsdBaseToBridgeType<AttributeCType>::Type; 
    constexpr int attributeNumComponents = UsdBridgeTypeTraits::NumComponents<AttributeEltType>::Value;
    constexpr int arrayNumComponents = UsdBridgeTypeTraits::NumComponents<ArrayEltBridgeType>::Value;
    using AttributeScalarCType = typename UsdBridgeTypeTraits::ScalarType<AttributeEltType>::Type;
    using ArrayScalarCType = typename UsdBridgeTypeTraits::ScalarType<ArrayEltBridgeType>::Type;

    // Actual span creation with the dest type AttributeCType
    SpanI<AttributeCType>& destSpan = GetStaticSpan<SpanInitType, SpanI, AttributeCType>(destSpanInit);
    size_t destSpanSize = destSpan.size();

    if(destSpanSize != arrayNumElements)
    {
      UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "Usd Attribute span size (" << destSpanSize << ") does not correspond to number of array elements to write (" << arrayNumElements << "), so copy is aborted");
      return nullptr;
    }

    if(arrayData != nullptr) // If null, explicitly don't write/assign, just return the span
    {
      if constexpr(arrayNumComponents == attributeNumComponents)
      {
        constexpr bool canDirectCopy = CanCopyType<ArrayScalarCType, AttributeScalarCType>::Value;
        constexpr bool canConvert = std::is_constructible<AttributeScalarCType, ArrayScalarCType>::value;

        if constexpr(canDirectCopy)
          WriteToSpanCopy<AttributeCType>(arrayData, destSpan);
        else if constexpr(canConvert)
        {
          if constexpr(arrayNumComponents == 1)
            WriteToSpanConvert<AttributeCType, ArrayScalarCType>(arrayData, destSpan);
          else if constexpr(
            std::is_same<AttributeCType, GfQuath>::value ||
            std::is_same<AttributeCType, GfQuatf>::value ||
            std::is_same<AttributeCType, GfQuatd>::value)
            WriteToSpanConvertQuat<AttributeCType, ArrayScalarCType>(arrayData, destSpan);
          else
            WriteToSpanConvertVector<AttributeCType, ArrayScalarCType, arrayNumComponents>(arrayData, destSpan);
        }
        else
        {
          UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "Cannot convert array data of type: " << ubutils::UsdBridgeTypeToString(ArrayEltBridgeType) << " to attribute data type: " << ubutils::UsdBridgeTypeToString(AttributeEltType))
          return nullptr;
        }

        // Check if assign is desired
        if(destSpanInit.GetAutoAssignToAttrib())
          destSpan.AssignToAttrib();
      }
      else
      {
        UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "Different number of components between array data type: " << ubutils::UsdBridgeTypeToString(ArrayEltBridgeType) << " and attribute data type: " << ubutils::UsdBridgeTypeToString(AttributeEltType));
        return nullptr;
      }
    }
    return &destSpan;
  }

  template<typename SpanInitType, template <typename T> class SpanI,
    typename AttributeCType, UsdBridgeType ArrayEltBridgeType, typename ReturnEltType>
  void WriteSpanToAttrib_Return(const UsdBridgeLogObject& logObj, 
    SpanInitType& destSpanInit, const void* arrayData, size_t arrayNumElements, UsdBridgeSpanI<ReturnEltType>*& resultSpan)
  {
    UsdBridgeSpanI<AttributeCType>* writeSpan = WriteSpanToAttrib<SpanInitType, SpanI, AttributeCType, ArrayEltBridgeType>
      (logObj, destSpanInit, arrayData, arrayNumElements);

    if constexpr(std::is_same<ReturnEltType, AttributeCType>::value)
      resultSpan = writeSpan;
    else if constexpr(!std::is_same<ReturnEltType, UsdBridgeNoneType>::value)
    {
      UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, 
        "Implementation error: Assumed element type of requested attribute span does not equal type of attribute itself. No span will be returned.")
    }
  }

  #define WRITE_SPAN_TO_ATTRIB(ArrayEltBridgeType)\
    case ArrayEltBridgeType:\
    {\
      WriteSpanToAttrib_Return<SpanInitType, SpanI, AttributeCType, ArrayEltBridgeType, ReturnEltType>\
        (logObj, destSpanInit, arrayData, arrayNumElements, resultSpan);\
      break;\
    }

  template<typename AttributeCType, typename SpanInitType, template <typename T> class SpanI, typename ReturnEltType>
  UsdBridgeSpanI<ReturnEltType>* AssignArrayToTypedAttribute(const UsdBridgeLogObject& logObj, 
    SpanInitType& destSpanInit, const void* arrayData, UsdBridgeType arrayDataType, size_t arrayNumElements)
  {
    UsdBridgeSpanI<ReturnEltType>* resultSpan = nullptr;
    switch (arrayDataType)
    {
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::UCHAR)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::CHAR)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::USHORT)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::SHORT)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::UINT)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::INT)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::LONG)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::ULONG)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::HALF)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::FLOAT)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::DOUBLE)

      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::UCHAR2)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::CHAR2)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::USHORT2)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::SHORT2)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::INT2)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::UINT2)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::LONG2)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::ULONG2)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::HALF2)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::FLOAT2)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::DOUBLE2)

      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::UCHAR3)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::CHAR3)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::USHORT3)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::SHORT3)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::INT3)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::UINT3)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::LONG3)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::ULONG3)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::HALF3)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::FLOAT3)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::DOUBLE3)

      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::UCHAR4)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::CHAR4)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::USHORT4)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::SHORT4)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::INT4)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::UINT4)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::LONG4)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::ULONG4)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::HALF4)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::FLOAT4)
      WRITE_SPAN_TO_ATTRIB(UsdBridgeType::DOUBLE4)

      default:
      {
        UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "No support for writing array data of type: " << ubutils::UsdBridgeTypeToString(arrayDataType) << " to a UsdAttribute.")
        break;
      }
    };
    return resultSpan;
  }

  #define MAP_VALUETYPE_TO_BASETYPE(SdfVTN, AttribCType)\
    ValueTypeToAssignArrayMap.emplace(::PXR_NS::SdfVTN.GetType(), AssignArrayToTypedAttribute<AttribCType, SpanInitType, SpanI, ReturnEltType>);

  // The assign array map essentially determines for any runtime attribute sdfvaluetype, which span element type should be employed (completing the span type)
  template<typename SpanInitType, template <typename T> class SpanI, typename ReturnEltType>
  class AssignArrayMapType
  {
    public:
      using ArrayToTypeAttributeFunc = std::function<UsdBridgeSpanI<ReturnEltType>*(const UsdBridgeLogObject& logObj, SpanInitType& destSpanInit, const void* arrayData, UsdBridgeType arrayDataType, size_t arrayNumElements)>;
      std::map<::PXR_NS::TfType, ArrayToTypeAttributeFunc> ValueTypeToAssignArrayMap;

      AssignArrayMapType()
      {
        // See: https://docs.omniverse.nvidia.com/usd/latest/technical_reference/usd-types.html
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->BoolArray, bool)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->UCharArray, uint8_t)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->IntArray, int32_t)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->UIntArray, uint32_t)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Int64Array, int64_t)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->UInt64Array, uint64_t)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->HalfArray, GfHalf)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->FloatArray, float)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->DoubleArray, double)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Int2Array, GfVec2i)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Int3Array, GfVec3i)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Int4Array, GfVec4i)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Half2Array, GfVec2h)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Half3Array, GfVec3h)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Half4Array, GfVec4h)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Float2Array, GfVec2f)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Float3Array, GfVec3f)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Float4Array, GfVec4f)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Double2Array, GfVec2d)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Double3Array, GfVec3d)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Double4Array, GfVec4d)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Point3hArray, GfVec3h)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Point3fArray, GfVec3f)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Point3dArray, GfVec3d)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Vector3hArray, GfVec3h)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Vector3fArray, GfVec3f)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Vector3dArray, GfVec3d)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Normal3hArray, GfVec3h)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Normal3fArray, GfVec3f)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Normal3dArray, GfVec3d)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Color3hArray, GfVec3h)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Color3fArray, GfVec3f)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Color3dArray, GfVec3d)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Color4hArray, GfVec4h)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Color4fArray, GfVec4f)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->Color4dArray, GfVec4d)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->QuathArray, GfQuath)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->QuatfArray, GfQuatf)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->QuatdArray, GfQuatd)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->TexCoord2hArray, GfVec2h)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->TexCoord2fArray, GfVec2f)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->TexCoord2dArray, GfVec2d)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->TexCoord3hArray, GfVec3h)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->TexCoord3fArray, GfVec3f)
        MAP_VALUETYPE_TO_BASETYPE(SdfValueTypeNames->TexCoord3dArray, GfVec3d)
      }
  };

  template<typename SpanInitType = AttribSpanInit, template <typename T> class SpanI = AttribSpan, typename ReturnEltType = UsdBridgeNoneType>
  UsdBridgeSpanI<ReturnEltType>* AssignArrayToAttribute(const UsdBridgeLogObject& logObj, const void* arrayData, UsdBridgeType arrayDataType, size_t arrayNumElements, 
    SpanInitType& destSpanInit)
  {
    static AssignArrayMapType<SpanInitType, SpanI, ReturnEltType> assignArrayMap;

    // Find the array assignment specialization for the runtime attribute value type, and execute it if available
    auto attribValueType = destSpanInit.GetAttribValueType();
    auto assignArrayEntry = assignArrayMap.ValueTypeToAssignArrayMap.find(attribValueType.GetType());
    if(assignArrayEntry != assignArrayMap.ValueTypeToAssignArrayMap.end())
    {
      auto assignArrayFunc = assignArrayEntry->second;
      auto resultSpan = assignArrayFunc(logObj, destSpanInit, arrayData, arrayDataType, arrayNumElements);

      // If the caller expects a span and none is returned, raise an error message
      if constexpr(!std::is_same<ReturnEltType, UsdBridgeNoneType>::value)
      {
        if(!resultSpan)
        {
          UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, 
            "Update of UsdAttribute '" << destSpanInit.GetAttribName() << "' with an array did not return writeable memory after update; " <<
            "data array and attribute types cannot be converted or are not supported, or the span and attribute types do not match - see previous errors");
        }
      }
      return resultSpan;
    }
    else
    {
      UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "No support for writing to UsdAttribute with SdfValueTypeName: " << attribValueType.GetAsToken().GetText())
    }

    return nullptr;
  }

}


} // internal linkage

#endif