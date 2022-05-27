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
  (st)
  (st1)
  (st2)
  (attribute0)
  (attribute1)
  (attribute2)
  (attribute3)
  (result)
  (displayColor)

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
  const char* const texFolder = "textures/";
  const char* const volFolder = "volumes/";

  // Postfixes for auto generated usd subprims
  const char* const texCoordReaderPrimPf = "texcoordreader";
  const char* const vertexColorReaderPrimPf = "vertexcolorreader";
  const char* const shaderPrimPf = "shader";
  const char* const mdlShaderPrimPf = "mdlshader";
  const char* const openVDBPrimPf = "ovdbfield";

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

  valid = Connect->CreateFolder("", true);

  //Connect->RemoveFolder(SessionDirectory.c_str());
  bool folderMayExist = !Settings.CreateNewSession;
  
  valid = valid && Connect->CreateFolder(SessionDirectory.c_str(), folderMayExist);

#ifdef VALUE_CLIP_RETIMING
  valid = valid && Connect->CreateFolder((SessionDirectory + manifestFolder).c_str(), folderMayExist);
  valid = valid && Connect->CreateFolder((SessionDirectory + primStageFolder).c_str(), folderMayExist);
#endif
#ifdef TIME_CLIP_STAGES
  valid = valid && Connect->CreateFolder((SessionDirectory + clipFolder).c_str(), folderMayExist);
#endif
#ifdef SUPPORT_MDL_SHADERS
  if(Settings.EnableMdlShader)
  {
    valid = valid && Connect->CreateFolder((SessionDirectory + mdlFolder).c_str(), folderMayExist);
  }
#endif
  valid = valid && Connect->CreateFolder((SessionDirectory + volFolder).c_str(), folderMayExist);

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

  Connect->WriteFile(Mdl_Contents, totalStrLen, fileName, false);

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
  this->RelativeSceneFile = "../" + SceneFile;

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

StageCreatePair UsdBridgeUsdWriter::GetTimeVarStage(UsdBridgePrimCache* cache
#ifdef TIME_CLIP_STAGES
  , bool useClipStage, const char* clipPf, double timeStep
#endif
  )
{
#ifdef VALUE_CLIP_RETIMING
#ifdef TIME_CLIP_STAGES
  if (useClipStage)
  {
    bool exists;
    UsdStageRefPtr primStage = this->FindOrCreatePrimClipStage(cache, clipPf, timeStep, exists).second;

    SdfPath rootPrimPath(this->RootClassName);
    assert(!exists == !primStage->GetPrimAtPath(rootPrimPath));
    if (!exists)
    {
      primStage->DefinePrim(rootPrimPath);
    }

    return StageCreatePair(primStage, !exists);
  }
  else
    return StageCreatePair(cache->PrimStage.second, false);
#else
  return StageCreatePair(cache->PrimStage.second, false);
#endif
#else
  return StageCreatePair(this->SceneStage, false);
#endif
}

#ifdef VALUE_CLIP_RETIMING
void UsdBridgeUsdWriter::OpenPrimStage(const char* name, const char* primPostfix, UsdBridgePrimCache* cacheEntry, bool isManifest)
{
  bool binary = this->Settings.BinaryOutput;

  cacheEntry->PrimStage.first = (isManifest ? manifestFolder : primStageFolder) + std::string(name) + primPostfix + (binary ? ".usd" : ".usda");

  std::string absoluteFileName = Connect->GetUrl((this->SessionDirectory + cacheEntry->PrimStage.first).c_str());

  cacheEntry->PrimStage.second = UsdStage::CreateNew(absoluteFileName);
  if (!cacheEntry->PrimStage.second)
    cacheEntry->PrimStage.second = UsdStage::Open(absoluteFileName);

  assert(cacheEntry->PrimStage.second);

  cacheEntry->PrimStage.second->DefinePrim(SdfPath(this->RootClassName));
}

void UsdBridgeUsdWriter::SharePrimStage(UsdBridgePrimCache* owningCache, UsdBridgePrimCache* sharingCache)
{
  sharingCache->PrimStage = owningCache->PrimStage;
  sharingCache->OwnsPrimStage = false;
}

void UsdBridgeUsdWriter::RemovePrimStage(const UsdBridgePrimCache* cacheEntry)
{
  if (!cacheEntry->OwnsPrimStage)
    return;

  // May be superfluous
  assert(cacheEntry->PrimStage.second);
  cacheEntry->PrimStage.second->RemovePrim(SdfPath(RootClassName));

  // Remove Primstage file itself
  assert(!cacheEntry->PrimStage.first.empty());
  Connect->RemoveFile((SessionDirectory + cacheEntry->PrimStage.first).c_str());

#ifdef TIME_CLIP_STAGES
  // remove all clipstage files
  for (auto& x : cacheEntry->ClipStages)
  {
    Connect->RemoveFile((SessionDirectory + x.second.first).c_str());
  }
#endif
}
#endif


#ifdef TIME_CLIP_STAGES
const UsdStagePair& UsdBridgeUsdWriter::FindOrCreatePrimClipStage(UsdBridgePrimCache* cacheEntry, const char* clipPostfix, double timeStep, bool& exists)
{
  exists = true;
  bool binary = this->Settings.BinaryOutput;

  auto it = cacheEntry->ClipStages.find(timeStep);
  if (it == cacheEntry->ClipStages.end())
  {
    // Create a new Clipstage
    std::string relativeFileName = clipFolder + cacheEntry->Name.GetString() + clipPostfix + std::to_string(timeStep) + (binary ? ".usd" : ".usda");
    std::string absoluteFileName = Connect->GetUrl((this->SessionDirectory + relativeFileName).c_str());

    UsdStageRefPtr clipStage = UsdStage::CreateNew(absoluteFileName);
    exists = !clipStage;
    //if (exists = !clipStage)
    //  clipStage = UsdStage::Open(absoluteFileName); //unlikely to happen, as we could not find timestep in clipstages. Test if this should be removed
    assert(clipStage);

    it = cacheEntry->ClipStages.emplace(timeStep, UsdStagePair(std::move(relativeFileName), clipStage)).first;
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
  sceneGraphPrim.GetReferences().AddReference("", worldCache->PrimPath); //local and one-one, so no difference between ref or inherit
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
  if (cacheEntry->PrimStage.second)
  {
    RemovePrimStage(cacheEntry);
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

  const std::string* refStagePath;
  const std::string* manifestPath;
#ifdef TIME_CLIP_STAGES
  if (clipStages)
  {
    //set interpolatemissingclipvalues?

    bool exists;
    const UsdStagePair& childStagePair = FindOrCreatePrimClipStage(childCache, clipPostfix, childTimeStep, exists);
    assert(exists);

    refStagePath = &childStagePair.first;
    manifestPath = &childCache->PrimStage.first;
  }
  else
#endif
  {
    refStagePath = &childCache->PrimStage.first;
    manifestPath = refStagePath;
  }

  clipsApi.SetClipManifestAssetPath(SdfAssetPath(*manifestPath));

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
    const UsdStagePair& childStagePair = FindOrCreatePrimClipStage(childCache, clipPostfix, childTimeStep, exists);
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
  bool timeVarying, double parentTimeStep, double childTimeStep, bool replaceExisting,
  const RefModFuncs& refModCallbacks)
{
  return AddRef_Impl(SceneStage, parentCache, childCache, refPathExt, timeVarying, false, false, nullptr, parentTimeStep, childTimeStep, replaceExisting, refModCallbacks);
}

SdfPath UsdBridgeUsdWriter::AddRef_NoClip(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
  bool timeVarying, double parentTimeStep, double childTimeStep, bool replaceExisting,
  const RefModFuncs& refModCallbacks)
{
  return AddRef_Impl(stage, parentCache, childCache, refPathExt, timeVarying, false, false, nullptr, parentTimeStep, childTimeStep, replaceExisting, refModCallbacks);
}

SdfPath UsdBridgeUsdWriter::AddRef(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
  bool timeVarying, bool valueClip, bool clipStages, const char* clipPostfix,
  double parentTimeStep, double childTimeStep,
  bool replaceExisting,
  const RefModFuncs& refModCallbacks)
{
  // Value clip-enabled references have to be defined on the scenestage, as usd does not re-time recursively.
  return AddRef_Impl(SceneStage, parentCache, childCache, refPathExt, timeVarying, valueClip, clipStages, clipPostfix, parentTimeStep, childTimeStep, replaceExisting, refModCallbacks);
}

SdfPath UsdBridgeUsdWriter::AddRef_Impl(UsdStageRefPtr parentStage, UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
  bool timeVarying, // Timevarying existence of the reference itself
  bool valueClip,   // Retiming through a value clip
  bool clipStages,  // Separate stages for separate time slots (can only exist in usd if valueClip enabled)
  const char* clipPostfix, double parentTimeStep, double childTimeStep,
  bool replaceExisting,
  const RefModFuncs& refModCallbacks)
{
  UsdTimeCode parentTimeCode(parentTimeStep);

  SdfPath childBasePath = parentCache->PrimPath;
  if (refPathExt)
    childBasePath = parentCache->PrimPath.AppendPath(SdfPath(refPathExt));
  SdfPath referencingPrimPath = childBasePath.AppendPath(childCache->Name);

  UsdPrim referencingPrim = parentStage->GetPrimAtPath(referencingPrimPath);

  if (!referencingPrim)
  {
    if (replaceExisting)
    {
#ifdef TIME_BASED_CACHING
      //SetChildrenVisibility(stage, childBasePath, parentTimeCode, false);
      ChildrenRemoveIfVisible(parentStage, parentCache, childBasePath, timeVarying, parentTimeCode, refModCallbacks.AtRemoveRef);
#else
      // Remove the whole base path or empty the parent prim
      if (refPathExt)
        parentStage->RemovePrim(childBasePath);
      else
        RemoveAllRefs(parentStage, parentCache, nullptr, timeVarying, 0.0, refModCallbacks.AtRemoveRef);
#endif
    }

    UsdPrim referencingPrim = parentStage->DefinePrim(referencingPrimPath);
    assert(referencingPrim);

#ifdef VALUE_CLIP_RETIMING
    if (valueClip)
    {
      InitializeClipMetaData(referencingPrim, childCache, parentTimeStep, childTimeStep, clipStages, clipPostfix);
    }
#endif

    {
      UsdReferences references = referencingPrim.GetReferences(); //references or inherits?
      references.ClearReferences();
      if (parentStage != SceneStage)
        references.AddReference(RelativeSceneFile, childCache->PrimPath);
      else
        references.AddInternalReference(childCache->PrimPath);
      //referencingPrim.SetInstanceable(true);
    }

#ifdef TIME_BASED_CACHING
    // If time domain of the stage extends beyond timestep in either direction, set visibility false for extremes.
    if (timeVarying)
      InitializePrimVisibility(parentStage, referencingPrimPath, parentTimeCode);
#endif

    refModCallbacks.AtNewRef(parentCache, childCache);
  }
  else
  {
#ifdef TIME_BASED_CACHING
    if (timeVarying)
    {
      if (replaceExisting)
      {
        // Set all children's visibility to false except for the referencing prim.
        //SetChildrenVisibility(stage, childBasePath, parentTimeCode, false); 

        // Use ChildrenRemoveIfVisible instead; removes references for all timesteps and decreases refcount for garbage collection (no tracking of visibility over all timesteps atm). 
        ChildrenRemoveIfVisible(parentStage, parentCache, childBasePath, timeVarying, parentTimeCode, refModCallbacks.AtRemoveRef, referencingPrimPath);
      }
      SetPrimVisibility(parentStage, referencingPrimPath, parentTimeCode, true);
    }
#ifdef VALUE_CLIP_RETIMING
    // Cliptimes are added as additional info, not actively removed (visibility values remain leading in defining existing relationships over timesteps)
    // Also, clip stages at childTimeSteps which are not referenced anymore, are not removed; they could still be referenced from other parents!
    if (valueClip)
    {
      UpdateClipMetaData(referencingPrim, childCache, parentTimeStep, childTimeStep, clipStages, clipPostfix);
    }
#endif
#endif
  }

  return referencingPrimPath;
}

void UsdBridgeUsdWriter::RemoveAllRefs(UsdBridgePrimCache* parentCache, const char* refPathExt, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef)
{
  RemoveAllRefs(SceneStage, parentCache, refPathExt, timeVarying, timeStep, atRemoveRef);
}

void UsdBridgeUsdWriter::RemoveAllRefs(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, const char* refPathExt, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef)
{
#ifdef TIME_BASED_CACHING
  UsdTimeCode timeCode(timeStep);

  SdfPath childBasePath = parentCache->PrimPath;
  if (refPathExt)
    childBasePath = parentCache->PrimPath.AppendPath(SdfPath(refPathExt));

  // Remove refs just for this timecode, so leave invisible refs (could still be visible in other timecodes) intact.
  //SetChildrenVisibility(stage, childBasePath, timeCode, false);
  ChildrenRemoveIfVisible(stage, parentCache, childBasePath, timeVarying, timeCode, atRemoveRef);
#else
  if (refPathExt)
  {
    SdfPath childBasePath = parentCache->PrimPath.AppendPath(SdfPath(refPathExt));
    stage->RemovePrim(childBasePath);
  }
  else
  {
    UsdPrim parentPrim = stage->GetPrimAtPath(parentCache->PrimPath);
    UsdPrimSiblingRange children = parentPrim.GetAllChildren();
    for (UsdPrim child : children)
    {
      stage->RemovePrim(child.GetPath());
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
      {// make referencing prim invisible at timestep
        //SetPrimVisibility(oldChild.GetPath(), timeCode, false);
        PrimRemoveIfVisible(stage, parentCache, oldChild, timeVarying, timeCode, atRemoveRef);
      }
#else
      {// remove the whole referencing prim
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

template<typename GeomDataType>
void CreateUsdGeomAttributePrimvars(UsdGeomPrimvarsAPI& primvarApi, const GeomDataType& geomData, TimeEvaluator<GeomDataType>* timeEval = nullptr)
{
  typedef typename GeomDataType::DataMemberId DMI;

  bool hasTexCoords = UsdGeomDataHasTexCoords(geomData);
  for(uint32_t attribIndex = hasTexCoords ? 1 : 0; attribIndex < geomData.NumAttributes; ++attribIndex)
  {
    bool timeVarChecked = true;
    if(timeEval)
    {
      DMI attributeId = DMI::ATTRIBUTE0 + attribIndex;
      timeVarChecked = timeEval->IsTimeVarying(attributeId);
    }

    const UsdBridgeAttribute& attrib = geomData.Attributes[attribIndex];
    if(attrib.Data != nullptr && timeVarChecked)
    {
      SdfValueTypeName primvarType = GetPrimvarArrayType(attrib.DataType);
      TfToken attribInterpolation = attrib.PerPrimData ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
      primvarApi.CreatePrimvar(AttribIndexToToken(attribIndex), primvarType, attribInterpolation);
    }
  }
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeMeshData& meshData, bool uniformPrim)
{
  UsdGeomMesh geomMesh = GetOrDefinePrim<UsdGeomMesh>(geometryStage, geomPath);
  UsdGeomPrimvarsAPI primvarApi(geomMesh);

  geomMesh.CreatePointsAttr();
  geomMesh.CreateExtentAttr();
  geomMesh.CreateFaceVertexIndicesAttr();
  geomMesh.CreateFaceVertexCountsAttr();
  geomMesh.CreateNormalsAttr();
  geomMesh.SetNormalsInterpolation(UsdGeomTokens->vertex);
  if(Settings.EnableDisplayColors)
  {
    geomMesh.CreateDisplayColorPrimvar(UsdGeomTokens->vertex);
  }
  
  if(UsdGeomDataHasTexCoords(meshData) || Settings.EnableMdlColors)
    primvarApi.CreatePrimvar(UsdBridgeTokens->st, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);

  CreateUsdGeomAttributePrimvars(primvarApi, meshData);

#ifdef SUPPORT_MDL_SHADERS
  if(Settings.EnableMdlColors)
  {
    primvarApi.CreatePrimvar(UsdBridgeTokens->st1, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
    primvarApi.CreatePrimvar(UsdBridgeTokens->st2, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
  }
#endif

  if (uniformPrim)
  {
    geomMesh.CreateDoubleSidedAttr(VtValue(true));
    geomMesh.CreateSubdivisionSchemeAttr().Set(UsdGeomTokens->none);
  }

  return geomMesh.GetPrim();
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeInstancerData& instancerData, bool uniformPrim)
{
  if (UsesUsdGeomPoints(instancerData))
  {
    UsdGeomPoints geomPoints = GetOrDefinePrim<UsdGeomPoints>(geometryStage, geomPath);
    UsdGeomPrimvarsAPI primvarApi(geomPoints);

    geomPoints.CreatePointsAttr();
    geomPoints.CreateExtentAttr();
    geomPoints.CreateIdsAttr();
    geomPoints.CreateNormalsAttr();
    geomPoints.CreateWidthsAttr();

    if(Settings.EnableDisplayColors)
    {
      geomPoints.CreateDisplayColorPrimvar(UsdGeomTokens->vertex);
    }

    if(UsdGeomDataHasTexCoords(instancerData) || Settings.EnableMdlColors)
      primvarApi.CreatePrimvar(UsdBridgeTokens->st, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);

#ifdef SUPPORT_MDL_SHADERS
    if(Settings.EnableMdlColors)
    {
      primvarApi.CreatePrimvar(UsdBridgeTokens->st1, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
      primvarApi.CreatePrimvar(UsdBridgeTokens->st2, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
    }
#endif

    CreateUsdGeomAttributePrimvars(primvarApi, instancerData);

    if (uniformPrim)
    {
      geomPoints.CreateDoubleSidedAttr(VtValue(true));
    }

    return geomPoints.GetPrim();
  }
  else
  {
    UsdGeomPointInstancer geomPoints = GetOrDefinePrim<UsdGeomPointInstancer>(geometryStage, geomPath);
    UsdGeomPrimvarsAPI primvarApi(geomPoints);

    geomPoints.CreatePositionsAttr();
    geomPoints.CreateExtentAttr();
    geomPoints.CreateIdsAttr();
    geomPoints.CreateOrientationsAttr();
    geomPoints.CreateScalesAttr();
    if(Settings.EnableDisplayColors)
    {
      primvarApi.CreatePrimvar(UsdBridgeTokens->displayColor, SdfValueTypeNames->Color3fArray, UsdGeomTokens->uniform);
    }

    if(UsdGeomDataHasTexCoords(instancerData) || Settings.EnableMdlColors)
      primvarApi.CreatePrimvar(UsdBridgeTokens->st, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);

#ifdef SUPPORT_MDL_SHADERS
    if(Settings.EnableMdlColors)
    {
      primvarApi.CreatePrimvar(UsdBridgeTokens->st1, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
      primvarApi.CreatePrimvar(UsdBridgeTokens->st2, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
    }
#endif

    CreateUsdGeomAttributePrimvars(primvarApi, instancerData);

    geomPoints.CreateProtoIndicesAttr();
    geomPoints.CreateVelocitiesAttr();
    geomPoints.CreateAngularVelocitiesAttr();
    geomPoints.CreateInvisibleIdsAttr();

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

UsdPrim UsdBridgeUsdWriter::InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeCurveData& curveData, bool uniformPrim)
{
  UsdGeomBasisCurves geomCurves = GetOrDefinePrim<UsdGeomBasisCurves>(geometryStage, geomPath);
  UsdGeomPrimvarsAPI primvarApi(geomCurves);

  geomCurves.CreatePointsAttr();
  geomCurves.CreateExtentAttr();
  geomCurves.CreateCurveVertexCountsAttr();
  geomCurves.CreateNormalsAttr();
  geomCurves.CreateWidthsAttr();
  if(Settings.EnableDisplayColors)
  {
    geomCurves.CreateDisplayColorPrimvar(UsdGeomTokens->vertex);
  }

  if(UsdGeomDataHasTexCoords(curveData) || Settings.EnableMdlColors)
    primvarApi.CreatePrimvar(UsdBridgeTokens->st, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);

#ifdef SUPPORT_MDL_SHADERS
  if(Settings.EnableMdlColors)
  {
    primvarApi.CreatePrimvar(UsdBridgeTokens->st1, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
    primvarApi.CreatePrimvar(UsdBridgeTokens->st2, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
  }
#endif

  CreateUsdGeomAttributePrimvars(primvarApi, curveData);

  if (uniformPrim)
  {
    geomCurves.CreateDoubleSidedAttr(VtValue(true));
    geomCurves.GetTypeAttr().Set(UsdGeomTokens->linear);
  }

  return geomCurves.GetPrim();
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdVolume(UsdStageRefPtr volumeStage, const SdfPath & volumePath, bool uniformPrim)
{
  UsdVolVolume volume = GetOrDefinePrim<UsdVolVolume>(volumeStage, volumePath);

  SdfPath ovdbFieldPath = volumePath.AppendPath(SdfPath(openVDBPrimPf));
  UsdVolOpenVDBAsset volAsset = UsdVolOpenVDBAsset::Define(volumeStage, ovdbFieldPath);

  volAsset.CreateFilePathAttr();
  volAsset.CreateExtentAttr();

  if (uniformPrim)
  {
    volume.CreateFieldRelationship(UsdBridgeTokens->density, ovdbFieldPath);
    volume.ClearXformOpOrder();
    volume.AddTranslateOp();
    volume.AddScaleOp();

    volAsset.CreateFieldNameAttr(VtValue(UsdBridgeTokens->density));
  }

  return volume.GetPrim();
}

UsdShadeMaterial UsdBridgeUsdWriter::InitializeUsdMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, bool uniformPrim)
{
  // Create the material
  UsdShadeMaterial matPrim = UsdShadeMaterial::Define(materialStage, matPrimPath);
  assert(matPrim);

  if(Settings.EnablePreviewSurfaceShader)
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
    UsdShadeOutput shaderOutput = this->InitializeUsdShader(materialStage, shaderPrimPath, uniformPrim);

    if(uniformPrim)
      this->BindShaderToMaterial(matPrim, shaderOutput, nullptr);
  }

#ifdef SUPPORT_MDL_SHADERS 
  if(Settings.EnableMdlShader)
  {
    // Create an mdl shader
    SdfPath mdlShaderPrimPath = matPrimPath.AppendPath(SdfPath(mdlShaderPrimPf));
    UsdShadeOutput mdlShaderPrim = this->InitializeMdlShader(materialStage, mdlShaderPrimPath, uniformPrim);

    if(uniformPrim)
      this->BindShaderToMaterial(matPrim, mdlShaderPrim, &UsdBridgeTokens->mdl);
  }
#endif

  return matPrim;
}

UsdShadeOutput UsdBridgeUsdWriter::InitializeUsdShader(UsdStageRefPtr shaderStage, const SdfPath& shadPrimPath, bool uniformPrim)
{
  // Create the shader
  UsdShadeShader shader = UsdShadeShader::Define(shaderStage, shadPrimPath);
  assert(shader);

  if (uniformPrim)
  {
    shader.CreateIdAttr(VtValue(UsdBridgeTokens->UsdPreviewSurface));
    shader.CreateInput(UsdBridgeTokens->useSpecularWorkflow, SdfValueTypeNames->Int).Set(1);
  }

  // Always predeclare all properties, even when using clip values (manifests do not declare properties, only make them available for value resolution)
  shader.CreateInput(UsdBridgeTokens->useSpecularWorkflow, SdfValueTypeNames->Int);
  shader.CreateInput(UsdBridgeTokens->roughness, SdfValueTypeNames->Float);
  shader.CreateInput(UsdBridgeTokens->opacity, SdfValueTypeNames->Float);
  shader.CreateInput(UsdBridgeTokens->metallic, SdfValueTypeNames->Float);
  shader.CreateInput(UsdBridgeTokens->ior, SdfValueTypeNames->Float);
  shader.CreateInput(UsdBridgeTokens->diffuseColor, SdfValueTypeNames->Color3f);
  shader.CreateInput(UsdBridgeTokens->specularColor, SdfValueTypeNames->Color3f);
  shader.CreateInput(UsdBridgeTokens->emissiveColor, SdfValueTypeNames->Color3f);

  return shader.CreateOutput(UsdBridgeTokens->surface, SdfValueTypeNames->Token);
}

#ifdef SUPPORT_MDL_SHADERS  
UsdShadeOutput UsdBridgeUsdWriter::InitializeMdlShader(UsdStageRefPtr shaderStage, const SdfPath& shadPrimPath, bool uniformPrim)
{
  // Create the shader
  UsdShadeShader shader = UsdShadeShader::Define(shaderStage, shadPrimPath);
  assert(shader);

  if (uniformPrim)
  {
    shader.CreateImplementationSourceAttr(VtValue(UsdBridgeTokens->sourceAsset));
    shader.SetSourceAssetSubIdentifier(UsdBridgeTokens->PBR_Base, UsdBridgeTokens->mdl);
    shader.CreateInput(UsdBridgeTokens->vertexcolor_coordinate_index, SdfValueTypeNames->Int);
  }

  shader.CreateInput(UsdBridgeTokens->diffuse_color_constant, SdfValueTypeNames->Color3f);
  shader.CreateInput(UsdBridgeTokens->emissive_color, SdfValueTypeNames->Color3f);
  shader.CreateInput(UsdBridgeTokens->emissive_intensity, SdfValueTypeNames->Float);
  shader.CreateInput(UsdBridgeTokens->enable_emission, SdfValueTypeNames->Bool);
  shader.CreateInput(UsdBridgeTokens->opacity_constant, SdfValueTypeNames->Float);
  shader.CreateInput(UsdBridgeTokens->reflection_roughness_constant, SdfValueTypeNames->Float);
  shader.CreateInput(UsdBridgeTokens->metallic_constant, SdfValueTypeNames->Float);
  shader.CreateInput(UsdBridgeTokens->ior_constant, SdfValueTypeNames->Float);

  return shader.CreateOutput(UsdBridgeTokens->surface, SdfValueTypeNames->Token);
}
#endif

void UsdBridgeUsdWriter::InitializeUsdSampler(const UsdBridgePrimCache* cacheEntry)
{
  const SdfPath& samplerPrimPath = cacheEntry->PrimPath;// .AppendPath(SdfPath(textureAttribPf));
  UsdShadeShader textureReader = UsdShadeShader::Define(this->SceneStage, samplerPrimPath);
  assert(textureReader);
  textureReader.CreateIdAttr(VtValue(UsdBridgeTokens->UsdUVTexture));
  textureReader.CreateInput(UsdBridgeTokens->fallback, SdfValueTypeNames->Float4).Set(GfVec4f(1.0f, 0.0f, 0.0f, 1.0f));
  textureReader.CreateOutput(UsdBridgeTokens->rgb, SdfValueTypeNames->Float3);
  textureReader.CreateOutput(UsdBridgeTokens->a, SdfValueTypeNames->Float);
}

#ifdef TIME_CLIP_STAGES
void UsdBridgeUsdWriter::CreateUsdGeometryManifest(const char* name, const UsdBridgePrimCache* cacheEntry, const UsdBridgeMeshData& meshData)
{
  TimeEvaluator<UsdBridgeMeshData> timeEval(meshData);
  typedef UsdBridgeMeshData::DataMemberId DMI;

  UsdGeomMesh meshGeom = UsdGeomMesh::Define(cacheEntry->PrimStage.second, cacheEntry->PrimPath);
  assert(meshGeom);
  UsdGeomPrimvarsAPI primvarApi(meshGeom);

  if (timeEval.IsTimeVarying(DMI::POINTS))
  {
    meshGeom.CreatePointsAttr();
    meshGeom.CreateExtentAttr();
  }

  if (timeEval.IsTimeVarying(DMI::INDICES))
  {
    meshGeom.CreateFaceVertexIndicesAttr();
    meshGeom.CreateFaceVertexCountsAttr();
  }

  if (timeEval.IsTimeVarying(DMI::NORMALS))
    meshGeom.CreateNormalsAttr();

  if (timeEval.IsTimeVarying(DMI::ATTRIBUTE0) && (UsdGeomDataHasTexCoords(meshData) || Settings.EnableMdlColors))
    primvarApi.CreatePrimvar(UsdBridgeTokens->st, SdfValueTypeNames->TexCoord2fArray);

  CreateUsdGeomAttributePrimvars(primvarApi, meshData, &timeEval);

  if (timeEval.IsTimeVarying(DMI::COLORS))
  {
    if(Settings.EnableDisplayColors)
    {
      meshGeom.CreateDisplayColorPrimvar();
    }
#ifdef SUPPORT_MDL_SHADERS
    if(Settings.EnableMdlColors)
    {
      primvarApi.CreatePrimvar(UsdBridgeTokens->st1, SdfValueTypeNames->TexCoord2fArray);
      primvarApi.CreatePrimvar(UsdBridgeTokens->st2, SdfValueTypeNames->TexCoord2fArray);
    }
#endif
  }

  if(this->EnableSaving)
    cacheEntry->PrimStage.second->Save();
}

void UsdBridgeUsdWriter::CreateUsdGeometryManifest(const char* name, const UsdBridgePrimCache* cacheEntry, const UsdBridgeInstancerData& instancerData)
{
  TimeEvaluator<UsdBridgeInstancerData> timeEval(instancerData);
  typedef UsdBridgeInstancerData::DataMemberId DMI;

  if(UsesUsdGeomPoints(instancerData))
  {
    UsdGeomPoints pointsGeom = UsdGeomPoints::Define(cacheEntry->PrimStage.second, cacheEntry->PrimPath);
    UsdGeomPrimvarsAPI primvarApi(pointsGeom);

    if (timeEval.IsTimeVarying(DMI::POINTS))
    {
      pointsGeom.CreatePointsAttr();
      pointsGeom.CreateExtentAttr();
    }

    if (timeEval.IsTimeVarying(DMI::INSTANCEIDS))
      pointsGeom.CreateIdsAttr();

    if (timeEval.IsTimeVarying(DMI::ORIENTATIONS))
      pointsGeom.CreateNormalsAttr();

    if (timeEval.IsTimeVarying(DMI::SCALES))
      pointsGeom.CreateWidthsAttr();

    if (timeEval.IsTimeVarying(DMI::COLORS))
    {
      if(Settings.EnableDisplayColors)
      {
        pointsGeom.CreateDisplayColorPrimvar();
      }
#ifdef SUPPORT_MDL_SHADERS
      if(Settings.EnableMdlColors)
      {
        primvarApi.CreatePrimvar(UsdBridgeTokens->st1, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
        primvarApi.CreatePrimvar(UsdBridgeTokens->st2, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
      }
#endif
    }

    if (timeEval.IsTimeVarying(DMI::ATTRIBUTE0) && (UsdGeomDataHasTexCoords(instancerData) || Settings.EnableMdlColors))
      primvarApi.CreatePrimvar(UsdBridgeTokens->st, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);

    CreateUsdGeomAttributePrimvars(primvarApi, instancerData, &timeEval);
  }
  else
  {
    UsdGeomPointInstancer pointsGeom = UsdGeomPointInstancer::Define(cacheEntry->PrimStage.second, cacheEntry->PrimPath);
    UsdGeomPrimvarsAPI primvarApi(pointsGeom);

    if (timeEval.IsTimeVarying(DMI::POINTS))
    {
      pointsGeom.CreatePositionsAttr();
      pointsGeom.CreateExtentAttr();
      pointsGeom.CreateProtoIndicesAttr();
    }

    if (timeEval.IsTimeVarying(DMI::SHAPEINDICES))
      pointsGeom.CreateProtoIndicesAttr();

    if (timeEval.IsTimeVarying(DMI::INSTANCEIDS))
      pointsGeom.CreateIdsAttr();

    if (timeEval.IsTimeVarying(DMI::ORIENTATIONS))
      pointsGeom.CreateOrientationsAttr();

    if (timeEval.IsTimeVarying(DMI::SCALES))
      pointsGeom.CreateScalesAttr();

    if (timeEval.IsTimeVarying(DMI::COLORS))
    {
      if(Settings.EnableDisplayColors)
      {
        primvarApi.CreatePrimvar(UsdBridgeTokens->displayColor, SdfValueTypeNames->Color3fArray, UsdGeomTokens->vertex);
      }
#ifdef SUPPORT_MDL_SHADERS
      if(Settings.EnableMdlColors)
      {
        primvarApi.CreatePrimvar(UsdBridgeTokens->st1, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
        primvarApi.CreatePrimvar(UsdBridgeTokens->st2, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
      }
#endif
    }

    if (timeEval.IsTimeVarying(DMI::ATTRIBUTE0) && (UsdGeomDataHasTexCoords(instancerData) || Settings.EnableMdlColors))
      primvarApi.CreatePrimvar(UsdBridgeTokens->st, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);

    CreateUsdGeomAttributePrimvars(primvarApi, instancerData, &timeEval);

    if (timeEval.IsTimeVarying(DMI::LINEARVELOCITIES))
      pointsGeom.CreateVelocitiesAttr();

    if (timeEval.IsTimeVarying(DMI::ANGULARVELOCITIES))
      pointsGeom.CreateAngularVelocitiesAttr();

    if (timeEval.IsTimeVarying(DMI::INVISIBLEIDS))
      pointsGeom.CreateInvisibleIdsAttr();
  }

  if(this->EnableSaving)
    cacheEntry->PrimStage.second->Save();
}

void UsdBridgeUsdWriter::CreateUsdGeometryManifest(const char* name, const UsdBridgePrimCache* cacheEntry, const UsdBridgeCurveData& curveData)
{
  TimeEvaluator<UsdBridgeCurveData> timeEval(curveData);
  typedef UsdBridgeCurveData::DataMemberId DMI;

  UsdGeomBasisCurves curveGeom = UsdGeomBasisCurves::Define(cacheEntry->PrimStage.second, cacheEntry->PrimPath);
  assert(curveGeom);
  UsdGeomPrimvarsAPI primvarApi(curveGeom);

  if (timeEval.IsTimeVarying(DMI::POINTS))
  {
    curveGeom.CreatePointsAttr();
    curveGeom.CreateExtentAttr();
  }

  if (timeEval.IsTimeVarying(DMI::CURVELENGTHS))
    curveGeom.CreateCurveVertexCountsAttr();

  if (timeEval.IsTimeVarying(DMI::NORMALS))
    curveGeom.CreateNormalsAttr();

  if (timeEval.IsTimeVarying(DMI::SCALES))
    curveGeom.CreateWidthsAttr();

  if (timeEval.IsTimeVarying(DMI::COLORS))
  {
    if(Settings.EnableDisplayColors)
    {
      curveGeom.CreateDisplayColorPrimvar(UsdGeomTokens->vertex);
    }
#ifdef SUPPORT_MDL_SHADERS
    if(Settings.EnableMdlColors)
    {
      primvarApi.CreatePrimvar(UsdBridgeTokens->st1, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
      primvarApi.CreatePrimvar(UsdBridgeTokens->st2, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);
    }
#endif
  }

  if (timeEval.IsTimeVarying(DMI::ATTRIBUTE0) && (UsdGeomDataHasTexCoords(curveData) || Settings.EnableMdlColors))
    primvarApi.CreatePrimvar(UsdBridgeTokens->st, SdfValueTypeNames->TexCoord2fArray, UsdGeomTokens->vertex);

  CreateUsdGeomAttributePrimvars(primvarApi, curveData, &timeEval);

  if(this->EnableSaving)
    cacheEntry->PrimStage.second->Save();
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

void UsdBridgeUsdWriter::BindShaderToMaterial(const UsdShadeMaterial& matPrim, const UsdShadeOutput& shadOut, TfToken* renderContext)
{
  // Bind the material to the shader reference subprim.
  if(renderContext)
    matPrim.CreateSurfaceOutput(*renderContext).ConnectToSource(shadOut);
  else
    matPrim.CreateSurfaceOutput().ConnectToSource(shadOut);
}

void UsdBridgeUsdWriter::BindSamplerToMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, const SdfPath& refSamplerPrimPath, const char* texFileName)
{
  if(Settings.EnablePreviewSurfaceShader)
  {
    // Find the coordinate reader using the material path
    SdfPath coordReaderPath = matPrimPath.AppendPath(SdfPath(texCoordReaderPrimPf));
    UsdShadeShader coordReaderPrim = UsdShadeShader::Get(this->SceneStage, coordReaderPath);
    assert(coordReaderPrim);

    // Bind the texture reader to the coordinate reader
    UsdShadeShader refTexReader = UsdShadeShader::Get(this->SceneStage, refSamplerPrimPath); // type inherited from sampler prim (in AddRef)
    assert(refTexReader);
    UsdShadeOutput tcReaderOutput = coordReaderPrim.CreateOutput(UsdBridgeTokens->result, SdfValueTypeNames->Float2);
    refTexReader.CreateInput(UsdBridgeTokens->st, SdfValueTypeNames->Float2).ConnectToSource(tcReaderOutput);

    // Update usd shader
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(shaderPrimPf));

    UsdShadeShader uniformShad = UsdShadeShader::Get(this->SceneStage, shaderPrimPath);
    assert(uniformShad);
    UsdShadeShader timeVarShad = UsdShadeShader::Get(materialStage, shaderPrimPath);
    assert(timeVarShad);

    //Bind the sampler to the diffuse color of the (mdl)shader
    timeVarShad.GetPrim().RemoveProperty(TfToken("input:diffuseColor"));
    timeVarShad.GetPrim().RemoveProperty(TfToken("input:specularColor"));

    UsdShadeOutput texReaderOutput = refTexReader.CreateOutput(UsdBridgeTokens->rgb, SdfValueTypeNames->Color3f);

    uniformShad.CreateInput(UsdBridgeTokens->diffuseColor, SdfValueTypeNames->Color3f).ConnectToSource(texReaderOutput);
    uniformShad.CreateInput(UsdBridgeTokens->specularColor, SdfValueTypeNames->Color3f).ConnectToSource(texReaderOutput);
  }

#ifdef SUPPORT_MDL_SHADERS
  if(Settings.EnableMdlShader)
  {
    SdfPath mdlShaderPrimPath = matPrimPath.AppendPath(SdfPath(mdlShaderPrimPf));
    UsdShadeShader uniformMdlShad = UsdShadeShader::Get(this->SceneStage, mdlShaderPrimPath);
    assert(uniformMdlShad);

    uniformMdlShad.GetInput(UsdBridgeTokens->vertexcolor_coordinate_index).Set(-1);
    uniformMdlShad.CreateInput(UsdBridgeTokens->diffuse_texture, SdfValueTypeNames->Asset).Set(SdfAssetPath(texFileName));
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

  GfMatrix4d transMat; // Transforms can only be double, see UsdGeomXform::AddTransformOp (UsdGeomXformable)
  transMat.SetColumn(0, GfVec4d(GfVec4f(&transform[0])));
  transMat.SetColumn(1, GfVec4d(GfVec4f(&transform[4])));
  transMat.SetColumn(2, GfVec4d(GfVec4f(&transform[8])));
  transMat.SetColumn(3, GfVec4d(GfVec4f(&transform[12])));

  //Note that instance transform nodes have already been created.
  UsdGeomXform tfPrim = UsdGeomXform::Get(this->SceneStage, transPrimPath);
  assert(tfPrim);
  tfPrim.ClearXformOpOrder();
  tfPrim.AddTransformOp().Set(transMat, timeEval.Eval());
}

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

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomPoints(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::POINTS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::POINTS);
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

  // Attribute is declared uniform, so we should make sure that no time clip value exists for it.
  // Executed regardless of performsUpdate, as the value may have been written at an ealier point in time when attribute was declared timeVarying.
  // Therefore, in case the clip stage is not to be touched, make sure the attributes that are not updated are declared timeVarying. 
  // In practice, a value should then already exist in the clip stage, or one risks a block-value usd resolve.
  if (!timeVaryingUpdate)
  {
    UsdGeomGetPointsAttribute(timeVarGeom).ClearAtTime(timeEval.TimeCode);
    timeVarGeom.GetExtentAttr().ClearAtTime(timeEval.TimeCode);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomIndices(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::INDICES);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INDICES);
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
  if (!timeVaryingUpdate)
  {
    timeVarGeom.GetFaceVertexIndicesAttr().ClearAtTime(timeEval.TimeCode);
    timeVarGeom.GetFaceVertexCountsAttr().ClearAtTime(timeEval.TimeCode);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomNormals(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::NORMALS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::NORMALS);
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
      normalsAttr.ClearAtTime(timeCode);
    }
  }
  if (!timeVaryingUpdate)
  {
    timeVarGeom.GetNormalsAttr().ClearAtTime(timeEval.TimeCode);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomTexCoords(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::ATTRIBUTE0);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ATTRIBUTE0);
  if (performsUpdate)
  {
    UsdGeomType* outGeom = timeVaryingUpdate ? &timeVarGeom : &uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::ATTRIBUTE0);

    UsdAttribute texcoordPrimvar = outGeom->GetPrimvar(UsdBridgeTokens->st);
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
      uniformGeom.GetPrimvar(UsdBridgeTokens->st).SetInterpolation(texcoordInterpolation);
    }
    else
    {
      texcoordPrimvar.ClearAtTime(timeCode);
    }
  }
  if (!timeVaryingUpdate)
  {
    UsdAttribute texcoordPrimvar = timeVarGeom.GetPrimvar(UsdBridgeTokens->st);
    texcoordPrimvar.ClearAtTime(timeEval.TimeCode);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomAttribute(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval, uint32_t attribIndex)
{
  assert(attribIndex < geomData.NumAttributes);
  const UsdBridgeAttribute& bridgeAttrib = geomData.Attributes[attribIndex];

  TfToken attribToken = AttribIndexToToken(attribIndex);

  using DMI = typename GeomDataType::DataMemberId;
  DMI attributeId = DMI::ATTRIBUTE0 + attribIndex;
  bool performsUpdate = updateEval.PerformsUpdate(attributeId);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(attributeId);
  if (performsUpdate)
  {
    UsdGeomType* outGeom = timeVaryingUpdate ? &timeVarGeom : &uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(attributeId);

    UsdAttribute attributePrimvar = outGeom->GetPrimvar(attribToken);

    if(!attributePrimvar)
       UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "UsdGeom Attribute<Index> primvar not found, was the attribute at requested index valid during initialization of the prim? Index is " << attribIndex);

    if (bridgeAttrib.Data != nullptr)
    {
      const void* arrayData = bridgeAttrib.Data;
      size_t arrayNumElements = bridgeAttrib.PerPrimData ? numPrims : geomData.NumPoints;
      UsdAttribute arrayPrimvar = attributePrimvar;

      CopyArrayToPrimvar(writer, arrayData, bridgeAttrib.DataType, arrayNumElements, arrayPrimvar, timeCode);
 
      // Per face or per-vertex interpolation. This will break timesteps that have been written before.
      TfToken attribInterpolation = bridgeAttrib.PerPrimData ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
      uniformGeom.GetPrimvar(attribToken).SetInterpolation(attribInterpolation);
    }
    else
    {
      attributePrimvar.ClearAtTime(timeCode);
    }
  }
  if (!timeVaryingUpdate)
  {
    UsdAttribute attributePrimvar = timeVarGeom.GetPrimvar(attribToken);
    attributePrimvar.ClearAtTime(timeEval.TimeCode);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomAttributes(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  uint32_t startIdx = UsdGeomDataHasTexCoords(geomData) ? 1 : 0;
  for(uint32_t attribIndex = startIdx; attribIndex < geomData.NumAttributes; ++attribIndex)
  {
    const UsdBridgeAttribute& attrib = geomData.Attributes[attribIndex];
    if(attrib.Data != nullptr)
    {
      UpdateUsdGeomAttribute(writer, timeVarGeom, uniformGeom, geomData, numPrims, updateEval, timeEval, attribIndex);
    }
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomColors(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::COLORS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::COLORS);
  if (performsUpdate)
  {
    UsdGeomType* outGeom = timeVaryingUpdate ? &timeVarGeom : &uniformGeom;
    UsdTimeCode timeCode = timeEval.Eval(DMI::COLORS);

    UsdGeomPrimvar colorPrimvar = outGeom->GetPrimvar(UsdBridgeTokens->displayColor);
#ifdef SUPPORT_MDL_SHADERS
    UsdGeomPrimvar vc0Primvar = outGeom->GetPrimvar(UsdBridgeTokens->st1);
    UsdGeomPrimvar vc1Primvar = outGeom->GetPrimvar(UsdBridgeTokens->st2);
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
        uniformGeom.GetPrimvar(UsdBridgeTokens->displayColor).SetInterpolation(colorInterpolation);
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
          UsdAttribute texcoordPrimvar = outGeom->GetPrimvar(UsdBridgeTokens->st);
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

        uniformGeom.GetPrimvar(UsdBridgeTokens->st1).SetInterpolation(colorInterpolation);
        uniformGeom.GetPrimvar(UsdBridgeTokens->st2).SetInterpolation(colorInterpolation);
      }
#endif
    }
    else
    {
      if(writer->Settings.EnableDisplayColors)
      {
        colorPrimvar.GetAttr().ClearAtTime(timeCode);
      }
#ifdef SUPPORT_MDL_SHADERS
      if (writer->Settings.EnableMdlColors)
      {
        vc0Primvar.GetAttr().ClearAtTime(timeCode);
        vc1Primvar.GetAttr().ClearAtTime(timeCode);
      }
#endif
    }
  }
  if (!timeVaryingUpdate)
  {
    if(writer->Settings.EnableDisplayColors)
    {
      timeVarGeom.GetPrimvar(UsdBridgeTokens->displayColor).GetAttr().ClearAtTime(timeEval.TimeCode);
    }
#ifdef SUPPORT_MDL_SHADERS
    if (writer->Settings.EnableMdlColors)
    {
      timeVarGeom.GetPrimvar(UsdBridgeTokens->st1).GetAttr().ClearAtTime(timeEval.TimeCode);
      timeVarGeom.GetPrimvar(UsdBridgeTokens->st2).GetAttr().ClearAtTime(timeEval.TimeCode);
    }
#endif
  }
}


template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomInstanceIds(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::INSTANCEIDS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INSTANCEIDS);
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
      idsAttr.ClearAtTime(timeCode);
    }
  }
  if (!timeVaryingUpdate)
  {
    timeVarGeom.GetIdsAttr().ClearAtTime(timeEval.TimeCode);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomWidths(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::SCALES);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::SCALES);
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
  if (!timeVaryingUpdate)
  {
    timeVarGeom.GetWidthsAttr().ClearAtTime(timeEval.TimeCode);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomScales(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::SCALES);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::SCALES);
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
  if (!timeVaryingUpdate)
  {
    timeVarGeom.GetScalesAttr().ClearAtTime(timeEval.TimeCode);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomOrientNormals(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::ORIENTATIONS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ORIENTATIONS);
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
  if (!timeVaryingUpdate)
  {
    timeVarGeom.GetNormalsAttr().ClearAtTime(timeEval.TimeCode);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomOrientations(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::ORIENTATIONS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ORIENTATIONS);
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
  if (!timeVaryingUpdate)
  {
    timeVarGeom.GetOrientationsAttr().ClearAtTime(timeEval.TimeCode);
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
      linearVelocitiesAttribute.ClearAtTime(timeCode);
    }
  }
  if (!timeVaryingUpdate)
  {
    timeVarGeom.GetVelocitiesAttr().ClearAtTime(timeEval.TimeCode);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomAngularVelocities(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::ANGULARVELOCITIES);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ANGULARVELOCITIES);
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
      angularVelocitiesAttribute.ClearAtTime(timeCode);
    }
  }
  if (!timeVaryingUpdate)
  {
    timeVarGeom.GetAngularVelocitiesAttr().ClearAtTime(timeEval.TimeCode);
  }
}

template<typename UsdGeomType, typename GeomDataType>
void UpdateUsdGeomInvisibleIds(UsdBridgeUsdWriter* writer, UsdGeomType& timeVarGeom, UsdGeomType& uniformGeom, const GeomDataType& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
{
  using DMI = typename GeomDataType::DataMemberId;
  bool performsUpdate = updateEval.PerformsUpdate(DMI::INVISIBLEIDS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INVISIBLEIDS);
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
      invisIdsAttr.ClearAtTime(timeCode);
    }
  }
  if (!timeVaryingUpdate)
  {
    timeVarGeom.GetInvisibleIdsAttr().ClearAtTime(timeEval.TimeCode);
  }
}

static void UpdateUsdGeomCurveLengths(UsdBridgeUsdWriter* writer, UsdGeomBasisCurves& timeVarGeom, UsdGeomBasisCurves& uniformGeom, const UsdBridgeCurveData& geomData, uint64_t numPrims,
  UsdBridgeUpdateEvaluator<const UsdBridgeCurveData>& updateEval, TimeEvaluator<UsdBridgeCurveData>& timeEval)
{
  using DMI = typename UsdBridgeCurveData::DataMemberId;
  // Fill geom prim and geometry layer with data.
  bool performsUpdate = updateEval.PerformsUpdate(DMI::CURVELENGTHS);
  bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::CURVELENGTHS);
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
  if (!timeVaryingUpdate)
  {
    timeVarGeom.GetCurveVertexCountsAttr().ClearAtTime(timeEval.TimeCode);
  }
}

#define UPDATE_USDGEOM_ARRAYS(FuncDef) \
  FuncDef(this, timeVarGeom, uniformGeom, geomData, numPrims, updateEval, timeEval)

void UsdBridgeUsdWriter::UpdateUsdGeometry(const UsdStagePtr& timeVarStage, const SdfPath& meshPath, const UsdBridgeMeshData& geomData, double timeStep)
{
  // To avoid data duplication when using of clip stages, we need to potentially use the scenestage prim for time-uniform data.
  UsdGeomMesh uniformGeom = UsdGeomMesh::Get(this->SceneStage, meshPath);
  assert(uniformGeom);

  UsdGeomMesh timeVarGeom = UsdGeomMesh::Get(timeVarStage, meshPath);
  assert(timeVarGeom);

  // Update the mesh
  UsdBridgeUpdateEvaluator<const UsdBridgeMeshData> updateEval(geomData);
  TimeEvaluator<UsdBridgeMeshData> timeEval(geomData, timeStep);

  assert((geomData.NumIndices % geomData.FaceVertexCount) == 0);
  uint64_t numPrims = int(geomData.NumIndices) / geomData.FaceVertexCount;

  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomPoints);
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomNormals);
  if( UsdGeomDataHasTexCoords(geomData) ) 
    { UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomTexCoords); }
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomAttributes);
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomColors);
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

    UsdGeomPoints timeVarGeom = UsdGeomPoints::Get(timeVarStage, instancerPath);
    assert(timeVarGeom);

    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomPoints);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomInstanceIds);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomWidths);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomOrientNormals);
    if( UsdGeomDataHasTexCoords(geomData) ) 
      { UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomTexCoords); }
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomAttributes);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomColors);
  }
  else
  {
    UsdGeomPointInstancer uniformGeom = UsdGeomPointInstancer::Get(this->SceneStage, instancerPath);
    assert(uniformGeom);

    UsdGeomPointInstancer timeVarGeom = UsdGeomPointInstancer::Get(timeVarStage, instancerPath);
    assert(timeVarGeom);

    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomPoints);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomInstanceIds);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomScales);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomOrientations);
    if( UsdGeomDataHasTexCoords(geomData) ) 
      { UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomTexCoords); }
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomAttributes);
    UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomColors);
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

  UsdGeomBasisCurves timeVarGeom = UsdGeomBasisCurves::Get(timeVarStage, curvePath);
  assert(timeVarGeom);

  // Update the curve
  UsdBridgeUpdateEvaluator<const UsdBridgeCurveData> updateEval(geomData);
  TimeEvaluator<UsdBridgeCurveData> timeEval(geomData, timeStep);

  uint64_t numPrims = geomData.NumCurveLengths;

  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomPoints);
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomNormals);
  if( UsdGeomDataHasTexCoords(geomData) ) 
    { UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomTexCoords); }
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomAttributes);
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomColors);
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomWidths);
  UPDATE_USDGEOM_ARRAYS(UpdateUsdGeomCurveLengths);
}

void UsdBridgeUsdWriter::UpdateUsdMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, const UsdBridgeMaterialData& matData, double timeStep)
{
  // Update usd shader
  if(Settings.EnablePreviewSurfaceShader)
  {
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(shaderPrimPf));
    this->UpdateUsdShader(materialStage, matPrimPath, shaderPrimPath, matData, timeStep);
  }

#ifdef SUPPORT_MDL_SHADERS  
  if(Settings.EnableMdlShader)
  {
    // Update mdl shader
    SdfPath mdlShaderPrimPath = matPrimPath.AppendPath(SdfPath(mdlShaderPrimPf));
    this->UpdateMdlShader(materialStage, mdlShaderPrimPath, matData, timeStep);
  }
#endif
}

void UsdBridgeUsdWriter::UpdateUsdShader(UsdStageRefPtr shaderStage, const SdfPath& matPrimPath, const SdfPath& shadPrimPath, const UsdBridgeMaterialData& matData, double timeStep)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData, timeStep);
  typedef UsdBridgeMaterialData::DataMemberId DMI;

  UsdShadeShader uniformShadPrim = UsdShadeShader::Get(SceneStage, shadPrimPath);
  assert(uniformShadPrim);

  UsdShadeShader timeVarShadPrim = UsdShadeShader::Get(shaderStage, shadPrimPath);
  assert(timeVarShadPrim);

  GfVec3f difColor(matData.Diffuse);
  GfVec3f specColor(matData.Specular); // Not sure yet how to incorporate the specular color, it doesn't directly map to usd specularColor
  GfVec3f emColor(matData.Emissive);
  emColor *= matData.EmissiveIntensity;

  timeVarShadPrim.GetInput(UsdBridgeTokens->useSpecularWorkflow).Set(matData.Metallic >= 0.0 ? 0 : 1);

  timeVarShadPrim.GetInput(UsdBridgeTokens->roughness).Set(matData.Roughness, timeEval.Eval(DMI::ROUGHNESS));
  timeVarShadPrim.GetInput(UsdBridgeTokens->opacity).Set(matData.Opacity, timeEval.Eval(DMI::OPACITY));
  timeVarShadPrim.GetInput(UsdBridgeTokens->metallic).Set(matData.Metallic, timeEval.Eval(DMI::METALLIC));
  timeVarShadPrim.GetInput(UsdBridgeTokens->ior).Set(matData.Ior, timeEval.Eval(DMI::IOR));

  timeVarShadPrim.GetInput(UsdBridgeTokens->emissiveColor).Set(emColor, timeEval.Eval(DMI::EMISSIVE));

  if (matData.UseVertexColors)
  {
    SdfPath vertexColorReaderPrimPath = matPrimPath.AppendPath(SdfPath(vertexColorReaderPrimPf));
    UsdShadeShader vertexColorReader = UsdShadeShader::Get(SceneStage, vertexColorReaderPrimPath);
    assert(vertexColorReader);

    timeVarShadPrim.GetPrim().RemoveProperty(TfToken("input:diffuseColor"));
    timeVarShadPrim.GetPrim().RemoveProperty(TfToken("input:specularColor"));
    
    UsdShadeOutput vcReaderOutput = vertexColorReader.CreateOutput(UsdBridgeTokens->result, SdfValueTypeNames->Color3f);

    uniformShadPrim.CreateInput(UsdBridgeTokens->diffuseColor, SdfValueTypeNames->Color3f).ConnectToSource(vcReaderOutput); // make sure to create input, as it may have been removed by other branch (or even the timeVarShadPrim, depending on compile defs)
    uniformShadPrim.CreateInput(UsdBridgeTokens->specularColor, SdfValueTypeNames->Color3f).ConnectToSource(vcReaderOutput);
  }
  else
  {
    uniformShadPrim.GetPrim().RemoveProperty(TfToken("input:diffuseColor"));
    uniformShadPrim.GetPrim().RemoveProperty(TfToken("input:specularColor"));
    timeVarShadPrim.CreateInput(UsdBridgeTokens->diffuseColor, SdfValueTypeNames->Color3f).Set(difColor, timeEval.Eval(DMI::DIFFUSE));
    timeVarShadPrim.CreateInput(UsdBridgeTokens->specularColor, SdfValueTypeNames->Color3f).Set(specColor, timeEval.Eval(DMI::SPECULAR));
  }
}

#ifdef SUPPORT_MDL_SHADERS 
void UsdBridgeUsdWriter::UpdateMdlShader(UsdStageRefPtr shaderStage, const SdfPath& shadPrimPath, const UsdBridgeMaterialData& matData, double timeStep)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData, timeStep);
  typedef UsdBridgeMaterialData::DataMemberId DMI;

  UsdShadeShader uniformShadPrim = UsdShadeShader::Get(SceneStage, shadPrimPath);
  assert(uniformShadPrim);

  UsdShadeShader timeVarShadPrim = UsdShadeShader::Get(shaderStage, shadPrimPath);
  assert(timeVarShadPrim);

  GfVec3f difColor(matData.Diffuse);
  GfVec3f specColor(matData.Specular); // Not sure yet how to incorporate the specular color, no mdl parameter available.
  GfVec3f emColor(matData.Emissive);

  uniformShadPrim.GetInput(UsdBridgeTokens->vertexcolor_coordinate_index).Set(matData.UseVertexColors ? 1 : -1);

  timeVarShadPrim.GetInput(UsdBridgeTokens->diffuse_color_constant).Set(difColor, timeEval.Eval(DMI::DIFFUSE));
  timeVarShadPrim.GetInput(UsdBridgeTokens->emissive_color).Set(emColor, timeEval.Eval(DMI::EMISSIVE));
  timeVarShadPrim.GetInput(UsdBridgeTokens->emissive_intensity).Set(matData.EmissiveIntensity, timeEval.Eval(DMI::EMISSIVEINTENSITY));
  timeVarShadPrim.GetInput(UsdBridgeTokens->opacity_constant).Set(matData.Opacity, timeEval.Eval(DMI::OPACITY));
  timeVarShadPrim.GetInput(UsdBridgeTokens->reflection_roughness_constant).Set(matData.Roughness, timeEval.Eval(DMI::ROUGHNESS));
  timeVarShadPrim.GetInput(UsdBridgeTokens->metallic_constant).Set(matData.Metallic, timeEval.Eval(DMI::METALLIC));
  timeVarShadPrim.GetInput(UsdBridgeTokens->ior_constant).Set(matData.Ior, timeEval.Eval(DMI::IOR));
  timeVarShadPrim.GetInput(UsdBridgeTokens->enable_emission).Set(matData.EmissiveIntensity > 0, timeEval.Eval(DMI::EMISSIVEINTENSITY));

  if (!matData.HasTranslucency)
    uniformShadPrim.SetSourceAsset(this->MdlOpaqueRelFilePath, UsdBridgeTokens->mdl);
  else
    uniformShadPrim.SetSourceAsset(this->MdlTranslucentRelFilePath, UsdBridgeTokens->mdl);
}
#endif

void UsdBridgeUsdWriter::UpdateUsdVolume(UsdStageRefPtr volumeStage, const SdfPath& volPrimPath, const std::string& name, const UsdBridgeVolumeData& volumeData, double timeStep)
{
  TimeEvaluator<UsdBridgeVolumeData> timeEval(volumeData, timeStep);
  typedef UsdBridgeVolumeData::DataMemberId DMI;

  // Get the volume and ovdb field prims
  UsdVolVolume volume = UsdVolVolume::Get(volumeStage, volPrimPath);
  assert(volume);

  SdfPath ovdbFieldPath = volPrimPath.AppendPath(SdfPath(openVDBPrimPf));
  UsdVolOpenVDBAsset ovdbField = UsdVolOpenVDBAsset::Get(volumeStage, ovdbFieldPath);

  // Set the file path reference in usd
  UsdAttribute fileAttr = ovdbField.GetFilePathAttr();

  std::string relVolPath(std::string(volFolder) + name);
#ifdef TIME_BASED_CACHING
  relVolPath += "_" + std::to_string(timeStep);
#endif
  relVolPath += ".vdb";

  SdfAssetPath volAsset(relVolPath);
  fileAttr.Set(volAsset, timeEval.Eval(DMI::DATA));

  // Output stream path
  std::string fullVolPath(SessionDirectory + relVolPath);

  // Translate-scale in usd
  volume.ClearXformOpOrder();

  UsdGeomXformOp transOp = volume.AddTranslateOp();
  GfVec3d trans(volumeData.Origin[0], volumeData.Origin[1], volumeData.Origin[2]);
  transOp.Set(trans, timeEval.Eval(DMI::ORIGIN));

  UsdGeomXformOp scaleOp = volume.AddScaleOp();
  GfVec3f scale(volumeData.CellDimensions[0], volumeData.CellDimensions[1], volumeData.CellDimensions[2]);
  scaleOp.Set(scale, timeEval.Eval(DMI::CELLDIMENSIONS));

  // Set extents in usd
  VtVec3fArray extentArray(2);
  extentArray[0].Set(0.0f, 0.0f, 0.0f);
  extentArray[1].Set((float)volumeData.NumElements[0], (float)volumeData.NumElements[1], (float)volumeData.NumElements[2]); //approx extents

  volume.GetExtentAttr().Set(extentArray, timeEval.Eval(DMI::DATA));

  // Write VDB data to stream
  VolumeWriter->ToVDB(volumeData);

  // Flush stream out to storage
  const char* volumeStreamData; size_t volumeStreamDataSize;
  VolumeWriter->GetSerializedVolumeData(volumeStreamData, volumeStreamDataSize);
  Connect->WriteFile(volumeStreamData, volumeStreamDataSize, fullVolPath.c_str());
}

void UsdBridgeUsdWriter::UpdateUsdSampler(const SdfPath& samplerPrimPath, const UsdBridgeSamplerData& samplerData, double timeStep)
{
  TimeEvaluator<UsdBridgeSamplerData> timeEval(samplerData, timeStep);
  typedef UsdBridgeSamplerData::DataMemberId DMI;

  UsdShadeShader texReaderPrim = UsdShadeShader::Get(this->SceneStage, samplerPrimPath);
  assert(texReaderPrim);

  SdfAssetPath texFile(samplerData.FileName);
  texReaderPrim.CreateInput(UsdBridgeTokens->file, SdfValueTypeNames->Asset).Set(texFile, timeEval.Eval(DMI::FILENAME));
  texReaderPrim.CreateInput(UsdBridgeTokens->WrapS, SdfValueTypeNames->Token).Set(TextureWrapToken(samplerData.WrapS), timeEval.Eval(DMI::WRAPS));
  texReaderPrim.CreateInput(UsdBridgeTokens->WrapT, SdfValueTypeNames->Token).Set(TextureWrapToken(samplerData.WrapT), timeEval.Eval(DMI::WRAPT));
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

void ResourceCollectVolume(const UsdBridgePrimCache* cache, const UsdBridgeUsdWriter& usdWriter)
{
#ifdef VALUE_CLIP_RETIMING
  const UsdStageRefPtr& volumeStage = cache->PrimStage.second;
#else
  const UsdStageRefPtr& volumeStage = usdWriter.SceneStage;
#endif

  const SdfPath& volPrimPath = cache->PrimPath;
  const std::string& name = cache->Name.GetString();

  UsdVolVolume volume = UsdVolVolume::Get(volumeStage, volPrimPath);
  assert(volume);

  SdfPath ovdbFieldPath = volPrimPath.AppendPath(SdfPath(openVDBPrimPf));
  UsdVolOpenVDBAsset ovdbField = UsdVolOpenVDBAsset::Get(volumeStage, ovdbFieldPath);

  UsdAttribute fileAttr = ovdbField.GetFilePathAttr();

  std::vector<double> fileTimes;
  fileAttr.GetTimeSamples(&fileTimes);

  for (double timeStep : fileTimes)
  {
    std::string volFileName(usdWriter.SessionDirectory + std::string(volFolder) + name);
#ifdef TIME_BASED_CACHING
    volFileName += "_" + std::to_string(timeStep);
#endif
    volFileName += ".vdb";

    usdWriter.Connect->RemoveFile(volFileName.c_str());
  }
}

void ResourceCollectSampler(const UsdBridgePrimCache* cache, const UsdBridgeUsdWriter& sceneStage)
{

}