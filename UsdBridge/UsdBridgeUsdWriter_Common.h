// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeUsdWriterConvenience_h
#define UsdBridgeUsdWriterConvenience_h

#include "UsdBridgeData.h"
#include "UsdBridgeUtils.h"

#include <string>
#include <sstream>

template<typename T>
using TimeEvaluator = UsdBridgeTimeEvaluator<T>;

#define UsdBridgeLogMacro(obj, level, message) \
  { std::stringstream logStream; \
    logStream << message; \
    std::string logString = logStream.str(); \
    obj->LogCallback(level, obj->LogUserData, logString.c_str()); } 

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

template<typename ArrayType>
ArrayType& GetStaticTempArray()
{
  static ArrayType array;
  array.resize(0);
  return array;
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

  template<typename NormalsType>
  void ConvertNormalsToQuaternions(VtQuathArray& quaternions, const void* normals, uint64_t numVertices)
  {
    GfVec3f from(0.0f, 0.0f, 1.0f);
    NormalsType* norms = (NormalsType*)(normals);
    for (int i = 0; i < numVertices; ++i)
    {
      GfVec3f to((float)(norms[i * 3]), (float)(norms[i * 3 + 1]), (float)(norms[i * 3 + 2]));
      GfRotation rot(from, to);
      const GfQuaternion& quat = rot.GetQuaternion();
      quaternions[i] = GfQuath((float)(quat.GetReal()), GfVec3h(quat.GetImaginary()));
    }
  }

  bool UsesUsdGeomPoints(const UsdBridgeInstancerData& geomData)
  {
    assert(geomData.NumShapes == 1); // Only single shapes supported
    bool simpleSphereInstancer = geomData.Shapes[0] == UsdBridgeInstancerData::SHAPE_SPHERE;
    return !geomData.UsePointInstancer && simpleSphereInstancer;
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

  SdfValueTypeName GetPrimvarArrayType(UsdBridgeType eltType)
  {
    assert(eltType != UsdBridgeType::UNDEFINED);

    SdfValueTypeName result = SdfValueTypeNames->UCharArray;

    switch (eltType)
    {
      case UsdBridgeType::UCHAR: 
      case UsdBridgeType::CHAR: { break;}
      case UsdBridgeType::USHORT: { result = SdfValueTypeNames->UIntArray; break; }
      case UsdBridgeType::SHORT: { result = SdfValueTypeNames->IntArray; break; }
      case UsdBridgeType::UINT: { result = SdfValueTypeNames->UIntArray; break; }
      case UsdBridgeType::INT: { result = SdfValueTypeNames->IntArray; break; }
      case UsdBridgeType::LONG: { result = SdfValueTypeNames->Int64Array; break; }
      case UsdBridgeType::ULONG: { result = SdfValueTypeNames->UInt64Array; break; }
      case UsdBridgeType::HALF: { result = SdfValueTypeNames->HalfArray; break; }
      case UsdBridgeType::FLOAT: { result = SdfValueTypeNames->FloatArray; break; }
      case UsdBridgeType::DOUBLE: { result = SdfValueTypeNames->DoubleArray; break; }

      case UsdBridgeType::INT2: { result = SdfValueTypeNames->Int2Array; break; }
      case UsdBridgeType::HALF2: { result = SdfValueTypeNames->Half2Array; break; }
      case UsdBridgeType::FLOAT2: { result = SdfValueTypeNames->Float2Array; break; }
      case UsdBridgeType::DOUBLE2: { result = SdfValueTypeNames->Double2Array; break; }

      case UsdBridgeType::INT3: { result = SdfValueTypeNames->Int3Array; break; }
      case UsdBridgeType::HALF3: { result = SdfValueTypeNames->Half3Array; break; }
      case UsdBridgeType::FLOAT3: { result = SdfValueTypeNames->Float3Array; break; }
      case UsdBridgeType::DOUBLE3: { result = SdfValueTypeNames->Double3Array; break; }

      case UsdBridgeType::INT4: { result = SdfValueTypeNames->Int4Array; break; }
      case UsdBridgeType::HALF4: { result = SdfValueTypeNames->Half4Array; break; }
      case UsdBridgeType::FLOAT4: { result = SdfValueTypeNames->Float4Array; break; }
      case UsdBridgeType::DOUBLE4: { result = SdfValueTypeNames->Double4Array; break; }

      case UsdBridgeType::UCHAR2:
      case UsdBridgeType::UCHAR3:
      case UsdBridgeType::UCHAR4: { result = SdfValueTypeNames->UCharArray; break; }
      case UsdBridgeType::CHAR2:
      case UsdBridgeType::CHAR3:
      case UsdBridgeType::CHAR4: { result = SdfValueTypeNames->UCharArray; break; }
      case UsdBridgeType::USHORT2:
      case UsdBridgeType::USHORT3:
      case UsdBridgeType::USHORT4: { result = SdfValueTypeNames->UIntArray; break; }
      case UsdBridgeType::SHORT2:
      case UsdBridgeType::SHORT3:
      case UsdBridgeType::SHORT4: { result = SdfValueTypeNames->IntArray; break; }
      case UsdBridgeType::UINT2:
      case UsdBridgeType::UINT3:
      case UsdBridgeType::UINT4: { result = SdfValueTypeNames->UIntArray; break; }
      case UsdBridgeType::LONG2:
      case UsdBridgeType::LONG3:
      case UsdBridgeType::LONG4: { result = SdfValueTypeNames->Int64Array; break; }
      case UsdBridgeType::ULONG2:
      case UsdBridgeType::ULONG3:
      case UsdBridgeType::ULONG4: { result = SdfValueTypeNames->UInt64Array; break; }

      default: assert(false); break;
    };

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

  const TfToken& GetMdlAttributeReaderId(UsdBridgeMaterialData::DataMemberId dataMemberId)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    switch(dataMemberId)
    {
      case DMI::DIFFUSE: { return UsdBridgeTokens->data_lookup_color; break; }
      case DMI::OPACITY: { return UsdBridgeTokens->data_lookup_float; break; } 
      case DMI::EMISSIVECOLOR: { return UsdBridgeTokens->data_lookup_color; break; }
      case DMI::EMISSIVEINTENSITY: { return UsdBridgeTokens->data_lookup_float; break; } 
      case DMI::ROUGHNESS: { return UsdBridgeTokens->data_lookup_float; break; } 
      case DMI::METALLIC: { return UsdBridgeTokens->data_lookup_float; break; } 
      case DMI::IOR: { return UsdBridgeTokens->data_lookup_float; break; } 

      default: { assert(false); break; }
    };

    return UsdBridgeTokens->data_lookup_float;
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


  const TfToken& GetSamplerAssetSubidentifier(int numComponents)
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
      // Create considers the referenced uniform prims as a stronger opinion than timeVarying clip values.
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

    // Array assignment
  template<class ArrayType>
  void AssignArrayToPrimvar(const void* data, size_t numElements, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    using ElementType = typename ArrayType::ElementType;
    ElementType* typedData = (ElementType*)data;
    usdArray->assign(typedData, typedData + numElements);
  }

  template<class ArrayType>
  void AssignArrayToPrimvarFlatten(const void* data, UsdBridgeType dataType, size_t numElements, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    int elementMultiplier = UsdBridgeTypeNumComponents(dataType);
    size_t numFlattenedElements = numElements * elementMultiplier;

    AssignArrayToPrimvar<ArrayType>(data, numFlattenedElements, timeCode, usdArray);
  }

  template<class ArrayType, class EltType>
  void AssignArrayToPrimvarConvert(const void* data, size_t numElements, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    using ElementType = typename ArrayType::ElementType;
    EltType* typedData = (EltType*)data;

    usdArray->resize(numElements);
    for (int i = 0; i < numElements; ++i)
    {
      (*usdArray)[i] = ElementType(typedData[i]);
    }
  }

  template<class ArrayType, class EltType>
  void AssignArrayToPrimvarConvertFlatten(const void* data, UsdBridgeType dataType, size_t numElements, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    int elementMultiplier = UsdBridgeTypeNumComponents(dataType);
    size_t numFlattenedElements = numElements * elementMultiplier;

    AssignArrayToPrimvarConvert<ArrayType, EltType>(data, numFlattenedElements, timeCode, usdArray);
  }

  template<typename ArrayType, typename EltType>
  void Expand1ToVec3(const void* data, uint64_t numElements, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    usdArray->resize(numElements);
    const EltType* typedInput = reinterpret_cast<const EltType*>(data);
    for (int i = 0; i < numElements; ++i)
    {
      (*usdArray)[i] = typename ArrayType::ElementType(typedInput[i], typedInput[i], typedInput[i]);
    }
  }

  template<typename InputEltType, int numComponents>
  void ExpandToColor(const void* data, uint64_t numElements, const UsdTimeCode& timeCode, VtVec4fArray* usdArray)
  {
    usdArray->resize(numElements);
    const InputEltType* typedInput = reinterpret_cast<const InputEltType*>(data);
    // No memcopies, as input is not guaranteed to be of float type
    if(numComponents == 1)
      for (int i = 0; i < numElements; ++i)
        (*usdArray)[i] = GfVec4f(typedInput[i], 0.0f, 0.0f, 1.0f);
    if(numComponents == 2)
      for (int i = 0; i < numElements; ++i)
        (*usdArray)[i] = GfVec4f(typedInput[i*2], typedInput[i*2+1], 0.0f, 1.0f);
    if(numComponents == 3)
      for (int i = 0; i < numElements; ++i)
        (*usdArray)[i] = GfVec4f(typedInput[i*3], typedInput[i*3+1], typedInput[i*3+2], 1.0f);
  }

  template<typename InputEltType, int numComponents>
  void ExpandToColorNormalize(const void* data, uint64_t numElements, const UsdTimeCode& timeCode, VtVec4fArray* usdArray)
  {
    usdArray->resize(numElements);
    const InputEltType* typedInput = reinterpret_cast<const InputEltType*>(data);
    double normFactor = 1.0f / (double)std::numeric_limits<InputEltType>::max(); // float may not be enough for uint32_t
    // No memcopies, as input is not guaranteed to be of float type
    if(numComponents == 1)
      for (int i = 0; i < numElements; ++i)
        (*usdArray)[i] = GfVec4f(typedInput[i]*normFactor, 0.0f, 0.0f, 1.0f);
    if(numComponents == 2)
      for (int i = 0; i < numElements; ++i)
        (*usdArray)[i] = GfVec4f(typedInput[i*2]*normFactor, typedInput[i*2+1]*normFactor, 0.0f, 1.0f);
    if(numComponents == 3)
      for (int i = 0; i < numElements; ++i)
        (*usdArray)[i] = GfVec4f(typedInput[i*3]*normFactor, typedInput[i*3+1]*normFactor, typedInput[i*3+2]*normFactor, 1.0f);
    if(numComponents == 4)
      for (int i = 0; i < numElements; ++i)
        (*usdArray)[i] = GfVec4f(typedInput[i*4]*normFactor, typedInput[i*4+1]*normFactor, typedInput[i*4+2]*normFactor, typedInput[i*4+3]*normFactor);
  }

  template<int numComponents>
  void ExpandSRGBToColor(const void* data, uint64_t numElements, const UsdTimeCode& timeCode, VtVec4fArray* usdArray)
  {
    usdArray->resize(numElements);
    const unsigned char* typedInput = reinterpret_cast<const unsigned char*>(data);
    float normFactor = 1.0f / 255.0f;
    const float* srgbTable = ubutils::SrgbToLinearTable();
    // No memcopies, as input is not guaranteed to be of float type
    if(numComponents == 1)
      for (int i = 0; i < numElements; ++i)
        (*usdArray)[i] = GfVec4f(srgbTable[typedInput[i]], 0.0f, 0.0f, 1.0f);
    if(numComponents == 2)
      for (int i = 0; i < numElements; ++i)
        (*usdArray)[i] = GfVec4f(srgbTable[typedInput[i*2]], 0.0f, 0.0f, typedInput[i*2+1]*normFactor); // Alpha is linear
    if(numComponents == 3)
      for (int i = 0; i < numElements; ++i)
        (*usdArray)[i] = GfVec4f(srgbTable[typedInput[i*3]], srgbTable[typedInput[i*3+1]], srgbTable[typedInput[i*3+2]], 1.0f);
    if(numComponents == 4)
      for (int i = 0; i < numElements; ++i)
        (*usdArray)[i] = GfVec4f(srgbTable[typedInput[i*4]], srgbTable[typedInput[i*4+1]], srgbTable[typedInput[i*4+2]], typedInput[i*4+3]*normFactor);
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

#define ASSIGN_SET_PRIMVAR if(setPrimvar) arrayPrimvar.Set(usdArray, timeCode)
#define ASSIGN_PRIMVAR_MACRO(ArrayType) \
  ArrayType& usdArray = GetStaticTempArray<ArrayType>(); AssignArrayToPrimvar<ArrayType>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_FLATTEN_MACRO(ArrayType) \
  ArrayType& usdArray = GetStaticTempArray<ArrayType>(); AssignArrayToPrimvarFlatten<ArrayType>(arrayData, arrayDataType, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_CONVERT_MACRO(ArrayType, EltType) \
  ArrayType& usdArray = GetStaticTempArray<ArrayType>(); AssignArrayToPrimvarConvert<ArrayType, EltType>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_CONVERT_FLATTEN_MACRO(ArrayType, EltType) \
  ArrayType& usdArray = GetStaticTempArray<ArrayType>(); AssignArrayToPrimvarConvertFlatten<ArrayType, EltType>(arrayData, arrayDataType, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_CUSTOM_ARRAY_MACRO(ArrayType, customArray) \
  ArrayType& usdArray = customArray; AssignArrayToPrimvar<ArrayType>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_CONVERT_CUSTOM_ARRAY_MACRO(ArrayType, EltType, customArray) \
  ArrayType& usdArray = customArray; AssignArrayToPrimvarConvert<ArrayType, EltType>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_1EXPAND3(ArrayType, EltType) \
  ArrayType& usdArray = GetStaticTempArray<ArrayType>(); Expand1ToVec3<ArrayType, EltType>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_1EXPAND_COL(EltType) \
  VtVec4fArray& usdArray = GetStaticTempArray<VtVec4fArray>(); ExpandToColor<EltType, 1>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_2EXPAND_COL(EltType) \
  VtVec4fArray& usdArray = GetStaticTempArray<VtVec4fArray>(); ExpandToColor<EltType, 2>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_3EXPAND_COL(EltType) \
  VtVec4fArray& usdArray = GetStaticTempArray<VtVec4fArray>(); ExpandToColor<EltType, 3>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_1EXPAND_NORMALIZE_COL(EltType) \
  VtVec4fArray& usdArray = GetStaticTempArray<VtVec4fArray>(); ExpandToColorNormalize<EltType, 1>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_2EXPAND_NORMALIZE_COL(EltType) \
  VtVec4fArray& usdArray = GetStaticTempArray<VtVec4fArray>(); ExpandToColorNormalize<EltType, 2>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_3EXPAND_NORMALIZE_COL(EltType) \
  VtVec4fArray& usdArray = GetStaticTempArray<VtVec4fArray>(); ExpandToColorNormalize<EltType, 3>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_4EXPAND_NORMALIZE_COL(EltType) \
  VtVec4fArray& usdArray = GetStaticTempArray<VtVec4fArray>(); ExpandToColorNormalize<EltType, 4>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_1EXPAND_SGRB() \
  VtVec4fArray& usdArray = GetStaticTempArray<VtVec4fArray>(); ExpandSRGBToColor<1>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_2EXPAND_SGRB() \
  VtVec4fArray& usdArray = GetStaticTempArray<VtVec4fArray>(); ExpandSRGBToColor<2>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_3EXPAND_SGRB() \
  VtVec4fArray& usdArray = GetStaticTempArray<VtVec4fArray>(); ExpandSRGBToColor<3>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_PRIMVAR_MACRO_4EXPAND_SGRB() \
  VtVec4fArray& usdArray = GetStaticTempArray<VtVec4fArray>(); ExpandSRGBToColor<4>(arrayData, arrayNumElements, timeCode, &usdArray); ASSIGN_SET_PRIMVAR

#define GET_USDARRAY_REF usdArrayRef = &usdArray

namespace
{
  // Assigns color data array to VtVec4fArray primvar
  VtVec4fArray* AssignColorArrayToPrimvar(UsdBridgeUsdWriter* writer, const void* arrayData, size_t arrayNumElements, UsdBridgeType arrayType, UsdTimeCode timeCode, const UsdAttribute& arrayPrimvar, bool setPrimvar = true)
  {
    VtVec4fArray* usdArrayRef = nullptr;
    switch (arrayType)
    {
      case UsdBridgeType::UCHAR: {ASSIGN_PRIMVAR_MACRO_1EXPAND_NORMALIZE_COL(uint8_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::UCHAR2: {ASSIGN_PRIMVAR_MACRO_2EXPAND_NORMALIZE_COL(uint8_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::UCHAR3: {ASSIGN_PRIMVAR_MACRO_3EXPAND_NORMALIZE_COL(uint8_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::UCHAR4: {ASSIGN_PRIMVAR_MACRO_4EXPAND_NORMALIZE_COL(uint8_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::UCHAR_SRGB_R: {ASSIGN_PRIMVAR_MACRO_1EXPAND_SGRB(); GET_USDARRAY_REF; break; }
      case UsdBridgeType::UCHAR_SRGB_RA: {ASSIGN_PRIMVAR_MACRO_2EXPAND_SGRB(); GET_USDARRAY_REF; break; }
      case UsdBridgeType::UCHAR_SRGB_RGB: {ASSIGN_PRIMVAR_MACRO_3EXPAND_SGRB(); GET_USDARRAY_REF; break; }
      case UsdBridgeType::UCHAR_SRGB_RGBA: {ASSIGN_PRIMVAR_MACRO_4EXPAND_SGRB(); GET_USDARRAY_REF; break; }
      case UsdBridgeType::USHORT: {ASSIGN_PRIMVAR_MACRO_1EXPAND_NORMALIZE_COL(uint16_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::USHORT2: {ASSIGN_PRIMVAR_MACRO_2EXPAND_NORMALIZE_COL(uint16_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::USHORT3: {ASSIGN_PRIMVAR_MACRO_3EXPAND_NORMALIZE_COL(uint16_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::USHORT4: {ASSIGN_PRIMVAR_MACRO_4EXPAND_NORMALIZE_COL(uint16_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::UINT: {ASSIGN_PRIMVAR_MACRO_1EXPAND_NORMALIZE_COL(uint32_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::UINT2: {ASSIGN_PRIMVAR_MACRO_2EXPAND_NORMALIZE_COL(uint32_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::UINT3: {ASSIGN_PRIMVAR_MACRO_3EXPAND_NORMALIZE_COL(uint32_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::UINT4: {ASSIGN_PRIMVAR_MACRO_4EXPAND_NORMALIZE_COL(uint32_t); GET_USDARRAY_REF; break; }
      case UsdBridgeType::FLOAT: {ASSIGN_PRIMVAR_MACRO_1EXPAND_COL(float); GET_USDARRAY_REF; break; }
      case UsdBridgeType::FLOAT2: {ASSIGN_PRIMVAR_MACRO_2EXPAND_COL(float); GET_USDARRAY_REF; break; }
      case UsdBridgeType::FLOAT3: {ASSIGN_PRIMVAR_MACRO_3EXPAND_COL(float); GET_USDARRAY_REF; break; }
      case UsdBridgeType::FLOAT4: {ASSIGN_PRIMVAR_MACRO(VtVec4fArray); GET_USDARRAY_REF; break; }
      case UsdBridgeType::DOUBLE: {ASSIGN_PRIMVAR_MACRO_1EXPAND_COL(double); GET_USDARRAY_REF; break; }
      case UsdBridgeType::DOUBLE2: {ASSIGN_PRIMVAR_MACRO_2EXPAND_COL(double); GET_USDARRAY_REF; break; }
      case UsdBridgeType::DOUBLE3: {ASSIGN_PRIMVAR_MACRO_3EXPAND_COL(double); GET_USDARRAY_REF; break; }
      case UsdBridgeType::DOUBLE4: {ASSIGN_PRIMVAR_CONVERT_MACRO(VtVec4fArray, GfVec4d); GET_USDARRAY_REF; break; }
      default: { UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom color primvar is not of type (UCHAR/USHORT/UINT/FLOAT/DOUBLE)(1/2/3/4) or UCHAR_SRGB_<X>."); break; }
    }

    return usdArrayRef;
  }

  void AssignAttribArrayToPrimvar(UsdBridgeUsdWriter* writer, const void* arrayData, UsdBridgeType arrayDataType, size_t arrayNumElements, const UsdAttribute& arrayPrimvar, const UsdTimeCode& timeCode)
  {
    bool setPrimvar = true;
    switch (arrayDataType)
    {
      case UsdBridgeType::UCHAR: { ASSIGN_PRIMVAR_MACRO(VtUCharArray); break; }
      case UsdBridgeType::UCHAR_SRGB_R: { ASSIGN_PRIMVAR_MACRO(VtUCharArray); break; }
      case UsdBridgeType::CHAR: { ASSIGN_PRIMVAR_MACRO(VtUCharArray); break; }
      case UsdBridgeType::USHORT: { ASSIGN_PRIMVAR_CONVERT_MACRO(VtUIntArray, short); break; }
      case UsdBridgeType::SHORT: { ASSIGN_PRIMVAR_CONVERT_MACRO(VtIntArray, unsigned short); break; }
      case UsdBridgeType::UINT: { ASSIGN_PRIMVAR_MACRO(VtUIntArray); break; }
      case UsdBridgeType::INT: { ASSIGN_PRIMVAR_MACRO(VtIntArray); break; }
      case UsdBridgeType::LONG: { ASSIGN_PRIMVAR_MACRO(VtInt64Array); break; }
      case UsdBridgeType::ULONG: { ASSIGN_PRIMVAR_MACRO(VtUInt64Array); break; }
      case UsdBridgeType::HALF: { ASSIGN_PRIMVAR_MACRO(VtHalfArray); break; }
      case UsdBridgeType::FLOAT: { ASSIGN_PRIMVAR_MACRO(VtFloatArray); break; }
      case UsdBridgeType::DOUBLE: { ASSIGN_PRIMVAR_MACRO(VtDoubleArray); break; }

      case UsdBridgeType::INT2: { ASSIGN_PRIMVAR_MACRO(VtVec2iArray); break; }
      case UsdBridgeType::FLOAT2: { ASSIGN_PRIMVAR_MACRO(VtVec2fArray); break; }
      case UsdBridgeType::DOUBLE2: { ASSIGN_PRIMVAR_MACRO(VtVec2dArray); break; }

      case UsdBridgeType::INT3: { ASSIGN_PRIMVAR_MACRO(VtVec3iArray); break; }
      case UsdBridgeType::FLOAT3: { ASSIGN_PRIMVAR_MACRO(VtVec3fArray); break; }
      case UsdBridgeType::DOUBLE3: { ASSIGN_PRIMVAR_MACRO(VtVec3dArray); break; }

      case UsdBridgeType::INT4: { ASSIGN_PRIMVAR_MACRO(VtVec4iArray); break; }
      case UsdBridgeType::FLOAT4: { ASSIGN_PRIMVAR_MACRO(VtVec4fArray); break; }
      case UsdBridgeType::DOUBLE4: { ASSIGN_PRIMVAR_MACRO(VtVec4dArray); break; }

      case UsdBridgeType::UCHAR2:
      case UsdBridgeType::UCHAR3: 
      case UsdBridgeType::UCHAR4: { ASSIGN_PRIMVAR_FLATTEN_MACRO(VtUCharArray); break; }
      case UsdBridgeType::UCHAR_SRGB_RA:
      case UsdBridgeType::UCHAR_SRGB_RGB: 
      case UsdBridgeType::UCHAR_SRGB_RGBA: { ASSIGN_PRIMVAR_FLATTEN_MACRO(VtUCharArray); break; }
      case UsdBridgeType::CHAR2:
      case UsdBridgeType::CHAR3: 
      case UsdBridgeType::CHAR4: { ASSIGN_PRIMVAR_FLATTEN_MACRO(VtUCharArray); break; }
      case UsdBridgeType::USHORT2:
      case UsdBridgeType::USHORT3:
      case UsdBridgeType::USHORT4: { ASSIGN_PRIMVAR_CONVERT_FLATTEN_MACRO(VtUIntArray, short); break; }
      case UsdBridgeType::SHORT2:
      case UsdBridgeType::SHORT3: 
      case UsdBridgeType::SHORT4: { ASSIGN_PRIMVAR_CONVERT_FLATTEN_MACRO(VtIntArray, unsigned short); break; }
      case UsdBridgeType::UINT2:
      case UsdBridgeType::UINT3: 
      case UsdBridgeType::UINT4: { ASSIGN_PRIMVAR_FLATTEN_MACRO(VtUIntArray); break; }
      case UsdBridgeType::LONG2:
      case UsdBridgeType::LONG3: 
      case UsdBridgeType::LONG4: { ASSIGN_PRIMVAR_FLATTEN_MACRO(VtInt64Array); break; }
      case UsdBridgeType::ULONG2:
      case UsdBridgeType::ULONG3: 
      case UsdBridgeType::ULONG4: { ASSIGN_PRIMVAR_FLATTEN_MACRO(VtUInt64Array); break; }
      case UsdBridgeType::HALF2:
      case UsdBridgeType::HALF3: 
      case UsdBridgeType::HALF4: { ASSIGN_PRIMVAR_FLATTEN_MACRO(VtHalfArray); break; }

      default: {UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom Attribute<Index> primvar copy does not support source data type: " << arrayDataType) break; }
    };
  }
}


#endif