// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeUsdWriterConvenience_h
#define UsdBridgeUsdWriterConvenience_h

#include "UsdBridgeTimeEvaluator.h"
#include "UsdBridgeData.h"

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
  (UsdUVTexture) \
  (fallback) \
  (r) \
  (rg) \
  (rgb) \
  (a) \
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

#define MDL_TOKEN_SEQ \
  (sourceAsset) \
  (mdl) \
  (OmniPBR) \
  (out) \
  (data_lookup_float) \
  (data_lookup_float2) \
  (data_lookup_float3) \
  (data_lookup_color) \
  (lookup_color) \
  (coord)

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
  (wrap_w)

#define VOLUME_TOKEN_SEQ \
  (density) \
  (filePath)

TF_DECLARE_PUBLIC_TOKENS(
  UsdBridgeTokens,

  ATTRIB_TOKEN_SEQ
  USDPREVSURF_TOKEN_SEQ
  USDPREVSURF_INPUT_TOKEN_SEQ
  MDL_TOKEN_SEQ
  MDL_INPUT_TOKEN_SEQ
  VOLUME_TOKEN_SEQ
  MISC_TOKEN_SEQ
);

TF_DECLARE_PUBLIC_TOKENS(
  QualifiedTokens,

  USDPREVSURF_INPUT_TOKEN_SEQ
  MDL_INPUT_TOKEN_SEQ
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

#ifdef CUSTOM_PBR_MDL
  extern const char* const mdlFolder;

  extern const char* const opaqueMaterialFile;
  extern const char* const transparentMaterialFile;
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

  TfToken VolumeFieldTypeToken(UsdBridgeVolumeFieldType fieldType)
  {
    TfToken token = UsdBridgeTokens->density;
    switch (fieldType)
    {
    case UsdBridgeVolumeFieldType::DENSITY:
      token = UsdBridgeTokens->density;
      break;
    case UsdBridgeVolumeFieldType::COLOR:
      token = UsdBridgeTokens->color;
      break;
    default:
      break;
    };
    return token;
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
        case DMI::DIFFUSE: { return QualifiedTokens->diffuseColor; break; } 
        case DMI::OPACITY: { return QualifiedTokens->opacity; break; } 
        case DMI::EMISSIVECOLOR: { return QualifiedTokens->emissiveColor; break; } 
        case DMI::ROUGHNESS: { return QualifiedTokens->roughness; break; } 
        case DMI::METALLIC: { return QualifiedTokens->metallic; break; } 
        case DMI::IOR: { return QualifiedTokens->ior; break; } 

        default: assert(false); break;
      };
    }
    else
    {
      switch(dataMemberId)
      {
        case DMI::DIFFUSE: { return QualifiedTokens->diffuse_color_constant; break; } 
        case DMI::OPACITY: { return QualifiedTokens->opacity_constant; break; } 
        case DMI::EMISSIVECOLOR: { return QualifiedTokens->emissive_color; break; } 
        case DMI::EMISSIVEINTENSITY: { return QualifiedTokens->emissive_intensity; break; } 
        case DMI::ROUGHNESS: { return QualifiedTokens->reflection_roughness_constant; break; } 
        case DMI::METALLIC: { return QualifiedTokens->metallic_constant; break; } 
        case DMI::IOR: { return QualifiedTokens->ior_constant; break; } 

        default: assert(false); break;
      };
    }

    return QualifiedTokens->diffuseColor;  
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

  const TfToken& GetUsdAttributeReaderId(UsdBridgeMaterialData::DataMemberId dataMemberId)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    switch(dataMemberId)
    {
      case DMI::DIFFUSE: { return UsdBridgeTokens->PrimVarReader_Float3; break; } 
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

  const SdfValueTypeName& GetShaderNodeOutputType(UsdBridgeMaterialData::DataMemberId dataMemberId)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    switch(dataMemberId)
    {
      case DMI::DIFFUSE: { return SdfValueTypeNames->Color3f; break; } 
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

  const SdfValueTypeName& GetSamplerOutputColorType(int numComponents)
  {
    switch(numComponents)
    {
      case 1: { return SdfValueTypeNames->Float; break; }
      case 2: { return SdfValueTypeNames->Float2; break; }
      default: { return SdfValueTypeNames->Float3; break; } // The alpha component is always separate
    }
    return SdfValueTypeNames->Float3;
  }

  template<typename OutputArrayType, typename InputEltType>
  void ExpandToVec3(OutputArrayType& output, const void* input, uint64_t numElements)
  {
    const InputEltType* typedInput = reinterpret_cast<const InputEltType*>(input);
    for (int i = 0; i < numElements; ++i)
    {
      output[i] = typename OutputArrayType::ElementType(typedInput[i], typedInput[i], typedInput[i]);
    }
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
}


#endif