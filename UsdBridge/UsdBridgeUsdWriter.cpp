// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeUsdWriter.h"

#include "UsdBridgeCaches.h"
#include "UsdBridgeMdlStrings.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <memory>

#define UsdBridgeLogMacro(obj, level, message) \
  { std::stringstream logStream; \
    logStream << message; \
    std::string logString = logStream.str(); \
    obj->LogCallback(level, obj->LogUserData, logString.c_str()); } 

TF_DEFINE_PRIVATE_TOKENS(
  UsdBridgeTokens,
  (Root)

  (extent)
  (faceVertexCounts)
  (faceVertexIndices)
  (points)
  (positions)
  (normals)
  (st)
  (st1)
  (st2)
  (attribute0)
  (attribute1)
  (attribute2)
  (attribute3)
  (result)
  (displayColor)
  (ids)
  (widths)
  (protoIndices)
  (orientations)
  (scales)
  (velocities)
  (angularVelocities)
  (invisibleIds)
  (curveVertexCounts)
  (filePath)

  // For usd preview surface
  (UsdPreviewSurface)
  (useSpecularWorkflow)
  (roughness)
  (opacity)
  (metallic)
  (ior)
  (diffuseColor)
  (specularColor)
  (emissiveColor)
  (surface)
  (varname)
  ((PrimStId, "UsdPrimvarReader_float2"))
  ((PrimDisplayColorId, "UsdPrimvarReader_float3"))

  // Volumes
  (density)
  (color)

  // Mdl
  (sourceAsset)
  (PBR_Base)
  (mdl)
  (vertexcolor_coordinate_index)
  (diffuse_color_constant)
  (emissive_color)
  (emissive_intensity)
  (enable_emission)
  (opacity_constant)
  (reflection_roughness_constant)
  (metallic_constant)
  (ior_constant)
  (diffuse_texture)

  // Texture/Sampler
  (UsdUVTexture)
  (fallback)
  (rgb)
  (a)
  (file)
  (WrapS)
  (WrapT)
  (WrapR)
  (black)
  (clamp)
  (repeat)
  (mirror)
);

template<typename UsdGeomType>
UsdAttribute UsdGeomGetPointsAttribute(UsdGeomType& usdGeom) { return UsdAttribute(); }

template<>
UsdAttribute UsdGeomGetPointsAttribute(UsdGeomMesh& usdGeom) { return usdGeom.GetPointsAttr(); }

template<>
UsdAttribute UsdGeomGetPointsAttribute(UsdGeomPoints& usdGeom) { return usdGeom.GetPointsAttr(); }

template<>
UsdAttribute UsdGeomGetPointsAttribute(UsdGeomBasisCurves& usdGeom) { return usdGeom.GetPointsAttr(); }

template<>
UsdAttribute UsdGeomGetPointsAttribute(UsdGeomPointInstancer& usdGeom) { return usdGeom.GetPositionsAttr(); }


namespace
{
  // Folder names
  const char* const manifestFolder = "manifests/";
  const char* const clipFolder = "clips/";
  const char* const primStageFolder = "primstages/";
  const char* const mdlFolder = "mdls/";
  const char* const imgFolder = "images/";
  const char* const volFolder = "volumes/";

  // Postfixes for auto generated usd subprims
  const char* const texCoordReaderPrimPf = "texcoordreader";
  const char* const vertexColorReaderPrimPf = "vertexcolorreader";
  const char* const shaderPrimPf = "shader";
  const char* const mdlShaderPrimPf = "mdlshader";
  const char* const openVDBPrimPf = "ovdbfield";

  // Extensions
  const char* const imageExtension = ".png";
  const char* const vdbExtension = ".vdb";

  TfToken GetTokenFromFieldType(UsdBridgeVolumeFieldType fieldType)
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

  std::string GetResourceFileName(const std::string& basePath, double timeStep, const char* fileExtension)
  {
    std::string generatedFileName(basePath);
#ifdef TIME_BASED_CACHING
    generatedFileName += "_" + std::to_string(timeStep);
#endif
    generatedFileName += fileExtension; 

    return generatedFileName;
  }

  std::string GetResourceFileName(const char* folderName, const std::string& objectName, double timeStep, const char* fileExtension)
  {
    return GetResourceFileName(folderName + objectName, timeStep, fileExtension);
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

  void BlockFieldRelationships(UsdVolVolume& volume, UsdBridgeVolumeFieldType* exception = nullptr)
  {
    UsdBridgeVolumeFieldType fieldTypes[] = {
      UsdBridgeVolumeFieldType::DENSITY,
      UsdBridgeVolumeFieldType::COLOR
    };
    for (int i = 0; i < sizeof(fieldTypes) / sizeof(UsdBridgeVolumeFieldType); ++i)
    {
      if (exception && fieldTypes[i] == *exception)
        continue;
      TfToken fieldToken = GetTokenFromFieldType(fieldTypes[i]);
      if (volume.HasFieldRelationship(fieldToken))
        volume.BlockFieldRelationship(fieldToken);
    }
  }

  template<class T>
  class TimeEvaluator
  {
  public:
    TimeEvaluator(const T& data, double timeStep)
      : Data(data)
      , TimeCode(timeStep)
    {
    }

    TimeEvaluator(const T& data)
      : Data(data)
    {
    }

    const UsdTimeCode& Eval(typename T::DataMemberId member) const
    {
#ifdef TIME_BASED_CACHING
      if ((Data.TimeVarying & member) != T::DataMemberId::NONE)
        return TimeCode;
      else
#endif
      return DefaultTime;
    }

    bool IsTimeVarying(typename T::DataMemberId member) const
    {
#ifdef TIME_BASED_CACHING
      return ((Data.TimeVarying & member) != T::DataMemberId::NONE);
#else
      return false;
#endif
    }

    UsdTimeCode Default() const { return DefaultTime; }

    const T& Data;
    const UsdTimeCode TimeCode;
    static const UsdTimeCode DefaultTime;
  };

  template<class T>
  const UsdTimeCode TimeEvaluator<T>::DefaultTime = UsdTimeCode::Default();

  template<>
  class TimeEvaluator<bool>
  {
  public:
    TimeEvaluator(bool timeVarying, double timeStep)
      : TimeVarying(timeVarying)
      , TimeCode(timeStep)
    {

    }

    const UsdTimeCode& Eval() const
    {
#ifdef TIME_BASED_CACHING
      if (TimeVarying)
        return TimeCode;
      else
#endif
      return DefaultTime;
    }

    UsdTimeCode Default() const { return DefaultTime; }

    const bool TimeVarying;
    const UsdTimeCode TimeCode;
    static const UsdTimeCode DefaultTime;
  };

  const UsdTimeCode TimeEvaluator<bool>::DefaultTime = UsdTimeCode::Default();

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

  template<class T>
  T GetOrDefinePrim(const UsdStageRefPtr& stage, const SdfPath& path)
  {
    T prim = T::Get(stage, path);
    if (!prim)
      prim = T::Define(stage, path);
    assert(prim);

    return prim;
  }

  template<class ArrayType>
  void AssignArrayToPrimvar(const void* data, size_t numElements, UsdAttribute& primvar, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    using ElementType = typename ArrayType::ElementType;
    ElementType* typedData = (ElementType*)data;
    usdArray->assign(typedData, typedData + numElements);

    primvar.Set(*usdArray, timeCode);
  }

  template<class ArrayType>
  void AssignArrayToPrimvarFlatten(const void* data, UsdBridgeType dataType, size_t numElements, UsdAttribute& primvar, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    int elementMultiplier = (int)dataType / UsdBridgeNumFundamentalTypes;
    size_t numFlattenedElements = numElements * elementMultiplier;

    AssignArrayToPrimvar<ArrayType>(data, numFlattenedElements, primvar, timeCode, usdArray);
  }

  template<class ArrayType, class EltType>
  void AssignArrayToPrimvarConvert(const void* data, size_t numElements, UsdAttribute& primvar, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    using ElementType = typename ArrayType::ElementType;
    EltType* typedData = (EltType*)data;

    usdArray->resize(numElements);
    for (int i = 0; i < numElements; ++i)
    {
      (*usdArray)[i] = ElementType(typedData[i]);
    }

    primvar.Set(*usdArray, timeCode);
  }

  template<class ArrayType, class EltType>
  void AssignArrayToPrimvarConvertFlatten(const void* data, UsdBridgeType dataType, size_t numElements, UsdAttribute& primvar, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    int elementMultiplier = (int)dataType / UsdBridgeNumFundamentalTypes;
    size_t numFlattenedElements = numElements * elementMultiplier;

    AssignArrayToPrimvarConvert<ArrayType, EltType>(data, numFlattenedElements, primvar, timeCode, usdArray);
  }

  template<class ArrayType, class EltType>
  void AssignArrayToPrimvarReduced(const void* data, size_t numElements, UsdAttribute& primvar, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    using ElementType = typename ArrayType::ElementType;
    EltType* typedData = (EltType*)data;

    usdArray->resize(numElements);
    for (int i = 0; i < numElements; ++i)
    {
      for (int j = 0; j < ElementType::dimension; ++j)
        (*usdArray)[i][j] = typedData[i][j];
    }

    primvar.Set(*usdArray, timeCode);
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

  void ClearAttributes(const UsdAttribute& uniformAttrib, const UsdAttribute& timeVarAttrib, bool timeVaryingUpdate)
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

  template<typename GeomDataType>
  void CreateUsdGeomColorPrimvars(UsdGeomPrimvarsAPI& primvarApi, const GeomDataType& geomData, const UsdBridgeSettings& settings, const TimeEvaluator<GeomDataType>* timeEval = nullptr)
  {
    using DMI = typename GeomDataType::DataMemberId;

    bool timeVarChecked = true;
    if(timeEval)
    {
      timeVarChecked = timeEval->IsTimeVarying(DMI::COLORS);
    }

    if (timeVarChecked)
    {
      if(settings.EnableDisplayColors)
      {
        primvarApi.CreatePrimvar(UsdBridgeTokens->displayColor, SdfValueTypeNames->Color3fArray);
      }
  #ifdef SUPPORT_MDL_SHADERS
      if(settings.EnableMdlColors)
      {
        primvarApi.CreatePrimvar(UsdBridgeTokens->st1, SdfValueTypeNames->TexCoord2fArray);
        primvarApi.CreatePrimvar(UsdBridgeTokens->st2, SdfValueTypeNames->TexCoord2fArray);
      }
  #endif
    }
    else
    {
      if(settings.EnableDisplayColors)
      {
        primvarApi.RemovePrimvar(UsdBridgeTokens->displayColor);
      }
  #ifdef SUPPORT_MDL_SHADERS
      if(settings.EnableMdlColors)
      {
        primvarApi.RemovePrimvar(UsdBridgeTokens->st1);
        primvarApi.RemovePrimvar(UsdBridgeTokens->st2);
      }
  #endif
    }
  }

  template<typename GeomDataType>
  void CreateUsdGeomTexturePrimvars(UsdGeomPrimvarsAPI& primvarApi, const GeomDataType& geomData, const UsdBridgeSettings& settings, const TimeEvaluator<GeomDataType>* timeEval = nullptr)
  {
    using DMI = typename GeomDataType::DataMemberId;

    bool timeVarChecked = true;
    if(timeEval)
    {
      timeVarChecked = timeEval->IsTimeVarying(DMI::ATTRIBUTE0);
    }

    if (timeVarChecked /*&& (UsdGeomDataHasTexCoords(geomData) || settings.EnableMdlColors)*/)
      primvarApi.CreatePrimvar(UsdBridgeTokens->st, SdfValueTypeNames->TexCoord2fArray);
    else if (timeEval)
      primvarApi.RemovePrimvar(UsdBridgeTokens->st);
  }

  template<typename GeomDataType>
  void CreateUsdGeomAttributePrimvars(UsdGeomPrimvarsAPI& primvarApi, const GeomDataType& geomData, const TimeEvaluator<GeomDataType>* timeEval = nullptr)
  {
    using DMI = typename GeomDataType::DataMemberId;

    //bool hasTexCoords = UsdGeomDataHasTexCoords(geomData);
    //if(hasTexCoords && timeEval)
    //    primvarApi.RemovePrimvar(AttribIndexToToken(0));

    for(uint32_t attribIndex = /*hasTexCoords ? 1 :*/ 0; attribIndex < geomData.NumAttributes; ++attribIndex)
    {
      bool timeVarChecked = true;
      if(timeEval)
      {
        DMI attributeId = DMI::ATTRIBUTE0 + attribIndex;
        timeVarChecked = timeEval->IsTimeVarying(attributeId);
      }

      const UsdBridgeAttribute& attrib = geomData.Attributes[attribIndex];
      if(timeVarChecked)
      {
        SdfValueTypeName primvarType = GetPrimvarArrayType(attrib.DataType);
        primvarApi.CreatePrimvar(AttribIndexToToken(attribIndex), primvarType);
      }
      else if(timeEval)
      {
        primvarApi.RemovePrimvar(AttribIndexToToken(attribIndex));
      }
    }
  }

  template<typename GeomDataType>
  void CreateShaderInput(UsdShadeShader shader, const TimeEvaluator<GeomDataType>* timeEval, typename GeomDataType::DataMemberId dataMemberId, 
    const TfToken& inputToken, const char* inputTokenName, const SdfValueTypeName& valueType)
  {
    if(!timeEval || timeEval->IsTimeVarying(dataMemberId))
      shader.CreateInput(inputToken, valueType);
    else
      shader.GetPrim().RemoveProperty(TfToken(inputTokenName));
  }

  template<typename ValueType, typename GeomDataType>
  void SetShaderInput(UsdShadeShader uniformShadPrim, UsdShadeShader timeVarShadPrim, const TimeEvaluator<GeomDataType>& timeEval, 
    const TfToken& inputToken, typename GeomDataType::DataMemberId dataMemberId, ValueType value)
  {
    using DMI = typename GeomDataType::DataMemberId;

    bool timeVaryingUpdate = timeEval.IsTimeVarying(dataMemberId);
    UsdShadeInput timeVarInput = timeVarShadPrim.GetInput(inputToken);
    UsdShadeInput uniformInput = uniformShadPrim.GetInput(inputToken);

    ClearAttributes(uniformInput.GetAttr(), timeVarInput.GetAttr(), timeVaryingUpdate);

    if(timeVaryingUpdate)
      timeVarInput.Set(value, timeEval.Eval(dataMemberId));
    else
      uniformInput.Set(value, timeEval.Eval(dataMemberId));
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

  template<typename OutputArrayType, typename InputEltType>
  void ExpandToVec3(OutputArrayType& output, const void* input, uint64_t numElements)
  {
    const InputEltType* typedInput = reinterpret_cast<const InputEltType*>(input);
    for (int i = 0; i < numElements; ++i)
    {
      output[i] = typename OutputArrayType::ElementType(typedInput[i], typedInput[i], typedInput[i]);
    }
  }
}

#define ASSIGN_ARRAY_TO_PRIMVAR_MACRO(ArrayType) \
  ArrayType usdArray; AssignArrayToPrimvar<ArrayType>(arrayData, arrayNumElements, arrayPrimvar, timeCode, &usdArray)
#define ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(ArrayType) \
  ArrayType usdArray; AssignArrayToPrimvarFlatten<ArrayType>(arrayData, arrayDataType, arrayNumElements, arrayPrimvar, timeCode, &usdArray)
#define ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(ArrayType, EltType) \
  ArrayType usdArray; AssignArrayToPrimvarConvert<ArrayType, EltType>(arrayData, arrayNumElements, arrayPrimvar, timeCode, &usdArray)
#define ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_FLATTEN_MACRO(ArrayType, EltType) \
  ArrayType usdArray; AssignArrayToPrimvarConvertFlatten<ArrayType, EltType>(arrayData, arrayDataType, arrayNumElements, arrayPrimvar, timeCode, &usdArray)
#define ASSIGN_ARRAY_TO_PRIMVAR_REDUCED_MACRO(ArrayType, EltType) \
  ArrayType usdArray; AssignArrayToPrimvarReduced<ArrayType, EltType>(arrayData, arrayNumElements, arrayPrimvar, timeCode, &usdArray)
#define ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_MACRO(ArrayType, customArray) \
  AssignArrayToPrimvar<ArrayType>(arrayData, arrayNumElements, arrayPrimvar, timeCode, &customArray)
#define ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_CONVERT_MACRO(ArrayType, EltType, customArray) \
  AssignArrayToPrimvarConvert<ArrayType, EltType>(arrayData, arrayNumElements, arrayPrimvar, timeCode, &customArray)
#define ASSIGN_ARRAY_TO_PRIMVAR_MACRO_EXPAND3(ArrayType, EltType, tempArray) \
  tempArray.resize(arrayNumElements); ExpandToVec3<ArrayType, EltType>(tempArray, arrayData, arrayNumElements); arrayPrimvar.Set(tempArray, timeCode);

UsdBridgeUsdWriter::UsdBridgeUsdWriter(const UsdBridgeSettings& settings)
  : Settings(settings)
  , VolumeWriter(Create_VolumeWriter(), std::mem_fn(&UsdBridgeVolumeWriterI::Release))
{
  if(Settings.HostName)
    ConnectionSettings.HostName = Settings.HostName;
  if(Settings.OutputPath)
    ConnectionSettings.WorkingDirectory = Settings.OutputPath;
  FormatDirName(ConnectionSettings.WorkingDirectory);
}

UsdBridgeUsdWriter::~UsdBridgeUsdWriter()
{
}

void UsdBridgeUsdWriter::SetSceneStage(UsdStageRefPtr sceneStage)
{
  this->SceneStage = sceneStage;
}

void UsdBridgeUsdWriter::SetEnableSaving(bool enableSaving)
{
  this->EnableSaving = enableSaving;
}

int UsdBridgeUsdWriter::FindSessionNumber()
{
  int sessionNr = Connect->MaxSessionNr();

  sessionNr = std::max(0, sessionNr + Settings.CreateNewSession);

  return sessionNr;
}

bool UsdBridgeUsdWriter::CreateDirectories()
{
  bool valid = true;

  valid = Connect->CreateFolder("", true, true);

  //Connect->RemoveFolder(SessionDirectory.c_str(), true);
  bool folderMayExist = !Settings.CreateNewSession;
  
  valid = valid && Connect->CreateFolder(SessionDirectory.c_str(), true, folderMayExist);

#ifdef VALUE_CLIP_RETIMING
  valid = valid && Connect->CreateFolder((SessionDirectory + manifestFolder).c_str(), true, folderMayExist);
  valid = valid && Connect->CreateFolder((SessionDirectory + primStageFolder).c_str(), true, folderMayExist);
#endif
#ifdef TIME_CLIP_STAGES
  valid = valid && Connect->CreateFolder((SessionDirectory + clipFolder).c_str(), true, folderMayExist);
#endif
#ifdef SUPPORT_MDL_SHADERS
  if(Settings.EnableMdlShader)
  {
    valid = valid && Connect->CreateFolder((SessionDirectory + mdlFolder).c_str(), true, folderMayExist);
  }
#endif
  valid = valid && Connect->CreateFolder((SessionDirectory + volFolder).c_str(), true, folderMayExist);

  if (!valid)
  {
    UsdBridgeLogMacro(this, UsdBridgeLogLevel::ERR, "Something went wrong in the filesystem creating the required output folders (permissions?).");
  }

  return valid;
}

#ifdef SUPPORT_MDL_SHADERS 
void WriteMdlFromStrings(const char* string0, const char* string1, const char* fileName, const UsdBridgeConnection* Connect)
{
  size_t strLen0 = std::strlen(string0);
  size_t strLen1 = std::strlen(string1);
  size_t totalStrLen = strLen0 + strLen1;
  char* Mdl_Contents = new char[totalStrLen];
  std::memcpy(Mdl_Contents, string0, strLen0);
  std::memcpy(Mdl_Contents + strLen0, string1, strLen1);

  Connect->WriteFile(Mdl_Contents, totalStrLen, fileName, true, false);

  delete[] Mdl_Contents;
}

bool UsdBridgeUsdWriter::CreateMdlFiles()
{
  //Write PBR opacity file contents
  {
    std::string relMdlPath = mdlFolder + std::string("PBR_Opaque.mdl");
    this->MdlOpaqueRelFilePath = SdfAssetPath(relMdlPath);
    std::string mdlFileName = SessionDirectory + relMdlPath;

    WriteMdlFromStrings(Mdl_PBRBase_string, Mdl_PBRBase_string_opaque, mdlFileName.c_str(), Connect.get());
  }

  {
    std::string relMdlPath = mdlFolder + std::string("PBR_Translucent.mdl");
    this->MdlTranslucentRelFilePath = SdfAssetPath(relMdlPath);
    std::string mdlFileName = SessionDirectory + relMdlPath;

    WriteMdlFromStrings(Mdl_PBRBase_string, Mdl_PBRBase_string_translucent, mdlFileName.c_str(), Connect.get());
  }

  return true;
}
#endif

bool UsdBridgeUsdWriter::InitializeSession()
{
  if (ConnectionSettings.HostName.empty())
  {
    if(ConnectionSettings.WorkingDirectory.compare("void") == 0)
      Connect = std::make_unique<UsdBridgeVoidConnection>();
    else
      Connect = std::make_unique<UsdBridgeLocalConnection>();
  }
  else
    Connect = std::make_unique<UsdBridgeRemoteConnection>();

  Connect->Initialize(ConnectionSettings, this->LogCallback, this->LogUserData);

  SessionNumber = FindSessionNumber();
  SessionDirectory = "Session_" + std::to_string(SessionNumber) + "/";

  bool valid = true;

  valid = CreateDirectories();

  valid = valid && VolumeWriter->Initialize(this->LogCallback, this->LogUserData);

#ifdef SUPPORT_MDL_SHADERS 
  if(Settings.EnableMdlShader)
  {
    valid = valid && CreateMdlFiles();
  }
#endif

  return valid;
}

void UsdBridgeUsdWriter::ResetSession()
{
  this->SessionNumber = -1;
  this->SceneStage = nullptr;
}

bool UsdBridgeUsdWriter::OpenSceneStage()
{
  bool binary = this->Settings.BinaryOutput;

  std::string SceneFile = (binary ? "FullScene.usd" : "FullScene.usda");
  this->SceneFileName = this->SessionDirectory + SceneFile;

  const char* absSceneFile = Connect->GetUrl(this->SceneFileName.c_str());
  if (!this->SceneStage && !Settings.CreateNewSession)
      this->SceneStage = UsdStage::Open(absSceneFile);
  if (!this->SceneStage)
    this->SceneStage = UsdStage::CreateNew(absSceneFile);
  
  if (!this->SceneStage)
  {
    UsdBridgeLogMacro(this, UsdBridgeLogLevel::ERR, "Scene UsdStage cannot be created or opened. Maybe a filesystem issue?");
    return false;
  }

  this->RootClassName = "/RootClass";
  UsdPrim rootClassPrim = this->SceneStage->CreateClassPrim(SdfPath(this->RootClassName));
  assert(rootClassPrim);
  this->RootName = "/Root";
  UsdPrim rootPrim = this->SceneStage->DefinePrim(SdfPath(this->RootName));
  assert(rootPrim);

  this->SceneStage->SetDefaultPrim(rootPrim);
  UsdModelAPI(rootPrim).SetKind(KindTokens->component);

  if(!this->SceneStage->HasAuthoredTimeCodeRange())
  {
    this->SceneStage->SetStartTimeCode(StartTime);
    this->SceneStage->SetEndTimeCode(EndTime);
  }
  else
  {
    StartTime = this->SceneStage->GetStartTimeCode();
    EndTime = this->SceneStage->GetEndTimeCode();
  }

  if(this->EnableSaving)
    this->SceneStage->Save();

  return true;
}

UsdStageRefPtr UsdBridgeUsdWriter::GetSceneStage()
{
  return this->SceneStage;
}

UsdStageRefPtr UsdBridgeUsdWriter::GetTimeVarStage(UsdBridgePrimCache* cache
#ifdef TIME_CLIP_STAGES
  , bool useClipStage, const char* clipPf, double timeStep
  , std::function<void (UsdStageRefPtr)> initFunc
#endif
  ) const
{
#ifdef VALUE_CLIP_RETIMING
#ifdef TIME_CLIP_STAGES
  if (useClipStage)
  {
    bool exists;
    UsdStageRefPtr clipStage = this->FindOrCreateClipStage(cache, clipPf, timeStep, exists).second;

    if(!exists)
      initFunc(clipStage);

    return clipStage;
  }
  else
    return cache->GetPrimStagePair().second;
#else
  return cache->GetPrimStagePair().second;
#endif
#else
  return this->SceneStage;
#endif
}

#ifdef VALUE_CLIP_RETIMING
void UsdBridgeUsdWriter::CreateManifestStage(const char* name, const char* primPostfix, UsdBridgePrimCache* cacheEntry)
{
  bool binary = this->Settings.BinaryOutput;

  cacheEntry->ManifestStage.first = manifestFolder + std::string(name) + primPostfix + (binary ? ".usd" : ".usda");

  std::string absoluteFileName = Connect->GetUrl((this->SessionDirectory + cacheEntry->ManifestStage.first).c_str());

  cacheEntry->ManifestStage.second = UsdStage::CreateNew(absoluteFileName);
  if (!cacheEntry->ManifestStage.second)
    cacheEntry->ManifestStage.second = UsdStage::Open(absoluteFileName);

  assert(cacheEntry->ManifestStage.second);

  cacheEntry->ManifestStage.second->DefinePrim(SdfPath(this->RootClassName));
}

void UsdBridgeUsdWriter::RemoveManifestAndClipStages(const UsdBridgePrimCache* cacheEntry)
{
  // May be superfluous
  assert(cacheEntry->ManifestStage.second);
  cacheEntry->ManifestStage.second->RemovePrim(SdfPath(RootClassName));

  // Remove ManifestStage file itself
  assert(!cacheEntry->ManifestStage.first.empty());
  Connect->RemoveFile((SessionDirectory + cacheEntry->ManifestStage.first).c_str(), true);

  // remove all clipstage files
  for (auto& x : cacheEntry->ClipStages)
  {
    Connect->RemoveFile((SessionDirectory + x.second.first).c_str(), true);
  }
}

const UsdStagePair& UsdBridgeUsdWriter::FindOrCreatePrimStage(UsdBridgePrimCache* cacheEntry, const char* namePostfix) const
{
  bool exists;
  return FindOrCreatePrimClipStage(cacheEntry, namePostfix, false, UsdBridgePrimCache::PrimStageTimeCode, exists);
}

const UsdStagePair& UsdBridgeUsdWriter::FindOrCreateClipStage(UsdBridgePrimCache* cacheEntry, const char* namePostfix, double timeStep, bool& exists) const
{
  return FindOrCreatePrimClipStage(cacheEntry, namePostfix, true, timeStep, exists);
}

const UsdStagePair& UsdBridgeUsdWriter::FindOrCreatePrimClipStage(UsdBridgePrimCache* cacheEntry, const char* namePostfix, bool isClip, double timeStep, bool& exists) const
{
  exists = true;
  bool binary = this->Settings.BinaryOutput;

  auto it = cacheEntry->ClipStages.find(timeStep);
  if (it == cacheEntry->ClipStages.end())
  {
    // Create a new Clipstage
    const char* folder = primStageFolder;
    std::string fullNamePostfix(namePostfix); 
    if(isClip) 
    {
      folder = clipFolder;
      fullNamePostfix += std::to_string(timeStep); 
    }
    std::string relativeFileName = folder + cacheEntry->Name.GetString() + fullNamePostfix + (binary ? ".usd" : ".usda");
    std::string absoluteFileName = Connect->GetUrl((this->SessionDirectory + relativeFileName).c_str());

    UsdStageRefPtr primClipStage = UsdStage::CreateNew(absoluteFileName);
    exists = !primClipStage;

    SdfPath rootPrimPath(this->RootClassName);
    if (exists)
    {
      primClipStage = UsdStage::Open(absoluteFileName); //Could happen if written folder is reused 
      assert(primClipStage->GetPrimAtPath(rootPrimPath));
    }
    else
      primClipStage->DefinePrim(rootPrimPath);

    it = cacheEntry->ClipStages.emplace(timeStep, UsdStagePair(std::move(relativeFileName), primClipStage)).first;
  }
  return it->second;
}
#endif

void UsdBridgeUsdWriter::SetSceneGraphRoot(UsdBridgePrimCache* worldCache, const char* name)
{
  // Also add concrete scenegraph prim with reference to world class
  SdfPath sceneGraphPath(this->RootName + "/" + name);

  UsdPrim sceneGraphPrim = this->SceneStage->DefinePrim(sceneGraphPath);
  assert(sceneGraphPrim);
  sceneGraphPrim.GetReferences().AddInternalReference(worldCache->PrimPath); //local and one-one, so no difference between ref or inherit
}

void UsdBridgeUsdWriter::RemoveSceneGraphRoot(UsdBridgePrimCache* worldCache)
{
  // Delete concrete scenegraph prim with reference to world class
  SdfPath sceneGraphPath(this->RootName);
  sceneGraphPath = sceneGraphPath.AppendPath(worldCache->Name);
  this->SceneStage->RemovePrim(sceneGraphPath);
}

const std::string & UsdBridgeUsdWriter::CreatePrimName(const char * name, const char * category)
{
  this->TempNameStr = this->RootClassName + "/" + category + "/" + name;
  return this->TempNameStr;
}

bool UsdBridgeUsdWriter::CreatePrim(const SdfPath& path)
{
  UsdPrim classPrim = SceneStage->GetPrimAtPath(path);
  if(!classPrim)
  {
    classPrim = SceneStage->DefinePrim(path);
    assert(classPrim);
    return true;
  }
  return false;
}

void UsdBridgeUsdWriter::DeletePrim(const UsdBridgePrimCache* cacheEntry)
{
  SceneStage->RemovePrim(cacheEntry->PrimPath);

#ifdef VALUE_CLIP_RETIMING
  if (cacheEntry->ManifestStage.second)
  {
    RemoveManifestAndClipStages(cacheEntry);
  }
#endif
}

void UsdBridgeUsdWriter::InitializePrimVisibility(UsdStageRefPtr stage, const SdfPath& primPath, const UsdTimeCode& timeCode)
{
  UsdGeomImageable imageable = UsdGeomImageable::Get(stage, primPath);

  if (imageable)
  {
    imageable.CreateVisibilityAttr(VtValue(UsdGeomTokens->invisible)); // default is invisible
    UsdAttribute visAttrib = imageable.GetVisibilityAttr(); // in case MakeVisible fails

    double startTime = stage->GetStartTimeCode();
    double endTime = stage->GetEndTimeCode();
    if (startTime < timeCode)
      visAttrib.Set(VtValue(UsdGeomTokens->invisible), startTime);//imageable.MakeInvisible(startTime);
    if (endTime > timeCode)
      visAttrib.Set(VtValue(UsdGeomTokens->invisible), endTime);//imageable.MakeInvisible(endTime);
    visAttrib.Set(VtValue(UsdGeomTokens->inherited), timeCode);//imageable.MakeVisible(timeCode);
  }
}

void UsdBridgeUsdWriter::SetPrimVisibility(UsdStageRefPtr stage, const SdfPath& primPath, const UsdTimeCode& timeCode, bool visible)
{
  UsdGeomImageable imageable = UsdGeomImageable::Get(stage, primPath);
  if (imageable)
  {
    UsdAttribute visAttrib = imageable.GetVisibilityAttr();
    assert(visAttrib);

    if (visible)
      visAttrib.Set(VtValue(UsdGeomTokens->inherited), timeCode);//imageable.MakeVisible(timeCode);
    else
      visAttrib.Set(VtValue(UsdGeomTokens->invisible), timeCode);//imageable.MakeInvisible(timeCode);
  }
}

void UsdBridgeUsdWriter::SetChildrenVisibility(UsdStageRefPtr stage, const SdfPath& parentPath, const UsdTimeCode& timeCode, bool visible)
{
  UsdPrim parentPrim = stage->GetPrimAtPath(parentPath);
  if (parentPrim)
  {
    UsdPrimSiblingRange children = parentPrim.GetAllChildren();
    for (UsdPrim child : children)
    {
      SetPrimVisibility(stage, child.GetPath(), timeCode, visible);
    }
  }
}

#ifdef TIME_BASED_CACHING
void UsdBridgeUsdWriter::PrimRemoveIfVisible(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, const UsdPrim& prim, bool timeVarying, const UsdTimeCode& timeCode, AtRemoveRefFunc atRemoveRef)
{
  const SdfPath& primPath = prim.GetPath();

  bool removePrim = true;

  if (timeVarying)
  {
    UsdGeomImageable imageable = UsdGeomImageable::Get(stage, primPath);
    if (imageable)
    {
      TfToken visibilityToken;
      UsdAttribute visAttrib = imageable.GetVisibilityAttr();
      if (visAttrib)
      {
        visAttrib.Get(&visibilityToken, timeCode);
        std::vector<double> times;
        visAttrib.GetTimeSamplesInInterval(GfInterval(timeCode.GetValue()), &times);

        if (times.size() == 0 || visibilityToken == UsdGeomTokens->invisible)
        {
          // Do not remove prim if timevarying and invisible, but make sure
          // that invisibility is explicitly set (non-default)
          removePrim = false;
          visAttrib.Set(UsdGeomTokens->invisible, timeCode);
        }
      }
    }
  }

  if (removePrim)
  {
    // Decrease the ref on the representing cache entry
    const std::string& childName = prim.GetName().GetString();
    
    atRemoveRef(parentCache, childName);
    
    // Remove the prim
    stage->RemovePrim(primPath);
  }
}

void UsdBridgeUsdWriter::ChildrenRemoveIfVisible(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, const SdfPath& parentPath, bool timeVarying, const UsdTimeCode& timeCode, AtRemoveRefFunc atRemoveRef, const SdfPath& exceptPath)
{
  UsdPrim parentPrim = stage->GetPrimAtPath(parentPath);
  if (parentPrim)
  {
    UsdPrimSiblingRange children = parentPrim.GetAllChildren();
    for (UsdPrim child : children)
    {
      if (child.GetPath() != exceptPath)
        PrimRemoveIfVisible(stage, parentCache, child, timeVarying, timeCode, atRemoveRef);
    }
  }
}

#endif


#ifdef VALUE_CLIP_RETIMING
void UsdBridgeUsdWriter::InitializeClipMetaData(const UsdPrim& clipPrim, UsdBridgePrimCache* childCache, double parentTimeStep, double childTimeStep, bool clipStages, const char* clipPostfix)
{
  UsdClipsAPI clipsApi(clipPrim);

  clipsApi.SetClipPrimPath(childCache->PrimPath.GetString());

  const std::string& manifestPath = childCache->ManifestStage.first;
  const std::string* refStagePath;
#ifdef TIME_CLIP_STAGES
  if (clipStages)
  {
    //set interpolatemissingclipvalues?

    bool exists;
    const UsdStagePair& childStagePair = FindOrCreateClipStage(childCache, clipPostfix, childTimeStep, exists);
    assert(exists);

    refStagePath = &childStagePair.first;
  }
  else
#endif
  {
    refStagePath = &childCache->GetPrimStagePair().first;
  }

  clipsApi.SetClipManifestAssetPath(SdfAssetPath(manifestPath));

  VtArray<SdfAssetPath> assetPaths;
  assetPaths.push_back(SdfAssetPath(*refStagePath));
  clipsApi.SetClipAssetPaths(assetPaths);

  VtVec2dArray clipActives;
  clipActives.push_back(GfVec2d(parentTimeStep, 0));
  clipsApi.SetClipActive(clipActives);

  VtVec2dArray clipTimes;
  clipTimes.push_back(GfVec2d(parentTimeStep, childTimeStep));
  clipsApi.SetClipTimes(clipTimes);
}

void UsdBridgeUsdWriter::UpdateClipMetaData(const UsdPrim& clipPrim, UsdBridgePrimCache* childCache, double parentTimeStep, double childTimeStep, bool clipStages, const char* clipPostfix)
{
  // Add parent-child timestep or update existing relationship
  UsdClipsAPI clipsApi(clipPrim);

#ifdef TIME_CLIP_STAGES
  if (clipStages)
  {
    bool exists;
    const UsdStagePair& childStagePair = FindOrCreateClipStage(childCache, clipPostfix, childTimeStep, exists);
    // At this point, exists should be true, but if clip stage creation failed earlier due to user error, 
    // exists will be false and we'll just link to the empty new stage created by FindOrCreatePrimClipStage()

    const std::string& refStagePath = childStagePair.first;

    VtVec2dArray clipActives;
    clipsApi.GetClipActive(&clipActives);
    VtArray<SdfAssetPath> assetPaths;
    clipsApi.GetClipAssetPaths(&assetPaths);

    // Find the asset path
    SdfAssetPath refStageSdf(refStagePath);
    auto assetIt = std::find_if(assetPaths.begin(), assetPaths.end(), [&refStageSdf](const SdfAssetPath& entry) -> bool { return refStageSdf.GetAssetPath() == entry.GetAssetPath(); });
    int assetIndex = int(assetIt - assetPaths.begin());
    bool newAsset = (assetIndex == assetPaths.size()); // Gives the opportunity to garbage collect unused asset references

    {
      // Find the parentTimeStep
      int timeFindIdx = 0;
      for (; timeFindIdx < clipActives.size() && clipActives[timeFindIdx][0] != parentTimeStep; ++timeFindIdx)
      {}

      bool replaceAsset = false;

      // If timestep not found, just add (time, asset ref idx) to actives
      if (timeFindIdx == clipActives.size())
        clipActives.push_back(GfVec2d(parentTimeStep, assetIndex));
      else
      {
        // Find out whether to update existing active entry with new asset ref idx, or let the entry unchanged and replace the asset itself
        double prevAssetIndex = clipActives[timeFindIdx][1];
       
        if (newAsset)
        {
          //Find prev asset index
          int assetIdxFindIdx = 0;
          for (; assetIdxFindIdx < clipActives.size() && 
            (assetIdxFindIdx == timeFindIdx || clipActives[assetIdxFindIdx][1] != prevAssetIndex); 
            ++assetIdxFindIdx)
          {}

          // replacement occurs when prevAssetIndex hasn't been found in other entries
          replaceAsset = (assetIdxFindIdx == clipActives.size());
        }

        if(replaceAsset)
          assetPaths[int(prevAssetIndex)] = refStageSdf;
        else
          clipActives[timeFindIdx][1] = assetIndex;
      }

      // If new asset and not put in place of an old asset, add to assetPaths
      if (newAsset && !replaceAsset)
        assetPaths.push_back(refStageSdf);

      // Send the result through to usd
      clipsApi.SetClipAssetPaths(assetPaths);
      clipsApi.SetClipActive(clipActives);
    }
  }
#endif

  // Find the parentTimeStep, and change its child (or add the pair if nonexistent)
  VtVec2dArray clipTimes;
  clipsApi.GetClipTimes(&clipTimes);
  {
    int findIdx = 0;
    for (; findIdx < clipTimes.size(); ++findIdx)
    {
      if (clipTimes[findIdx][0] == parentTimeStep)
      {
        clipTimes[findIdx][1] = childTimeStep;
        break;
      }
    }
    if (findIdx == clipTimes.size())
      clipTimes.push_back(GfVec2d(parentTimeStep, childTimeStep));
    clipsApi.SetClipTimes(clipTimes);
  }
}

#endif

SdfPath UsdBridgeUsdWriter::AddRef_NoClip(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
  bool timeVarying, double parentTimeStep, double childTimeStep,
  const RefModFuncs& refModCallbacks)
{
  return AddRef_Impl(parentCache, childCache, refPathExt, timeVarying, false, false, nullptr, parentTimeStep, childTimeStep, refModCallbacks);
}

SdfPath UsdBridgeUsdWriter::AddRef(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
  bool timeVarying, bool valueClip, bool clipStages, const char* clipPostfix,
  double parentTimeStep, double childTimeStep,
  const RefModFuncs& refModCallbacks)
{
  // Value clip-enabled references have to be defined on the scenestage, as usd does not re-time recursively.
  return AddRef_Impl(parentCache, childCache, refPathExt, timeVarying, valueClip, clipStages, clipPostfix, parentTimeStep, childTimeStep, refModCallbacks);
}

SdfPath UsdBridgeUsdWriter::AddRef_Impl(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
  bool timeVarying, // Timevarying existence of the reference itself
  bool valueClip,   // Retiming through a value clip
  bool clipStages,  // Separate stages for separate time slots (can only exist in usd if valueClip enabled)
  const char* clipPostfix, double parentTimeStep, double childTimeStep,
  const RefModFuncs& refModCallbacks)
{
  UsdTimeCode parentTimeCode(parentTimeStep);

  SdfPath childBasePath = parentCache->PrimPath;
  if (refPathExt)
    childBasePath = parentCache->PrimPath.AppendPath(SdfPath(refPathExt));
  
  SdfPath referencingPrimPath = childBasePath.AppendPath(childCache->Name);
  UsdPrim referencingPrim = SceneStage->GetPrimAtPath(referencingPrimPath);

  if (!referencingPrim)
  {
    referencingPrim = SceneStage->DefinePrim(referencingPrimPath);
    assert(referencingPrim);

#ifdef VALUE_CLIP_RETIMING
    if (valueClip)
      InitializeClipMetaData(referencingPrim, childCache, parentTimeStep, childTimeStep, clipStages, clipPostfix);
#endif

    {
      UsdReferences references = referencingPrim.GetReferences(); //references or inherits?
      references.ClearReferences();
      references.AddInternalReference(childCache->PrimPath);
      //referencingPrim.SetInstanceable(true);
    }

#ifdef TIME_BASED_CACHING
    // If time domain of the stage extends beyond timestep in either direction, set visibility false for extremes.
    if (timeVarying)
      InitializePrimVisibility(SceneStage, referencingPrimPath, parentTimeCode);
#endif

    refModCallbacks.AtNewRef(parentCache, childCache);
  }
  else
  {
#ifdef TIME_BASED_CACHING
    if (timeVarying)
      SetPrimVisibility(SceneStage, referencingPrimPath, parentTimeCode, true);

#ifdef VALUE_CLIP_RETIMING
    // Cliptimes are added as additional info, not actively removed (visibility values remain leading in defining existing relationships over timesteps)
    // Also, clip stages at childTimeSteps which are not referenced anymore, are not removed; they could still be referenced from other parents!
    if (valueClip)
      UpdateClipMetaData(referencingPrim, childCache, parentTimeStep, childTimeStep, clipStages, clipPostfix);
#endif
#endif
  }

  return referencingPrimPath;
}

void UsdBridgeUsdWriter::RemoveAllRefs(UsdBridgePrimCache* parentCache, const char* refPathExt, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef)
{
  SdfPath childBasePath = parentCache->PrimPath;
  if (refPathExt)
    childBasePath = parentCache->PrimPath.AppendPath(SdfPath(refPathExt));

  RemoveAllRefs(SceneStage, parentCache, childBasePath, timeVarying, timeStep, atRemoveRef);
}

void UsdBridgeUsdWriter::RemoveAllRefs(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, SdfPath childBasePath, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef)
{
#ifdef TIME_BASED_CACHING
  UsdTimeCode timeCode(timeStep);

  // Remove refs just for this timecode, so leave invisible refs (could still be visible in other timecodes) intact.
  //SetChildrenVisibility(stage, childBasePath, timeCode, false);
  ChildrenRemoveIfVisible(stage, parentCache, childBasePath, timeVarying, timeCode, atRemoveRef);
#else
  UsdPrim parentPrim = stage->GetPrimAtPath(childBasePath);
  if(parentPrim)
  {
    UsdPrimSiblingRange children = parentPrim.GetAllChildren();
    for (UsdPrim child : children)
    {
      const std::string& childName = child.GetName().GetString();
      atRemoveRef(parentCache, childName); // Decrease reference count in caches
      stage->RemovePrim(child.GetPath()); // Remove reference prim
    }
  }
#endif
}

void UsdBridgeUsdWriter::ManageUnusedRefs(UsdBridgePrimCache* parentCache, const UsdBridgePrimCacheList& newChildren, const char* refPathExt, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef)
{
  ManageUnusedRefs(SceneStage, parentCache, newChildren, refPathExt, timeVarying, timeStep, atRemoveRef);
}

void UsdBridgeUsdWriter::ManageUnusedRefs(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, const UsdBridgePrimCacheList& newChildren, const char* refPathExt, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef)
{
  UsdTimeCode timeCode(timeStep);

  UsdPrim basePrim;
  if (refPathExt)
  {
    SdfPath childBasePath = parentCache->PrimPath.AppendPath(SdfPath(refPathExt));
    basePrim = stage->GetPrimAtPath(childBasePath);
  }
  else
    basePrim = stage->GetPrimAtPath(parentCache->PrimPath);

  if (basePrim)
  {
    UsdPrimSiblingRange children = basePrim.GetAllChildren();
    for (UsdPrim oldChild : children)
    {
      const std::string& oldChildName = oldChild.GetPrimPath().GetName();

      bool found = false;
      for (size_t newChildIdx = 0; newChildIdx < newChildren.size() && !found; ++newChildIdx)
      {
        const std::string& newChildName = newChildren[newChildIdx]->Name.GetString();
        found = (newChildName == oldChildName);
      }

      if (!found)
#ifdef TIME_BASED_CACHING
      {
        // make *referencing* prim invisible at timestep
        //SetPrimVisibility(oldChild.GetPath(), timeCode, false);

        // Remove *referencing* prim if visible
        PrimRemoveIfVisible(stage, parentCache, oldChild, timeVarying, timeCode, atRemoveRef); 
      }
#else
      {// remove the whole referencing prim
        atRemoveRef(parentCache, oldChildName);
        stage->RemovePrim(oldChild.GetPath());
      }
#endif
    }
  }
}

void UsdBridgeUsdWriter::InitializeUsdTransform(const UsdBridgePrimCache* cacheEntry)
{
  SdfPath transformPath = cacheEntry->PrimPath;
  UsdGeomXform transform = UsdGeomXform::Define(SceneStage, transformPath);
  assert(transform);
}

namespace
{
  void InitializeUsdGeometryTimeVar(UsdGeomMesh& meshGeom, const UsdBridgeMeshData& meshData, const UsdBridgeSettings& settings, 
    const TimeEvaluator<UsdBridgeMeshData>* timeEval = nullptr)
  {
    typedef UsdBridgeMeshData::DataMemberId DMI;
    UsdGeomPrimvarsAPI primvarApi(meshGeom);

    UsdPrim meshPrim = meshGeom.GetPrim();

    if (!timeEval || timeEval->IsTimeVarying(DMI::POINTS))
    {
      meshGeom.CreatePointsAttr();
      meshGeom.CreateExtentAttr();
    }
    else
    {
      meshPrim.RemoveProperty(UsdBridgeTokens->points);
      meshPrim.RemoveProperty(UsdBridgeTokens->extent);
    }

    if (!timeEval || timeEval->IsTimeVarying(DMI::INDICES))
    {
      meshGeom.CreateFaceVertexIndicesAttr();
      meshGeom.CreateFaceVertexCountsAttr();
    }
    else
    {
      meshPrim.RemoveProperty(UsdBridgeTokens->faceVertexCounts);
      meshPrim.RemoveProperty(UsdBridgeTokens->faceVertexIndices);
    }

    if (!timeEval || timeEval->IsTimeVarying(DMI::NORMALS))
      meshGeom.CreateNormalsAttr();
    else
      meshPrim.RemoveProperty(UsdBridgeTokens->normals);

    CreateUsdGeomColorPrimvars(primvarApi, meshData, settings, timeEval);

    CreateUsdGeomTexturePrimvars(primvarApi, meshData, settings, timeEval);

    CreateUsdGeomAttributePrimvars(primvarApi, meshData, timeEval);
  }
  
  void InitializeUsdGeometryTimeVar(UsdGeomPoints& pointsGeom, const UsdBridgeInstancerData& instancerData, const UsdBridgeSettings& settings,
    const TimeEvaluator<UsdBridgeInstancerData>* timeEval = nullptr)
  {
    typedef UsdBridgeInstancerData::DataMemberId DMI;
    UsdGeomPrimvarsAPI primvarApi(pointsGeom);

    UsdPrim pointsPrim = pointsGeom.GetPrim();

    if (!timeEval || timeEval->IsTimeVarying(DMI::POINTS))
    {
      pointsGeom.CreatePointsAttr();
      pointsGeom.CreateExtentAttr();
    }
    else
    {
      pointsPrim.RemoveProperty(UsdBridgeTokens->points);
      pointsPrim.RemoveProperty(UsdBridgeTokens->extent);
    }

    if (!timeEval || timeEval->IsTimeVarying(DMI::INSTANCEIDS))
      pointsGeom.CreateIdsAttr();
    else
      pointsPrim.RemoveProperty(UsdBridgeTokens->ids);

    if (!timeEval || timeEval->IsTimeVarying(DMI::ORIENTATIONS))
      pointsGeom.CreateNormalsAttr();
    else
      pointsPrim.RemoveProperty(UsdBridgeTokens->normals);

    if (!timeEval || timeEval->IsTimeVarying(DMI::SCALES))
      pointsGeom.CreateWidthsAttr();
    else
      pointsPrim.RemoveProperty(UsdBridgeTokens->widths);

    CreateUsdGeomColorPrimvars(primvarApi, instancerData, settings, timeEval);

    CreateUsdGeomTexturePrimvars(primvarApi, instancerData, settings, timeEval);

    CreateUsdGeomAttributePrimvars(primvarApi, instancerData, timeEval);
  }

  void InitializeUsdGeometryTimeVar(UsdGeomPointInstancer& pointsGeom, const UsdBridgeInstancerData& instancerData, const UsdBridgeSettings& settings,
    const TimeEvaluator<UsdBridgeInstancerData>* timeEval = nullptr)
  {
    typedef UsdBridgeInstancerData::DataMemberId DMI;
    UsdGeomPrimvarsAPI primvarApi(pointsGeom);

    UsdPrim pointsPrim = pointsGeom.GetPrim();

    if (!timeEval || timeEval->IsTimeVarying(DMI::POINTS))
    {
      pointsGeom.CreatePositionsAttr();
      pointsGeom.CreateExtentAttr();
    }
    else
    {
      pointsPrim.RemoveProperty(UsdBridgeTokens->positions);
      pointsPrim.RemoveProperty(UsdBridgeTokens->extent);
    }

    if (!timeEval || timeEval->IsTimeVarying(DMI::SHAPEINDICES))
      pointsGeom.CreateProtoIndicesAttr();
    else
      pointsPrim.RemoveProperty(UsdBridgeTokens->protoIndices);

    if (!timeEval || timeEval->IsTimeVarying(DMI::INSTANCEIDS))
      pointsGeom.CreateIdsAttr();
    else
      pointsPrim.RemoveProperty(UsdBridgeTokens->ids);

    if (!timeEval || timeEval->IsTimeVarying(DMI::ORIENTATIONS))
      pointsGeom.CreateOrientationsAttr();
    else
      pointsPrim.RemoveProperty(UsdBridgeTokens->orientations);

    if (!timeEval || timeEval->IsTimeVarying(DMI::SCALES))
      pointsGeom.CreateScalesAttr();
    else
      pointsPrim.RemoveProperty(UsdBridgeTokens->scales);

    CreateUsdGeomColorPrimvars(primvarApi, instancerData, settings, timeEval);

    CreateUsdGeomTexturePrimvars(primvarApi, instancerData, settings, timeEval);

    CreateUsdGeomAttributePrimvars(primvarApi, instancerData, timeEval);

    if (!timeEval || timeEval->IsTimeVarying(DMI::LINEARVELOCITIES))
      pointsGeom.CreateVelocitiesAttr();
    else
      pointsPrim.RemoveProperty(UsdBridgeTokens->velocities);

    if (!timeEval || timeEval->IsTimeVarying(DMI::ANGULARVELOCITIES))
      pointsGeom.CreateAngularVelocitiesAttr();
    else
      pointsPrim.RemoveProperty(UsdBridgeTokens->angularVelocities);

    if (!timeEval || timeEval->IsTimeVarying(DMI::INVISIBLEIDS))
      pointsGeom.CreateInvisibleIdsAttr();
    else
      pointsPrim.RemoveProperty(UsdBridgeTokens->invisibleIds);
  }

  void InitializeUsdGeometryTimeVar(UsdGeomBasisCurves& curveGeom, const UsdBridgeCurveData& curveData, const UsdBridgeSettings& settings,
    const TimeEvaluator<UsdBridgeCurveData>* timeEval = nullptr)
  {
    typedef UsdBridgeCurveData::DataMemberId DMI;
    UsdGeomPrimvarsAPI primvarApi(curveGeom);

    UsdPrim curvePrim = curveGeom.GetPrim();

    if (!timeEval || timeEval->IsTimeVarying(DMI::POINTS))
    {
      curveGeom.CreatePointsAttr();
      curveGeom.CreateExtentAttr();
    }
    else
    {
      curvePrim.RemoveProperty(UsdBridgeTokens->positions);
      curvePrim.RemoveProperty(UsdBridgeTokens->extent);
    }

    if (!timeEval || timeEval->IsTimeVarying(DMI::CURVELENGTHS))
      curveGeom.CreateCurveVertexCountsAttr();
    else
      curvePrim.RemoveProperty(UsdBridgeTokens->curveVertexCounts);

    if (!timeEval || timeEval->IsTimeVarying(DMI::NORMALS))
      curveGeom.CreateNormalsAttr();
    else
      curvePrim.RemoveProperty(UsdBridgeTokens->normals);

    if (!timeEval || timeEval->IsTimeVarying(DMI::SCALES))
      curveGeom.CreateWidthsAttr();
    else
      curvePrim.RemoveProperty(UsdBridgeTokens->widths);

    CreateUsdGeomColorPrimvars(primvarApi, curveData, settings, timeEval);

    CreateUsdGeomTexturePrimvars(primvarApi, curveData, settings, timeEval);

    CreateUsdGeomAttributePrimvars(primvarApi, curveData, timeEval);

  }

  UsdPrim InitializeUsdGeometry_Impl(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeMeshData& meshData, bool uniformPrim,
    const UsdBridgeSettings& settings,
    TimeEvaluator<UsdBridgeMeshData>* timeEval = nullptr)
  {
    UsdGeomMesh geomMesh = GetOrDefinePrim<UsdGeomMesh>(geometryStage, geomPath);
    
    InitializeUsdGeometryTimeVar(geomMesh, meshData, settings, timeEval);

    if (uniformPrim)
    {
      geomMesh.CreateDoubleSidedAttr(VtValue(true));
      geomMesh.CreateSubdivisionSchemeAttr().Set(UsdGeomTokens->none);
    }

    return geomMesh.GetPrim();
  }

  UsdPrim InitializeUsdGeometry_Impl(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeInstancerData& instancerData, bool uniformPrim,
    const UsdBridgeSettings& settings,
    TimeEvaluator<UsdBridgeInstancerData>* timeEval = nullptr)
  {
    if (UsesUsdGeomPoints(instancerData))
    {
      UsdGeomPoints geomPoints = GetOrDefinePrim<UsdGeomPoints>(geometryStage, geomPath);
      
      InitializeUsdGeometryTimeVar(geomPoints, instancerData, settings, timeEval);

      if (uniformPrim)
      {
        geomPoints.CreateDoubleSidedAttr(VtValue(true));
      }

      return geomPoints.GetPrim();
    }
    else
    {
      UsdGeomPointInstancer geomPoints = GetOrDefinePrim<UsdGeomPointInstancer>(geometryStage, geomPath);
      
      InitializeUsdGeometryTimeVar(geomPoints, instancerData, settings, timeEval);

      if (uniformPrim)
      {
        // Initialize the point instancer with a sphere shape
        SdfPath shapePath;
        switch (instancerData.Shapes[0])
        {
          case UsdBridgeInstancerData::SHAPE_SPHERE:
          {
            shapePath = geomPath.AppendPath(SdfPath("sphere"));
            UsdGeomSphere::Define(geometryStage, shapePath);
            break;
          }
          case UsdBridgeInstancerData::SHAPE_CYLINDER:
          {
            shapePath = geomPath.AppendPath(SdfPath("cylinder"));
            UsdGeomCylinder::Define(geometryStage, shapePath);
            break;
          }
          case UsdBridgeInstancerData::SHAPE_CONE:
          {
            shapePath = geomPath.AppendPath(SdfPath("cone"));
            UsdGeomCone::Define(geometryStage, shapePath);
            break;
          }
        }

        UsdRelationship protoRel = geomPoints.GetPrototypesRel();
        protoRel.AddTarget(shapePath);
      }

      return geomPoints.GetPrim();
    }
  }

  UsdPrim InitializeUsdGeometry_Impl(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeCurveData& curveData, bool uniformPrim,
    const UsdBridgeSettings& settings,
    TimeEvaluator<UsdBridgeCurveData>* timeEval = nullptr)
  {
    UsdGeomBasisCurves geomCurves = GetOrDefinePrim<UsdGeomBasisCurves>(geometryStage, geomPath);

    InitializeUsdGeometryTimeVar(geomCurves, curveData, settings, timeEval);

    if (uniformPrim)
    {
      geomCurves.CreateDoubleSidedAttr(VtValue(true));
      geomCurves.GetTypeAttr().Set(UsdGeomTokens->linear);
    }

    return geomCurves.GetPrim();
  }

  void InitializeUsdVolumeTimeVar(UsdVolVolume& volume, const TimeEvaluator<UsdBridgeVolumeData>* timeEval = nullptr)
  {
    typedef UsdBridgeVolumeData::DataMemberId DMI;

    volume.ClearXformOpOrder();

    if (!timeEval || timeEval->IsTimeVarying(DMI::TRANSFORM))
    {
      volume.AddTranslateOp();
      volume.AddScaleOp();
    }
  }

  void InitializeUsdVolumeAssetTimeVar(UsdVolOpenVDBAsset& volAsset, const TimeEvaluator<UsdBridgeVolumeData>* timeEval = nullptr)
  {
    typedef UsdBridgeVolumeData::DataMemberId DMI;

    if (!timeEval || timeEval->IsTimeVarying(DMI::DATA))
    {
      volAsset.CreateFilePathAttr();
      volAsset.CreateExtentAttr();
    }
    else
    {
      volAsset.GetPrim().RemoveProperty(UsdBridgeTokens->filePath);
      volAsset.GetPrim().RemoveProperty(UsdBridgeTokens->extent);
    }
  }

  UsdPrim InitializeUsdVolume_Impl(UsdStageRefPtr volumeStage, const SdfPath & volumePath, bool uniformPrim,
    TimeEvaluator<UsdBridgeVolumeData>* timeEval = nullptr)
  {
    UsdVolVolume volume = GetOrDefinePrim<UsdVolVolume>(volumeStage, volumePath);

    SdfPath ovdbFieldPath = volumePath.AppendPath(SdfPath(openVDBPrimPf));
    UsdVolOpenVDBAsset volAsset = GetOrDefinePrim<UsdVolOpenVDBAsset>(volumeStage, ovdbFieldPath);

    if (uniformPrim)
    {
      volume.CreateFieldRelationship(UsdBridgeTokens->density, ovdbFieldPath);
      volAsset.CreateFieldNameAttr(VtValue(UsdBridgeTokens->density));
    }

    InitializeUsdVolumeTimeVar(volume, timeEval);
    InitializeUsdVolumeAssetTimeVar(volAsset, timeEval);

    return volume.GetPrim();
  } 

  void InitializeUsdShaderTimeVar(UsdShadeShader& shader, const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    CreateShaderInput(shader, timeEval, DMI::ROUGHNESS, UsdBridgeTokens->roughness, "inputs:roughness", SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::OPACITY, UsdBridgeTokens->opacity, "inputs:opacity", SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::METALLIC, UsdBridgeTokens->metallic, "inputs:metallic", SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::IOR, UsdBridgeTokens->ior, "inputs:ior", SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::DIFFUSE, UsdBridgeTokens->diffuseColor, "inputs:diffuseColor", SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::SPECULAR, UsdBridgeTokens->specularColor, "inputs:specularColor", SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVE, UsdBridgeTokens->emissiveColor, "inputs:emissiveColor", SdfValueTypeNames->Color3f);
  }

#ifdef SUPPORT_MDL_SHADERS  
  void InitializeUsdMdlShaderTimeVar(UsdShadeShader& shader, const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    CreateShaderInput(shader, timeEval, DMI::ROUGHNESS, UsdBridgeTokens->reflection_roughness_constant, "inputs:reflection_roughness_constant", SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::OPACITY, UsdBridgeTokens->opacity_constant, "inputs:opacity_constant", SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::METALLIC, UsdBridgeTokens->metallic_constant, "inputs:metallic_constant", SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::IOR, UsdBridgeTokens->ior_constant, "inputs:ior_constant", SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::DIFFUSE, UsdBridgeTokens->diffuse_color_constant, "inputs:diffuse_color_constant", SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVE, UsdBridgeTokens->emissive_color, "inputs:emissive_color", SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVEINTENSITY, UsdBridgeTokens->emissive_intensity, "inputs:emissive_intensity", SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVEINTENSITY, UsdBridgeTokens->enable_emission, "inputs:enable_emission", SdfValueTypeNames->Bool);
  }
#endif

  void InitializeUsdSamplerTimeVar(UsdShadeShader& sampler, UsdBridgeSamplerData::SamplerType type, const TimeEvaluator<UsdBridgeSamplerData>* timeEval = nullptr)
  {
    typedef UsdBridgeSamplerData::DataMemberId DMI;

    CreateShaderInput(sampler, timeEval, DMI::FILENAME, UsdBridgeTokens->file, "inputs:file", SdfValueTypeNames->Asset);
    CreateShaderInput(sampler, timeEval, DMI::WRAPS, UsdBridgeTokens->WrapS, "inputs:WrapS", SdfValueTypeNames->Token);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPT, UsdBridgeTokens->WrapT, "inputs:WrapT", SdfValueTypeNames->Token);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_3D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPR, UsdBridgeTokens->WrapR, "inputs:WrapR", SdfValueTypeNames->Token);
  }
    
  UsdShadeOutput InitializeUsdShader(UsdStageRefPtr shaderStage, const SdfPath& shadPrimPath, bool uniformPrim
    , const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    // Create the shader
    UsdShadeShader shader = GetOrDefinePrim<UsdShadeShader>(shaderStage, shadPrimPath);
    assert(shader);

    if (uniformPrim)
    {
      shader.CreateIdAttr(VtValue(UsdBridgeTokens->UsdPreviewSurface));
      shader.CreateInput(UsdBridgeTokens->useSpecularWorkflow, SdfValueTypeNames->Int).Set(1);
    }

    InitializeUsdShaderTimeVar(shader, timeEval);

    if(uniformPrim)
      return shader.CreateOutput(UsdBridgeTokens->surface, SdfValueTypeNames->Token);
    else
      return UsdShadeOutput();
  }

#ifdef SUPPORT_MDL_SHADERS  
  UsdShadeOutput InitializeMdlShader(UsdStageRefPtr shaderStage, const SdfPath& shadPrimPath, bool uniformPrim
    , const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    // Create the shader
    UsdShadeShader shader = GetOrDefinePrim<UsdShadeShader>(shaderStage, shadPrimPath);
    assert(shader);

    if (uniformPrim)
    {
      shader.CreateImplementationSourceAttr(VtValue(UsdBridgeTokens->sourceAsset));
      shader.SetSourceAssetSubIdentifier(UsdBridgeTokens->PBR_Base, UsdBridgeTokens->mdl);
      shader.CreateInput(UsdBridgeTokens->vertexcolor_coordinate_index, SdfValueTypeNames->Int);
    }

    InitializeUsdMdlShaderTimeVar(shader, timeEval);

    if(uniformPrim)
      return shader.CreateOutput(UsdBridgeTokens->surface, SdfValueTypeNames->Token);
    else
      return UsdShadeOutput();
  }
#endif

  void BindShaderToMaterial(const UsdShadeMaterial& matPrim, const UsdShadeOutput& shadOut, TfToken* renderContext)
  {
    // Bind the material to the shader reference subprim.
    if(renderContext)
      matPrim.CreateSurfaceOutput(*renderContext).ConnectToSource(shadOut);
    else
      matPrim.CreateSurfaceOutput().ConnectToSource(shadOut);
  }

  UsdShadeMaterial InitializeUsdMaterial_Impl(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, bool uniformPrim, const UsdBridgeSettings& settings,
    const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    // Create the material
    UsdShadeMaterial matPrim = GetOrDefinePrim<UsdShadeMaterial>(materialStage, matPrimPath);
    assert(matPrim);

    if(settings.EnablePreviewSurfaceShader)
    {
      if (uniformPrim)
      {
        // Create a standard texture coordinate reader
        SdfPath texCoordReaderPrimPath = matPrimPath.AppendPath(SdfPath(texCoordReaderPrimPf));
        UsdShadeShader texCoordReader = UsdShadeShader::Define(materialStage, texCoordReaderPrimPath);
        assert(texCoordReader);
        texCoordReader.CreateIdAttr(VtValue(UsdBridgeTokens->PrimStId));
        texCoordReader.CreateInput(UsdBridgeTokens->varname, SdfValueTypeNames->Token).Set(UsdBridgeTokens->st);

        // Create a vertexcolorreader
        SdfPath vertexColorReaderPrimPath = matPrimPath.AppendPath(SdfPath(vertexColorReaderPrimPf));
        UsdShadeShader vertexColorReader = UsdShadeShader::Define(materialStage, vertexColorReaderPrimPath);
        assert(vertexColorReader);
        vertexColorReader.CreateIdAttr(VtValue(UsdBridgeTokens->PrimDisplayColorId));
        vertexColorReader.CreateInput(UsdBridgeTokens->varname, SdfValueTypeNames->Token).Set(UsdBridgeTokens->displayColor);
      }

      // Create a shader
      SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(shaderPrimPf));
      UsdShadeOutput shaderOutput = InitializeUsdShader(materialStage, shaderPrimPath, uniformPrim, timeEval);

      if(uniformPrim)
        BindShaderToMaterial(matPrim, shaderOutput, nullptr);
    }

#ifdef SUPPORT_MDL_SHADERS 
    if(settings.EnableMdlShader)
    {
      // Create an mdl shader
      SdfPath mdlShaderPrimPath = matPrimPath.AppendPath(SdfPath(mdlShaderPrimPf));
      UsdShadeOutput mdlShaderPrim = InitializeMdlShader(materialStage, mdlShaderPrimPath, uniformPrim, timeEval);

      if(uniformPrim)
        BindShaderToMaterial(matPrim, mdlShaderPrim, &UsdBridgeTokens->mdl);
    }
#endif

    return matPrim;
  }

  UsdPrim InitializeUsdSampler_Impl(UsdStageRefPtr samplerStage, const SdfPath& samplerPrimPath, UsdBridgeSamplerData::SamplerType type, bool uniformPrim,
    const TimeEvaluator<UsdBridgeSamplerData>* timeEval = nullptr)
  {
    UsdShadeShader sampler = GetOrDefinePrim<UsdShadeShader>(samplerStage, samplerPrimPath);
    assert(sampler);

    if(uniformPrim)
    {
      sampler.CreateIdAttr(VtValue(UsdBridgeTokens->UsdUVTexture));
      sampler.CreateInput(UsdBridgeTokens->fallback, SdfValueTypeNames->Float4).Set(GfVec4f(1.0f, 0.0f, 0.0f, 1.0f));
      sampler.CreateOutput(UsdBridgeTokens->rgb, SdfValueTypeNames->Float3);
      sampler.CreateOutput(UsdBridgeTokens->a, SdfValueTypeNames->Float);
    }

    InitializeUsdSamplerTimeVar(sampler, type, timeEval);

    return sampler.GetPrim();
  }
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeMeshData& meshData, bool uniformPrim) const
{
  return InitializeUsdGeometry_Impl(geometryStage, geomPath, meshData, uniformPrim, Settings);
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeInstancerData& instancerData, bool uniformPrim) const
{
  return InitializeUsdGeometry_Impl(geometryStage, geomPath, instancerData, uniformPrim, Settings);
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeCurveData& curveData, bool uniformPrim) const
{
  return InitializeUsdGeometry_Impl(geometryStage, geomPath, curveData, uniformPrim, Settings);
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdVolume(UsdStageRefPtr volumeStage, const SdfPath & volumePath, bool uniformPrim) const
{ 
  return InitializeUsdVolume_Impl(volumeStage, volumePath, uniformPrim);
}

UsdShadeMaterial UsdBridgeUsdWriter::InitializeUsdMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, bool uniformPrim) const
{
  return InitializeUsdMaterial_Impl(materialStage, matPrimPath, uniformPrim, Settings);
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdSampler(UsdStageRefPtr samplerStage, const SdfPath& samplerPrimPath, UsdBridgeSamplerData::SamplerType type, bool uniformPrim) const
{
  return InitializeUsdSampler_Impl(samplerStage, samplerPrimPath, type, uniformPrim);
}

#ifdef VALUE_CLIP_RETIMING
void UsdBridgeUsdWriter::UpdateUsdGeometryManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeMeshData& meshData)
{
  TimeEvaluator<UsdBridgeMeshData> timeEval(meshData);
  InitializeUsdGeometry_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, meshData, false, 
    Settings, &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}

void UsdBridgeUsdWriter::UpdateUsdGeometryManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeInstancerData& instancerData)
{
  TimeEvaluator<UsdBridgeInstancerData> timeEval(instancerData);
  InitializeUsdGeometry_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, instancerData, false, 
    Settings, &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}

void UsdBridgeUsdWriter::UpdateUsdGeometryManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeCurveData& curveData)
{
  TimeEvaluator<UsdBridgeCurveData> timeEval(curveData);
  InitializeUsdGeometry_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, curveData, false, 
    Settings, &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}

void UsdBridgeUsdWriter::UpdateUsdVolumeManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeVolumeData& volumeData)
{
  TimeEvaluator<UsdBridgeVolumeData> timeEval(volumeData);
  InitializeUsdVolume_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, false,
    &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}

void UsdBridgeUsdWriter::UpdateUsdMaterialManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeMaterialData& matData)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData);
  InitializeUsdMaterial_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, false, 
    Settings, &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}

void UsdBridgeUsdWriter::UpdateUsdSamplerManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeSamplerData& samplerData)
{
  TimeEvaluator<UsdBridgeSamplerData> timeEval(samplerData);
  InitializeUsdSampler_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, samplerData.Type,
    false, &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}
#endif

void UsdBridgeUsdWriter::BindMaterialToGeom(const SdfPath & refGeomPath, const SdfPath & refMatPath)
{
  UsdPrim refGeomPrim = this->SceneStage->GetPrimAtPath(refGeomPath);
  assert(refGeomPrim);

  // Bind the material to the mesh (use paths existing fully within the surface class definition, not the inherited geometry/material)
  UsdShadeMaterial refMatPrim = UsdShadeMaterial::Get(this->SceneStage, refMatPath);
  assert(refMatPrim);

  UsdShadeMaterialBindingAPI(refGeomPrim).Bind(refMatPrim);
}

void UsdBridgeUsdWriter::UnbindMaterialFromGeom(const SdfPath & refGeomPath)
{
  UsdPrim refGeomPrim = this->SceneStage->GetPrimAtPath(refGeomPath);
  assert(refGeomPrim);

  UsdShadeMaterialBindingAPI(refGeomPrim).UnbindDirectBinding();
}

void UsdBridgeUsdWriter::BindSamplerToMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, const SdfPath& refSamplerPrimPath, 
  const std::string& samplerPrimName, const char* samplerImageUrl, bool samplerImageTimeVarying, double worldTimeStep, double samplerTimeStep)
{
  if(Settings.EnablePreviewSurfaceShader)
  {
    // Find the coordinate reader using the material path
    SdfPath coordReaderPath = matPrimPath.AppendPath(SdfPath(texCoordReaderPrimPf));
    UsdShadeShader coordReaderPrim = UsdShadeShader::Get(this->SceneStage, coordReaderPath);
    assert(coordReaderPrim);

    // Bind the coordinate reader's output to the sampler's st input
    UsdShadeShader refSampler = UsdShadeShader::Get(this->SceneStage, refSamplerPrimPath); // type inherited from sampler prim (in AddRef)
    assert(refSampler);
    UsdShadeOutput tcReaderOutput = coordReaderPrim.CreateOutput(UsdBridgeTokens->result, SdfValueTypeNames->Float2);
    refSampler.CreateInput(UsdBridgeTokens->st, SdfValueTypeNames->Float2).ConnectToSource(tcReaderOutput);

    // Get shader prims
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(shaderPrimPf));

    UsdShadeShader uniformShad = UsdShadeShader::Get(this->SceneStage, shaderPrimPath);
    assert(uniformShad);
    UsdShadeShader timeVarShad = UsdShadeShader::Get(materialStage, shaderPrimPath);
    assert(timeVarShad);

    //Bind the sampler to the diffuse color of the uniform shader, so remove any existing values from the timeVar prim 
    //(ideally user sets diffuse and specular to uniform)
    timeVarShad.GetInput(UsdBridgeTokens->diffuseColor).GetAttr().Clear();
    timeVarShad.GetInput(UsdBridgeTokens->specularColor).GetAttr().Clear();

    UsdShadeOutput refSamplerOutput = refSampler.CreateOutput(UsdBridgeTokens->rgb, SdfValueTypeNames->Color3f);

    uniformShad.GetInput(UsdBridgeTokens->diffuseColor).ConnectToSource(refSamplerOutput);
    uniformShad.GetInput(UsdBridgeTokens->specularColor).ConnectToSource(refSamplerOutput);
  }

#ifdef SUPPORT_MDL_SHADERS
  if(Settings.EnableMdlShader)
  {
    // If no url available, use generated filename using data from the referenced sampler
    const char* imgFileName = samplerImageUrl;
    std::string generatedFileName = GetResourceFileName(imgFolder, samplerPrimName, samplerTimeStep, imageExtension);

    if(!imgFileName)
      imgFileName = generatedFileName.c_str();

    // Only uses the uniform prim and world timestep, as the timevar material subprim is value-clip-referenced 
    // from the material's parent, and therefore not in world-time. Figuring out material to sampler mapping is not worth the effort 
    // and would not fix the underlying issue (this has to be part of a separate sampler prim)
    
    // Should ideally be connected to the sampler prim's file input, but that isn't supported by Create as of writing.

    SdfPath mdlShaderPrimPath = matPrimPath.AppendPath(SdfPath(mdlShaderPrimPf));
    UsdShadeShader uniformMdlShad = UsdShadeShader::Get(this->SceneStage, mdlShaderPrimPath);
    assert(uniformMdlShad); 

    UsdTimeCode timeCode = samplerImageTimeVarying ? UsdTimeCode(worldTimeStep) : UsdTimeCode::Default();

    UsdShadeInput diffTexInput = uniformMdlShad.CreateInput(UsdBridgeTokens->diffuse_texture, SdfValueTypeNames->Asset);
    if(!samplerImageTimeVarying)
      diffTexInput.GetAttr().Clear();
    diffTexInput.Set(SdfAssetPath(imgFileName), timeCode);

    uniformMdlShad.GetInput(UsdBridgeTokens->vertexcolor_coordinate_index).Set(-1);
  }
#endif
}

void UsdBridgeUsdWriter::UnBindSamplerFromMaterial(const SdfPath& matPrimPath)
{
  if(Settings.EnablePreviewSurfaceShader)
  {
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(shaderPrimPf));

    // Disconnect the sampler from the shader
    UsdShadeShader uniformShad = UsdShadeShader::Get(this->SceneStage, shaderPrimPath);
    assert(uniformShad);

    UsdShadeInput diffuseInput = uniformShad.GetInput(UsdBridgeTokens->diffuseColor);
    if (diffuseInput && diffuseInput.HasConnectedSource())
      diffuseInput.DisconnectSource();
    UsdShadeInput specularInput = uniformShad.GetInput(UsdBridgeTokens->specularColor);
    if (specularInput && specularInput.HasConnectedSource())
      specularInput.DisconnectSource();
  }

#ifdef SUPPORT_MDL_SHADERS
  if(Settings.EnableMdlShader)
  {
    SdfPath mdlShaderPrimPath = matPrimPath.AppendPath(SdfPath(mdlShaderPrimPf));
    UsdShadeShader uniformMdlShad = UsdShadeShader::Get(this->SceneStage, mdlShaderPrimPath);
    assert(uniformMdlShad);

    uniformMdlShad.GetPrim().RemoveProperty(TfToken("inputs:diffuse_texture"));
  }
#endif
}

void UsdBridgeUsdWriter::UpdateUsdTransform(const SdfPath& transPrimPath, float* transform, bool timeVarying, double timeStep)
{
  TimeEvaluator<bool> timeEval(timeVarying, timeStep);

  // Note: USD employs left-multiplication (vector in row-space)
  GfMatrix4d transMat; // Transforms can only be double, see UsdGeomXform::AddTransformOp (UsdGeomXformable)
  transMat.SetRow(0, GfVec4d(GfVec4f(&transform[0])));
  transMat.SetRow(1, GfVec4d(GfVec4f(&transform[4])));
  transMat.SetRow(2, GfVec4d(GfVec4f(&transform[8])));
  transMat.SetRow(3, GfVec4d(GfVec4f(&transform[12])));

  //Note that instance transform nodes have already been created.
  UsdGeomXform tfPrim = UsdGeomXform::Get(this->SceneStage, transPrimPath);
  assert(tfPrim);
  tfPrim.ClearXformOpOrder();
  tfPrim.AddTransformOp().Set(transMat, timeEval.Eval());
}

namespace
{
  void CopyArrayToPrimvar(UsdBridgeUsdWriter* writer, const void* arrayData, UsdBridgeType arrayDataType, size_t arrayNumElements, UsdAttribute arrayPrimvar, const UsdTimeCode& timeCode)
  {
    SdfValueTypeName primvarType = GetPrimvarArrayType(arrayDataType);

    switch (arrayDataType)
    {
      case UsdBridgeType::UCHAR: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtUCharArray); break; }
      case UsdBridgeType::CHAR: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtUCharArray); break; }
      case UsdBridgeType::USHORT: { ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtUIntArray, short); break; }
      case UsdBridgeType::SHORT: { ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtIntArray, unsigned short); break; }
      case UsdBridgeType::UINT: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtUIntArray); break; }
      case UsdBridgeType::INT: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtIntArray); break; }
      case UsdBridgeType::LONG: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtInt64Array); break; }
      case UsdBridgeType::ULONG: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtUInt64Array); break; }
      case UsdBridgeType::HALF: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtHalfArray); break; }
      case UsdBridgeType::FLOAT: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtFloatArray); break; }
      case UsdBridgeType::DOUBLE: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtDoubleArray); break; }

      case UsdBridgeType::INT2: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec2iArray); break; }
      case UsdBridgeType::FLOAT2: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec2fArray); break; }
      case UsdBridgeType::DOUBLE2: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec2dArray); break; }

      case UsdBridgeType::INT3: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3iArray); break; }
      case UsdBridgeType::FLOAT3: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3fArray); break; }
      case UsdBridgeType::DOUBLE3: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3dArray); break; }

      case UsdBridgeType::INT4: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec4iArray); break; }
      case UsdBridgeType::FLOAT4: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec4fArray); break; }
      case UsdBridgeType::DOUBLE4: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec4dArray); break; }

      case UsdBridgeType::UCHAR2:
      case UsdBridgeType::UCHAR3: 
      case UsdBridgeType::UCHAR4: { ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(VtUCharArray); break; }
      case UsdBridgeType::CHAR2:
      case UsdBridgeType::CHAR3: 
      case UsdBridgeType::CHAR4: { ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(VtUCharArray); break; }
      case UsdBridgeType::USHORT2:
      case UsdBridgeType::USHORT3:
      case UsdBridgeType::USHORT4: { ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_FLATTEN_MACRO(VtUIntArray, short); break; }
      case UsdBridgeType::SHORT2:
      case UsdBridgeType::SHORT3: 
      case UsdBridgeType::SHORT4: { ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_FLATTEN_MACRO(VtIntArray, unsigned short); break; }
      case UsdBridgeType::UINT2:
      case UsdBridgeType::UINT3: 
      case UsdBridgeType::UINT4: { ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(VtUIntArray); break; }
      case UsdBridgeType::LONG2:
      case UsdBridgeType::LONG3: 
      case UsdBridgeType::LONG4: { ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(VtInt64Array); break; }
      case UsdBridgeType::ULONG2:
      case UsdBridgeType::ULONG3: 
      case UsdBridgeType::ULONG4: { ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(VtUInt64Array); break; }
      case UsdBridgeType::HALF2:
      case UsdBridgeType::HALF3: 
      case UsdBridgeType::HALF4: { ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(VtHalfArray); break; }

      default: {UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom Attribute<Index> primvar copy does not support source data type: " << arrayDataType) break; }
    };
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomPoints(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::POINTS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::POINTS);

  ClearAttributes(UsdGeomGetPointsAttribute(uniformGeom), UsdGeomGetPointsAttribute(timeVarGeom), timeVaryingUpdate);
  ClearAttributes(uniformGeom.GetExtentAttr(), timeVarGeom.GetExtentAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    if (!geomData.Points)
    {
      UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "GeomData requires points.");
    }
    else
    {
      UsdGeomType* outGeom = timeVaryingUpdate ? &timeVarGeom : &uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::POINTS);

      // Points
      UsdAttribute pointsAttr = UsdGeomGetPointsAttribute(*outGeom);

      const void* arrayData = geomData.Points;
      size_t arrayNumElements = geomData.NumPoints;
      UsdAttribute arrayPrimvar = pointsAttr;
      VtVec3fArray usdVerts;
      switch (geomData.PointsType)
      {
      case UsdBridgeType::FLOAT3: {ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_MACRO(VtVec3fArray, usdVerts); break; }
      case UsdBridgeType::DOUBLE3: {ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec3fArray, GfVec3d, usdVerts); break; }
      default: { UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom PointsAttr should be FLOAT3 or DOUBLE3."); break; }
      }

      // Usd requires extent.
      GfRange3f extent;
      for (const auto& pt : usdVerts) {
        extent.UnionWith(pt);
      }
      VtVec3fArray extentArray(2);
      extentArray[0] = extent.GetMin();
      extentArray[1] = extent.GetMax();

      outGeom->GetExtentAttr().Set(extentArray, timeCode);
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomIndices(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::INDICES);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INDICES);

  ClearAttributes(uniformGeom.GetFaceVertexIndicesAttr(), timeVarGeom.GetFaceVertexIndicesAttr(), timeVaryingUpdate);
  ClearAttributes(uniformGeom.GetFaceVertexCountsAttr(), timeVarGeom.GetFaceVertexCountsAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdGeomType* outGeom = timeVaryingUpdate ? &timeVarGeom : &uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::INDICES);

    uint64_t numIndices = geomData.NumIndices;
   
    VtArray<int> usdVertexCounts(numPrims);
    int vertexCount = numIndices / numPrims;
    for (uint64_t i = 0; i < numPrims; ++i)
      usdVertexCounts[i] = vertexCount;//geomData.FaceVertCounts[i];

    // Face Vertex counts
    UsdAttribute faceVertCountsAttr = outGeom->GetFaceVertexCountsAttr();
    faceVertCountsAttr.Set(usdVertexCounts, timeCode);

    if (!geomData.Indices)
    {
      writer->TempIndexArray.resize(numIndices);
      for (uint64_t i = 0; i < numIndices; ++i)
        writer->TempIndexArray[i] = (int)i;

      UsdAttribute arrayPrimvar = outGeom->GetFaceVertexIndicesAttr();
      arrayPrimvar.Set(writer->TempIndexArray, timeCode);
    }
    else
    {
      // Face indices
      const void* arrayData = geomData.Indices;
      size_t arrayNumElements = numIndices;
      UsdAttribute arrayPrimvar = outGeom->GetFaceVertexIndicesAttr();
      switch (geomData.IndicesType)
      {
      case UsdBridgeType::ULONG: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtIntArray, uint64_t); break; }
      case UsdBridgeType::LONG: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtIntArray, int64_t); break; }
      case UsdBridgeType::INT: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtIntArray); break; }
      case UsdBridgeType::UINT: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtIntArray); break; }
      default: { UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom FaceVertexIndicesAttr should be (U)LONG or (U)INT."); break; }
      }
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomNormals(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::NORMALS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::NORMALS);

  ClearAttributes(uniformGeom.GetNormalsAttr(), timeVarGeom.GetNormalsAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdGeomType* outGeom = timeVaryingUpdate ? &timeVarGeom : &uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::NORMALS);

    UsdAttribute normalsAttr = outGeom->GetNormalsAttr();

    if (geomData.Normals != nullptr)
    {
      const void* arrayData = geomData.Normals;
      size_t arrayNumElements = geomData.PerPrimNormals ? numPrims : geomData.NumPoints;
      UsdAttribute arrayPrimvar = normalsAttr;
      switch (geomData.NormalsType)
      {
      case UsdBridgeType::FLOAT3: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3fArray); break; }
      case UsdBridgeType::DOUBLE3: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec3fArray, GfVec3d); break; }
      default: { UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom NormalsAttr should be FLOAT3 or DOUBLE3."); break; }
      }

      // Per face or per-vertex interpolation. This will break timesteps that have been written before.
      TfToken normalInterpolation = geomData.PerPrimNormals ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
      uniformGeom.SetNormalsInterpolation(normalInterpolation);
    }
    else
    {
      normalsAttr.Set(SdfValueBlock(), timeCode);
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomTexCoords(UsdBridgeUsdWriter* writer, UsdGeomPrimvarsAPI& timeVarPrimvars, UsdGeomType& uniformPrimvars, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::ATTRIBUTE0);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ATTRIBUTE0);

  UsdGeomPrimvar uniformPrimvar = uniformPrimvars.GetPrimvar(UsdBridgeTokens->st);
  UsdGeomPrimvar timeVarPrimvar = timeVarPrimvars.GetPrimvar(UsdBridgeTokens->st);

  ClearAttributes(uniformPrimvar.GetAttr(), timeVarPrimvar.GetAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdTimeCode timeCode = timeEval.Eval(DMI::ATTRIBUTE0);

    UsdAttribute texcoordPrimvar = timeVaryingUpdate ? timeVarPrimvar : uniformPrimvar;
    assert(texcoordPrimvar);

    const UsdBridgeAttribute& texCoordAttrib = geomData.Attributes[0];

    if (texCoordAttrib.Data != nullptr)
    {
      const void* arrayData = texCoordAttrib.Data;
      size_t arrayNumElements = texCoordAttrib.PerPrimData ? numPrims : geomData.NumPoints;
      UsdAttribute arrayPrimvar = texcoordPrimvar;

      switch (texCoordAttrib.DataType)
      {
      case UsdBridgeType::FLOAT2: { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec2fArray); break; }
      case UsdBridgeType::DOUBLE2: { ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec2fArray, GfVec2d); break; }
      default: { UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom st primvar should be FLOAT2 or DOUBLE2."); break; }
      }
 
      // Per face or per-vertex interpolation. This will break timesteps that have been written before.
      TfToken texcoordInterpolation = texCoordAttrib.PerPrimData ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
      uniformPrimvar.SetInterpolation(texcoordInterpolation);
    }
    else
    {
      texcoordPrimvar.Set(SdfValueBlock(), timeCode);
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomAttribute(UsdBridgeUsdWriter* writer, UsdGeomPrimvarsAPI& timeVarPrimvars, UsdGeomType& uniformPrimvars, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval, uint32_t attribIndex)
{
  assert(attribIndex < geomData.NumAttributes);
  const UsdBridgeAttribute& bridgeAttrib = geomData.Attributes[attribIndex];

  TfToken attribToken = AttribIndexToToken(attribIndex);
  UsdGeomPrimvar uniformPrimvar = uniformPrimvars.GetPrimvar(attribToken);
  UsdGeomPrimvar timeVarPrimvar = timeVarPrimvars.GetPrimvar(attribToken);

  using DMI = typename GeomDataType::DataMemberId;
  DMI attributeId = DMI::ATTRIBUTE0 + attribIndex;
  bool performsUpdate = updateEval.PerformsUpdate(attributeId);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(attributeId);

  ClearAttributes(uniformPrimvar.GetAttr(), timeVarPrimvar.GetAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdTimeCode timeCode = timeEval.Eval(attributeId);

    UsdAttribute attributePrimvar = timeVaryingUpdate ? timeVarPrimvar : uniformPrimvar;

    if(!attributePrimvar)
    {
      UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom Attribute<Index> primvar not found, was the attribute at requested index valid during initialization of the prim? Index is " << attribIndex);
    }
    else
    {
      if (bridgeAttrib.Data != nullptr)
      {
        const void* arrayData = bridgeAttrib.Data;
        size_t arrayNumElements = bridgeAttrib.PerPrimData ? numPrims : geomData.NumPoints;
        UsdAttribute arrayPrimvar = attributePrimvar;

        CopyArrayToPrimvar(writer, arrayData, bridgeAttrib.DataType, arrayNumElements, arrayPrimvar, timeCode);
  
        // Per face or per-vertex interpolation. This will break timesteps that have been written before.
        TfToken attribInterpolation = bridgeAttrib.PerPrimData ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
        uniformPrimvar.SetInterpolation(attribInterpolation);
      }
      else
      {
        attributePrimvar.Set(SdfValueBlock(), timeCode);
      }
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomAttributes(UsdBridgeUsdWriter* writer, UsdGeomPrimvarsAPI& timeVarPrimvars, UsdGeomType& uniformPrimvars, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  uint32_t startIdx = UsdGeomDataHasTexCoords(geomData) ? 1 : 0;
  for(uint32_t attribIndex = startIdx; attribIndex < geomData.NumAttributes; ++attribIndex)
  {
    const UsdBridgeAttribute& attrib = geomData.Attributes[attribIndex];
    UpdateUsdGeomAttribute(writer, timeVarPrimvars, uniformPrimvars, geomData, numPrims, updateEval, timeEval, attribIndex);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomColors(UsdBridgeUsdWriter* writer, UsdGeomPrimvarsAPI& timeVarPrimvars, UsdGeomType& uniformPrimvars, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::COLORS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::COLORS);

  UsdGeomPrimvar uniformDispPrimvar = uniformPrimvars.GetPrimvar(UsdBridgeTokens->displayColor);
  UsdGeomPrimvar timeVarDispPrimvar = timeVarPrimvars.GetPrimvar(UsdBridgeTokens->displayColor);
#ifdef SUPPORT_MDL_SHADERS
  UsdGeomPrimvar uniformSt1Primvar = uniformPrimvars.GetPrimvar(UsdBridgeTokens->st1);
  UsdGeomPrimvar timeVarSt1Primvar = timeVarPrimvars.GetPrimvar(UsdBridgeTokens->st1);
  UsdGeomPrimvar uniformSt2Primvar = uniformPrimvars.GetPrimvar(UsdBridgeTokens->st2);
  UsdGeomPrimvar timeVarSt2Primvar = timeVarPrimvars.GetPrimvar(UsdBridgeTokens->st2);
#endif

  if(writer->Settings.EnableDisplayColors)
  {
    ClearAttributes(uniformDispPrimvar.GetAttr(), timeVarDispPrimvar.GetAttr(), timeVaryingUpdate);
  }
#ifdef SUPPORT_MDL_SHADERS
  if (writer->Settings.EnableMdlColors)
  {
    ClearAttributes(uniformSt1Primvar.GetAttr(), timeVarSt1Primvar.GetAttr(), timeVaryingUpdate);
    ClearAttributes(uniformSt2Primvar.GetAttr(), timeVarSt2Primvar.GetAttr(), timeVaryingUpdate);
  }
#endif

  if (performsUpdate)
  {
    UsdTimeCode timeCode = timeEval.Eval(DMI::COLORS);

    UsdGeomPrimvar colorPrimvar = timeVaryingUpdate ? timeVarDispPrimvar : uniformDispPrimvar;
#ifdef SUPPORT_MDL_SHADERS
    UsdGeomPrimvar vc0Primvar = timeVaryingUpdate ? timeVarSt1Primvar : uniformSt1Primvar;
    UsdGeomPrimvar vc1Primvar = timeVaryingUpdate ? timeVarSt2Primvar : uniformSt2Primvar;
#endif

    if (geomData.Colors != nullptr)
    {
      const void* arrayData = geomData.Colors;
      size_t arrayNumElements = geomData.PerPrimColors ? numPrims : geomData.NumPoints;
      TfToken colorInterpolation = geomData.PerPrimColors ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;

      bool typeSupported = true;
      if(writer->Settings.EnableDisplayColors)
      {
        assert(colorPrimvar);

        UsdAttribute arrayPrimvar = colorPrimvar;
        switch (geomData.ColorsType)
        {
        case UsdBridgeType::FLOAT3: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3fArray); break; }
        case UsdBridgeType::FLOAT4: {ASSIGN_ARRAY_TO_PRIMVAR_REDUCED_MACRO(VtVec3fArray, GfVec4f); break; }
        case UsdBridgeType::DOUBLE3: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec3fArray, GfVec3d); break; }
        case UsdBridgeType::DOUBLE4: {ASSIGN_ARRAY_TO_PRIMVAR_REDUCED_MACRO(VtVec3dArray, GfVec4d); break; }
        default: { typeSupported = false; UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom DisplayColorPrimvar should be FLOAT3, FLOAT4, DOUBLE3 or DOUBLE4."); break; }
        }

        // Per face or per-vertex interpolation. This will break timesteps that have been written before.
        uniformDispPrimvar.SetInterpolation(colorInterpolation);
      }
      else
      {
        typeSupported = (geomData.ColorsType == UsdBridgeType::FLOAT3 || geomData.ColorsType == UsdBridgeType::FLOAT4 ||
          geomData.ColorsType == UsdBridgeType::DOUBLE3 || geomData.ColorsType == UsdBridgeType::DOUBLE4);
      }

#ifdef SUPPORT_MDL_SHADERS
      if (typeSupported && writer->Settings.EnableMdlColors)
      {
        assert(vc0Primvar);
        assert(vc1Primvar);

        // Make sure the "st" array has values
        {
          const UsdGeomPrimvarsAPI& outPrimvars = timeVaryingUpdate ? timeVarPrimvars : uniformPrimvars;
          UsdAttribute texcoordPrimvar = outPrimvars.GetPrimvar(UsdBridgeTokens->st);
          assert(texcoordPrimvar);

          if (!texcoordPrimvar.HasValue())
          {
            VtVec2fArray defaultTexCoords(geomData.NumPoints);
            defaultTexCoords.assign(geomData.NumPoints, GfVec2f(0.0f, 0.0f));

            texcoordPrimvar.Set(defaultTexCoords, timeCode);
          }
        }

        // Fill the vc arrays
        VtVec2fArray customVertexColors0(arrayNumElements), customVertexColors1(arrayNumElements);
        int numComponents = (geomData.ColorsType == UsdBridgeType::FLOAT3 || geomData.ColorsType == UsdBridgeType::DOUBLE3) ?
          3 : 4;
        double* dElts = (double*)arrayData;
        float* fElts = (float*)arrayData;
        bool alphaComponent = (numComponents == 4);

        if (geomData.ColorsType == UsdBridgeType::FLOAT3 || geomData.ColorsType == UsdBridgeType::FLOAT4)
        {
          for (int i = 0; i < arrayNumElements; ++i, fElts += numComponents)
          {
            customVertexColors0[i] = GfVec2f(fElts[0], fElts[1]);
            customVertexColors1[i] = GfVec2f(fElts[2], alphaComponent ? fElts[3] : 1.0f);
          }
        }
        else
        {
          for (int i = 0; i < arrayNumElements; ++i, dElts += numComponents)
          {
            customVertexColors0[i] = GfVec2f((float)(dElts[0]), (float)(dElts[1]));
            customVertexColors1[i] = GfVec2f((float)(dElts[2]), alphaComponent ? (float)(dElts[3]) : 1.0f);
          }
        }

        vc0Primvar.Set(customVertexColors0, timeCode);
        vc1Primvar.Set(customVertexColors1, timeCode);

        uniformSt1Primvar.SetInterpolation(colorInterpolation);
        uniformSt2Primvar.SetInterpolation(colorInterpolation);
      }
#endif
    }
    else
    {
      if(writer->Settings.EnableDisplayColors)
      {
        colorPrimvar.GetAttr().Set(SdfValueBlock(), timeCode);
      }
#ifdef SUPPORT_MDL_SHADERS
      if (writer->Settings.EnableMdlColors)
      {
        vc0Primvar.GetAttr().Set(SdfValueBlock(), timeCode);
        vc1Primvar.GetAttr().Set(SdfValueBlock(), timeCode);
      }
#endif
    }
  }
}


template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomInstanceIds(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::INSTANCEIDS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INSTANCEIDS);

  ClearAttributes(uniformGeom.GetIdsAttr(), timeVarGeom.GetIdsAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdGeomType* outGeom = timeVaryingUpdate ? &timeVarGeom : &uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::INSTANCEIDS);

    UsdAttribute idsAttr = outGeom->GetIdsAttr();

    if (geomData.InstanceIds)
    {
      const void* arrayData = geomData.InstanceIds;
      size_t arrayNumElements = geomData.NumPoints;
      UsdAttribute arrayPrimvar = idsAttr;
      switch (geomData.InstanceIdsType)
      {
      case UsdBridgeType::UINT: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtInt64Array, unsigned int); break; }
      case UsdBridgeType::INT: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtInt64Array, int); break; }
      case UsdBridgeType::LONG: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtInt64Array); break; }
      case UsdBridgeType::ULONG: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtInt64Array); break; }
      default: { UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom IdsAttribute should be (U)LONG or (U)INT."); break; }
      }
    }
    else
    {
      idsAttr.Set(SdfValueBlock(), timeCode);
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomWidths(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::SCALES);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::SCALES);

  ClearAttributes(uniformGeom.GetWidthsAttr(), timeVarGeom.GetWidthsAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::SCALES);

    UsdAttribute widthsAttribute = outGeom.GetWidthsAttr();
    assert(widthsAttribute);
    if (geomData.Scales)
    {
      const void* arrayData = geomData.Scales;
      size_t arrayNumElements = geomData.NumPoints;
      UsdAttribute arrayPrimvar = widthsAttribute;
      switch (geomData.ScalesType)
      {
      case UsdBridgeType::FLOAT: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtFloatArray); break; }
      case UsdBridgeType::DOUBLE: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtFloatArray, double); break; }
      default: { UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom WidthsAttribute should be FLOAT or DOUBLE."); break; }
      }
    }
    else
    {
      VtFloatArray usdWidths(geomData.NumPoints, (float)geomData.UniformScale);
      widthsAttribute.Set(usdWidths, timeCode);
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomScales(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::SCALES);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::SCALES);

  ClearAttributes(uniformGeom.GetScalesAttr(), timeVarGeom.GetScalesAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::SCALES);

    UsdAttribute scalesAttribute = outGeom.GetScalesAttr();
    assert(scalesAttribute);
    if (geomData.Scales)
    {
      const void* arrayData = geomData.Scales;
      size_t arrayNumElements = geomData.NumPoints;
      UsdAttribute arrayPrimvar = scalesAttribute;
      switch (geomData.ScalesType)
      {
      case UsdBridgeType::FLOAT: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO_EXPAND3(VtVec3fArray, float, writer->TempScalesArray); break;}
      case UsdBridgeType::DOUBLE: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO_EXPAND3(VtVec3fArray, double, writer->TempScalesArray); break;}
      case UsdBridgeType::FLOAT3: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3fArray); break; }
      case UsdBridgeType::DOUBLE3: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec3fArray, GfVec3d); break; }
      default: { UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom ScalesAttribute should be FLOAT(3) or DOUBLE(3)."); break; }
      }
    }
    else
    {
      double pointScale = geomData.UniformScale;
      GfVec3f defaultScale((float)pointScale, (float)pointScale, (float)pointScale);
      VtVec3fArray usdScales(geomData.NumPoints, defaultScale);
      scalesAttribute.Set(usdScales, timeCode);
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomOrientNormals(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::ORIENTATIONS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ORIENTATIONS);

  ClearAttributes(uniformGeom.GetNormalsAttr(), timeVarGeom.GetNormalsAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::ORIENTATIONS);

    UsdAttribute normalsAttribute = outGeom.GetNormalsAttr();
    assert(normalsAttribute);
    if (geomData.Orientations)
    {
      const void* arrayData = geomData.Orientations;
      size_t arrayNumElements = geomData.NumPoints;
      UsdAttribute arrayPrimvar = normalsAttribute;
      switch (geomData.OrientationsType)
      {
      case UsdBridgeType::FLOAT3: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3fArray); break; }
      case UsdBridgeType::DOUBLE3: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec3fArray, GfVec3d); break; }
      default: { UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom NormalsAttribute (orientations) should be FLOAT3 or DOUBLE3."); break; }
      }
    }
    else
    {
      //Always provide a default orientation
      GfVec3f defaultNormal(1, 0, 0);
      VtVec3fArray usdNormals(geomData.NumPoints, defaultNormal);
      normalsAttribute.Set(usdNormals, timeCode);
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomOrientations(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::ORIENTATIONS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ORIENTATIONS);

  ClearAttributes(uniformGeom.GetOrientationsAttr(), timeVarGeom.GetOrientationsAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::ORIENTATIONS);

    // Orientations
    UsdAttribute orientationsAttribute = outGeom.GetOrientationsAttr();
    assert(orientationsAttribute);
    if (geomData.Orientations)
    {
      VtQuathArray usdOrients(geomData.NumPoints);
      switch (geomData.OrientationsType)
      {
      case UsdBridgeType::FLOAT3: { ConvertNormalsToQuaternions<float>(usdOrients, geomData.Orientations, geomData.NumPoints); break; }
      case UsdBridgeType::DOUBLE3: { ConvertNormalsToQuaternions<double>(usdOrients, geomData.Orientations, geomData.NumPoints); break; }
      case UsdBridgeType::FLOAT4: 
        { 
          for (uint64_t i = 0; i < geomData.NumPoints; ++i)
          {
            const float* orients = reinterpret_cast<const float*>(geomData.Orientations);
            usdOrients[i] = GfQuath(orients[i * 4], orients[i * 4 + 1], orients[i * 4 + 2], orients[i * 4 + 3]);
          }
          orientationsAttribute.Set(usdOrients, timeCode);
          break; 
        }
      default: { UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom OrientationsAttribute should be FLOAT3, DOUBLE3 or FLOAT4."); break; }
      }
      orientationsAttribute.Set(usdOrients, timeCode);
    }
    else
    {
      //Always provide a default orientation
      GfQuath defaultOrient(1, 0, 0, 0);
      VtQuathArray usdOrients(geomData.NumPoints, defaultOrient);
      orientationsAttribute.Set(usdOrients, timeCode);
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomShapeIndices(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;

  UsdTimeCode timeCode = timeEval.Eval(DMI::SHAPEINDICES);
  UsdGeomType* outGeom = timeCode.IsDefault() ? &uniformGeom : &timeVarGeom;
  
  //Shape indices
  UsdAttribute protoIndexAttr = outGeom->GetProtoIndicesAttr();
  VtIntArray protoIndices(geomData.NumPoints, 0);
  protoIndexAttr.Set(protoIndices, timeCode);
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomLinearVelocities(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::LINEARVELOCITIES);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::LINEARVELOCITIES);

  ClearAttributes(uniformGeom.GetVelocitiesAttr(), timeVarGeom.GetVelocitiesAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::LINEARVELOCITIES);

    // Linear velocities
    UsdAttribute linearVelocitiesAttribute = outGeom.GetVelocitiesAttr();
    assert(linearVelocitiesAttribute);
    if (geomData.LinearVelocities)
    {
      GfVec3f* linVels = (GfVec3f*)geomData.LinearVelocities;

      VtVec3fArray usdVelocities;
      usdVelocities.assign(linVels, linVels + geomData.NumPoints);
      linearVelocitiesAttribute.Set(usdVelocities, timeCode);
    }
    else
    {
      linearVelocitiesAttribute.Set(SdfValueBlock(), timeCode);
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomAngularVelocities(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::ANGULARVELOCITIES);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ANGULARVELOCITIES);

  ClearAttributes(uniformGeom.GetAngularVelocitiesAttr(), timeVarGeom.GetAngularVelocitiesAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::ANGULARVELOCITIES);

    // Angular velocities
    UsdAttribute angularVelocitiesAttribute = outGeom.GetAngularVelocitiesAttr();
    assert(angularVelocitiesAttribute);
    if (geomData.AngularVelocities)
    {
      GfVec3f* angVels = (GfVec3f*)geomData.AngularVelocities;

      VtVec3fArray usdAngularVelocities;
      usdAngularVelocities.assign(angVels, angVels + geomData.NumPoints);
      angularVelocitiesAttribute.Set(usdAngularVelocities, timeCode);
    }
    else
    {
      angularVelocitiesAttribute.Set(SdfValueBlock(), timeCode);
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomInvisibleIds(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::INVISIBLEIDS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INVISIBLEIDS);

  ClearAttributes(uniformGeom.GetInvisibleIdsAttr(), timeVarGeom.GetInvisibleIdsAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::INVISIBLEIDS);

    // Invisible ids
    UsdAttribute invisIdsAttr = outGeom.GetInvisibleIdsAttr();
    assert(invisIdsAttr);
    uint64_t numInvisibleIds = geomData.NumInvisibleIds;
    if (numInvisibleIds)
    {
      const void* arrayData = geomData.InvisibleIds;
      size_t arrayNumElements = numInvisibleIds;
      UsdAttribute arrayPrimvar = invisIdsAttr;
      switch (geomData.InvisibleIdsType)
      {
      case UsdBridgeType::UINT: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtInt64Array, unsigned int); break; }
      case UsdBridgeType::INT: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtInt64Array, int); break; }
      case UsdBridgeType::LONG: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtInt64Array); break; }
      case UsdBridgeType::ULONG: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtInt64Array); break; }
      default: { UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom GetInvisibleIdsAttr should be (U)LONG or (U)INT."); break; }
      }
    }
    else
    {
      invisIdsAttr.Set(SdfValueBlock(), timeCode);
    }
  }
}

static void UpdateUsdGeomCurveLengths(UsdBridgeUsdWriter* writer, UsdGeomBasisCurves& timeVarGeom, UsdGeomBasisCurves& uniformGeom, const UsdBridgeCurveData& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const UsdBridgeCurveData>& updateEval, TimeEvaluator<UsdBridgeCurveData>& timeEval)
{
  using DMI = typename UsdBridgeCurveData::DataMemberId;
  // Fill geom prim and geometry layer with data.
  bool performsUpdate = updateEval.PerformsUpdate(DMI::CURVELENGTHS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::CURVELENGTHS);

  ClearAttributes(uniformGeom.GetCurveVertexCountsAttr(), timeVarGeom.GetCurveVertexCountsAttr(), timeVaryingUpdate);

  if (performsUpdate)
  {
    UsdGeomBasisCurves& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::POINTS);

    UsdAttribute vertCountAttr = outGeom.GetCurveVertexCountsAttr();
    assert(vertCountAttr);

    const void* arrayData = geomData.CurveLengths;
    size_t arrayNumElements = geomData.NumCurveLengths;
    UsdAttribute arrayPrimvar = vertCountAttr;
    VtVec3fArray usdVerts;
    { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtIntArray); }
  }
}

#define UPDATE_USDGEOM_ARRAYS(FuncDef) \
  FuncDef(this, timeVarGeom, uniformGeom, geomData, numPrims, updateEval, timeEval)

#define UPDATE_USDGEOM_PRIMVAR_ARRAYS(FuncDef) \
  FuncDef(this, timeVarPrimvars, uniformPrimvars, geomData, numPrims, updateEval, timeEval)

void UsdBridgeUsdWriter::UpdateUsdGeometry(const UsdStagePtr& timeVarStage, const SdfPath& meshPath, const UsdBridgeMeshData& geomData, double timeStep)
{
  // To avoid data duplication when using of clip stages, we need to potentially use the scenestage prim for time-uniform data.
  UsdGeomMesh uniformGeom = UsdGeomMesh::Get(this->SceneStage, meshPath);
  assert(uniformGeom);
  UsdGeomPrimvarsAPI uniformPrimvars(uniformGeom);

  UsdGeomMesh timeVarGeom = UsdGeomMesh::Get(timeVarStage, meshPath);
  assert(timeVarGeom);
  UsdGeomPrimvarsAPI timeVarPrimvars(timeVarGeom);

  // Update the mesh
  UsdBridgeUpdateEvaluator<const UsdBridgeMeshData> updateEval(geomData);
  TimeEvaluator<UsdBridgeMeshData> timeEval(geomData, timeStep);

  assert((geomData.NumIndices % geomData.FaceVertexCount) == 0);
  uint64_t numPrims = int(geomData.NumIndices) / geomData.FaceVertexCount;

  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomPoints);
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomNormals);
  if( UsdGeomDataHasTexCoords(geomData) ) 
    { UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomTexCoords); }
  UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomAttributes);
  UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomColors);
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomIndices);
}

void UsdBridgeUsdWriter::UpdateUsdGeometry(const UsdStagePtr& timeVarStage, const SdfPath& instancerPath, const UsdBridgeInstancerData& geomData, double timeStep)
{
  UsdBridgeUpdateEvaluator<const UsdBridgeInstancerData> updateEval(geomData);
  TimeEvaluator<UsdBridgeInstancerData> timeEval(geomData, timeStep);

  bool useGeomPoints = UsesUsdGeomPoints(geomData);

  uint64_t numPrims = geomData.NumPoints;

  if (useGeomPoints)
  {
    UsdGeomPoints uniformGeom = UsdGeomPoints::Get(this->SceneStage, instancerPath);
    assert(uniformGeom);
    UsdGeomPrimvarsAPI uniformPrimvars(uniformGeom);

    UsdGeomPoints timeVarGeom = UsdGeomPoints::Get(timeVarStage, instancerPath);
    assert(timeVarGeom);
    UsdGeomPrimvarsAPI timeVarPrimvars(timeVarGeom);

    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomPoints);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomInstanceIds);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomWidths);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomOrientNormals);
    if( UsdGeomDataHasTexCoords(geomData) ) 
      { UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomTexCoords); }
    UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomAttributes);
    UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomColors);
  }
  else
  {
    UsdGeomPointInstancer uniformGeom = UsdGeomPointInstancer::Get(this->SceneStage, instancerPath);
    assert(uniformGeom);
    UsdGeomPrimvarsAPI uniformPrimvars(uniformGeom);

    UsdGeomPointInstancer timeVarGeom = UsdGeomPointInstancer::Get(timeVarStage, instancerPath);
    assert(timeVarGeom);
    UsdGeomPrimvarsAPI timeVarPrimvars(timeVarGeom);

    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomPoints);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomInstanceIds);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomScales);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomOrientations);
    if( UsdGeomDataHasTexCoords(geomData) ) 
      { UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomTexCoords); }
    UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomAttributes);
    UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomColors);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomShapeIndices);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomLinearVelocities);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomAngularVelocities);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomInvisibleIds);
  }
}

void UsdBridgeUsdWriter::UpdateUsdGeometry(const UsdStagePtr& timeVarStage, const SdfPath& curvePath, const UsdBridgeCurveData& geomData, double timeStep)
{
  // To avoid data duplication when using of clip stages, we need to potentially use the scenestage prim for time-uniform data.
  UsdGeomBasisCurves uniformGeom = UsdGeomBasisCurves::Get(this->SceneStage, curvePath);
  assert(uniformGeom);
  UsdGeomPrimvarsAPI uniformPrimvars(uniformGeom);

  UsdGeomBasisCurves timeVarGeom = UsdGeomBasisCurves::Get(timeVarStage, curvePath);
  assert(timeVarGeom);
  UsdGeomPrimvarsAPI timeVarPrimvars(timeVarGeom);

  // Update the curve
  UsdBridgeUpdateEvaluator<const UsdBridgeCurveData> updateEval(geomData);
  TimeEvaluator<UsdBridgeCurveData> timeEval(geomData, timeStep);

  uint64_t numPrims = geomData.NumCurveLengths;

  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomPoints);
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomNormals);
  if( UsdGeomDataHasTexCoords(geomData) ) 
    { UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomTexCoords); }
  UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomAttributes);
  UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomColors);
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomWidths);
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomCurveLengths);
}

void UsdBridgeUsdWriter::UpdateUsdMaterial(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, const UsdBridgeMaterialData& matData, double timeStep)
{
  // Update usd shader
  if(Settings.EnablePreviewSurfaceShader)
  {
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(shaderPrimPf));
    this->UpdateUsdShader(timeVarStage, matPrimPath, shaderPrimPath, matData, timeStep);
  }

#ifdef SUPPORT_MDL_SHADERS  
  if(Settings.EnableMdlShader)
  {
    // Update mdl shader
    SdfPath mdlShaderPrimPath = matPrimPath.AppendPath(SdfPath(mdlShaderPrimPf));
    this->UpdateMdlShader(timeVarStage, mdlShaderPrimPath, matData, timeStep);
  }
#endif
}

void UsdBridgeUsdWriter::UpdateUsdShader(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, const SdfPath& shadPrimPath, const UsdBridgeMaterialData& matData, double timeStep)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData, timeStep);
  typedef UsdBridgeMaterialData::DataMemberId DMI;

  UsdShadeShader uniformShadPrim = UsdShadeShader::Get(SceneStage, shadPrimPath);
  assert(uniformShadPrim);

  UsdShadeShader timeVarShadPrim = UsdShadeShader::Get(timeVarStage, shadPrimPath);
  assert(timeVarShadPrim);

  GfVec3f difColor(matData.Diffuse);
  GfVec3f specColor(matData.Specular); // Not sure yet how to incorporate the specular color, it doesn't directly map to usd specularColor
  GfVec3f emColor(matData.Emissive);
  emColor *= matData.EmissiveIntensity;

  uniformShadPrim.GetInput(UsdBridgeTokens->useSpecularWorkflow).Set(matData.Metallic >= 0.0 ? 0 : 1);

  // Only set values on either timevar or uniform prim
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->roughness, DMI::ROUGHNESS, matData.Roughness);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->opacity, DMI::OPACITY, matData.Opacity);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->metallic, DMI::METALLIC, matData.Metallic);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->ior, DMI::IOR, matData.Ior);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->emissiveColor, DMI::EMISSIVE, emColor);

  UsdShadeInput uniformDiffInput = uniformShadPrim.GetInput(UsdBridgeTokens->diffuseColor);
  UsdShadeInput uniformSpecInput = uniformShadPrim.GetInput(UsdBridgeTokens->specularColor);

  // Always sets diffuse/specular on one primstage and completely removes it from the other
  if (matData.UseVertexColors)
  {
    SdfPath vertexColorReaderPrimPath = matPrimPath.AppendPath(SdfPath(vertexColorReaderPrimPf));
    UsdShadeShader vertexColorReader = UsdShadeShader::Get(SceneStage, vertexColorReaderPrimPath);
    assert(vertexColorReader);

    //Bind the vertex color reader to the diffuse/specular color of the uniform shader, so remove any existing values from the timeVar prim 
    //(ideally user sets diffuse and specular to uniform)
    timeVarShadPrim.GetInput(UsdBridgeTokens->diffuseColor).GetAttr().Clear();
    timeVarShadPrim.GetInput(UsdBridgeTokens->specularColor).GetAttr().Clear();
    
    UsdShadeOutput vcReaderOutput = vertexColorReader.CreateOutput(UsdBridgeTokens->result, SdfValueTypeNames->Color3f);

    uniformDiffInput.ConnectToSource(vcReaderOutput); // make sure to create input, as it may have been removed by other branch (or even the timeVarShadPrim, depending on compile defs)
    uniformSpecInput.ConnectToSource(vcReaderOutput);
  }
  else
  {
    if(uniformDiffInput.HasConnectedSource())
      uniformDiffInput.DisconnectSource();
    if(uniformSpecInput.HasConnectedSource())
      uniformSpecInput.DisconnectSource();

    // Just treat like regular time-varying inputs
    SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->diffuseColor, DMI::DIFFUSE, difColor);
    SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->specularColor, DMI::SPECULAR, specColor);
  }
}

#ifdef SUPPORT_MDL_SHADERS 
void UsdBridgeUsdWriter::UpdateMdlShader(UsdStageRefPtr timeVarStage, const SdfPath& shadPrimPath, const UsdBridgeMaterialData& matData, double timeStep)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData, timeStep);
  typedef UsdBridgeMaterialData::DataMemberId DMI;

  UsdShadeShader uniformShadPrim = UsdShadeShader::Get(SceneStage, shadPrimPath);
  assert(uniformShadPrim);

  UsdShadeShader timeVarShadPrim = UsdShadeShader::Get(timeVarStage, shadPrimPath);
  assert(timeVarShadPrim);

  GfVec3f difColor(matData.Diffuse);
  GfVec3f specColor(matData.Specular); // Not sure yet how to incorporate the specular color, no mdl parameter available.
  GfVec3f emColor(matData.Emissive);

  uniformShadPrim.GetInput(UsdBridgeTokens->vertexcolor_coordinate_index).Set(matData.UseVertexColors ? 1 : -1);

  // Only set values on either timevar or uniform prim
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->diffuse_color_constant, DMI::DIFFUSE, difColor);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->emissive_color, DMI::EMISSIVE, emColor);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->emissive_intensity, DMI::EMISSIVEINTENSITY, matData.EmissiveIntensity);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->opacity_constant, DMI::OPACITY, matData.Opacity);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->reflection_roughness_constant, DMI::ROUGHNESS, matData.Roughness);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->metallic_constant, DMI::METALLIC, matData.Metallic);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->ior_constant, DMI::IOR, matData.Ior);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->enable_emission, DMI::EMISSIVEINTENSITY, matData.EmissiveIntensity > 0);

  if (!matData.HasTranslucency)
    uniformShadPrim.SetSourceAsset(this->MdlOpaqueRelFilePath, UsdBridgeTokens->mdl);
  else
    uniformShadPrim.SetSourceAsset(this->MdlTranslucentRelFilePath, UsdBridgeTokens->mdl);
}
#endif

namespace
{
  void UpdateUsdVolumeAttributes(UsdVolVolume& uniformVolume, UsdVolVolume& timeVarVolume, UsdVolOpenVDBAsset& uniformField, UsdVolOpenVDBAsset& timeVarField,
    const UsdBridgeVolumeData& volumeData, double timeStep, const std::string& relVolPath)
  {
    TimeEvaluator<UsdBridgeVolumeData> timeEval(volumeData, timeStep);
    typedef UsdBridgeVolumeData::DataMemberId DMI;

    SdfAssetPath volAsset(relVolPath);

    // Set extents in usd
    VtVec3fArray extentArray(2);
    extentArray[0].Set(0.0f, 0.0f, 0.0f);
    extentArray[1].Set((float)volumeData.NumElements[0], (float)volumeData.NumElements[1], (float)volumeData.NumElements[2]); //approx extents

    UsdAttribute uniformFileAttr = uniformField.GetFilePathAttr();
    UsdAttribute timeVarFileAttr = timeVarField.GetFilePathAttr();
    UsdAttribute uniformExtentAttr = uniformVolume.GetExtentAttr();
    UsdAttribute timeVarExtentAttr = timeVarVolume.GetExtentAttr();

    bool dataTimeVarying = timeEval.IsTimeVarying(DMI::DATA);
    bool transformTimeVarying = timeEval.IsTimeVarying(DMI::TRANSFORM);

    // Clear timevarying attributes if necessary
    ClearAttributes(uniformFileAttr, timeVarFileAttr, dataTimeVarying);
    ClearAttributes(uniformExtentAttr, timeVarExtentAttr, dataTimeVarying);

    // Set the attributes
    if(dataTimeVarying)
    {
      timeVarFileAttr.Set(volAsset, timeEval.Eval(DMI::DATA));
      timeVarExtentAttr.Set(extentArray, timeEval.Eval(DMI::DATA));
    }
    else
    { 
      uniformFileAttr.Set(volAsset, timeEval.Eval(DMI::DATA));
      uniformExtentAttr.Set(extentArray, timeEval.Eval(DMI::DATA));
    }

    // Translate-scale in usd (slightly different from normal attribute setting)
    UsdGeomXformOp transOp, scaleOp;
    if(transformTimeVarying)
    {
  #ifdef VALUE_CLIP_RETIMING
  #ifdef OMNIVERSE_CREATE_WORKAROUNDS
      uniformVolume.ClearXformOpOrder();
  #endif
  #endif
      timeVarVolume.ClearXformOpOrder();
      transOp = timeVarVolume.AddTranslateOp();
      scaleOp = timeVarVolume.AddScaleOp();
    }
    else
    {
      uniformVolume.ClearXformOpOrder();
      transOp = uniformVolume.AddTranslateOp();
      scaleOp = uniformVolume.AddScaleOp();
    }

    GfVec3d trans(volumeData.Origin[0], volumeData.Origin[1], volumeData.Origin[2]);
    transOp.Set(trans, timeEval.Eval(DMI::TRANSFORM));

    GfVec3f scale(volumeData.CellDimensions[0], volumeData.CellDimensions[1], volumeData.CellDimensions[2]);
    scaleOp.Set(scale, timeEval.Eval(DMI::TRANSFORM));
  }
}

void UsdBridgeUsdWriter::UpdateUsdVolume(UsdStageRefPtr timeVarStage, const SdfPath& volPrimPath, const UsdBridgeVolumeData& volumeData, double timeStep, UsdBridgePrimCache* cacheEntry)
{
  // Get the volume and ovdb field prims
  UsdVolVolume uniformVolume = UsdVolVolume::Get(SceneStage, volPrimPath);
  assert(uniformVolume);
  UsdVolVolume timeVarVolume = UsdVolVolume::Get(timeVarStage, volPrimPath);
  assert(timeVarVolume);

  SdfPath ovdbFieldPath = volPrimPath.AppendPath(SdfPath(openVDBPrimPf));

  UsdVolOpenVDBAsset uniformField = UsdVolOpenVDBAsset::Get(SceneStage, ovdbFieldPath);
  assert(uniformField);
  UsdVolOpenVDBAsset timeVarField = UsdVolOpenVDBAsset::Get(timeVarStage, ovdbFieldPath);
  assert(timeVarField);

  // Set the file path reference in usd
  std::string relVolPath = GetResourceFileName(volFolder, cacheEntry->Name.GetString(), timeStep, vdbExtension);

  UpdateUsdVolumeAttributes(uniformVolume, timeVarVolume, uniformField, timeVarField, volumeData, timeStep, relVolPath);

  // Output stream path
  std::string fullVolPath(SessionDirectory + relVolPath);

  // Write VDB data to stream
  VolumeWriter->ToVDB(volumeData);

  // Flush stream out to storage
  const char* volumeStreamData; size_t volumeStreamDataSize;
  VolumeWriter->GetSerializedVolumeData(volumeStreamData, volumeStreamDataSize);
  Connect->WriteFile(volumeStreamData, volumeStreamDataSize, fullVolPath.c_str(), true);
  // Record file write for timestep
  cacheEntry->AddResourceTimeStep(timeStep);
}

void UsdBridgeUsdWriter::UpdateUsdSampler(UsdStageRefPtr timeVarStage, const SdfPath& samplerPrimPath, const UsdBridgeSamplerData& samplerData, double timeStep, UsdBridgePrimCache* cacheEntry)
{
  TimeEvaluator<UsdBridgeSamplerData> timeEval(samplerData, timeStep);
  typedef UsdBridgeSamplerData::DataMemberId DMI;

  UsdShadeShader uniformSamplerPrim = UsdShadeShader::Get(SceneStage, samplerPrimPath);
  assert(uniformSamplerPrim);

  UsdShadeShader timeVarSamplerPrim = UsdShadeShader::Get(timeVarStage, samplerPrimPath);
  assert(timeVarSamplerPrim);

  const char* imgFileName = samplerData.ImageUrl;
  // Generate an image url
  std::string generatedFileName = GetResourceFileName(imgFolder, cacheEntry->Name.GetString(), timeStep, imageExtension);

  if(!samplerData.ImageUrl)
  {
    imgFileName = generatedFileName.c_str();

    // If data array available, upload as image to texFile

    // Record file write for timestep
    cacheEntry->AddResourceTimeStep(timeStep);
  }

  SdfAssetPath texFile(imgFileName);
  SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->file, DMI::FILENAME, texFile);
  SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->WrapS, DMI::WRAPS, TextureWrapToken(samplerData.WrapS));
  if((uint32_t)samplerData.Type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
    SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->WrapT, DMI::WRAPT, TextureWrapToken(samplerData.WrapT));
  if((uint32_t)samplerData.Type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_3D)
    SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->WrapR, DMI::WRAPR, TextureWrapToken(samplerData.WrapR));

}

void UsdBridgeUsdWriter::UpdateBeginEndTime(double timeStep)
{
  if (timeStep < StartTime)
  {
    StartTime = timeStep;
    SceneStage->SetStartTimeCode(timeStep);
  }
  if (timeStep > EndTime)
  {
    EndTime = timeStep;
    SceneStage->SetEndTimeCode(timeStep);
  }
}

void RemoveResourceFiles(UsdBridgePrimCache* cache, const UsdBridgeUsdWriter& usdWriter, 
  const char* resourceFolder, const char* fileExtension)
{
  const std::string& name = cache->Name.GetString();

  // Directly from usd is inaccurate, as timesteps may have been cleared without file removal
  //// Assuming prim without clip stages.
  //const UsdStageRefPtr volumeStage = usdWriter.GetTimeVarStage(cache);
  //const UsdStageRefPtr volumeStage = usdWriter.GetTimeVarStage(cache);
  //const SdfPath& volPrimPath = cache->PrimPath;
  //
  //UsdVolVolume volume = UsdVolVolume::Get(volumeStage, volPrimPath);
  //assert(volume);
  //
  //SdfPath ovdbFieldPath = volPrimPath.AppendPath(SdfPath(openVDBPrimPf));
  //UsdVolOpenVDBAsset ovdbField = UsdVolOpenVDBAsset::Get(volumeStage, ovdbFieldPath);
  //
  //UsdAttribute fileAttr = ovdbField.GetFilePathAttr();
  //
  //std::vector<double> fileTimes;
  //fileAttr.GetTimeSamples(&fileTimes);

  assert(cache->ResourceTimes);
  std::vector<double>& fileTimes = *(cache->ResourceTimes);

  std::string basePath = usdWriter.SessionDirectory + std::string(resourceFolder) + name;
  for (double timeStep : fileTimes)
  {
    std::string volFileName = GetResourceFileName(basePath, timeStep, fileExtension);

    usdWriter.Connect->RemoveFile(volFileName.c_str(), true);
  }
  fileTimes.resize(0);
}

void ResourceCollectVolume(UsdBridgePrimCache* cache, const UsdBridgeUsdWriter& usdWriter)
{
  RemoveResourceFiles(cache, usdWriter, volFolder, vdbExtension);
}

void ResourceCollectSampler(UsdBridgePrimCache* cache, const UsdBridgeUsdWriter& usdWriter)
{
  RemoveResourceFiles(cache, usdWriter, imgFolder, imageExtension);
}