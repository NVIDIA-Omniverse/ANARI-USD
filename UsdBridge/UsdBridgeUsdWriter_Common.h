// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeUsdWriterCommon_h
#define UsdBridgeUsdWriterCommon_h

#include "UsdBridgeData.h"
#include "UsdBridgeUtils.h"

#include <string>
#include <sstream>

template<typename T>
using TimeEvaluator = UsdBridgeTimeEvaluator<T>;

#define MISC_TOKEN_SEQ \
  (Root) \
  (extent)

#define ATTRIB_TOKEN_SEQ \
  (faceVertexCounts) \
  (faceVertexIndices) \
  (points) \
  (positions) \
  (normals) \
  (st) \
  (attribute0) \
  (attribute1) \
  (attribute2) \
  (attribute3) \
  (attribute4) \
  (attribute5) \
  (attribute6) \
  (attribute7) \
  (attribute8) \
  (attribute9) \
  (attribute10) \
  (attribute11) \
  (attribute12) \
  (attribute13) \
  (attribute14) \
  (attribute15) \
  (color) \
  (ids) \
  (widths) \
  (protoIndices) \
  (orientations) \
  (scales) \
  (velocities) \
  (angularVelocities) \
  (invisibleIds) \
  (curveVertexCounts)

#define USDPREVSURF_TOKEN_SEQ \
  (UsdPreviewSurface) \
  (useSpecularWorkflow) \
  (result) \
  (surface) \
  ((PrimVarReader_Float, "UsdPrimvarReader_float")) \
  ((PrimVarReader_Float2, "UsdPrimvarReader_float2")) \
  ((PrimVarReader_Float3, "UsdPrimvarReader_float3")) \
  ((PrimVarReader_Float4, "UsdPrimvarReader_float4")) \
  (UsdUVTexture) \
  (fallback) \
  (r) \
  (rg) \
  (rgb) \
  (black) \
  (clamp) \
  (repeat) \
  (mirror)

#define USDPREVSURF_INPUT_TOKEN_SEQ \
  (roughness) \
  (opacity) \
  (opacityThreshold) \
  (metallic) \
  (ior) \
  (diffuseColor) \
  (specularColor) \
  (emissiveColor) \
  (file) \
  (WrapS) \
  (WrapT) \
  (WrapR) \
  (varname)

#define USDPREVSURF_OUTPUT_TOKEN_SEQ \
  (result) \
  (r) \
  (rg) \
  (rgb) \
  (a)

#define MDL_OUTPUT_TOKEN_SEQ \
  (out)

#define MDL_TOKEN_SEQ \
  (mdl) \
  (OmniPBR) \
  (sourceAsset) \
  (out) \
  (data_lookup_float) \
  (data_lookup_float2) \
  (data_lookup_float3) \
  (data_lookup_float4) \
  (data_lookup_int) \
  (data_lookup_int2) \
  (data_lookup_int3) \
  (data_lookup_int4) \
  (data_lookup_color) \
  (lookup_color) \
  (lookup_float) \
  (lookup_float3) \
  (lookup_float4) \
  ((xyz, "xyz(float4)")) \
  ((x, "x(float4)")) \
  ((y, "y(float4)")) \
  ((z, "z(float4)")) \
  ((w, "w(float4)")) \
  ((construct_color, "construct_color(float3)")) \
  ((mul_float, "multiply(float,float)")) \
  (coord) \
  (colorSpace)

#define MDL_INPUT_TOKEN_SEQ \
  (reflection_roughness_constant) \
  (opacity_constant) \
  (opacity_threshold) \
  (enable_opacity) \
  (metallic_constant) \
  (ior_constant) \
  (diffuse_color_constant) \
  (emissive_color) \
  (emissive_intensity) \
  (enable_emission) \
  (name) \
  (tex) \
  (wrap_u) \
  (wrap_v) \
  (wrap_w) \
  (a) \
  (b) \

#define VOLUME_TOKEN_SEQ \
  (density) \
  (filePath)

#define INDEX_TOKEN_SEQ \
  (nvindex) \
  (volume) \
  (colormap) \
  (Colormap) \
  (colormapValues) \
  (colormapSource) \
  (domain) \
  (domainBoundaryMode) \
  (clampToEdge)

TF_DECLARE_PUBLIC_TOKENS(
  UsdBridgeTokens,

  ATTRIB_TOKEN_SEQ
  USDPREVSURF_TOKEN_SEQ
  USDPREVSURF_INPUT_TOKEN_SEQ
  MDL_TOKEN_SEQ
  MDL_INPUT_TOKEN_SEQ
  VOLUME_TOKEN_SEQ
  INDEX_TOKEN_SEQ
  MISC_TOKEN_SEQ
);

TF_DECLARE_PUBLIC_TOKENS(
  QualifiedInputTokens,

  USDPREVSURF_INPUT_TOKEN_SEQ
  MDL_INPUT_TOKEN_SEQ
);

TF_DECLARE_PUBLIC_TOKENS(
  QualifiedOutputTokens,

  USDPREVSURF_OUTPUT_TOKEN_SEQ
  MDL_OUTPUT_TOKEN_SEQ
);

namespace constring
{
  // Default names
  extern const char* const sessionPf;
  extern const char* const rootClassName;
  extern const char* const rootPrimName;

  // Folder names
  extern const char* const manifestFolder;
  extern const char* const clipFolder;
  extern const char* const primStageFolder;
  extern const char* const imgFolder;
  extern const char* const volFolder;

  // Postfixes for auto generated usd subprims
  extern const char* const texCoordReaderPrimPf;
  extern const char* const psShaderPrimPf;
  extern const char* const mdlShaderPrimPf;
  extern const char* const mdlOpacityMulPrimPf;
  extern const char* const mdlDiffuseOpacityPrimPf;
  extern const char* const mdlGraphXYZPrimPf;
  extern const char* const mdlGraphColorPrimPf;
  extern const char* const mdlGraphXPrimPf;
  extern const char* const mdlGraphYPrimPf;
  extern const char* const mdlGraphZPrimPf;
  extern const char* const mdlGraphWPrimPf;
  extern const char* const psSamplerPrimPf;
  extern const char* const mdlSamplerPrimPf;
  extern const char* const openVDBPrimPf;
  extern const char* const protoShapePf;

  // Extensions
  extern const char* const imageExtension;
  extern const char* const vdbExtension;

  // Files
  extern const char* const fullSceneNameBin;
  extern const char* const fullSceneNameAscii;

  extern const char* const mdlShaderAssetName;
  extern const char* const mdlSupportAssetName;
  extern const char* const mdlAuxAssetName;

#ifdef CUSTOM_PBR_MDL
  extern const char* const mdlFolder;

  extern const char* const opaqueMaterialFile;
  extern const char* const transparentMaterialFile;
#endif

#ifdef USE_INDEX_MATERIALS
  extern const char* const indexMaterialPf;
  extern const char* const indexShaderPf;
  extern const char* const indexColorMapPf;
#endif
}

namespace
{
  size_t FindLength(std::stringstream& strStream)
  {
    strStream.seekg(0, std::ios::end);
    size_t contentSize = strStream.tellg();
    strStream.seekg(0, std::ios::beg);
    return contentSize;
  }

  void FormatDirName(std::string& dirName)
  {
    if (dirName.length() > 0)
    {
      if (dirName.back() != '/' && dirName.back() != '\\')
        dirName.append("/");
    }
  }

  template<typename GeomDataType>
  bool UsdGeomDataHasTexCoords(const GeomDataType& geomData)
  {
    return geomData.Attributes != nullptr &&
      geomData.Attributes[0].Data != nullptr &&
      ( 
        geomData.Attributes[0].DataType == UsdBridgeType::FLOAT2 ||
        geomData.Attributes[0].DataType == UsdBridgeType::DOUBLE2
      );
  }

  TfToken AttribIndexToToken(uint32_t attribIndex)
  {
    TfToken attribToken;
    switch(attribIndex)
    {
      case 0: attribToken = UsdBridgeTokens->attribute0; break;
      case 1: attribToken = UsdBridgeTokens->attribute1; break;
      case 2: attribToken = UsdBridgeTokens->attribute2; break;
      case 3: attribToken = UsdBridgeTokens->attribute3; break;
      case 4: attribToken = UsdBridgeTokens->attribute4; break;
      case 5: attribToken = UsdBridgeTokens->attribute5; break;
      case 6: attribToken = UsdBridgeTokens->attribute6; break;
      case 7: attribToken = UsdBridgeTokens->attribute7; break;
      case 8: attribToken = UsdBridgeTokens->attribute8; break;
      case 9: attribToken = UsdBridgeTokens->attribute9; break;
      case 10: attribToken = UsdBridgeTokens->attribute10; break;
      case 11: attribToken = UsdBridgeTokens->attribute11; break;
      case 12: attribToken = UsdBridgeTokens->attribute12; break;
      case 13: attribToken = UsdBridgeTokens->attribute13; break;
      case 14: attribToken = UsdBridgeTokens->attribute14; break;
      case 15: attribToken = UsdBridgeTokens->attribute15; break;
      default: attribToken = TfToken((std::string("attribute")+std::to_string(attribIndex)).c_str()); break;
    } 
    return attribToken;
  }

  TfToken TextureWrapToken(UsdBridgeSamplerData::WrapMode wrapMode)
  {
    TfToken result = UsdBridgeTokens->black;
    switch (wrapMode)
    {
    case UsdBridgeSamplerData::WrapMode::CLAMP:
      result = UsdBridgeTokens->clamp;
      break;
    case UsdBridgeSamplerData::WrapMode::REPEAT:
      result = UsdBridgeTokens->repeat;
      break;
    case UsdBridgeSamplerData::WrapMode::MIRROR:
      result = UsdBridgeTokens->mirror;
      break;
    default:
      break;
    }
    return result;
  }

  int TextureWrapInt(UsdBridgeSamplerData::WrapMode wrapMode)
  {
    int result = 3;
    switch (wrapMode)
    {
    case UsdBridgeSamplerData::WrapMode::CLAMP:
      result = 0;
      break;
    case UsdBridgeSamplerData::WrapMode::REPEAT:
      result = 1;
      break;
    case UsdBridgeSamplerData::WrapMode::MIRROR:
      result = 2;
      break;
    default:
      break;
    }
    return result;
  }

  template<bool PreviewSurface>
  const TfToken& GetMaterialShaderInputToken(UsdBridgeMaterialData::DataMemberId dataMemberId)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    if(PreviewSurface)
    {
      switch(dataMemberId)
      {
        case DMI::DIFFUSE: { return UsdBridgeTokens->diffuseColor; break; } 
        case DMI::OPACITY: { return UsdBridgeTokens->opacity; break; } 
        case DMI::EMISSIVECOLOR: { return UsdBridgeTokens->emissiveColor; break; } 
        case DMI::ROUGHNESS: { return UsdBridgeTokens->roughness; break; } 
        case DMI::METALLIC: { return UsdBridgeTokens->metallic; break; } 
        case DMI::IOR: { return UsdBridgeTokens->ior; break; } 

        default: assert(false); break;
      };
    }
    else
    {
      switch(dataMemberId)
      {
        case DMI::DIFFUSE: { return UsdBridgeTokens->diffuse_color_constant; break; } 
        case DMI::OPACITY: { return UsdBridgeTokens->opacity_constant; break; } 
        case DMI::EMISSIVECOLOR: { return UsdBridgeTokens->emissive_color; break; } 
        case DMI::EMISSIVEINTENSITY: { return UsdBridgeTokens->emissive_intensity; break; } 
        case DMI::ROUGHNESS: { return UsdBridgeTokens->reflection_roughness_constant; break; } 
        case DMI::METALLIC: { return UsdBridgeTokens->metallic_constant; break; } 
        case DMI::IOR: { return UsdBridgeTokens->ior_constant; break; } 

        default: assert(false); break;
      };
    }

    return UsdBridgeTokens->diffuseColor;  
  }

  template<bool PreviewSurface>
  const TfToken& GetMaterialShaderInputQualifiedToken(UsdBridgeMaterialData::DataMemberId dataMemberId)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    if(PreviewSurface)
    {
      switch(dataMemberId)
      {
        case DMI::DIFFUSE: { return QualifiedInputTokens->diffuseColor; break; }
        case DMI::OPACITY: { return QualifiedInputTokens->opacity; break; }
        case DMI::EMISSIVECOLOR: { return QualifiedInputTokens->emissiveColor; break; }
        case DMI::ROUGHNESS: { return QualifiedInputTokens->roughness; break; }
        case DMI::METALLIC: { return QualifiedInputTokens->metallic; break; }
        case DMI::IOR: { return QualifiedInputTokens->ior; break; }

        default: assert(false); break;
      };
    }
    else
    {
      switch(dataMemberId)
      {
        case DMI::DIFFUSE: { return QualifiedInputTokens->diffuse_color_constant; break; }
        case DMI::OPACITY: { return QualifiedInputTokens->opacity_constant; break; }
        case DMI::EMISSIVECOLOR: { return QualifiedInputTokens->emissive_color; break; }
        case DMI::EMISSIVEINTENSITY: { return QualifiedInputTokens->emissive_intensity; break; }
        case DMI::ROUGHNESS: { return QualifiedInputTokens->reflection_roughness_constant; break; }
        case DMI::METALLIC: { return QualifiedInputTokens->metallic_constant; break; }
        case DMI::IOR: { return QualifiedInputTokens->ior_constant; break; }

        default: assert(false); break;
      };
    }

    return QualifiedInputTokens->diffuseColor;
  }

  template<bool PreviewSurface>
  SdfPath GetAttributeReaderPathPf(UsdBridgeMaterialData::DataMemberId dataMemberId)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    const char* const diffuseAttribReaderPrimPf_ps = "diffuseattribreader_ps";
    const char* const opacityAttribReaderPrimPf_ps = "opacityattribreader_ps";
    const char* const emissiveColorAttribReaderPrimPf_ps = "emissivecolorattribreader_ps";
    const char* const emissiveIntensityAttribReaderPrimPf_ps = "emissiveintensityattribreader_ps";
    const char* const roughnessAttribReaderPrimPf_ps = "roughnessattribreader_ps";
    const char* const metallicAttribReaderPrimPf_ps = "metallicattribreader_ps";
    const char* const iorAttribReaderPrimPf_ps = "iorattribreader_ps";

    const char* const diffuseAttribReaderPrimPf_mdl = "diffuseattribreader_mdl";
    const char* const opacityAttribReaderPrimPf_mdl = "opacityattribreader_mdl";
    const char* const emissiveColorAttribReaderPrimPf_mdl = "emissivecolorattribreader_mdl";
    const char* const emissiveIntensityAttribReaderPrimPf_mdl = "emissiveintensityattribreader_mdl";
    const char* const roughnessAttribReaderPrimPf_mdl = "roughnessattribreader_mdl";
    const char* const metallicAttribReaderPrimPf_mdl = "metallicattribreader_mdl";
    const char* const iorAttribReaderPrimPf_mdl = "iorattribreader_mdl";

    const char* result = nullptr;
    if(PreviewSurface)
    {
      switch(dataMemberId)
      {
        case DMI::DIFFUSE: { result = diffuseAttribReaderPrimPf_ps; break; } 
        case DMI::OPACITY: { result = opacityAttribReaderPrimPf_ps; break; } 
        case DMI::EMISSIVECOLOR: { result = emissiveColorAttribReaderPrimPf_ps; break; } 
        case DMI::EMISSIVEINTENSITY: { result = emissiveIntensityAttribReaderPrimPf_ps; break; } 
        case DMI::ROUGHNESS: { result = roughnessAttribReaderPrimPf_ps; break; } 
        case DMI::METALLIC: { result = metallicAttribReaderPrimPf_ps; break; } 
        case DMI::IOR: { result = iorAttribReaderPrimPf_ps; break; } 

        default: assert(false); break;
      };
    }
    else
    {
      switch(dataMemberId)
      {
        case DMI::DIFFUSE: { result = diffuseAttribReaderPrimPf_mdl; break; } 
        case DMI::OPACITY: { result = opacityAttribReaderPrimPf_mdl; break; } 
        case DMI::EMISSIVECOLOR: { result = emissiveColorAttribReaderPrimPf_mdl; break; } 
        case DMI::EMISSIVEINTENSITY: { result = emissiveIntensityAttribReaderPrimPf_mdl; break; } 
        case DMI::ROUGHNESS: { result = roughnessAttribReaderPrimPf_mdl; break; } 
        case DMI::METALLIC: { result = metallicAttribReaderPrimPf_mdl; break; } 
        case DMI::IOR: { result = iorAttribReaderPrimPf_mdl; break; } 

        default: assert(false); break;
      };
    }

    return SdfPath(result);
  }

  const TfToken& GetPsAttributeReaderId(UsdBridgeMaterialData::DataMemberId dataMemberId)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    switch(dataMemberId)
    {
      case DMI::DIFFUSE: { return UsdBridgeTokens->PrimVarReader_Float4; break; } // float4 instead of float4, to fix implicit conversion issue in storm
      case DMI::OPACITY: { return UsdBridgeTokens->PrimVarReader_Float; break; } 
      case DMI::EMISSIVECOLOR: { return UsdBridgeTokens->PrimVarReader_Float3; break; } 
      case DMI::EMISSIVEINTENSITY: { return UsdBridgeTokens->PrimVarReader_Float; break; } 
      case DMI::ROUGHNESS: { return UsdBridgeTokens->PrimVarReader_Float; break; } 
      case DMI::METALLIC: { return UsdBridgeTokens->PrimVarReader_Float; break; } 
      case DMI::IOR: { return UsdBridgeTokens->PrimVarReader_Float; break; } 

      default: { assert(false); break; }
    };

    return UsdBridgeTokens->PrimVarReader_Float;
  }

  const TfToken& GetMdlAttributeReaderSubId(UsdBridgeMaterialData::DataMemberId dataMemberId)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    switch(dataMemberId)
    {
      case DMI::DIFFUSE: { return UsdBridgeTokens->data_lookup_float4; break; }
      case DMI::OPACITY: { return UsdBridgeTokens->data_lookup_float; break; } 
      case DMI::EMISSIVECOLOR: { return UsdBridgeTokens->data_lookup_color; break; }
      case DMI::EMISSIVEINTENSITY: { return UsdBridgeTokens->data_lookup_float; break; } 
      case DMI::ROUGHNESS: { return UsdBridgeTokens->data_lookup_float; break; } 
      case DMI::METALLIC: { return UsdBridgeTokens->data_lookup_float; break; } 
      case DMI::IOR: { return UsdBridgeTokens->data_lookup_float; break; } 

      default: { assert(false); break; }
    };

    return UsdBridgeTokens->data_lookup_float4;
  }

  const TfToken& GetMdlAttributeReaderSubId(const SdfValueTypeName& readerOutputType)
  {
    if(readerOutputType == SdfValueTypeNames->Float4)
      return UsdBridgeTokens->data_lookup_float4;
    if(readerOutputType == SdfValueTypeNames->Float3)
      return UsdBridgeTokens->data_lookup_float3;
    if(readerOutputType == SdfValueTypeNames->Float2)
      return UsdBridgeTokens->data_lookup_float2;
    if(readerOutputType == SdfValueTypeNames->Float)
      return UsdBridgeTokens->data_lookup_float;
    if(readerOutputType == SdfValueTypeNames->Int4)
      return UsdBridgeTokens->data_lookup_int4;
    if(readerOutputType == SdfValueTypeNames->Int3)
      return UsdBridgeTokens->data_lookup_int3;
    if(readerOutputType == SdfValueTypeNames->Int2)
      return UsdBridgeTokens->data_lookup_int2;
    if(readerOutputType == SdfValueTypeNames->Int)
      return UsdBridgeTokens->data_lookup_int;
    if(readerOutputType == SdfValueTypeNames->Color3f)
      return UsdBridgeTokens->data_lookup_color;

    return UsdBridgeTokens->data_lookup_float4;
  }

  const SdfValueTypeName& GetAttributeOutputType(UsdBridgeMaterialData::DataMemberId dataMemberId)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    // An educated guess for the type belonging to a particular attribute bound to material inputs
    switch(dataMemberId)
    {
      case DMI::DIFFUSE: { return SdfValueTypeNames->Float4; break; } // Typically bound to the color array, containing rgb and a information.
      case DMI::OPACITY: { return SdfValueTypeNames->Float; break; } 
      case DMI::EMISSIVECOLOR: { return SdfValueTypeNames->Color3f; break; } 
      case DMI::EMISSIVEINTENSITY: { return SdfValueTypeNames->Float; break; } 
      case DMI::ROUGHNESS: { return SdfValueTypeNames->Float; break; } 
      case DMI::METALLIC: { return SdfValueTypeNames->Float; break; } 
      case DMI::IOR: { return SdfValueTypeNames->Float; break; } 

      default: assert(false); break;
    };
    
    return SdfValueTypeNames->Float;
  }


  const TfToken& GetSamplerAssetSubId(int numComponents)
  {
    switch(numComponents)
    {
      case 1: { return UsdBridgeTokens->lookup_float; break; }
      case 4: { return UsdBridgeTokens->lookup_float4; break; }
      default: { return UsdBridgeTokens->lookup_float3; break; }
    }

    return UsdBridgeTokens->lookup_float3;
  }

  template<bool PreviewSurface>
  const TfToken& GetSamplerOutputColorToken(int numComponents)
  {
    if(PreviewSurface)
    {
      switch(numComponents)
      {
        case 1: { return UsdBridgeTokens->r; break; }
        case 2: { return UsdBridgeTokens->rg; break; }
        default: { return UsdBridgeTokens->rgb; break; } // The alpha component is always separate
      }
    }
    return UsdBridgeTokens->out;
  }

  template<bool PreviewSurface>
  const SdfValueTypeName& GetSamplerOutputColorType(int numComponents)
  {
    if(PreviewSurface)
    {
      switch(numComponents)
      {
        case 1: { return SdfValueTypeNames->Float; break; }
        case 2: { return SdfValueTypeNames->Float2; break; }
        default: { return SdfValueTypeNames->Float3; break; } // The alpha component is always separate
      }
    }
    else
    {
      switch(numComponents)
      {
        case 1: { return SdfValueTypeNames->Float; break; }
        case 4: { return SdfValueTypeNames->Float4; break; }
        default: { return SdfValueTypeNames->Float3; break; }
      }
    }
    return SdfValueTypeNames->Float3;
  }

  template<class T>
  T GetOrDefinePrim(const UsdStageRefPtr& stage, const SdfPath& path)
  {
    T prim = T::Get(stage, path);
    if (!prim)
      prim = T::Define(stage, path);
    assert(prim);

    return prim;
  }

  template<typename ValueType>
  void ClearAndSetUsdAttribute(const UsdAttribute& attribute, const ValueType& value, const UsdTimeCode& timeCode, bool clearAttrib)
  {
    if(clearAttrib)
      attribute.Clear();
    attribute.Set(value, timeCode);
  }

  void ClearUsdAttributes(const UsdAttribute& uniformAttrib, const UsdAttribute& timeVarAttrib, bool timeVaryingUpdate)
  {
#ifdef TIME_BASED_CACHING
#ifdef VALUE_CLIP_RETIMING
    // Step could be performed to keep timeVar stage more in sync, but removal of attrib from manifest makes this superfluous
    //if (!timeVaryingUpdate && timeVarGeom)
    //{
    //  timeVarAttrib.ClearAtTime(timeEval.TimeCode);
    //}
#ifdef OMNIVERSE_CREATE_WORKAROUNDS
    if(timeVaryingUpdate && uniformAttrib)
    {
      // Create considers the referenced uniform prims as a stronger opinion than timeVarying clip values (against the spec, see 'value resolution').
      // Just remove the referenced uniform opinion altogether.
      uniformAttrib.ClearAtTime(TimeEvaluator<bool>::DefaultTime);
    }
#endif
#else // !VALUE_CLIP_RETIMING
    // Uniform and timeVar geoms are the same. In case of uniform values, make sure the timesamples are cleared out.
    if(!timeVaryingUpdate && timeVarAttrib)
    {
      timeVarAttrib.Clear();
    }
#endif
#endif
  }
}

#define CREATE_REMOVE_TIMEVARYING_ATTRIB(_prim, _dataMemberId, _token, _type) \
  if(!timeEval || timeEval->IsTimeVarying(_dataMemberId)) \
    _prim.CreateAttribute(_token, _type); \
  else \
    _prim.RemoveProperty(_token);

#define CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(_dataMemberId, _CreateFunc, _token) \
  if(!timeEval || timeEval->IsTimeVarying(_dataMemberId)) \
    attribCreatePrim._CreateFunc(); \
  else \
    attribRemovePrim.RemoveProperty(_token);

#define SET_TIMEVARYING_ATTRIB(_timeVaryingUpdate, _timeVarAttrib, _uniformAttrib, _value) \
  if(_timeVaryingUpdate) \
    _timeVarAttrib.Set(_value, timeEval.TimeCode); \
  else \
    _uniformAttrib.Set(_value, timeEval.Default());

#endif