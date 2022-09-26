// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeUsdWriter.h"

#include "UsdBridgeUsdWriter_Common.h"

namespace
{
  struct QualifiedTokenCollection
  {
    TfToken roughness = TfToken("inputs:roughness", TfToken::Immortal);
    TfToken opacity = TfToken("inputs:opacity", TfToken::Immortal);
    TfToken metallic = TfToken("inputs:metallic", TfToken::Immortal);
    TfToken ior = TfToken("inputs:ior", TfToken::Immortal);
    TfToken diffuseColor = TfToken("inputs:diffuseColor", TfToken::Immortal);
    TfToken specularColor = TfToken("inputs:specularColor", TfToken::Immortal);
    TfToken emissiveColor = TfToken("inputs:emissiveColor", TfToken::Immortal);

    TfToken reflection_roughness_constant = TfToken("inputs:reflection_roughness_constant", TfToken::Immortal);
    TfToken opacity_constant = TfToken("inputs:opacity_constant", TfToken::Immortal);
    TfToken metallic_constant = TfToken("inputs:metallic_constant", TfToken::Immortal);
    TfToken ior_constant = TfToken("inputs:ior_constant");
    TfToken diffuse_color_constant = TfToken("inputs:diffuse_color_constant", TfToken::Immortal);
    TfToken emissive_color = TfToken("inputs:emissive_color", TfToken::Immortal);
    TfToken emissive_intensity = TfToken("inputs:emissive_intensity", TfToken::Immortal);
    TfToken enable_emission = TfToken("inputs:enable_emission", TfToken::Immortal);
    TfToken diffuse_texture = TfToken("inputs:diffuse_texture", TfToken::Immortal);

    TfToken file = TfToken("inputs:file", TfToken::Immortal);
    TfToken WrapS = TfToken("inputs:WrapS", TfToken::Immortal);
    TfToken WrapT = TfToken("inputs:WrapT", TfToken::Immortal);
    TfToken WrapR = TfToken("inputs:WrapR", TfToken::Immortal);
    TfToken varname = TfToken("inputs:varname", TfToken::Immortal);
  };
}
TfStaticData<QualifiedTokenCollection> QualifiedTokens;

namespace
{
  template<typename GeomDataType>
  void CreateShaderInput(UsdShadeShader& shader, const TimeEvaluator<GeomDataType>* timeEval, typename GeomDataType::DataMemberId dataMemberId, 
    const TfToken& inputToken, const TfToken& qualifiedInputToken, const SdfValueTypeName& valueType)
  {
    if(!timeEval || timeEval->IsTimeVarying(dataMemberId))
      shader.CreateInput(inputToken, valueType);
    else
      shader.GetPrim().RemoveProperty(qualifiedInputToken);
  }

  template<typename ValueType, typename GeomDataType>
  void SetShaderInput(UsdShadeShader& uniformShadPrim, UsdShadeShader& timeVarShadPrim, const TimeEvaluator<GeomDataType>& timeEval, 
    const TfToken& inputToken, typename GeomDataType::DataMemberId dataMemberId, ValueType value)
  {
    using DMI = typename GeomDataType::DataMemberId;

    bool timeVaryingUpdate = timeEval.IsTimeVarying(dataMemberId);

    UsdShadeInput timeVarInput, uniformInput;
    UsdAttribute uniformAttrib, timeVarAttrib;
    if(timeVarShadPrim) // Allow for non-existing prims (based on timeVaryingUpdate)
    {
      timeVarInput = timeVarShadPrim.GetInput(inputToken);
      timeVarAttrib = timeVarInput.GetAttr();
    }
    if(uniformShadPrim)
    {
      uniformInput = uniformShadPrim.GetInput(inputToken);
      uniformAttrib = uniformInput.GetAttr();
    }

    // Clear the attributes that are not set (based on timeVaryingUpdate)
    ClearAttributes(uniformAttrib, timeVarAttrib, timeVaryingUpdate);

    // Set the input that requires an update
    if(timeVaryingUpdate)
      timeVarInput.Set(value, timeEval.Eval(dataMemberId));
    else
      uniformInput.Set(value, timeEval.Eval(dataMemberId));
  }

  UsdShadeShader InitializeUsdAttributeReader_Impl(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, bool uniformPrim,
    UsdBridgeMaterialData::DataMemberId dataMemberId, const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    // Create a vertexcolorreader
    SdfPath attributeReaderPath = matPrimPath.AppendPath(GetAttributeReaderPathPf(dataMemberId));
    UsdShadeShader attributeReader = UsdShadeShader::Get(materialStage, attributeReaderPath);

    // Allow for uniform and timevar prims to return already existing prim without triggering re-initialization
    // manifest <==> timeEval, and will always take this branch
    if(!attributeReader || timeEval) 
    {
      if(!attributeReader)
        attributeReader = UsdShadeShader::Define(materialStage, attributeReaderPath);

      if(uniformPrim) // Implies !timeEval, so initialize
      {
        attributeReader.CreateIdAttr(VtValue(GetAttributeReaderId(dataMemberId)));
        attributeReader.CreateOutput(UsdBridgeTokens->result, GetShaderNodeOutputType(dataMemberId));
      }

      // Create attribute reader varname, and if timeEval (manifest), can also remove the input
      CreateShaderInput(attributeReader, timeEval, dataMemberId, UsdBridgeTokens->varname, QualifiedTokens->varname, SdfValueTypeNames->Token);
    }

    return attributeReader;
  }

  void GetOrCreateAttributeReaders(UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, UsdBridgeMaterialData::DataMemberId dataMemberId,
    UsdShadeShader& uniformReaderPrim, UsdShadeShader& timeVarReaderPrim)
  {
    uniformReaderPrim = InitializeUsdAttributeReader_Impl(sceneStage, matPrimPath, true, dataMemberId, nullptr);
    if(timeVarStage && (timeVarStage != sceneStage))
      timeVarReaderPrim = InitializeUsdAttributeReader_Impl(timeVarStage, matPrimPath, false, dataMemberId, nullptr);
  }

  template<typename InputValueType, typename ValueType, typename GeomDataType>
  void UpdateUsdShaderInput(UsdBridgeUsdWriter* writer, UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, 
    UsdShadeShader& uniformShadPrim, UsdShadeShader& timeVarShadPrim, const SdfPath& matPrimPath, const TimeEvaluator<GeomDataType>& timeEval,
    UsdBridgeMaterialData::DataMemberId dataMemberId, const TfToken& inputToken, 
    const UsdBridgeMaterialData::MaterialInput<InputValueType>& param, const ValueType& inputValue)
  {
    UsdShadeInput uniformDiffInput = uniformShadPrim.GetInput(inputToken);

    if (param.SrcAttrib != nullptr)
    {
      bool isTimeVarying = timeEval.IsTimeVarying(dataMemberId);
      
      // Create attribute reader(s) to connect to the material shader input
      UsdShadeShader uniformReaderPrim, timeVarReaderPrim;
      GetOrCreateAttributeReaders(sceneStage,
        isTimeVarying ? timeVarStage : sceneStage, // Skip creation of the timevar reader if it's not used anyway
        matPrimPath, dataMemberId, uniformReaderPrim, timeVarReaderPrim);
      assert(uniformReaderPrim);
      assert(!isTimeVarying || timeVarReaderPrim);

      // Set the correct attribute token for the reader varname
      SetShaderInput(uniformReaderPrim, timeVarReaderPrim, timeEval, UsdBridgeTokens->varname, dataMemberId, writer->AttributeNameToken(param.SrcAttrib));

      // Connect the reader to the material shader input
      timeVarShadPrim.GetInput(inputToken).GetAttr().Clear(); // Clear timevar data written earlier, will be replaced by connection
      
      UsdShadeOutput attribReaderOutput = uniformReaderPrim.GetOutput(UsdBridgeTokens->result);
      uniformDiffInput.ConnectToSource(attribReaderOutput); // Can only connect to uniform shader, as connections cannot be timevarying
    }
    else if(!param.Sampler)
    {
      if(uniformDiffInput.HasConnectedSource())
        uniformDiffInput.DisconnectSource();

      // Just treat like regular time-varying inputs
      SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, inputToken, dataMemberId, inputValue);
    }
  }

  // Convenience for when param.Value is simply the intended value to be set
  template<typename InputValueType, typename GeomDataType>
  void UpdateUsdShaderInput(UsdBridgeUsdWriter* writer, UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, 
    UsdShadeShader& uniformShadPrim, UsdShadeShader& timeVarShadPrim, const SdfPath& matPrimPath, const TimeEvaluator<GeomDataType>& timeEval,
    UsdBridgeMaterialData::DataMemberId dataMemberId, const TfToken& inputToken, 
    const UsdBridgeMaterialData::MaterialInput<InputValueType>& param)
  {
    UpdateUsdShaderInput(writer, sceneStage, timeVarStage, uniformShadPrim, timeVarShadPrim, matPrimPath, timeEval, 
      dataMemberId, inputToken, param, param.Value);
  }

  void InitializeUsdShaderTimeVar(UsdShadeShader& shader, const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    CreateShaderInput(shader, timeEval, DMI::ROUGHNESS, UsdBridgeTokens->roughness, QualifiedTokens->roughness, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::OPACITY, UsdBridgeTokens->opacity, QualifiedTokens->opacity, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::METALLIC, UsdBridgeTokens->metallic, QualifiedTokens->metallic, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::IOR, UsdBridgeTokens->ior, QualifiedTokens->ior, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::DIFFUSE, UsdBridgeTokens->diffuseColor, QualifiedTokens->diffuseColor, SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::SPECULAR, UsdBridgeTokens->specularColor, QualifiedTokens->specularColor, SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVE, UsdBridgeTokens->emissiveColor, QualifiedTokens->emissiveColor, SdfValueTypeNames->Color3f);
  }

#ifdef SUPPORT_MDL_SHADERS  
  void InitializeUsdMdlShaderTimeVar(UsdShadeShader& shader, const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    CreateShaderInput(shader, timeEval, DMI::ROUGHNESS, UsdBridgeTokens->reflection_roughness_constant, QualifiedTokens->reflection_roughness_constant, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::OPACITY, UsdBridgeTokens->opacity_constant, QualifiedTokens->opacity_constant, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::METALLIC, UsdBridgeTokens->metallic_constant, QualifiedTokens->metallic_constant, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::IOR, UsdBridgeTokens->ior_constant, QualifiedTokens->ior_constant, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::DIFFUSE, UsdBridgeTokens->diffuse_color_constant, QualifiedTokens->diffuse_color_constant, SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVE, UsdBridgeTokens->emissive_color, QualifiedTokens->emissive_color, SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVEINTENSITY, UsdBridgeTokens->emissive_intensity, QualifiedTokens->emissive_intensity, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVEINTENSITY, UsdBridgeTokens->enable_emission, QualifiedTokens->enable_emission, SdfValueTypeNames->Bool);
  }
#endif

  void InitializeUsdSamplerTimeVar(UsdShadeShader& sampler, UsdShadeShader& texCoordReader, UsdBridgeSamplerData::SamplerType type, const TimeEvaluator<UsdBridgeSamplerData>* timeEval = nullptr)
  {
    typedef UsdBridgeSamplerData::DataMemberId DMI;

    CreateShaderInput(sampler, timeEval, DMI::DATA, UsdBridgeTokens->file, QualifiedTokens->file, SdfValueTypeNames->Asset);
    CreateShaderInput(sampler, timeEval, DMI::WRAPS, UsdBridgeTokens->WrapS, QualifiedTokens->WrapS, SdfValueTypeNames->Token);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPT, UsdBridgeTokens->WrapT, QualifiedTokens->WrapT, SdfValueTypeNames->Token);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_3D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPR, UsdBridgeTokens->WrapR, QualifiedTokens->WrapR, SdfValueTypeNames->Token);

    CreateShaderInput(texCoordReader, timeEval, DMI::INATTRIBUTE, UsdBridgeTokens->varname, QualifiedTokens->varname, SdfValueTypeNames->Token);
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
      // Create a shader
      SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::shaderPrimPf));
      UsdShadeOutput shaderOutput = InitializeUsdShader(materialStage, shaderPrimPath, uniformPrim, timeEval);

      if(uniformPrim)
        BindShaderToMaterial(matPrim, shaderOutput, nullptr);
    }

#ifdef SUPPORT_MDL_SHADERS 
    if(settings.EnableMdlShader)
    {
      // Create an mdl shader
      SdfPath mdlShaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlShaderPrimPf));
      UsdShadeOutput mdlShaderPrim = InitializeMdlShader(materialStage, mdlShaderPrimPath, uniformPrim, timeEval);

      if(uniformPrim)
        BindShaderToMaterial(matPrim, mdlShaderPrim, &UsdBridgeTokens->mdl);
    }
#endif

    return matPrim;
  }

  #define INITIALIZE_USD_ATTRIBUTE_READER_MACRO(srcAttrib, dmi) \
    if(srcAttrib) InitializeUsdAttributeReader_Impl(materialStage, matPrimPath, false, dmi, timeEval);

  void InitializeUsdAttributeReaders_Impl(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, const UsdBridgeSettings& settings,
    const UsdBridgeMaterialData& matData, const TimeEvaluator<UsdBridgeMaterialData>* timeEval)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    // So far, only used for manifest, so no uniform path required. 
    // Uniform and timevar attrib readers are created on demand per-attribute in GetOrCreateAttributeReaders().

    if(settings.EnablePreviewSurfaceShader)
    {
      INITIALIZE_USD_ATTRIBUTE_READER_MACRO(matData.Diffuse.SrcAttrib, DMI::DIFFUSE);
      INITIALIZE_USD_ATTRIBUTE_READER_MACRO(matData.Opacity.SrcAttrib, DMI::OPACITY);
      INITIALIZE_USD_ATTRIBUTE_READER_MACRO(matData.Specular.SrcAttrib, DMI::SPECULAR);
      INITIALIZE_USD_ATTRIBUTE_READER_MACRO(matData.Emissive.SrcAttrib, DMI::EMISSIVE);
      INITIALIZE_USD_ATTRIBUTE_READER_MACRO(matData.EmissiveIntensity.SrcAttrib, DMI::EMISSIVEINTENSITY);
      INITIALIZE_USD_ATTRIBUTE_READER_MACRO(matData.Roughness.SrcAttrib, DMI::ROUGHNESS);
      INITIALIZE_USD_ATTRIBUTE_READER_MACRO(matData.Metallic.SrcAttrib, DMI::METALLIC);
      INITIALIZE_USD_ATTRIBUTE_READER_MACRO(matData.Ior.SrcAttrib, DMI::IOR);
    }
  }

  UsdPrim InitializeUsdSampler_Impl(UsdStageRefPtr samplerStage, const SdfPath& samplerPrimPath, UsdBridgeSamplerData::SamplerType type, bool uniformPrim,
    const TimeEvaluator<UsdBridgeSamplerData>* timeEval = nullptr)
  {
    UsdShadeShader sampler = GetOrDefinePrim<UsdShadeShader>(samplerStage, samplerPrimPath);
    assert(sampler);

    SdfPath texCoordReaderPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::texCoordReaderPrimPf));
    UsdShadeShader texCoordReader = UsdShadeShader::Define(samplerStage, texCoordReaderPrimPath);
    assert(texCoordReader);

    if(uniformPrim)
    {
      // USD does not yet allow for anything but 2D coords, but let's try anyway
      TfToken idAttrib;
      SdfValueTypeName valueType;
      if(type == UsdBridgeSamplerData::SamplerType::SAMPLER_1D)
      {
        idAttrib = UsdBridgeTokens->PrimVarReader_Float;
        valueType = SdfValueTypeNames->Float;
      }
      else if (type == UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
      {
        idAttrib = UsdBridgeTokens->PrimVarReader_Float2;
        valueType = SdfValueTypeNames->Float2;
      }
      else
      {
        idAttrib = UsdBridgeTokens->PrimVarReader_Float3;
        valueType = SdfValueTypeNames->Float3;
      }

      texCoordReader.CreateIdAttr(VtValue(idAttrib));
      UsdShadeOutput tcOutput = texCoordReader.CreateOutput(UsdBridgeTokens->result, valueType);

      sampler.CreateIdAttr(VtValue(UsdBridgeTokens->UsdUVTexture));
      sampler.CreateInput(UsdBridgeTokens->fallback, SdfValueTypeNames->Float4).Set(GfVec4f(1.0f, 0.0f, 0.0f, 1.0f));
      
      sampler.CreateOutput(UsdBridgeTokens->rgb, SdfValueTypeNames->Float3); // Input images with less components are automatically expanded, see usd docs
      sampler.CreateOutput(UsdBridgeTokens->a, SdfValueTypeNames->Float);

      // Bind the texcoord reader's output to the sampler's st input
      sampler.CreateInput(UsdBridgeTokens->st, valueType).ConnectToSource(tcOutput);
    }

    InitializeUsdSamplerTimeVar(sampler, texCoordReader, type, timeEval);

    return sampler.GetPrim();
  }
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
void UsdBridgeUsdWriter::UpdateUsdMaterialManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeMaterialData& matData)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData);
  InitializeUsdMaterial_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, false, 
    Settings, &timeEval);

  InitializeUsdAttributeReaders_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, 
    Settings, matData, &timeEval);

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

void UsdBridgeUsdWriter::ConnectSamplerToMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, const SdfPath& refSamplerPrimPath, 
  const std::string& samplerPrimName, const UsdSamplerRefData& samplerRefData, double worldTimeStep)
{
  using DMI = UsdBridgeMaterialData::DataMemberId;

  // Essentially, this is an extension of UpdateUsdShaderInput() for the case of param.Sampler
  if(Settings.EnablePreviewSurfaceShader)
  {
    // Referenced sampler prim
    UsdShadeShader refSampler = UsdShadeShader::Get(this->SceneStage, refSamplerPrimPath); // type inherited from sampler prim (in AddRef)
    assert(refSampler);

    // Get shader prims
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::shaderPrimPf));

    UsdShadeShader uniformShad = UsdShadeShader::Get(this->SceneStage, shaderPrimPath);
    assert(uniformShad);
    UsdShadeShader timeVarShad = UsdShadeShader::Get(materialStage, shaderPrimPath);
    assert(timeVarShad);

    //Bind the sampler to the diffuse color of the uniform shader, so remove any existing values from the timeVar prim 
    DMI samplerDMI = samplerRefData.DataMemberId;
    const TfToken& inputToken = GetMaterialShaderInputToken(samplerDMI);
    timeVarShad.GetInput(inputToken).GetAttr().Clear();

    // Connect refSampler output to uniformShad input
    const TfToken& outputToken = GetSamplerOutputColorToken(samplerRefData.ImageNumComponents);
    UsdShadeOutput refSamplerOutput = refSampler.GetOutput(outputToken);
    assert(refSamplerOutput);

    uniformShad.GetInput(inputToken).ConnectToSource(refSamplerOutput);

    // Specialcase opacity as fourth channel of diffuse
    bool affectsOpacity = (samplerDMI == DMI::DIFFUSE && samplerRefData.ImageNumComponents == 4);
    if(affectsOpacity)
    {
      const TfToken& opacityToken = GetMaterialShaderInputToken(DMI::OPACITY);
      timeVarShad.GetInput(opacityToken).GetAttr().Clear();

      UsdShadeOutput refSamplerAlphaOutput = refSampler.GetOutput(UsdBridgeTokens->a);
      assert(refSamplerAlphaOutput);

      uniformShad.GetInput(opacityToken).ConnectToSource(refSamplerAlphaOutput);
    }
  }

#ifdef SUPPORT_MDL_SHADERS
  if(Settings.EnableMdlShader)
  {
    // If no url available, use generated filename using data from the referenced sampler
    const char* imgFileName = samplerRefData.ImageUrl;
    const std::string& generatedFileName = GetResourceFileName(constring::imgFolder, samplerRefData.ImageName, samplerPrimName, samplerRefData.TimeStep, constring::imageExtension);

    if(!imgFileName)
      imgFileName = generatedFileName.c_str();

    // Only uses the uniform prim and world timestep, as the timevar material subprim is value-clip-referenced 
    // from the material's parent, and therefore not in world-time. Figuring out material to sampler mapping is not worth the effort 
    // and would not fix the underlying issue (this has to be part of a separate sampler prim)
    
    // Should ideally be connected to the sampler prim's file input, but that isn't supported by Create as of writing.

    SdfPath mdlShaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlShaderPrimPf));
    UsdShadeShader uniformMdlShad = UsdShadeShader::Get(this->SceneStage, mdlShaderPrimPath);
    assert(uniformMdlShad); 

    UsdTimeCode timeCode = samplerRefData.ImageTimeVarying ? UsdTimeCode(worldTimeStep) : UsdTimeCode::Default(); // For worldTimeStep see commentary above

    UsdShadeInput diffTexInput = uniformMdlShad.CreateInput(UsdBridgeTokens->diffuse_texture, SdfValueTypeNames->Asset);
    if(!samplerRefData.ImageTimeVarying)
      diffTexInput.GetAttr().Clear();
    diffTexInput.Set(SdfAssetPath(imgFileName), timeCode);

    uniformMdlShad.GetInput(UsdBridgeTokens->vertexcolor_coordinate_index).Set(-1);
  }
#endif
}

void UsdBridgeUsdWriter::UpdateUsdMaterial(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, const UsdBridgeMaterialData& matData, double timeStep)
{
  // Update usd shader
  if(Settings.EnablePreviewSurfaceShader)
  {
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::shaderPrimPf));
    this->UpdateUsdShader(timeVarStage, matPrimPath, shaderPrimPath, matData, timeStep);
  }

#ifdef SUPPORT_MDL_SHADERS  
  if(Settings.EnableMdlShader)
  {
    // Update mdl shader
    SdfPath mdlShaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlShaderPrimPf));
    this->UpdateMdlShader(timeVarStage, mdlShaderPrimPath, matData, timeStep);
  }
#endif
}

#define UPDATE_USD_SHADER_INPUT_MACRO(...) \
  UpdateUsdShaderInput(this, SceneStage, timeVarStage, uniformShadPrim, timeVarShadPrim, matPrimPath, timeEval, __VA_ARGS__)

void UsdBridgeUsdWriter::UpdateUsdShader(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, const SdfPath& shadPrimPath, const UsdBridgeMaterialData& matData, double timeStep)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData, timeStep);
  typedef UsdBridgeMaterialData::DataMemberId DMI;

  UsdShadeShader uniformShadPrim = UsdShadeShader::Get(SceneStage, shadPrimPath);
  assert(uniformShadPrim);

  UsdShadeShader timeVarShadPrim = UsdShadeShader::Get(timeVarStage, shadPrimPath);
  assert(timeVarShadPrim);

  GfVec3f difColor(GetValuePtr(matData.Diffuse));
  GfVec3f specColor(GetValuePtr(matData.Specular)); // Not sure yet how to incorporate the specular color, it doesn't directly map to usd specularColor
  GfVec3f emColor(GetValuePtr(matData.Emissive));
  emColor *= matData.EmissiveIntensity.Value; // This multiplication won't translate to vcr/sampler usage (same as the shininess->roughness transformation)

  uniformShadPrim.GetInput(UsdBridgeTokens->useSpecularWorkflow).Set(
    (matData.Metallic.Value >= 0.0 || matData.Metallic.SrcAttrib || matData.Metallic.Sampler) ? 0 : 1);

  UPDATE_USD_SHADER_INPUT_MACRO(DMI::DIFFUSE, UsdBridgeTokens->diffuseColor, matData.Diffuse, difColor);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::SPECULAR, UsdBridgeTokens->specularColor, matData.Specular, specColor);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::EMISSIVE, UsdBridgeTokens->emissiveColor, matData.Emissive, emColor);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::ROUGHNESS, UsdBridgeTokens->roughness, matData.Roughness);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::OPACITY, UsdBridgeTokens->opacity, matData.Opacity);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::METALLIC, UsdBridgeTokens->metallic, matData.Metallic);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::IOR, UsdBridgeTokens->ior, matData.Ior);
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

  GfVec3f difColor(GetValuePtr(matData.Diffuse));
  GfVec3f specColor(GetValuePtr(matData.Specular)); // Not sure yet how to incorporate the specular color, no mdl parameter available.
  GfVec3f emColor(GetValuePtr(matData.Emissive));

  uniformShadPrim.GetInput(UsdBridgeTokens->vertexcolor_coordinate_index).Set(matData.Diffuse.SrcAttrib ? 1 : -1);

  // Only set values on either timevar or uniform prim
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->diffuse_color_constant, DMI::DIFFUSE, difColor);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->emissive_color, DMI::EMISSIVE, emColor);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->emissive_intensity, DMI::EMISSIVEINTENSITY, matData.EmissiveIntensity.Value);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->opacity_constant, DMI::OPACITY, matData.Opacity.Value);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->reflection_roughness_constant, DMI::ROUGHNESS, matData.Roughness.Value);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->metallic_constant, DMI::METALLIC, matData.Metallic.Value);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->ior_constant, DMI::IOR, matData.Ior.Value);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->enable_emission, DMI::EMISSIVEINTENSITY, matData.EmissiveIntensity.Value > 0);

  if (!matData.HasTranslucency)
    uniformShadPrim.SetSourceAsset(this->MdlOpaqueRelFilePath, UsdBridgeTokens->mdl);
  else
    uniformShadPrim.SetSourceAsset(this->MdlTranslucentRelFilePath, UsdBridgeTokens->mdl);

  if(!matData.Diffuse.Sampler)
    uniformShadPrim.GetPrim().RemoveProperty(QualifiedTokens->diffuse_texture);
}
#endif

void UsdBridgeUsdWriter::UpdateUsdSampler(UsdStageRefPtr timeVarStage, const SdfPath& samplerPrimPath, const UsdBridgeSamplerData& samplerData, double timeStep, UsdBridgePrimCache* cacheEntry)
{
  TimeEvaluator<UsdBridgeSamplerData> timeEval(samplerData, timeStep);
  typedef UsdBridgeSamplerData::DataMemberId DMI;

  // Collect the various prims
  SdfPath tcReaderPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::texCoordReaderPrimPf));

  UsdShadeShader uniformTcReaderPrim = UsdShadeShader::Get(SceneStage, tcReaderPrimPath);
  assert(uniformTcReaderPrim);

  UsdShadeShader timeVarTcReaderPrim = UsdShadeShader::Get(timeVarStage, tcReaderPrimPath);
  assert(timeVarTcReaderPrim);

  UsdShadeShader uniformSamplerPrim = UsdShadeShader::Get(SceneStage, samplerPrimPath);
  assert(uniformSamplerPrim);

  UsdShadeShader timeVarSamplerPrim = UsdShadeShader::Get(timeVarStage, samplerPrimPath);
  assert(timeVarSamplerPrim);

  // Generate an image url
  const std::string& defaultName = cacheEntry->Name.GetString();
  const std::string& generatedFileName = GetResourceFileName(constring::imgFolder, samplerData.ImageName, defaultName, timeStep, constring::imageExtension);

  const char* imgFileName = samplerData.ImageUrl;
  bool writeFile = false;
  if(!imgFileName)
  {
    imgFileName = generatedFileName.c_str();
    writeFile = true;
  }

  // Set all the inputs
  SdfAssetPath texFile(imgFileName);
  SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->file, DMI::DATA, texFile);
  SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->WrapS, DMI::WRAPS, TextureWrapToken(samplerData.WrapS));
  if((uint32_t)samplerData.Type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
    SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->WrapT, DMI::WRAPT, TextureWrapToken(samplerData.WrapT));
  if((uint32_t)samplerData.Type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_3D)
    SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->WrapR, DMI::WRAPR, TextureWrapToken(samplerData.WrapR));

  // Check whether the output type is still correct
  size_t numComponents = samplerData.ImageNumComponents;
  const TfToken& outputToken = GetSamplerOutputColorToken(numComponents);
  if(!uniformSamplerPrim.GetOutput(outputToken))
  {
    // As the previous output type isn't cached, just remove everything:
    uniformSamplerPrim.GetPrim().RemoveProperty(UsdBridgeTokens->r);
    uniformSamplerPrim.GetPrim().RemoveProperty(UsdBridgeTokens->rg);
    uniformSamplerPrim.GetPrim().RemoveProperty(UsdBridgeTokens->rgb);

    uniformSamplerPrim.CreateOutput(outputToken, GetSamplerOutputColorType(numComponents));
  }
  
  SetShaderInput(uniformTcReaderPrim, timeVarTcReaderPrim, timeEval, UsdBridgeTokens->varname, DMI::INATTRIBUTE, AttributeNameToken(samplerData.InAttribute));

  // Update resources
  if(writeFile)
  {
    // Create a resource reference representing the file write
    const char* resourceName = samplerData.ImageName ? samplerData.ImageName : defaultName.c_str();
    UsdBridgeResourceKey key(resourceName, timeStep);
    bool newEntry = cacheEntry->AddResourceKey(key);

    bool isSharedResource = samplerData.ImageName;
    if(newEntry && isSharedResource)
      AddSharedResourceRef(key);

    // Upload as image to texFile (in case this hasn't yet been performed)
    assert(samplerData.Data);
    if(!isSharedResource || !IsSharedResourceModified(key))
    {
      const char* imageData = nullptr;
      size_t imageSize = 0;  

      // Filename, relative from connection working dir
      std::string wdRelFilename(SessionDirectory + imgFileName);
      Connect->WriteFile(imageData, imageSize, wdRelFilename.c_str(), true);
    }
  }
}

void UsdBridgeUsdWriter::UpdateAttributeReader(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, MaterialDMI dataMemberId, const char* newName, double timeStep, MaterialDMI timeVarying)
{
  // Time eval with dummy data
  UsdBridgeMaterialData materialData;
  materialData.TimeVarying = timeVarying;

  TimeEvaluator<UsdBridgeMaterialData> timeEval(materialData, timeStep);
  typedef MaterialDMI DMI;

  if(dataMemberId == DMI::EMISSIVEINTENSITY)
    return; // Emissive intensity is not represented as a shader input in the USD preview surface model

  // Collect the various prims
  SdfPath attributeReaderPath = matPrimPath.AppendPath(GetAttributeReaderPathPf(dataMemberId));

  UsdShadeShader uniformAttribReader = UsdShadeShader::Get(SceneStage, attributeReaderPath);
  if(!uniformAttribReader)
  {
    UsdBridgeLogMacro(this, UsdBridgeLogLevel::ERR, "In UsdBridgeUsdWriter::UpdateAttributeReader(): requesting an attribute reader that hasn't been created during fixup of name token.");
    return;
  }

  UsdShadeShader timeVarAttribReader;
  if(timeEval.IsTimeVarying(dataMemberId))
  {
    timeVarAttribReader = UsdShadeShader::Get(timeVarStage, attributeReaderPath);
    assert(timeVarAttribReader);
  }

  // Set the new Inattribute
  SetShaderInput(uniformAttribReader, timeVarAttribReader, timeEval, UsdBridgeTokens->varname, dataMemberId, AttributeNameToken(newName));
}

void UsdBridgeUsdWriter::UpdateInAttribute(UsdStageRefPtr timeVarStage, const SdfPath& samplerPrimPath, const char* newName, double timeStep, SamplerDMI timeVarying)
{
  // Time eval with dummy data
  UsdBridgeSamplerData samplerData;
  samplerData.TimeVarying = timeVarying;

  TimeEvaluator<UsdBridgeSamplerData> timeEval(samplerData, timeStep);
  typedef SamplerDMI DMI;

  // Collect the various prims
  SdfPath tcReaderPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::texCoordReaderPrimPf));

  UsdShadeShader uniformTcReaderPrim = UsdShadeShader::Get(SceneStage, tcReaderPrimPath);
  assert(uniformTcReaderPrim);

  UsdShadeShader timeVarTcReaderPrim = UsdShadeShader::Get(timeVarStage, tcReaderPrimPath);
  assert(timeVarTcReaderPrim);

  // Set the new Inattribute
  SetShaderInput(uniformTcReaderPrim, timeVarTcReaderPrim, timeEval, UsdBridgeTokens->varname, DMI::INATTRIBUTE, AttributeNameToken(newName));
}

