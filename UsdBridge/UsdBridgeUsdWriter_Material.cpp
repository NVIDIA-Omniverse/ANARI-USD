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
    TfToken file = TfToken("inputs:file", TfToken::Immortal);
    TfToken WrapS = TfToken("inputs:WrapS", TfToken::Immortal);
    TfToken WrapT = TfToken("inputs:WrapT", TfToken::Immortal);
    TfToken WrapR = TfToken("inputs:WrapR", TfToken::Immortal);
    TfToken varname = TfToken("inputs:varname", TfToken::Immortal);

    TfToken reflection_roughness_constant = TfToken("inputs:reflection_roughness_constant", TfToken::Immortal);
    TfToken opacity_constant = TfToken("inputs:opacity_constant", TfToken::Immortal);
    TfToken metallic_constant = TfToken("inputs:metallic_constant", TfToken::Immortal);
    TfToken ior_constant = TfToken("inputs:ior_constant");
    TfToken diffuse_color_constant = TfToken("inputs:diffuse_color_constant", TfToken::Immortal);
    TfToken emissive_color = TfToken("inputs:emissive_color", TfToken::Immortal);
    TfToken emissive_intensity = TfToken("inputs:emissive_intensity", TfToken::Immortal);
    TfToken enable_emission = TfToken("inputs:enable_emission", TfToken::Immortal);
    TfToken name = TfToken("inputs:name", TfToken::Immortal);
    TfToken tex = TfToken("inputs:tex", TfToken::Immortal);
    TfToken wrap_u = TfToken("inputs:wrap_u", TfToken::Immortal);
    TfToken wrap_v = TfToken("inputs:wrap_v", TfToken::Immortal);
    TfToken wrap_w = TfToken("inputs:wrap_w", TfToken::Immortal);

  };
}
TfStaticData<QualifiedTokenCollection> QualifiedTokens;

namespace
{
  template<typename DataType>
  void CreateShaderInput(UsdShadeShader& shader, const TimeEvaluator<DataType>* timeEval, typename DataType::DataMemberId dataMemberId, 
    const TfToken& inputToken, const TfToken& qualifiedInputToken, const SdfValueTypeName& valueType)
  {
    if(!timeEval || timeEval->IsTimeVarying(dataMemberId))
      shader.CreateInput(inputToken, valueType);
    else
      shader.GetPrim().RemoveProperty(qualifiedInputToken);
  }

  template<typename ValueType, typename DataType>
  void SetShaderInput(UsdShadeShader& uniformShadPrim, UsdShadeShader& timeVarShadPrim, const TimeEvaluator<DataType>& timeEval, 
    const TfToken& inputToken, typename DataType::DataMemberId dataMemberId, ValueType value)
  {
    using DMI = typename DataType::DataMemberId;

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
    ClearUsdAttributes(uniformAttrib, timeVarAttrib, timeVaryingUpdate);

    // Set the input that requires an update
    if(timeVaryingUpdate)
      timeVarInput.Set(value, timeEval.Eval(dataMemberId));
    else
      uniformInput.Set(value, timeEval.Eval(dataMemberId));
  }

  void InitializePsShaderUniform(UsdShadeShader& shader)
  {
    shader.CreateIdAttr(VtValue(UsdBridgeTokens->UsdPreviewSurface));
    shader.CreateInput(UsdBridgeTokens->useSpecularWorkflow, SdfValueTypeNames->Int).Set(0);
  }

  void InitializePsShaderTimeVar(UsdShadeShader& shader, const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    CreateShaderInput(shader, timeEval, DMI::DIFFUSE, UsdBridgeTokens->diffuseColor, QualifiedTokens->diffuseColor, SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVECOLOR, UsdBridgeTokens->emissiveColor, QualifiedTokens->emissiveColor, SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::ROUGHNESS, UsdBridgeTokens->roughness, QualifiedTokens->roughness, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::OPACITY, UsdBridgeTokens->opacity, QualifiedTokens->opacity, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::METALLIC, UsdBridgeTokens->metallic, QualifiedTokens->metallic, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::IOR, UsdBridgeTokens->ior, QualifiedTokens->ior, SdfValueTypeNames->Float);
  }

  void InitializeMdlShaderUniform(UsdShadeShader& shader)
  {
    shader.CreateImplementationSourceAttr(VtValue(UsdBridgeTokens->sourceAsset));
    shader.SetSourceAsset(SdfAssetPath(constring::mdlShaderAssetName), UsdBridgeTokens->mdl);
    shader.SetSourceAssetSubIdentifier(UsdBridgeTokens->OmniPBR, UsdBridgeTokens->mdl);
  }

  void InitializeMdlShaderTimeVar(UsdShadeShader& shader, const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    CreateShaderInput(shader, timeEval, DMI::DIFFUSE, UsdBridgeTokens->diffuse_color_constant, QualifiedTokens->diffuse_color_constant, SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVECOLOR, UsdBridgeTokens->emissive_color, QualifiedTokens->emissive_color, SdfValueTypeNames->Color3f);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVEINTENSITY, UsdBridgeTokens->emissive_intensity, QualifiedTokens->emissive_intensity, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVEINTENSITY, UsdBridgeTokens->enable_emission, QualifiedTokens->enable_emission, SdfValueTypeNames->Bool);
    CreateShaderInput(shader, timeEval, DMI::ROUGHNESS, UsdBridgeTokens->reflection_roughness_constant, QualifiedTokens->reflection_roughness_constant, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::OPACITY, UsdBridgeTokens->opacity_constant, QualifiedTokens->opacity_constant, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::METALLIC, UsdBridgeTokens->metallic_constant, QualifiedTokens->metallic_constant, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::IOR, UsdBridgeTokens->ior_constant, QualifiedTokens->ior_constant, SdfValueTypeNames->Float);
  }

  UsdShadeOutput InitializePsAttributeReaderUniform(UsdShadeShader& attributeReader, const TfToken& readerId, const SdfValueTypeName& returnType)
  {
    attributeReader.CreateIdAttr(VtValue(readerId));
    return attributeReader.CreateOutput(UsdBridgeTokens->result, returnType);
  }

  template<typename DataType>
  void InitializePsAttributeReaderTimeVar(UsdShadeShader& attributeReader, typename DataType::DataMemberId dataMemberId, const TimeEvaluator<DataType>* timeEval)
  {
    CreateShaderInput(attributeReader, timeEval, dataMemberId, UsdBridgeTokens->varname, QualifiedTokens->varname, SdfValueTypeNames->Token);
  }

  UsdShadeOutput InitializeMdlAttributeReaderUniform(UsdShadeShader& attributeReader, const TfToken& readerId, const SdfValueTypeName& returnType)
  {
    attributeReader.CreateImplementationSourceAttr(VtValue(UsdBridgeTokens->sourceAsset));
    attributeReader.SetSourceAsset(SdfAssetPath(constring::mdlSupportAssetName), UsdBridgeTokens->mdl);
    attributeReader.SetSourceAssetSubIdentifier(readerId, UsdBridgeTokens->mdl);

    return attributeReader.CreateOutput(UsdBridgeTokens->out, returnType);
  }

  template<typename DataType>
  void InitializeMdlAttributeReaderTimeVar(UsdShadeShader& attributeReader, typename DataType::DataMemberId dataMemberId, const TimeEvaluator<DataType>* timeEval)
  {
    CreateShaderInput(attributeReader, timeEval, dataMemberId, UsdBridgeTokens->name, QualifiedTokens->name, SdfValueTypeNames->String);
  }

  void InitializePsSamplerUniform(UsdShadeShader& sampler, const SdfValueTypeName& coordType, const UsdShadeOutput& tcrOutput)
  {
    sampler.CreateIdAttr(VtValue(UsdBridgeTokens->UsdUVTexture));
    
    sampler.CreateOutput(UsdBridgeTokens->rgb, SdfValueTypeNames->Float3); // Input images with less components are automatically expanded, see usd docs
    sampler.CreateOutput(UsdBridgeTokens->a, SdfValueTypeNames->Float);

    sampler.CreateInput(UsdBridgeTokens->fallback, SdfValueTypeNames->Float4).Set(GfVec4f(1.0f, 0.0f, 0.0f, 1.0f));
    // Bind the texcoord reader's output to the sampler's st input
    sampler.CreateInput(UsdBridgeTokens->st, coordType).ConnectToSource(tcrOutput);
  }

  void InitializePsSamplerTimeVar(UsdShadeShader& sampler, UsdBridgeSamplerData::SamplerType type, const TimeEvaluator<UsdBridgeSamplerData>* timeEval = nullptr)
  {
    typedef UsdBridgeSamplerData::DataMemberId DMI;

    CreateShaderInput(sampler, timeEval, DMI::DATA, UsdBridgeTokens->file, QualifiedTokens->file, SdfValueTypeNames->Asset);
    CreateShaderInput(sampler, timeEval, DMI::WRAPS, UsdBridgeTokens->WrapS, QualifiedTokens->WrapS, SdfValueTypeNames->Token);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPT, UsdBridgeTokens->WrapT, QualifiedTokens->WrapT, SdfValueTypeNames->Token);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_3D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPR, UsdBridgeTokens->WrapR, QualifiedTokens->WrapR, SdfValueTypeNames->Token);
  }

  void InitializeMdlSamplerUniform(UsdShadeShader& sampler, const SdfValueTypeName& coordType, const UsdShadeOutput& tcrOutput)
  {
    sampler.CreateImplementationSourceAttr(VtValue(UsdBridgeTokens->sourceAsset));
    sampler.SetSourceAsset(SdfAssetPath(constring::mdlSupportAssetName), UsdBridgeTokens->mdl);
    sampler.SetSourceAssetSubIdentifier(UsdBridgeTokens->lookup_color, UsdBridgeTokens->mdl);
    
    sampler.CreateOutput(UsdBridgeTokens->out, SdfValueTypeNames->Color3f); // todo: Input images with alpha channel?

    // Bind the texcoord reader's output to the sampler's coord input
    sampler.CreateInput(UsdBridgeTokens->coord, coordType).ConnectToSource(tcrOutput);
  }

  void InitializeMdlSamplerTimeVar(UsdShadeShader& sampler, UsdBridgeSamplerData::SamplerType type, const TimeEvaluator<UsdBridgeSamplerData>* timeEval = nullptr)
  {
    typedef UsdBridgeSamplerData::DataMemberId DMI;

    CreateShaderInput(sampler, timeEval, DMI::DATA, UsdBridgeTokens->tex, QualifiedTokens->tex, SdfValueTypeNames->Asset);
    CreateShaderInput(sampler, timeEval, DMI::WRAPS, UsdBridgeTokens->wrap_u, QualifiedTokens->wrap_u, SdfValueTypeNames->Int);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPT, UsdBridgeTokens->wrap_v, QualifiedTokens->wrap_v, SdfValueTypeNames->Int);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_3D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPR, UsdBridgeTokens->wrap_w, QualifiedTokens->wrap_w, SdfValueTypeNames->Int);
  }
    
  UsdShadeOutput InitializePsShader(UsdStageRefPtr shaderStage, const SdfPath& shadPrimPath, bool uniformPrim
    , const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    // Create the shader
    UsdShadeShader shader = GetOrDefinePrim<UsdShadeShader>(shaderStage, shadPrimPath);
    assert(shader);

    if (uniformPrim)
    {
      InitializePsShaderUniform(shader);
    }

    InitializePsShaderTimeVar(shader, timeEval);

    if(uniformPrim)
      return shader.CreateOutput(UsdBridgeTokens->surface, SdfValueTypeNames->Token);
    else
      return UsdShadeOutput();
  }

  UsdShadeOutput InitializeMdlShader(UsdStageRefPtr shaderStage, const SdfPath& shadPrimPath, bool uniformPrim
    , const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    // Create the shader
    UsdShadeShader shader = GetOrDefinePrim<UsdShadeShader>(shaderStage, shadPrimPath);
    assert(shader);

    if (uniformPrim)
    {
      InitializeMdlShaderUniform(shader);
    }

    InitializeMdlShaderTimeVar(shader, timeEval);

    if(uniformPrim)
      return shader.CreateOutput(UsdBridgeTokens->out, SdfValueTypeNames->Token);
    else
      return UsdShadeOutput();
  }

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
      SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::psShaderPrimPf));
      UsdShadeOutput shaderOutput = InitializePsShader(materialStage, shaderPrimPath, uniformPrim, timeEval);

      if(uniformPrim)
        BindShaderToMaterial(matPrim, shaderOutput, nullptr);
    }

    if(settings.EnableMdlShader)
    {
      // Create an mdl shader
      SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlShaderPrimPf));
      UsdShadeOutput shaderOutput = InitializeMdlShader(materialStage, shaderPrimPath, uniformPrim, timeEval);

      if(uniformPrim)
        BindShaderToMaterial(matPrim, shaderOutput, &UsdBridgeTokens->mdl);
    }

    return matPrim;
  }

  UsdPrim InitializePsSampler_Impl(UsdStageRefPtr samplerStage, const SdfPath& samplerPrimPath, UsdBridgeSamplerData::SamplerType type, bool uniformPrim,
    const TimeEvaluator<UsdBridgeSamplerData>* timeEval = nullptr)
  {
    typedef UsdBridgeSamplerData::DataMemberId DMI;

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

      UsdShadeOutput tcrOutput = InitializePsAttributeReaderUniform(texCoordReader, idAttrib, valueType);

      InitializePsSamplerUniform(sampler, valueType, tcrOutput);
    }

    InitializeMdlAttributeReaderTimeVar(texCoordReader, DMI::INATTRIBUTE, timeEval);
    InitializePsSamplerTimeVar(sampler, type, timeEval);

    return sampler.GetPrim();
  }

  UsdPrim InitializeMdlSampler_Impl(UsdStageRefPtr samplerStage, const SdfPath& samplerPrimPath, UsdBridgeSamplerData::SamplerType type, bool uniformPrim,
    const TimeEvaluator<UsdBridgeSamplerData>* timeEval = nullptr)
  {
    typedef UsdBridgeSamplerData::DataMemberId DMI;

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
        idAttrib = UsdBridgeTokens->data_lookup_float;
        valueType = SdfValueTypeNames->Float;
      }
      else if (type == UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
      {
        idAttrib = UsdBridgeTokens->data_lookup_float2;
        valueType = SdfValueTypeNames->Float2;
      }
      else
      {
        idAttrib = UsdBridgeTokens->data_lookup_float3;
        valueType = SdfValueTypeNames->Float3;
      }

      UsdShadeOutput tcrOutput = InitializeMdlAttributeReaderUniform(texCoordReader, idAttrib, valueType);

      InitializeMdlSamplerUniform(sampler, valueType, tcrOutput);
    }

    InitializeMdlAttributeReaderTimeVar(texCoordReader, DMI::INATTRIBUTE, timeEval);
    InitializeMdlSamplerTimeVar(sampler, type, timeEval);

    return sampler.GetPrim();
  }

  void InitializeSampler_Impl(UsdStageRefPtr samplerStage, const SdfPath& samplerPrimPath, UsdBridgeSamplerData::SamplerType type, bool uniformPrim,
    const UsdBridgeSettings& settings, const TimeEvaluator<UsdBridgeSamplerData>* timeEval = nullptr)
  {
    if(settings.EnablePreviewSurfaceShader)
    {
      SdfPath usdSamplerPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::psSamplerPrimPf));
      InitializePsSampler_Impl(samplerStage, usdSamplerPrimPath, type, uniformPrim, timeEval);
    }

    if(settings.EnableMdlShader)
    {
      SdfPath usdSamplerPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::mdlSamplerPrimPf));
      InitializeMdlSampler_Impl(samplerStage, usdSamplerPrimPath, type, uniformPrim, timeEval);
    }
  }

  template<bool PreviewSurface>
  UsdShadeShader InitializeAttributeReader_Impl(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, bool uniformPrim,
    UsdBridgeMaterialData::DataMemberId dataMemberId, const TimeEvaluator<UsdBridgeMaterialData>* timeEval)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    // Create a vertexcolorreader
    SdfPath attributeReaderPath = matPrimPath.AppendPath(GetAttributeReaderPathPf<PreviewSurface>(dataMemberId));
    UsdShadeShader attributeReader = UsdShadeShader::Get(materialStage, attributeReaderPath);

    // Allow for uniform and timevar prims to return already existing prim without triggering re-initialization
    // manifest <==> timeEval, and will always take this branch
    if(!attributeReader || timeEval) 
    {
      if(!attributeReader)
        attributeReader = UsdShadeShader::Define(materialStage, attributeReaderPath);

      if(PreviewSurface)
      {
        if(uniformPrim) // Implies !timeEval, so initialize
        {
          InitializePsAttributeReaderUniform(attributeReader, GetUsdAttributeReaderId(dataMemberId), GetShaderNodeOutputType(dataMemberId));
        }

        // Create attribute reader varname, and if timeEval (manifest), can also remove the input
        InitializePsAttributeReaderTimeVar(attributeReader, dataMemberId, timeEval);
      }
      else
      {
        if(uniformPrim) // Implies !timeEval, so initialize
        {
          InitializeMdlAttributeReaderUniform(attributeReader, GetMdlAttributeReaderId(dataMemberId), GetShaderNodeOutputType(dataMemberId));
        }

        InitializeMdlAttributeReaderTimeVar(attributeReader, dataMemberId, timeEval);
      }
    }

    return attributeReader;
  }

  #define INITIALIZE_ATTRIBUTE_READER_MACRO(srcAttrib, dmi) \
    if(srcAttrib) InitializeAttributeReader_Impl<PreviewSurface>(materialStage, matPrimPath, false, dmi, timeEval);

  template<bool PreviewSurface>
  void InitializeAttributeReaderSet(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, const UsdBridgeSettings& settings,
    const UsdBridgeMaterialData& matData, const TimeEvaluator<UsdBridgeMaterialData>* timeEval)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;
    
    INITIALIZE_ATTRIBUTE_READER_MACRO(matData.Diffuse.SrcAttrib, DMI::DIFFUSE); \
    INITIALIZE_ATTRIBUTE_READER_MACRO(matData.Opacity.SrcAttrib, DMI::OPACITY); \
    INITIALIZE_ATTRIBUTE_READER_MACRO(matData.Emissive.SrcAttrib, DMI::EMISSIVECOLOR); \
    INITIALIZE_ATTRIBUTE_READER_MACRO(matData.EmissiveIntensity.SrcAttrib, DMI::EMISSIVEINTENSITY); \
    INITIALIZE_ATTRIBUTE_READER_MACRO(matData.Roughness.SrcAttrib, DMI::ROUGHNESS);
    INITIALIZE_ATTRIBUTE_READER_MACRO(matData.Metallic.SrcAttrib, DMI::METALLIC);
    INITIALIZE_ATTRIBUTE_READER_MACRO(matData.Ior.SrcAttrib, DMI::IOR);
  }

  void InitializeAttributeReaders_Impl(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, const UsdBridgeSettings& settings,
    const UsdBridgeMaterialData& matData, const TimeEvaluator<UsdBridgeMaterialData>* timeEval)
  {
    // So far, only used for manifest, so no uniform path required for initialization of attribute readers. 
    // Instead, uniform and timevar attrib readers are created on demand per-attribute in GetOrCreateAttributeReaders().

    if(settings.EnablePreviewSurfaceShader)
    {
      InitializeAttributeReaderSet<true>(materialStage, matPrimPath, settings, matData, timeEval);
    }

    if(settings.EnableMdlShader)
    {
      InitializeAttributeReaderSet<false>(materialStage, matPrimPath, settings, matData, timeEval);
    }
  }

  template<bool PreviewSurface>
  void GetOrCreateAttributeReaders(UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, UsdBridgeMaterialData::DataMemberId dataMemberId,
    UsdShadeShader& uniformReaderPrim, UsdShadeShader& timeVarReaderPrim)
  {
    uniformReaderPrim = InitializeAttributeReader_Impl<PreviewSurface>(sceneStage, matPrimPath, true, dataMemberId, nullptr);
    if(timeVarStage && (timeVarStage != sceneStage))
      timeVarReaderPrim = InitializeAttributeReader_Impl<PreviewSurface>(timeVarStage, matPrimPath, false, dataMemberId, nullptr);
  }

  template<bool PreviewSurface, typename DataType>
  void UpdateAttributeReaderName(UsdShadeShader& uniformReaderPrim, UsdShadeShader& timeVarReaderPrim, 
    const TimeEvaluator<DataType>& timeEval, typename DataType::DataMemberId dataMemberId, const TfToken& nameToken)
  {
    // Set the correct attribute token for the reader varname
    if(PreviewSurface)
    {
      SetShaderInput(uniformReaderPrim, timeVarReaderPrim, timeEval, UsdBridgeTokens->varname, dataMemberId, nameToken);
    }
    else
    {
      SetShaderInput(uniformReaderPrim, timeVarReaderPrim, timeEval, UsdBridgeTokens->name, dataMemberId, nameToken.GetString());
    }
  }

  template<bool PreviewSurface, typename InputValueType, typename ValueType>
  void UpdateShaderInput(UsdBridgeUsdWriter* writer, UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, 
    UsdShadeShader& uniformShadPrim, UsdShadeShader& timeVarShadPrim, const SdfPath& matPrimPath, const TimeEvaluator<UsdBridgeMaterialData>& timeEval,
    UsdBridgeMaterialData::DataMemberId dataMemberId, const UsdBridgeMaterialData::MaterialInput<InputValueType>& param, const ValueType& inputValue)
  {
    const TfToken& inputToken = GetMaterialShaderInputToken<PreviewSurface>(dataMemberId);
    UsdShadeInput uniformDiffInput = uniformShadPrim.GetInput(inputToken);

    if (param.SrcAttrib != nullptr)
    {
      bool isTimeVarying = timeEval.IsTimeVarying(dataMemberId);
      
      // Create attribute reader(s) to connect to the material shader input
      UsdShadeShader uniformReaderPrim, timeVarReaderPrim;
      GetOrCreateAttributeReaders<PreviewSurface>(sceneStage,
        isTimeVarying ? timeVarStage : sceneStage, // Skip creation of the timevar reader if it's not used anyway
        matPrimPath, dataMemberId, uniformReaderPrim, timeVarReaderPrim);
      assert(uniformReaderPrim);
      assert(!isTimeVarying || timeVarReaderPrim);

      UpdateAttributeReaderName<PreviewSurface>(uniformReaderPrim, timeVarReaderPrim, timeEval, dataMemberId, writer->AttributeNameToken(param.SrcAttrib));

      // Connect the reader to the material shader input
      timeVarShadPrim.GetInput(inputToken).GetAttr().Clear(); // Clear timevar data written earlier, will be replaced by connection
      
      UsdShadeOutput attribReaderOutput = uniformReaderPrim.GetOutput(PreviewSurface ? UsdBridgeTokens->result : UsdBridgeTokens->out);
      assert(attribReaderOutput);
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
  template<bool PreviewSurface, typename InputValueType>
  void UpdateShaderInput(UsdBridgeUsdWriter* writer, UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, 
    UsdShadeShader& uniformShadPrim, UsdShadeShader& timeVarShadPrim, const SdfPath& matPrimPath, const TimeEvaluator<UsdBridgeMaterialData>& timeEval,
    UsdBridgeMaterialData::DataMemberId dataMemberId, const UsdBridgeMaterialData::MaterialInput<InputValueType>& param)
  {
    UpdateShaderInput<PreviewSurface>(writer, sceneStage, timeVarStage, uniformShadPrim, timeVarShadPrim, matPrimPath, timeEval, 
      dataMemberId, param, param.Value);
  }

  template<bool PreviewSurface>
  void UpdateShaderInput_Sampler(UsdStageRefPtr sceneStage, UsdStageRefPtr materialStage, const SdfPath& shaderPrimPath, const SdfPath& samplerPrimPath, const UsdSamplerRefData& samplerRefData)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    // Referenced sampler prim
    UsdShadeShader refSampler = UsdShadeShader::Get(sceneStage, samplerPrimPath); // type inherited from sampler prim (in AddRef)
    assert(refSampler);

    UsdShadeShader uniformShad = UsdShadeShader::Get(sceneStage, shaderPrimPath);
    assert(uniformShad);
    UsdShadeShader timeVarShad = UsdShadeShader::Get(materialStage, shaderPrimPath);
    assert(timeVarShad);

    //Bind the sampler to the diffuse color of the uniform shader, so remove any existing values from the timeVar prim 
    DMI samplerDMI = samplerRefData.DataMemberId;
    const TfToken& inputToken = GetMaterialShaderInputToken<PreviewSurface>(samplerDMI);
    timeVarShad.GetInput(inputToken).GetAttr().Clear();

    // Connect refSampler output to uniformShad input
    const TfToken& outputToken = GetSamplerOutputColorToken<PreviewSurface>(samplerRefData.ImageNumComponents);
    UsdShadeOutput refSamplerOutput = refSampler.GetOutput(outputToken);
    assert(refSamplerOutput);

    uniformShad.GetInput(inputToken).ConnectToSource(refSamplerOutput);

    if(PreviewSurface)
    {
      // Specialcase opacity as fourth channel of diffuse
      bool affectsOpacity = (samplerDMI == DMI::DIFFUSE && samplerRefData.ImageNumComponents == 4);
      if(affectsOpacity)
      {
        const TfToken& opacityToken = GetMaterialShaderInputToken<PreviewSurface>(DMI::OPACITY);
        timeVarShad.GetInput(opacityToken).GetAttr().Clear();

        UsdShadeOutput refSamplerAlphaOutput = refSampler.GetOutput(UsdBridgeTokens->a);
        assert(refSamplerAlphaOutput);

        uniformShad.GetInput(opacityToken).ConnectToSource(refSamplerAlphaOutput);
      }
    }
    else
    {
      // No equivalent yet for fourth channel binding
    }
  }

  template<bool PreviewSurface>
  void UpdateSamplerInputs(UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, const SdfPath& samplerPrimPath, const UsdBridgeSamplerData& samplerData, 
    const char* imgFileName, const TfToken& attributeNameToken, const TimeEvaluator<UsdBridgeSamplerData>& timeEval)
  {
    typedef UsdBridgeSamplerData::DataMemberId DMI;
    
    // Collect the various prims
    SdfPath tcReaderPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::texCoordReaderPrimPf));

    UsdShadeShader uniformTcReaderPrim = UsdShadeShader::Get(sceneStage, tcReaderPrimPath);
    assert(uniformTcReaderPrim);

    UsdShadeShader timeVarTcReaderPrim = UsdShadeShader::Get(timeVarStage, tcReaderPrimPath);
    assert(timeVarTcReaderPrim);

    UsdShadeShader uniformSamplerPrim = UsdShadeShader::Get(sceneStage, samplerPrimPath);
    assert(uniformSamplerPrim);

    UsdShadeShader timeVarSamplerPrim = UsdShadeShader::Get(timeVarStage, samplerPrimPath);
    assert(timeVarSamplerPrim);

    SdfAssetPath texFile(imgFileName);

    if(PreviewSurface)
    {
      // Set all the inputs
      SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->file, DMI::DATA, texFile);
      SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->WrapS, DMI::WRAPS, TextureWrapToken(samplerData.WrapS));
      if((uint32_t)samplerData.Type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
        SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->WrapT, DMI::WRAPT, TextureWrapToken(samplerData.WrapT));
      if((uint32_t)samplerData.Type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_3D)
        SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->WrapR, DMI::WRAPR, TextureWrapToken(samplerData.WrapR));

      // Check whether the output type is still correct
      // (Experimental: assume the output type doesn't change over time, this just gives it a chance to match the image type)
      size_t numComponents = samplerData.ImageNumComponents;
      const TfToken& outputToken = GetSamplerOutputColorToken<true>(numComponents);
      if(!uniformSamplerPrim.GetOutput(outputToken))
      {
        // As the previous output type isn't cached, just remove everything:
        uniformSamplerPrim.GetPrim().RemoveProperty(UsdBridgeTokens->r);
        uniformSamplerPrim.GetPrim().RemoveProperty(UsdBridgeTokens->rg);
        uniformSamplerPrim.GetPrim().RemoveProperty(UsdBridgeTokens->rgb);

        uniformSamplerPrim.CreateOutput(outputToken, GetSamplerOutputColorType(numComponents));
      } 
    }
    else
    {
      // Set all the inputs
      SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->tex, DMI::DATA, texFile);
      SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->wrap_u, DMI::WRAPS, TextureWrapInt(samplerData.WrapS));
      if((uint32_t)samplerData.Type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
        SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->wrap_v, DMI::WRAPT, TextureWrapInt(samplerData.WrapT));
      if((uint32_t)samplerData.Type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_3D)
        SetShaderInput(uniformSamplerPrim, timeVarSamplerPrim, timeEval, UsdBridgeTokens->wrap_w, DMI::WRAPR, TextureWrapInt(samplerData.WrapR)); 
    }

    UpdateAttributeReaderName<PreviewSurface>(uniformTcReaderPrim, timeVarTcReaderPrim, timeEval, DMI::INATTRIBUTE, attributeNameToken);
  }
}

UsdShadeMaterial UsdBridgeUsdWriter::InitializeUsdMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, bool uniformPrim) const
{
  return InitializeUsdMaterial_Impl(materialStage, matPrimPath, uniformPrim, Settings);
}

void UsdBridgeUsdWriter::InitializeUsdSampler(UsdStageRefPtr samplerStage,const SdfPath& samplerPrimPath, UsdBridgeSamplerData::SamplerType type, bool uniformPrim) const
{
  InitializeSampler_Impl(samplerStage, samplerPrimPath, type, uniformPrim, Settings);
}

#ifdef VALUE_CLIP_RETIMING
void UsdBridgeUsdWriter::UpdateUsdMaterialManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeMaterialData& matData)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData);
  InitializeUsdMaterial_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, false, 
    Settings, &timeEval);

  InitializeAttributeReaders_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, 
    Settings, matData, &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}

void UsdBridgeUsdWriter::UpdateUsdSamplerManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeSamplerData& samplerData)
{
  TimeEvaluator<UsdBridgeSamplerData> timeEval(samplerData);
  
  InitializeSampler_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, samplerData.Type,
    false, Settings, &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}
#endif

void UsdBridgeUsdWriter::ConnectSamplerToMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, const SdfPath& refSamplerPrimPath, 
  const std::string& samplerPrimName, const UsdSamplerRefData& samplerRefData, double worldTimeStep)
{
  // Essentially, this is an extension of UpdateShaderInput() for the case of param.Sampler
  if(Settings.EnablePreviewSurfaceShader)
  {
    // Get shader prims
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::psShaderPrimPf));
    SdfPath samplerPrimPath = refSamplerPrimPath.AppendPath(SdfPath(constring::psSamplerPrimPf));

    UpdateShaderInput_Sampler<true>(this->SceneStage, materialStage, shaderPrimPath, samplerPrimPath, samplerRefData);
  }

  if(Settings.EnableMdlShader)
  {
    // Get shader prims
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlShaderPrimPf));
    SdfPath samplerPrimPath = refSamplerPrimPath.AppendPath(SdfPath(constring::mdlSamplerPrimPf));

    UpdateShaderInput_Sampler<true>(this->SceneStage, materialStage, shaderPrimPath, samplerPrimPath, samplerRefData);
  }
}

void UsdBridgeUsdWriter::UpdateUsdMaterial(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, const UsdBridgeMaterialData& matData, double timeStep)
{
  // Update usd shader
  if(Settings.EnablePreviewSurfaceShader)
  {
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::psShaderPrimPf));
    this->UpdatePsShader(timeVarStage, matPrimPath, shaderPrimPath, matData, timeStep);
  }

  if(Settings.EnableMdlShader)
  {
    // Update mdl shader
    SdfPath mdlShaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlShaderPrimPf));
    this->UpdateMdlShader(timeVarStage, matPrimPath, mdlShaderPrimPath, matData, timeStep);
  }
}

#define UPDATE_USD_SHADER_INPUT_MACRO(...) \
  UpdateShaderInput<true>(this, SceneStage, timeVarStage, uniformShadPrim, timeVarShadPrim, matPrimPath, timeEval, __VA_ARGS__)

void UsdBridgeUsdWriter::UpdatePsShader(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, const SdfPath& shadPrimPath, const UsdBridgeMaterialData& matData, double timeStep)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData, timeStep);
  typedef UsdBridgeMaterialData::DataMemberId DMI;

  UsdShadeShader uniformShadPrim = UsdShadeShader::Get(SceneStage, shadPrimPath);
  assert(uniformShadPrim);

  UsdShadeShader timeVarShadPrim = UsdShadeShader::Get(timeVarStage, shadPrimPath);
  assert(timeVarShadPrim);

  GfVec3f difColor(GetValuePtr(matData.Diffuse));
  GfVec3f emColor(GetValuePtr(matData.Emissive));
  emColor *= matData.EmissiveIntensity.Value; // This multiplication won't translate to vcr/sampler usage

  //uniformShadPrim.GetInput(UsdBridgeTokens->useSpecularWorkflow).Set(
  //  (matData.Metallic.Value >= 0.0 || matData.Metallic.SrcAttrib || matData.Metallic.Sampler) ? 0 : 1);

  UPDATE_USD_SHADER_INPUT_MACRO(DMI::DIFFUSE, matData.Diffuse, difColor);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::EMISSIVECOLOR, matData.Emissive, emColor);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::ROUGHNESS, matData.Roughness);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::OPACITY, matData.Opacity);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::METALLIC, matData.Metallic);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::IOR, matData.Ior);
}

#define UPDATE_MDL_SHADER_INPUT_MACRO(...) \
  UpdateShaderInput<false>(this, SceneStage, timeVarStage, uniformShadPrim, timeVarShadPrim, matPrimPath, timeEval, __VA_ARGS__)

void UsdBridgeUsdWriter::UpdateMdlShader(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, const SdfPath& shadPrimPath, const UsdBridgeMaterialData& matData, double timeStep)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData, timeStep);
  typedef UsdBridgeMaterialData::DataMemberId DMI;

  UsdShadeShader uniformShadPrim = UsdShadeShader::Get(SceneStage, shadPrimPath);
  assert(uniformShadPrim);

  UsdShadeShader timeVarShadPrim = UsdShadeShader::Get(timeVarStage, shadPrimPath);
  assert(timeVarShadPrim);

  GfVec3f difColor(GetValuePtr(matData.Diffuse));
  GfVec3f emColor(GetValuePtr(matData.Emissive));

  bool enableEmission = (matData.EmissiveIntensity.Value >= 0.0 || matData.EmissiveIntensity.SrcAttrib || matData.EmissiveIntensity.Sampler);

  // Only set values on either timevar or uniform prim
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::DIFFUSE, matData.Diffuse, difColor);
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::EMISSIVECOLOR, matData.Emissive, emColor);
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::EMISSIVEINTENSITY, matData.EmissiveIntensity);
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::OPACITY, matData.Opacity);
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::ROUGHNESS, matData.Roughness);
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::METALLIC, matData.Metallic);
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::IOR, matData.Ior);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->enable_emission, DMI::EMISSIVEINTENSITY, enableEmission); // Just a value, not connected to attribreaders

#ifdef CUSTOM_PBR_MDL
  if (!matData.HasTranslucency)
    uniformShadPrim.SetSourceAsset(this->MdlOpaqueRelFilePath, UsdBridgeTokens->mdl);
  else
    uniformShadPrim.SetSourceAsset(this->MdlTranslucentRelFilePath, UsdBridgeTokens->mdl);
#endif
}

void UsdBridgeUsdWriter::UpdateUsdSampler(UsdStageRefPtr timeVarStage, const SdfPath& samplerPrimPath, const UsdBridgeSamplerData& samplerData, double timeStep, UsdBridgePrimCache* cacheEntry)
{
  TimeEvaluator<UsdBridgeSamplerData> timeEval(samplerData, timeStep);
  typedef UsdBridgeSamplerData::DataMemberId DMI;

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

  const TfToken& attribNameToken = AttributeNameToken(samplerData.InAttribute);

  if(Settings.EnablePreviewSurfaceShader)
  {
    SdfPath usdSamplerPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::psSamplerPrimPf));
    UpdateSamplerInputs<true>(SceneStage, timeVarStage, usdSamplerPrimPath, samplerData, imgFileName, attribNameToken, timeEval);
  }

  if(Settings.EnableMdlShader)
  {
    SdfPath usdSamplerPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::mdlSamplerPrimPf));
    UpdateSamplerInputs<false>(SceneStage, timeVarStage, usdSamplerPrimPath, samplerData, imgFileName, attribNameToken, timeEval);
  }

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

namespace
{
  template<bool PreviewSurface>
  void UpdateAttributeReader(UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, 
    UsdBridgeMaterialData::DataMemberId dataMemberId, const TfToken& newNameToken, const TimeEvaluator<UsdBridgeMaterialData>& timeEval, UsdBridgeUsdWriter* writer)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    if(PreviewSurface && dataMemberId == DMI::EMISSIVEINTENSITY)
      return; // Emissive intensity is not represented as a shader input in the USD preview surface model

    // Collect the various prims
    SdfPath attributeReaderPath = matPrimPath.AppendPath(GetAttributeReaderPathPf<PreviewSurface>(dataMemberId));

    UsdShadeShader uniformAttribReader = UsdShadeShader::Get(sceneStage, attributeReaderPath);
    if(!uniformAttribReader)
    {
      UsdBridgeLogMacro(writer, UsdBridgeLogLevel::ERR, "In UsdBridgeUsdWriter::UpdateAttributeReader(): requesting an attribute reader that hasn't been created during fixup of name token.");
      return;
    }

    UsdShadeShader timeVarAttribReader;
    if(timeEval.IsTimeVarying(dataMemberId))
    {
      timeVarAttribReader = UsdShadeShader::Get(timeVarStage, attributeReaderPath);
      assert(timeVarAttribReader);
    }

    UpdateAttributeReaderName<PreviewSurface>(uniformAttribReader, timeVarAttribReader, timeEval, dataMemberId, newNameToken);
  }

  template<bool PreviewSurface>
  void UpdateSamplerTcReader(UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, const SdfPath& samplerPrimPath, const TfToken& newNameToken, const TimeEvaluator<UsdBridgeSamplerData>& timeEval)
  {
    typedef UsdBridgeSamplerData::DataMemberId DMI;

    // Collect the various prims
    SdfPath tcReaderPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::texCoordReaderPrimPf));

    UsdShadeShader uniformTcReaderPrim = UsdShadeShader::Get(sceneStage, tcReaderPrimPath);
    assert(uniformTcReaderPrim);

    UsdShadeShader timeVarTcReaderPrim = UsdShadeShader::Get(timeVarStage, tcReaderPrimPath);
    assert(timeVarTcReaderPrim);

    // Set the new Inattribute
    UpdateAttributeReaderName<PreviewSurface>(uniformTcReaderPrim, timeVarTcReaderPrim, timeEval, DMI::INATTRIBUTE, newNameToken);
  }
}

void UsdBridgeUsdWriter::UpdateAttributeReaders(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, MaterialDMI dataMemberId, const char* newName, double timeStep, MaterialDMI timeVarying)
{
  // Time eval with dummy data
  UsdBridgeMaterialData materialData;
  materialData.TimeVarying = timeVarying;

  TimeEvaluator<UsdBridgeMaterialData> timeEval(materialData, timeStep);

  const TfToken& newNameToken = AttributeNameToken(newName);

  if(Settings.EnablePreviewSurfaceShader)
  {
    UpdateAttributeReader<true>(SceneStage, timeVarStage, matPrimPath, dataMemberId, newNameToken, timeEval, this);
  }

  if(Settings.EnableMdlShader)
  {
    UpdateAttributeReader<false>(SceneStage, timeVarStage, matPrimPath, dataMemberId, newNameToken, timeEval, this);
  }
}

void UsdBridgeUsdWriter::UpdateInAttribute(UsdStageRefPtr timeVarStage, const SdfPath& samplerPrimPath, const char* newName, double timeStep, SamplerDMI timeVarying)
{
  // Time eval with dummy data
  UsdBridgeSamplerData samplerData;
  samplerData.TimeVarying = timeVarying;

  TimeEvaluator<UsdBridgeSamplerData> timeEval(samplerData, timeStep);

  const TfToken& newNameToken = AttributeNameToken(newName);

  if(Settings.EnablePreviewSurfaceShader)
  {
    SdfPath usdSamplerPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::psSamplerPrimPf));
    UpdateSamplerTcReader<true>(SceneStage, timeVarStage, usdSamplerPrimPath, newNameToken, timeEval);
  }

  if(Settings.EnableMdlShader)
  {
    SdfPath usdSamplerPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::mdlSamplerPrimPf));
    UpdateSamplerTcReader<false>(SceneStage, timeVarStage, usdSamplerPrimPath, newNameToken, timeEval);
  }
}

