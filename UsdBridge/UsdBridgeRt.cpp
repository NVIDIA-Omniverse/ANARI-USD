// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "usd.h"
#include "UsdBridgeRt.h"

#ifdef USE_USDRT
#include <usdrt/scenegraph/base/gf/quath.h>
#include <usdrt/scenegraph/base/gf/quatf.h>
#include <usdrt/scenegraph/base/gf/quatd.h>
#include <usdrt/scenegraph/base/gf/rotation.h>
#include <usdrt/scenegraph/base/gf/transform.h>
#include <usdrt/scenegraph/base/vt/array.h>
#include <usdrt/scenegraph/base/tf/token.h>
#include <usdrt/scenegraph/usd/sdf/path.h>
#include <usdrt/scenegraph/usd/usd/stage.h>
#include <usdrt/scenegraph/usd/usd/prim.h>
#include <usdrt/scenegraph/usd/usd/attribute.h>
#include <usdrt/scenegraph/usd/usdGeom/tokens.h>
#include <usdrt/scenegraph/usd/usdGeom/primvarsAPI.h>
#include <omni/fabric/stage/PrimBucketList.h>
#include <omni/fabric/stage/StageReaderWriter.h>
#endif

#ifdef USE_USDRT
using UsdRtStageRefPtrType = usdrt::UsdStageRefPtr;
using UsdRtPrimType = usdrt::UsdPrim;
using UsdRtAttribType = usdrt::UsdAttribute;
using UsdRtRelationshipType = usdrt::UsdRelationship;
using UsdRtSdfPathType = usdrt::SdfPath;
#else
using UsdRtStageRefPtrType = PXR_NS::UsdStagePtr;
using UsdRtPrimType = PXR_NS::UsdPrim;
using UsdRtAttribType = PXR_NS::UsdAttribute;
using UsdRtRelationshipType = PXR_NS::UsdRelationship;
using UsdRtSdfPathType = PXR_NS::SdfPath;
#endif

namespace
{
  UsdRtStageRefPtrType ToUsdRtStage(const PXR_NS::UsdStagePtr& usdStage)
  {
#ifdef USE_USDRT
    PXR_NS::UsdStageCache& stageCache = PXR_NS::UsdUtilsStageCache::Get();

    // If stage doesn't exist in stagecache, insert
    PXR_NS::UsdStageCache::Id usdStageId = stageCache.GetId(usdStage);
    if(!usdStageId.IsValid())
      usdStageId = stageCache.Insert(usdStage);
    assert(usdStageId.IsValid());

    omni::fabric::UsdStageId stageId(usdStageId.ToLongInt());

    return usdrt::UsdStage::Attach(stageId);
#else
    return usdStage;
#endif
  }
}

class UsdBridgeRtInternals
{
  public:
    UsdBridgeRtInternals(const PXR_NS::UsdStagePtr& sceneStage, const PXR_NS::SdfPath& primPath)
    {
      PxrSceneStage = sceneStage;
      SceneStage = ToUsdRtStage(sceneStage);

#ifdef USE_USDRT
      omni::fabric::StageReaderWriterId stageReaderWriterId = SceneStage->GetStageReaderWriterId();
      StageReaderWriter = omni::fabric::StageReaderWriter(stageReaderWriterId);

      std::string pathString = primPath.GetString();
      ClassPrimPath = pathString;

      FindUniformPrim(PxrSceneStage->GetPrimAtPath(PXR_NS::SdfPath("/Root")));

      // Get every Mesh prim with a extent attribute in Fabric
      const omni::fabric::Token extentAttribName("faceVertexCounts");
      omni::fabric::AttrNameAndType extentAttrib(omni::fabric::Type(omni::fabric::BaseDataType::eInt, 1, 1), extentAttribName);
      omni::fabric::AttrNameAndType mesh(
        omni::fabric::Type(omni::fabric::BaseDataType::eTag, 1, 0, omni::fabric::AttributeRole::ePrimTypeName),
        omni::fabric::Token("Mesh"));
      omni::fabric::PrimBucketList buckets = StageReaderWriter.findPrims({ extentAttrib, mesh });

      // Iterate over the buckets that match the query
      for (size_t bucketId = 0; bucketId < buckets.bucketCount(); bucketId++)
      {
        auto primPaths = StageReaderWriter.getPathArray(buckets, bucketId);

        auto attribs = StageReaderWriter.getAttributeNamesAndTypes(buckets, bucketId);

        for (omni::fabric::Path primPath : primPaths)
        {
          UsdRtSdfPathType sdfPrimPath(primPath.getText());

          if(sdfPrimPath.GetNameToken() == ClassPrimPath.GetNameToken())
          {
#ifdef USE_FABRIC
            UniformFabricPath = primPath;
#else
            UniformPrim = SceneStage->GetPrimAtPath(sdfPrimPath);
#endif

            //bool hasAttrib = UniformPrim.HasAttribute(usdrt::TfToken("primvars:attribute0"));

            // Check array of attribs that exist on the prim
            //for(const omni::fabric::AttrNameAndType& attribNameType : attribs)
            //{
            //  const char* nameText = attribNameType.name.getText();
            //  nameText = nameText;
            //}

            //UsdRtRelationshipType protoRel = rtPrim.GetRelationship(usdrt::TfToken("_protoPath"));
            //if (protoRel)
            //{
            //  usdrt::SdfPathVector targets;
            //  protoRel.GetTargets(&targets);
            //  for(auto& relTarget : targets)
            //  {
            //    std::string relString = relTarget.GetString();
//
            //    UniformPrim = SceneStage->GetPrimAtPath(relTarget);
            //    assert(UniformPrim);
            //  }
            //}
          }
        }
      }
#else
      ClassPrimPath = primPath;
      UniformPrim = PxrSceneStage->GetPrimAtPath(ClassPrimPath);
#endif
    }

#ifdef USE_USDRT
    void ReCalc()
    {
      SceneStage = ToUsdRtStage(PxrSceneStage);
#ifdef USE_FABRIC
      UniformFabricPath = omni::fabric::Path();
#else
      UniformPrim = UsdRtPrimType();
#endif
      //FindUniformPrim(PxrSceneStage->GetPrimAtPath(PXR_NS::SdfPath("/Root")));

    }

    void FindUniformPrim(PXR_NS::UsdPrim rootPrim)
    {
      UsdRtPrimType rtPrim = SceneStage->GetPrimAtPath(
        rootPrim.GetPath().GetString()
        );

      if(rtPrim && rtPrim.GetPath().GetNameToken() == ClassPrimPath.GetNameToken())
      {
#ifdef USE_FABRIC
        UniformFabricPath = rtPrim.GetPath().GetText();
#else
        UniformPrim = rtPrim;
#endif

        //bool hasAttrib = UniformPrim.HasAttribute(usdrt::TfToken("primvars:attribute0"));
        //hasAttrib = hasAttrib;

        //UsdRtRelationshipType protoRel = rtPrim.GetRelationship(usdrt::TfToken("_protoPath"));
        //if (protoRel)
        //{
        //  usdrt::SdfPathVector targets;
        //  protoRel.GetTargets(&targets);
        //  for(auto& relTarget : targets)
        //  {
        //    std::string relString = relTarget.GetString();
//
        //    UniformPrim = SceneStage->GetPrimAtPath(relTarget);
        //    assert(UniformPrim);
        //  }
        //}
      }
//
      for(const auto& childPrim : rootPrim.GetFilteredChildren(PXR_NS::UsdTraverseInstanceProxies()))
      {
        FindUniformPrim(childPrim);
      }
    }
#endif

    PXR_NS::UsdStagePtr PxrSceneStage;
    UsdRtStageRefPtrType SceneStage;
    UsdRtSdfPathType ClassPrimPath;
#ifdef USE_USDRT
    omni::fabric::StageReaderWriter StageReaderWriter;
#ifdef USE_FABRIC
    omni::fabric::Path UniformFabricPath;
#else
    UsdRtPrimType UniformPrim;
#endif
#else
    UsdRtPrimType UniformPrim;
#endif
};

UsdBridgeRt::UsdBridgeRt(const PXR_NS::UsdStagePtr& sceneStage, const PXR_NS::SdfPath& primPath)
  : Internals(new UsdBridgeRtInternals(sceneStage, primPath))
{
}

UsdBridgeRt::~UsdBridgeRt()
{
  delete Internals;
}

#ifdef USE_USDRT
using namespace usdrt;
#else
PXR_NAMESPACE_USING_DIRECTIVE
#endif

#define USE_USDRT_ELTTYPE // To get the right elementtype type def for arrays (not the same between usdrt and usd)
#include "UsdBridgeUsdWriter_Arrays.h"

#ifdef USE_FABRIC

struct UsdBridgeFabricSpanInit
{
  UsdBridgeFabricSpanInit(
    size_t numElements,
    omni::fabric::Path& primPath,
    omni::fabric::Token& attrName,
    ::PXR_NS::SdfValueTypeName attrValueType,
    omni::fabric::StageReaderWriter& stageReaderWriter)
    : PrimPath(primPath)
    , AttrName(attrName)
    , AttrValueType(attrValueType)
    , StageReaderWriter(stageReaderWriter)
  {
    stageReaderWriter.setArrayAttributeSize(primPath, attrName, numElements);
    size_t checkSize = stageReaderWriter.getArrayAttributeSize(primPath, attrName);
    checkSize = checkSize;
  }

  const char* GetAttribName()
  {
    return AttrName.getText();
  }

  ::PXR_NS::SdfValueTypeName GetAttribValueType()
  {
    return AttrValueType;
  }

  bool GetAutoAssignToAttrib() const
  {
    return false; // Not relevant for fabric spans, since AssignToAttrib does nothing
  }

  omni::fabric::Path& PrimPath;
  omni::fabric::Token& AttrName;
  ::PXR_NS::SdfValueTypeName AttrValueType;
  omni::fabric::StageReaderWriter& StageReaderWriter;
};

template<typename EltType>
class UsdBridgeFabricSpan : public UsdBridgeSpanI<EltType>
{
  public:
    UsdBridgeFabricSpan(UsdBridgeFabricSpanInit& spanInit)
      : AttribSpan(spanInit.StageReaderWriter.getArrayAttributeWr<EltType>(spanInit.PrimPath, spanInit.AttrName))
    {}

    EltType* begin() override
    {
      if(AttribSpan.size())
        return &AttribSpan[0];
      return nullptr;
    }

    const EltType* begin() const override
    {
      if(AttribSpan.size())
        return &AttribSpan[0];
      return nullptr;
    }

    EltType* end() override
    {
      if(AttribSpan.size())
        return &AttribSpan[0]+AttribSpan.size();
      return nullptr;
    }

    const EltType* end() const override
    {
      if(AttribSpan.size())
        return &AttribSpan[0]+AttribSpan.size();
      return nullptr;
    }

    size_t size() const override
    {
      return AttribSpan.size();
    }

    void AssignToAttrib() override
    {}

    gsl::span<EltType> AttribSpan;
};
#endif

template<typename ReturnEltType>
UsdBridgeSpanI<ReturnEltType>* UsdBridgeRt::UpdateUsdAttribute(const UsdBridgeLogObject& logObj, const void* arrayData, UsdBridgeType arrayDataType, size_t arrayNumElements,
  const ::PXR_NS::UsdAttribute& attrib, const ::PXR_NS::UsdTimeCode& timeCode, bool writeToAttrib)
{
  //ReCalc(); //fix

  using RtReturnEltType = typename UsdBridgeTypeTraits::PxrToRtEltType<ReturnEltType>::Type;

#ifdef USE_FABRIC
  const std::string& attribName = attrib.GetName().GetString();
  omni::fabric::Token fAttribName(attribName.c_str());

  UsdBridgeFabricSpanInit fabricSpanInit(arrayNumElements, Internals->UniformFabricPath, fAttribName, attrib.GetTypeName(), Internals->StageReaderWriter);
  UsdBridgeSpanI<RtReturnEltType>* rtSpan = UsdBridgeArrays::AssignArrayToAttribute<UsdBridgeFabricSpanInit, UsdBridgeFabricSpan, RtReturnEltType>(
    logObj, arrayData, arrayDataType, arrayNumElements, fabricSpanInit);

#else

#ifdef USE_USDRT
  UsdPrim rtPrim = Internals->UniformPrim;
  const std::string& attribName = attrib.GetName().GetString();
    
  UsdAttribute rtAttrib = rtPrim.GetAttribute(attribName);
  UsdTimeCode rtTimeCode(timeCode.GetValue());

#else
  UsdAttribute rtAttrib = attrib;
  UsdTimeCode rtTimeCode = timeCode;
#endif
  assert(rtAttrib);

  UsdBridgeArrays::AttribSpanInit spanInit(arrayNumElements, attrib.GetTypeName(), rtAttrib, rtTimeCode);
  spanInit.SetAutoAssignToAttrib(writeToAttrib);
  UsdBridgeSpanI<RtReturnEltType>* rtSpan = UsdBridgeArrays::AssignArrayToAttribute<UsdBridgeArrays::AttribSpanInit, UsdBridgeArrays::AttribSpan, RtReturnEltType>(
    logObj, arrayData, arrayDataType, arrayNumElements, spanInit);

#endif

  // usd and rt elt types should be compatible
  return reinterpret_cast<UsdBridgeSpanI<ReturnEltType>*>(rtSpan);
}

void UsdBridgeRt::ReCalc()
{
#ifdef USE_USDRT
  Internals->ReCalc();
#endif
}

bool UsdBridgeRt::ValidPrim()
{
#ifdef USE_FABRIC
  return Internals->UniformFabricPath != omni::fabric::Path();
#else
  return Internals->UniformPrim.IsValid();
#endif
}

#define UPDATE_USD_ATTRIBUTE_EXPLICIT_DEFINITION(SpecializedEltType)\
  template UsdBridgeSpanI<SpecializedEltType>* UsdBridgeRt::UpdateUsdAttribute<SpecializedEltType>(const UsdBridgeLogObject& logObj,\
    const void* arrayData, UsdBridgeType arrayDataType, size_t arrayNumElements,\
    const ::PXR_NS::UsdAttribute& attrib, const ::PXR_NS::UsdTimeCode& timeCode, bool writeToAttrib);

UPDATE_USD_ATTRIBUTE_EXPLICIT_DEFINITION(UsdBridgeNoneType)
UPDATE_USD_ATTRIBUTE_EXPLICIT_DEFINITION(int)
UPDATE_USD_ATTRIBUTE_EXPLICIT_DEFINITION(float)
UPDATE_USD_ATTRIBUTE_EXPLICIT_DEFINITION(::PXR_NS::GfVec3f)
UPDATE_USD_ATTRIBUTE_EXPLICIT_DEFINITION(::PXR_NS::GfVec4f)
UPDATE_USD_ATTRIBUTE_EXPLICIT_DEFINITION(::PXR_NS::GfQuath)