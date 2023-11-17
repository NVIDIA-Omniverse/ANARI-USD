// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeUsdWriter.h"

#include "UsdBridgeUsdWriter_Common.h"
#include "stb_image_write.h"

#include <limits>

_TF_TOKENS_STRUCT_NAME(QualifiedInputTokens)::_TF_TOKENS_STRUCT_NAME(QualifiedInputTokens)()
  : roughness(TfToken("inputs:roughness", TfToken::Immortal))
  , opacity(TfToken("inputs:opacity", TfToken::Immortal))
  , metallic(TfToken("inputs:metallic", TfToken::Immortal))
  , ior(TfToken("inputs:ior", TfToken::Immortal))
  , diffuseColor(TfToken("inputs:diffuseColor", TfToken::Immortal))
  , specularColor(TfToken("inputs:specularColor", TfToken::Immortal))
  , emissiveColor(TfToken("inputs:emissiveColor", TfToken::Immortal))
  , file(TfToken("inputs:file", TfToken::Immortal))
  , WrapS(TfToken("inputs:WrapS", TfToken::Immortal))
  , WrapT(TfToken("inputs:WrapT", TfToken::Immortal))
  , WrapR(TfToken("inputs:WrapR", TfToken::Immortal))
  , varname(TfToken("inputs:varname", TfToken::Immortal))

  , reflection_roughness_constant(TfToken("inputs:reflection_roughness_constant", TfToken::Immortal))
  , opacity_constant(TfToken("inputs:opacity_constant", TfToken::Immortal))
  , metallic_constant(TfToken("inputs:metallic_constant", TfToken::Immortal))
  , ior_constant(TfToken("inputs:ior_constant"))
  , diffuse_color_constant(TfToken("inputs:diffuse_color_constant", TfToken::Immortal))
  , emissive_color(TfToken("inputs:emissive_color", TfToken::Immortal))
  , emissive_intensity(TfToken("inputs:emissive_intensity", TfToken::Immortal))
  , enable_emission(TfToken("inputs:enable_emission", TfToken::Immortal))
  , name(TfToken("inputs:name", TfToken::Immortal))
  , tex(TfToken("inputs:tex", TfToken::Immortal))
  , wrap_u(TfToken("inputs:wrap_u", TfToken::Immortal))
  , wrap_v(TfToken("inputs:wrap_v", TfToken::Immortal))
  , wrap_w(TfToken("inputs:wrap_w", TfToken::Immortal))
{}
_TF_TOKENS_STRUCT_NAME(QualifiedInputTokens)::~_TF_TOKENS_STRUCT_NAME(QualifiedInputTokens)() = default;
TfStaticData<_TF_TOKENS_STRUCT_NAME(QualifiedInputTokens)> QualifiedInputTokens;

_TF_TOKENS_STRUCT_NAME(QualifiedOutputTokens)::_TF_TOKENS_STRUCT_NAME(QualifiedOutputTokens)()
  : r(TfToken("outputs:r", TfToken::Immortal))
  , rg(TfToken("outputs:rg", TfToken::Immortal))
  , rgb(TfToken("outputs:rgb", TfToken::Immortal))
  , a(TfToken("outputs:a", TfToken::Immortal))

  , out(TfToken("outputs:out", TfToken::Immortal))
{}
_TF_TOKENS_STRUCT_NAME(QualifiedOutputTokens)::~_TF_TOKENS_STRUCT_NAME(QualifiedOutputTokens)() = default;
TfStaticData<_TF_TOKENS_STRUCT_NAME(QualifiedOutputTokens)> QualifiedOutputTokens;

namespace
{
  struct StbWriteOutput
  {
    StbWriteOutput() = default;
    ~StbWriteOutput()
    {
      delete[] imageData;
    }

    char* imageData = nullptr;
    size_t imageSize = 0;
  };

  void StbWriteToBuffer(void *context, void *data, int size)
  {
    if(data)
    {
      StbWriteOutput* output = reinterpret_cast<StbWriteOutput*>(context);
      output->imageData = new char[size];
      output->imageSize = size;

      memcpy(output->imageData, data, size);
    }
  }

  template<typename CType>
  void ConvertSamplerData_Inner(const UsdBridgeSamplerData& samplerData, double normFactor, std::vector<unsigned char>& imageData)
  {
    int numComponents = samplerData.ImageNumComponents;
    uint64_t imageDimX = samplerData.ImageDims[0];
    uint64_t imageDimY = samplerData.ImageDims[1];
    int64_t yStride = samplerData.ImageStride[1];
    const char* samplerDataPtr = reinterpret_cast<const char*>(samplerData.Data);

    int64_t baseAddr = 0;
    for(uint64_t pY = 0; pY < imageDimY; ++pY, baseAddr += yStride)
    {
      const CType* lineAddr = reinterpret_cast<const CType*>(samplerDataPtr + baseAddr);
      for(uint64_t flatX = 0; flatX < imageDimX*numComponents; ++flatX) //flattened X index
      {
        uint64_t dstElt = numComponents*pY*imageDimX + flatX;

        double result = *(lineAddr+flatX)*normFactor;
        result = (result < 0.0) ? 0.0 : ((result > 255.0) ? 255.0 : result);

        imageData[dstElt] = static_cast<unsigned char>(result);
      }
    }
  }

  void ConvertSamplerDataToImage(const UsdBridgeSamplerData& samplerData, std::vector<unsigned char>& imageData)
  {
    UsdBridgeType flattenedType = ubutils::UsdBridgeTypeFlatten(samplerData.DataType);
    int numComponents = samplerData.ImageNumComponents;
    uint64_t imageDimX = samplerData.ImageDims[0];
    uint64_t imageDimY = samplerData.ImageDims[1];

    bool normalizedFp = (flattenedType == UsdBridgeType::FLOAT) || (flattenedType == UsdBridgeType::DOUBLE);
    bool fixedPoint = (flattenedType == UsdBridgeType::USHORT) || (flattenedType == UsdBridgeType::UINT);

    if(normalizedFp || fixedPoint)
    {
      imageData.resize(numComponents*imageDimX*imageDimY);

      switch(flattenedType)
      {
        case UsdBridgeType::FLOAT: ConvertSamplerData_Inner<float>(samplerData, 255.0, imageData); break;
        case UsdBridgeType::DOUBLE: ConvertSamplerData_Inner<double>(samplerData, 255.0, imageData); break;
        case UsdBridgeType::USHORT: ConvertSamplerData_Inner<unsigned short>(samplerData,
          255.0 / static_cast<double>(std::numeric_limits<unsigned short>::max()), imageData); break;
        case UsdBridgeType::UINT: ConvertSamplerData_Inner<unsigned int>(samplerData,
          255.0 / static_cast<double>(std::numeric_limits<unsigned int>::max()), imageData); break;
        default: break;
      }
    }
  }

  template<typename DataType>
  void CreateShaderInput(UsdShadeShader& shader, const TimeEvaluator<DataType>* timeEval, typename DataType::DataMemberId dataMemberId,
    const TfToken& inputToken, const TfToken& qualifiedInputToken, const SdfValueTypeName& valueType)
  {
    if(!timeEval || timeEval->IsTimeVarying(dataMemberId))
      shader.CreateInput(inputToken, valueType);
    else
      shader.GetPrim().RemoveProperty(qualifiedInputToken);
  }

  template<bool PreviewSurface>
  void CreateMaterialShaderInput(UsdShadeShader& shader, const TimeEvaluator<UsdBridgeMaterialData>* timeEval,
    typename UsdBridgeMaterialData::DataMemberId dataMemberId, const SdfValueTypeName& valueType)
  {
    const TfToken& inputToken = GetMaterialShaderInputToken<PreviewSurface>(dataMemberId);
    const TfToken& qualifiedInputToken = GetMaterialShaderInputQualifiedToken<PreviewSurface>(dataMemberId);
    CreateShaderInput(shader, timeEval, dataMemberId, inputToken, qualifiedInputToken, valueType);
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
      assert(timeVarInput);
      timeVarAttrib = timeVarInput.GetAttr();
    }
    if(uniformShadPrim)
    {
      uniformInput = uniformShadPrim.GetInput(inputToken);
      assert(uniformInput);
      uniformAttrib = uniformInput.GetAttr();
    }

    // Clear the attributes that are not set (based on timeVaryingUpdate)
    ClearUsdAttributes(uniformAttrib, timeVarAttrib, timeVaryingUpdate);

    // Set the input that requires an update
    SET_TIMEVARYING_ATTRIB(timeVaryingUpdate, timeVarInput, uniformInput, value);
  }

  UsdShadeOutput InitializeMdlGraphNode(UsdShadeShader& graphNode, const TfToken& assetSubIdent, const SdfValueTypeName& returnType,
    const char* sourceAssetPath = constring::mdlSupportAssetName)
  {
    graphNode.CreateImplementationSourceAttr(VtValue(UsdBridgeTokens->sourceAsset));
    graphNode.SetSourceAsset(SdfAssetPath(sourceAssetPath), UsdBridgeTokens->mdl);
    graphNode.SetSourceAssetSubIdentifier(assetSubIdent, UsdBridgeTokens->mdl);
    return graphNode.CreateOutput(UsdBridgeTokens->out, returnType);
  }

  struct ShadeGraphTypeConversionNodeContext
  {
    ShadeGraphTypeConversionNodeContext(UsdStageRefPtr sceneStage,
      const SdfPath& matPrimPath,
      const char* connectionIdentifier)
      : SceneStage(sceneStage)
      , MatPrimPath(matPrimPath)
      , ConnectionIdentifier(connectionIdentifier)
    {
    }

    // sourceOutput is set to the new node's output upon return
    UsdShadeShader CreateAndConnectMdlTypeConversionNode(UsdShadeOutput& sourceOutput, const char* primNamePf, const TfToken& assetSubIdent,
      const SdfValueTypeName& inType, const SdfValueTypeName& outType, const char* sourceAssetPath = constring::mdlSupportAssetName) const
    {
      SdfPath nodePrimPath = this->MatPrimPath.AppendPath(SdfPath(std::string(this->ConnectionIdentifier)+primNamePf));
      UsdShadeShader conversionNode = UsdShadeShader::Get(this->SceneStage, nodePrimPath);
      UsdShadeOutput nodeOut;
      if(!conversionNode)
      {
        conversionNode = UsdShadeShader::Define(this->SceneStage, nodePrimPath);
        assert(conversionNode);
        nodeOut = InitializeMdlGraphNode(conversionNode, assetSubIdent, outType, sourceAssetPath);
        conversionNode.CreateInput(UsdBridgeTokens->a, inType);
      }
      else
        nodeOut = conversionNode.GetOutput(UsdBridgeTokens->out);

      conversionNode.GetInput(UsdBridgeTokens->a).ConnectToSource(sourceOutput);
      sourceOutput = nodeOut;

      return conversionNode;
    }

    void InsertMdlShaderTypeConversionNodes(const UsdShadeInput& shadeInput, UsdShadeOutput& nodeOutput) const
    {
      if(shadeInput.GetTypeName() == SdfValueTypeNames->Color3f)
      {
        // Introduce new nodes until the type matches color3f.
        // Otherwise, just connect any random output type to the input.
        if(nodeOutput.GetTypeName() == SdfValueTypeNames->Float4)
        {
          // Changes nodeOutput to the output of the new node
          CreateAndConnectMdlTypeConversionNode(nodeOutput, constring::mdlGraphXYZPrimPf,
            UsdBridgeTokens->xyz, SdfValueTypeNames->Float4, SdfValueTypeNames->Float3, constring::mdlAuxAssetName);
        }
        if(nodeOutput.GetTypeName() == SdfValueTypeNames->Float3)
        {
          CreateAndConnectMdlTypeConversionNode(nodeOutput, constring::mdlGraphColorPrimPf,
            UsdBridgeTokens->construct_color, SdfValueTypeNames->Float3, SdfValueTypeNames->Color3f, constring::mdlAuxAssetName);
        }
      }
      if(shadeInput.GetTypeName() == SdfValueTypeNames->Float)
      {
        const char* componentPrimPf = constring::mdlGraphWPrimPf;
        TfToken componentAssetIdent = (nodeOutput.GetTypeName() == SdfValueTypeNames->Float4) ? UsdBridgeTokens->w : UsdBridgeTokens->x;
        switch(ChannelSelector)
        {
          case 0: componentPrimPf = constring::mdlGraphXPrimPf; componentAssetIdent = UsdBridgeTokens->x; break;
          case 1: componentPrimPf = constring::mdlGraphYPrimPf; componentAssetIdent = UsdBridgeTokens->y; break;
          case 2: componentPrimPf = constring::mdlGraphZPrimPf; componentAssetIdent = UsdBridgeTokens->z; break;
          default: break;
        }

        if(nodeOutput.GetTypeName() == SdfValueTypeNames->Float4)
        {
          CreateAndConnectMdlTypeConversionNode(nodeOutput, componentPrimPf,
            componentAssetIdent, SdfValueTypeNames->Float4, SdfValueTypeNames->Float, constring::mdlAuxAssetName);
        }
        else if(nodeOutput.GetTypeName() == SdfValueTypeNames->Float3)
        {
          CreateAndConnectMdlTypeConversionNode(nodeOutput, componentPrimPf,
            componentAssetIdent, SdfValueTypeNames->Float3, SdfValueTypeNames->Float, constring::mdlAuxAssetName);
        }
      }
    }

    UsdStageRefPtr SceneStage;
    const SdfPath& MatPrimPath;
    const char* ConnectionIdentifier = nullptr;

    // Optional
    int ChannelSelector = 0;
  };

  void InitializePsShaderUniform(UsdShadeShader& shader)
  {
    shader.CreateIdAttr(VtValue(UsdBridgeTokens->UsdPreviewSurface));
    shader.CreateInput(UsdBridgeTokens->useSpecularWorkflow, SdfValueTypeNames->Int).Set(0);
  }

  void InitializePsShaderTimeVar(UsdShadeShader& shader, const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    CreateMaterialShaderInput<true>(shader, timeEval, DMI::DIFFUSE, SdfValueTypeNames->Color3f);
    CreateMaterialShaderInput<true>(shader, timeEval, DMI::EMISSIVECOLOR, SdfValueTypeNames->Color3f);
    CreateMaterialShaderInput<true>(shader, timeEval, DMI::ROUGHNESS, SdfValueTypeNames->Float);
    CreateMaterialShaderInput<true>(shader, timeEval, DMI::OPACITY, SdfValueTypeNames->Float);
    CreateMaterialShaderInput<true>(shader, timeEval, DMI::METALLIC, SdfValueTypeNames->Float);
    CreateMaterialShaderInput<true>(shader, timeEval, DMI::IOR, SdfValueTypeNames->Float);

    CreateShaderInput(shader, timeEval, DMI::OPACITY, UsdBridgeTokens->opacityThreshold, QualifiedInputTokens->opacityThreshold, SdfValueTypeNames->Float);
  }

  void InitializeMdlShaderUniform(UsdShadeShader& shader)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    shader.CreateImplementationSourceAttr(VtValue(UsdBridgeTokens->sourceAsset));
    shader.SetSourceAsset(SdfAssetPath(constring::mdlShaderAssetName), UsdBridgeTokens->mdl);
    shader.SetSourceAssetSubIdentifier(UsdBridgeTokens->OmniPBR, UsdBridgeTokens->mdl);

    shader.CreateInput(GetMaterialShaderInputToken<false>(DMI::OPACITY), SdfValueTypeNames->Float); // Opacity is handled by the opacitymul node
  }

  void InitializeMdlShaderTimeVar(UsdShadeShader& shader, const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    CreateMaterialShaderInput<false>(shader, timeEval, DMI::DIFFUSE, SdfValueTypeNames->Color3f);
    CreateMaterialShaderInput<false>(shader, timeEval, DMI::EMISSIVECOLOR, SdfValueTypeNames->Color3f);
    CreateMaterialShaderInput<false>(shader, timeEval, DMI::EMISSIVEINTENSITY, SdfValueTypeNames->Float);
    CreateMaterialShaderInput<false>(shader, timeEval, DMI::ROUGHNESS, SdfValueTypeNames->Float);
    CreateMaterialShaderInput<false>(shader, timeEval, DMI::METALLIC, SdfValueTypeNames->Float);
    //CreateMaterialShaderInput<false>(shader, timeEval, DMI::IOR, SdfValueTypeNames->Float); // not supported in OmniPBR.mdl

    CreateShaderInput(shader, timeEval, DMI::OPACITY, UsdBridgeTokens->enable_opacity, QualifiedInputTokens->enable_opacity, SdfValueTypeNames->Bool);
    CreateShaderInput(shader, timeEval, DMI::OPACITY, UsdBridgeTokens->opacity_threshold, QualifiedInputTokens->opacity_threshold, SdfValueTypeNames->Float);
    CreateShaderInput(shader, timeEval, DMI::EMISSIVEINTENSITY, UsdBridgeTokens->enable_emission, QualifiedInputTokens->enable_emission, SdfValueTypeNames->Bool);
  }

  UsdShadeOutput InitializePsAttributeReaderUniform(UsdShadeShader& attributeReader, const TfToken& readerId, const SdfValueTypeName& returnType)
  {
    attributeReader.CreateIdAttr(VtValue(readerId));

    // Input name and output type are tightly coupled; output type cannot be timevarying, so neither can the name
    attributeReader.CreateInput(UsdBridgeTokens->varname, SdfValueTypeNames->Token);
    return attributeReader.CreateOutput(UsdBridgeTokens->result, returnType);
  }

  template<typename DataType>
  void InitializePsAttributeReaderTimeVar(UsdShadeShader& attributeReader, typename DataType::DataMemberId dataMemberId, const TimeEvaluator<DataType>* timeEval)
  {
    //CreateShaderInput(attributeReader, timeEval, dataMemberId, UsdBridgeTokens->varname, QualifiedInputTokens->varname, SdfValueTypeNames->Token);
  }

  UsdShadeOutput InitializeMdlAttributeReaderUniform(UsdShadeShader& attributeReader, const TfToken& readerId, const SdfValueTypeName& returnType)
  {
    attributeReader.CreateImplementationSourceAttr(VtValue(UsdBridgeTokens->sourceAsset));
    attributeReader.SetSourceAsset(SdfAssetPath(constring::mdlSupportAssetName), UsdBridgeTokens->mdl);
    attributeReader.SetSourceAssetSubIdentifier(readerId, UsdBridgeTokens->mdl);

    // Input name and output type are tightly coupled; output type cannot be timevarying, so neither can the name
    attributeReader.CreateInput(UsdBridgeTokens->name, SdfValueTypeNames->String);
    return attributeReader.CreateOutput(UsdBridgeTokens->out, returnType);
  }

  template<typename DataType>
  void InitializeMdlAttributeReaderTimeVar(UsdShadeShader& attributeReader, typename DataType::DataMemberId dataMemberId, const TimeEvaluator<DataType>* timeEval)
  {
    //CreateShaderInput(attributeReader, timeEval, dataMemberId, UsdBridgeTokens->name, QualifiedInputTokens->name, SdfValueTypeNames->String);
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

    CreateShaderInput(sampler, timeEval, DMI::DATA, UsdBridgeTokens->file, QualifiedInputTokens->file, SdfValueTypeNames->Asset);
    CreateShaderInput(sampler, timeEval, DMI::WRAPS, UsdBridgeTokens->WrapS, QualifiedInputTokens->WrapS, SdfValueTypeNames->Token);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPT, UsdBridgeTokens->WrapT, QualifiedInputTokens->WrapT, SdfValueTypeNames->Token);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_3D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPR, UsdBridgeTokens->WrapR, QualifiedInputTokens->WrapR, SdfValueTypeNames->Token);
  }

  void InitializeMdlSamplerUniform(UsdShadeShader& sampler, const SdfValueTypeName& coordType, const UsdShadeOutput& tcrOutput)
  {
    sampler.CreateImplementationSourceAttr(VtValue(UsdBridgeTokens->sourceAsset));
    sampler.SetSourceAsset(SdfAssetPath(constring::mdlSupportAssetName), UsdBridgeTokens->mdl);
    sampler.SetSourceAssetSubIdentifier(UsdBridgeTokens->lookup_float4, UsdBridgeTokens->mdl);

    sampler.CreateOutput(UsdBridgeTokens->out, SdfValueTypeNames->Float4);

    // Bind the texcoord reader's output to the sampler's coord input
    sampler.CreateInput(UsdBridgeTokens->coord, coordType).ConnectToSource(tcrOutput);
  }

  void InitializeMdlSamplerTimeVar(UsdShadeShader& sampler, UsdBridgeSamplerData::SamplerType type, const TimeEvaluator<UsdBridgeSamplerData>* timeEval = nullptr)
  {
    typedef UsdBridgeSamplerData::DataMemberId DMI;

    CreateShaderInput(sampler, timeEval, DMI::DATA, UsdBridgeTokens->tex, QualifiedInputTokens->tex, SdfValueTypeNames->Asset);
    CreateShaderInput(sampler, timeEval, DMI::WRAPS, UsdBridgeTokens->wrap_u, QualifiedInputTokens->wrap_u, SdfValueTypeNames->Int);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_2D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPT, UsdBridgeTokens->wrap_v, QualifiedInputTokens->wrap_v, SdfValueTypeNames->Int);
    if((uint32_t)type >= (uint32_t)UsdBridgeSamplerData::SamplerType::SAMPLER_3D)
      CreateShaderInput(sampler, timeEval, DMI::WRAPR, UsdBridgeTokens->wrap_w, QualifiedInputTokens->wrap_w, SdfValueTypeNames->Int);
  }

  UsdShadeOutput InitializePsShader(UsdStageRefPtr shaderStage, const SdfPath& matPrimPath, bool uniformPrim
    , const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    // Create the shader
    SdfPath shadPrimPath = matPrimPath.AppendPath(SdfPath(constring::psShaderPrimPf));
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

  UsdShadeOutput InitializeMdlShader(UsdStageRefPtr shaderStage, const SdfPath& matPrimPath, bool uniformPrim
    , const TimeEvaluator<UsdBridgeMaterialData>* timeEval = nullptr)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    // Create the shader
    SdfPath shadPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlShaderPrimPf));
    UsdShadeShader shader = GetOrDefinePrim<UsdShadeShader>(shaderStage, shadPrimPath);
    assert(shader);

    // Create a mul shader node (for connection to opacity)
    SdfPath shadPrimPath_opmul = matPrimPath.AppendPath(SdfPath(constring::mdlOpacityMulPrimPf));
    UsdShadeShader opacityMul = GetOrDefinePrim<UsdShadeShader>(shaderStage, shadPrimPath_opmul);
    assert(opacityMul);
    UsdShadeOutput opacityMulOut;

    if(uniformPrim)
    {
      opacityMulOut = InitializeMdlGraphNode(opacityMul, UsdBridgeTokens->mul_float, SdfValueTypeNames->Float);
      opacityMul.CreateInput(UsdBridgeTokens->a, SdfValueTypeNames->Float); // Input a is either connected (to sampler/vc opacity) or 1.0, so never timevarying

      InitializeMdlShaderUniform(shader);
    }

    CreateShaderInput(opacityMul, timeEval, DMI::OPACITY, UsdBridgeTokens->b, QualifiedInputTokens->b, SdfValueTypeNames->Float);
    InitializeMdlShaderTimeVar(shader, timeEval);

    // Connect the opacity mul node to the shader
    if(uniformPrim)
    {
      shader.GetInput(GetMaterialShaderInputToken<false>(DMI::OPACITY)).ConnectToSource(opacityMulOut);
    }

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
      UsdShadeOutput shaderOutput = InitializePsShader(materialStage, matPrimPath, uniformPrim, timeEval);

      if(uniformPrim)
        BindShaderToMaterial(matPrim, shaderOutput, nullptr);
    }

    if(settings.EnableMdlShader)
    {
      // Create an mdl shader
      UsdShadeOutput shaderOutput = InitializeMdlShader(materialStage, matPrimPath, uniformPrim, timeEval);

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

    if(uniformPrim)
    {
      SdfPath texCoordReaderPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::texCoordReaderPrimPf));
      UsdShadeShader texCoordReader = UsdShadeShader::Define(samplerStage, texCoordReaderPrimPath);
      assert(texCoordReader);

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

    //InitializePsAttributeReaderTimeVar(texCoordReader, DMI::INATTRIBUTE, timeEval); // timevar attribute reader disabled
    InitializePsSamplerTimeVar(sampler, type, timeEval);

    return sampler.GetPrim();
  }

  UsdPrim InitializeMdlSampler_Impl(UsdStageRefPtr samplerStage, const SdfPath& samplerPrimPath, UsdBridgeSamplerData::SamplerType type, bool uniformPrim,
    const TimeEvaluator<UsdBridgeSamplerData>* timeEval = nullptr)
  {
    typedef UsdBridgeSamplerData::DataMemberId DMI;

    UsdShadeShader sampler = GetOrDefinePrim<UsdShadeShader>(samplerStage, samplerPrimPath);
    assert(sampler);

    if(uniformPrim)
    {
      SdfPath texCoordReaderPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::texCoordReaderPrimPf));
      UsdShadeShader texCoordReader = UsdShadeShader::Define(samplerStage, texCoordReaderPrimPath);
      assert(texCoordReader);

      // USD does not yet allow for anything but 2D coords, but let's try anyway
      TfToken idAttrib;
      SdfValueTypeName valueType;
      if(type == UsdBridgeSamplerData::SamplerType::SAMPLER_1D)
      {
        idAttrib = UsdBridgeTokens->data_lookup_float2; // Currently, there is no 1D lookup
        valueType = SdfValueTypeNames->Float2;
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

    //InitializeMdlAttributeReaderTimeVar(texCoordReader, DMI::INATTRIBUTE, timeEval); // timevar attribute reader disabled
    InitializeMdlSamplerTimeVar(sampler, type, timeEval);

    if(uniformPrim)
    {
      sampler.GetInput(UsdBridgeTokens->tex).GetAttr().SetMetadata(UsdBridgeTokens->colorSpace, VtValue("sRGB"));
    }

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

    if(!uniformPrim) // timevar attribute reader disabled
      return UsdShadeShader();

    // Create a vertexcolorreader
    SdfPath attributeReaderPath = matPrimPath.AppendPath(GetAttributeReaderPathPf<PreviewSurface>(dataMemberId));
    UsdShadeShader attributeReader = UsdShadeShader::Get(materialStage, attributeReaderPath);

    // Allow for uniform and timevar prims to return already existing prim without triggering re-initialization
    // manifest <==> timeEval, and will always take this branch
    if(!attributeReader || timeEval)
    {
      if(!attributeReader) // Currently no need for timevar properties on attribute readers
        attributeReader = UsdShadeShader::Define(materialStage, attributeReaderPath);

      if(PreviewSurface)
      {
        if(uniformPrim) // Implies !timeEval, so initialize
        {
          InitializePsAttributeReaderUniform(attributeReader, GetPsAttributeReaderId(dataMemberId), GetAttributeOutputType(dataMemberId));
        }

        // Create attribute reader varname, and if timeEval (manifest), can also remove the input
        //InitializePsAttributeReaderTimeVar(attributeReader, dataMemberId, timeEval);
      }
      else
      {
        if(uniformPrim) // Implies !timeEval, so initialize
        {
          InitializeMdlAttributeReaderUniform(attributeReader, GetMdlAttributeReaderSubId(dataMemberId), GetAttributeOutputType(dataMemberId));
        }

        //InitializeMdlAttributeReaderTimeVar(attributeReader, dataMemberId, timeEval);
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
    UsdShadeShader& uniformReaderPrim)
  {
    uniformReaderPrim = InitializeAttributeReader_Impl<PreviewSurface>(sceneStage, matPrimPath, true, dataMemberId, nullptr);
    //if(timeVarStage && (timeVarStage != sceneStage))
    //  timeVarReaderPrim = InitializeAttributeReader_Impl<PreviewSurface>(timeVarStage, matPrimPath, false, dataMemberId, nullptr);
  }

  template<bool PreviewSurface>
  void UpdateAttributeReaderName(UsdShadeShader& uniformReaderPrim, const TfToken& nameToken)
  {
    // Set the correct attribute token for the reader varname
    if(PreviewSurface)
    {
      uniformReaderPrim.GetInput(UsdBridgeTokens->varname).Set(nameToken);
    }
    else
    {
      uniformReaderPrim.GetInput(UsdBridgeTokens->name).Set(nameToken.GetString());
    }
  }

  template<bool PreviewSurface>
  void UpdateAttributeReaderOutput(UsdShadeShader& uniformReaderPrim, const UsdGeomPrimvarsAPI& boundGeomPrimvars, const TfToken& attribNameToken)
  {
    const TfToken& outAttribToken = PreviewSurface ? UsdBridgeTokens->result : UsdBridgeTokens->out;
    UsdGeomPrimvar geomPrimvar = boundGeomPrimvars.GetPrimvar(attribNameToken);
    UsdShadeOutput readerOutput = uniformReaderPrim.GetOutput(outAttribToken);

    if(geomPrimvar)
    {
      const SdfValueTypeName& geomPrimTypeName = geomPrimvar.GetTypeName();
      if(geomPrimTypeName != readerOutput.GetTypeName())
      {
        boundGeomPrimvars.GetPrim().RemoveProperty(PreviewSurface ? QualifiedOutputTokens->result : QualifiedOutputTokens->out);
        uniformReaderPrim.CreateOutput(outAttribToken, geomPrimTypeName);
        // Also change the asset subidentifier based on the outAttribToken
        uniformReaderPrim.SetSourceAssetSubIdentifier(GetMdlAttributeReaderSubId(geomPrimTypeName), UsdBridgeTokens->mdl);
      }
    }
  }

  template<bool PreviewSurface>
  void UpdateShaderInput_ShadeNode( const UsdShadeShader& uniformShaderIn, const UsdShadeShader& timeVarShaderIn, const TfToken& inputToken,
    const UsdShadeShader& shadeNodeOut, const TfToken& outputToken,
    const ShadeGraphTypeConversionNodeContext& conversionContext)
  {
    //Bind the shade node to the input of the uniform shader, so remove any existing values from the timeVar prim
    UsdShadeInput timeVarInput = timeVarShaderIn.GetInput(inputToken);
    if(timeVarInput)
      timeVarInput.GetAttr().Clear();

    // Connect shadeNode output to uniformShad input
    UsdShadeOutput nodeOutput = shadeNodeOut.GetOutput(outputToken);
    assert(nodeOutput);

    UsdShadeInput shadeInput = uniformShaderIn.GetInput(inputToken);

    if(!PreviewSurface)
    {
      conversionContext.InsertMdlShaderTypeConversionNodes(shadeInput, nodeOutput);
    }

    uniformShaderIn.GetInput(inputToken).ConnectToSource(nodeOutput);
  }

  template<bool PreviewSurface>
  void UpdateShaderInputColorOpacity_Constant(UsdStageRefPtr sceneStage, const SdfPath& matPrimPath)
  {
    if(!PreviewSurface)
    {
      SdfPath opMulPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlOpacityMulPrimPf));
      UsdShadeShader uniformOpMul = UsdShadeShader::Get(sceneStage, opMulPrimPath);

      UsdShadeInput uniformOpMulInput = uniformOpMul.GetInput(UsdBridgeTokens->a);
      assert(uniformOpMulInput);
      uniformOpMulInput.Set(1.0f);
    }
  }

  template<bool PreviewSurface>
  void UpdateShaderInputColorOpacity_AttributeReader(UsdStageRefPtr materialStage,
    const UsdShadeShader& outputNode, const ShadeGraphTypeConversionNodeContext& conversionContext)
  {
    // No PreviewSurface implementation to connect a 4 component attrib reader to a 1 component opacity input.
    if(!PreviewSurface)
    {
      SdfPath opMulPrimPath = conversionContext.MatPrimPath.AppendPath(SdfPath(constring::mdlOpacityMulPrimPf));
      UsdShadeShader uniformOpMul = UsdShadeShader::Get(conversionContext.SceneStage, opMulPrimPath);
      UsdShadeShader timeVarOpMul = UsdShadeShader::Get(materialStage, opMulPrimPath);

      // Input 'a' of the multiply node the target for connecting opacity to; see InitializeMdlShader
      TfToken opacityInputToken = UsdBridgeTokens->a;
      TfToken opacityOutputToken = UsdBridgeTokens->out;

      UpdateShaderInput_ShadeNode<false>(uniformOpMul, timeVarOpMul, opacityInputToken, outputNode, opacityOutputToken, conversionContext);
    }
  }

  template<bool PreviewSurface, typename InputValueType, typename ValueType>
  void UpdateShaderInput(UsdBridgeUsdWriter* writer, UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage,
    UsdShadeShader& uniformShadPrim, UsdShadeShader& timeVarShadPrim, const SdfPath& matPrimPath, const UsdGeomPrimvarsAPI& boundGeomPrimvars,
    const TimeEvaluator<UsdBridgeMaterialData>& timeEval, UsdBridgeMaterialData::DataMemberId dataMemberId,
    const UsdBridgeMaterialData::MaterialInput<InputValueType>& param, const TfToken& inputToken, const ValueType& inputValue)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    // Handle the case where a geometry attribute reader is connected
    if (param.SrcAttrib != nullptr)
    {
      //bool isTimeVarying = timeEval.IsTimeVarying(dataMemberId);

      // Create attribute reader(s) to connect to the material shader input
      UsdShadeShader uniformReaderPrim;
      GetOrCreateAttributeReaders<PreviewSurface>(sceneStage,
        sceneStage, matPrimPath, dataMemberId, uniformReaderPrim);
      assert(uniformReaderPrim);

      const TfToken& attribNameToken = writer->AttributeNameToken(param.SrcAttrib);
      UpdateAttributeReaderName<PreviewSurface>(uniformReaderPrim, writer->AttributeNameToken(param.SrcAttrib));
      UpdateAttributeReaderOutput<PreviewSurface>(uniformReaderPrim, boundGeomPrimvars, attribNameToken);

      // Connect the reader to the material shader input
      ShadeGraphTypeConversionNodeContext conversionContext(
        sceneStage, matPrimPath, GetAttributeReaderPathPf<PreviewSurface>(dataMemberId).GetString().c_str());

      const TfToken& outputToken = PreviewSurface ? UsdBridgeTokens->result : UsdBridgeTokens->out;
      UpdateShaderInput_ShadeNode<PreviewSurface>(uniformShadPrim, timeVarShadPrim, inputToken, uniformReaderPrim, outputToken, conversionContext);

      // Diffuse attrib also has to connect to the opacitymul 'a' input. The color primvar (and therefore attrib reader out) is always a float4.
      if(dataMemberId == DMI::DIFFUSE)
      {
        conversionContext.ConnectionIdentifier = PreviewSurface ? "" : constring::mdlDiffuseOpacityPrimPf;
        conversionContext.ChannelSelector = 3; // Select the w channel from the float4 input

        UpdateShaderInputColorOpacity_AttributeReader<PreviewSurface>(timeVarStage, uniformReaderPrim, conversionContext);
      }
    }
    else if(!param.Sampler)
    {
      // Handle the case for normal direct values
      UsdShadeInput uniformDiffInput = uniformShadPrim.GetInput(inputToken);
      if(uniformDiffInput.HasConnectedSource())
        uniformDiffInput.DisconnectSource();

      // Just treat like regular time-varying inputs
      SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, inputToken, dataMemberId, inputValue);

      // The diffuse input owns the opacitymul's 'a' input.
      if(dataMemberId == DMI::DIFFUSE)
        UpdateShaderInputColorOpacity_Constant<PreviewSurface>(sceneStage, matPrimPath);
    }
  }

  // Variation for standard material shader input tokens
  template<bool PreviewSurface, typename InputValueType, typename ValueType>
  void UpdateShaderInput(UsdBridgeUsdWriter* writer, UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage,
    UsdShadeShader& uniformShadPrim, UsdShadeShader& timeVarShadPrim, const SdfPath& matPrimPath, const UsdGeomPrimvarsAPI& boundGeomPrimvars, const TimeEvaluator<UsdBridgeMaterialData>& timeEval,
    UsdBridgeMaterialData::DataMemberId dataMemberId, const UsdBridgeMaterialData::MaterialInput<InputValueType>& param, const ValueType& inputValue)
  {
    const TfToken& inputToken = GetMaterialShaderInputToken<PreviewSurface>(dataMemberId);
    UpdateShaderInput<PreviewSurface>(writer, sceneStage, timeVarStage, uniformShadPrim, timeVarShadPrim, matPrimPath, boundGeomPrimvars, timeEval,
      dataMemberId, param, inputToken, inputValue);
  }

  // Convenience for when param.Value is simply the intended value to be set
  template<bool PreviewSurface, typename InputValueType>
  void UpdateShaderInput(UsdBridgeUsdWriter* writer, UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage,
    UsdShadeShader& uniformShadPrim, UsdShadeShader& timeVarShadPrim, const SdfPath& matPrimPath, const UsdGeomPrimvarsAPI& boundGeomPrimvars, const TimeEvaluator<UsdBridgeMaterialData>& timeEval,
    UsdBridgeMaterialData::DataMemberId dataMemberId, const UsdBridgeMaterialData::MaterialInput<InputValueType>& param)
  {
    const TfToken& inputToken = GetMaterialShaderInputToken<PreviewSurface>(dataMemberId);
    UpdateShaderInput<PreviewSurface>(writer, sceneStage, timeVarStage, uniformShadPrim, timeVarShadPrim, matPrimPath, boundGeomPrimvars, timeEval,
      dataMemberId, param, inputToken, param.Value);
  }

  template<bool PreviewSurface>
  void UpdateShaderInput_Sampler(const UsdShadeShader& uniformShader, const UsdShadeShader& timeVarShader,
    const UsdShadeShader& refSampler, const UsdSamplerRefData& samplerRefData, const ShadeGraphTypeConversionNodeContext& conversionContext)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;
    DMI samplerDMI = samplerRefData.DataMemberId;

    const TfToken& inputToken = GetMaterialShaderInputToken<PreviewSurface>(samplerDMI);
    const TfToken& outputToken = GetSamplerOutputColorToken<PreviewSurface>(samplerRefData.ImageNumComponents);
    UpdateShaderInput_ShadeNode<PreviewSurface>(uniformShader, timeVarShader, inputToken, refSampler, outputToken, conversionContext);
  }

  template<bool PreviewSurface>
  void UpdateShaderInputColorOpacity_Sampler(const UsdShadeShader& uniformShader, const UsdShadeShader& timeVarShader,
    const UsdShadeShader& refSampler, ShadeGraphTypeConversionNodeContext& conversionContext)
  {
    using DMI = UsdBridgeMaterialData::DataMemberId;

    TfToken opacityInputToken;
    TfToken opacityOutputToken;
    if(PreviewSurface)
    {
      opacityInputToken = GetMaterialShaderInputToken<PreviewSurface>(DMI::OPACITY);
      opacityOutputToken = UsdBridgeTokens->a;
    }
    else
    {
      // Input 'a' of the multiply node the target for connecting opacity to; see InitializeMdlShader
      opacityInputToken = UsdBridgeTokens->a;
      opacityOutputToken = UsdBridgeTokens->out;
    }

    UpdateShaderInput_ShadeNode<PreviewSurface>(uniformShader, timeVarShader, opacityInputToken, refSampler, opacityOutputToken, conversionContext);
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
      int numComponents = samplerData.ImageNumComponents;
      const TfToken& outputToken = GetSamplerOutputColorToken<true>(numComponents);
      if(!uniformSamplerPrim.GetOutput(outputToken))
      {
        // As the previous output type isn't cached, just remove everything:
        uniformSamplerPrim.GetPrim().RemoveProperty(QualifiedOutputTokens->r);
        uniformSamplerPrim.GetPrim().RemoveProperty(QualifiedOutputTokens->rg);
        uniformSamplerPrim.GetPrim().RemoveProperty(QualifiedOutputTokens->rgb);

        uniformSamplerPrim.CreateOutput(outputToken, GetSamplerOutputColorType<true>(numComponents));
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

      // Check whether the output type is still correct
      int numComponents = samplerData.ImageNumComponents;
      const TfToken& assetIdent = GetSamplerAssetSubId(numComponents);
      const SdfValueTypeName& outputType = GetSamplerOutputColorType<false>(numComponents);
      if(uniformSamplerPrim.GetOutput(UsdBridgeTokens->out).GetTypeName() != outputType)
      {
        uniformSamplerPrim.SetSourceAssetSubIdentifier(assetIdent, UsdBridgeTokens->mdl);
        uniformSamplerPrim.GetPrim().RemoveProperty(QualifiedOutputTokens->out);
        uniformSamplerPrim.CreateOutput(UsdBridgeTokens->out, outputType);
      }
    }

    UpdateAttributeReaderName<PreviewSurface>(uniformTcReaderPrim, attributeNameToken);
  }

  void GetMaterialCoreShader(UsdStageRefPtr sceneStage, UsdStageRefPtr materialStage, const SdfPath& shaderPrimPath,
    UsdShadeShader& uniformShader, UsdShadeShader& timeVarShader)
  {
    uniformShader = UsdShadeShader::Get(sceneStage, shaderPrimPath);
    assert(uniformShader);
    timeVarShader = UsdShadeShader::Get(materialStage, shaderPrimPath);
    assert(timeVarShader);
  }

  template<bool PreviewSurface>
  void GetRefSamplerPrim(UsdStageRefPtr sceneStage, const SdfPath& refSamplerPrimPath, UsdShadeShader& refSampler)
  {
    SdfPath refSamplerPrimPath_Child = refSamplerPrimPath.AppendPath(SdfPath(PreviewSurface ? constring::psSamplerPrimPf : constring::mdlSamplerPrimPf)); // type inherited from sampler prim (in AddRef)
    refSampler = UsdShadeShader::Get(sceneStage, refSamplerPrimPath_Child);
    assert(refSampler);
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

#ifdef USE_INDEX_MATERIALS
namespace
{
  UsdShadeOutput InitializeIndexShaderUniform(UsdStageRefPtr volumeStage, UsdShadeShader& indexShader, UsdPrim& colorMapPrim)
  {
    UsdShadeConnectableAPI colorMapConn(colorMapPrim);
    UsdShadeOutput colorMapShadOut = colorMapConn.CreateOutput(UsdBridgeTokens->colormap, SdfValueTypeNames->Token);

    colorMapPrim.CreateAttribute(UsdBridgeTokens->domainBoundaryMode , SdfValueTypeNames->Token).Set(UsdBridgeTokens->clampToEdge);
    colorMapPrim.CreateAttribute(UsdBridgeTokens->colormapSource , SdfValueTypeNames->Token).Set(UsdBridgeTokens->colormapValues);

    indexShader.CreateInput(UsdBridgeTokens->colormap, SdfValueTypeNames->Token).ConnectToSource(colorMapShadOut);
    return indexShader.CreateOutput(UsdBridgeTokens->volume, SdfValueTypeNames->Token);
  }

  void InitializeIndexShaderTimeVar(UsdPrim& colorMapPrim, const TimeEvaluator<UsdBridgeVolumeData>* timeEval = nullptr)
  {
    typedef UsdBridgeVolumeData::DataMemberId DMI;

    // Value range
    CREATE_REMOVE_TIMEVARYING_ATTRIB(colorMapPrim, DMI::TFVALUERANGE, UsdBridgeTokens->domain, SdfValueTypeNames->Float2);

    // Colors and opacities are grouped into the same attrib
    CREATE_REMOVE_TIMEVARYING_ATTRIB(colorMapPrim, (DMI::TFCOLORS | DMI::TFOPACITIES), UsdBridgeTokens->colormapValues, SdfValueTypeNames->Float4Array);
  }
}

UsdShadeMaterial UsdBridgeUsdWriter::InitializeIndexVolumeMaterial_Impl(UsdStageRefPtr volumeStage,
  const SdfPath& volumePath, bool uniformPrim, const TimeEvaluator<UsdBridgeVolumeData>* timeEval) const
{
  SdfPath indexMatPath = volumePath.AppendPath(SdfPath(constring::indexMaterialPf));
  SdfPath indexShadPath = indexMatPath.AppendPath(SdfPath(constring::indexShaderPf));
  SdfPath colorMapPath = indexMatPath.AppendPath(SdfPath(constring::indexColorMapPf));

  UsdShadeMaterial indexMaterial = GetOrDefinePrim<UsdShadeMaterial>(volumeStage, indexMatPath);
  assert(indexMaterial);

  UsdPrim colorMapPrim = volumeStage->GetPrimAtPath(colorMapPath);
  if(!colorMapPrim)
    colorMapPrim = volumeStage->DefinePrim(colorMapPath, UsdBridgeTokens->Colormap);
  assert(colorMapPrim);

  if(uniformPrim)
  {
    UsdShadeShader indexShader = GetOrDefinePrim<UsdShadeShader>(volumeStage, indexShadPath);
    assert(indexShader);

    UsdShadeOutput indexShaderOutput = InitializeIndexShaderUniform(volumeStage, indexShader, colorMapPrim);
    indexMaterial.CreateVolumeOutput(UsdBridgeTokens->nvindex).ConnectToSource(indexShaderOutput);
  }

  InitializeIndexShaderTimeVar(colorMapPrim, timeEval);

  return indexMaterial;
}
#endif

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

void UsdBridgeUsdWriter::ConnectSamplersToMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, const SdfPrimPathList& refSamplerPrimPaths,
    const UsdBridgePrimCacheList& samplerCacheEntries, const UsdSamplerRefData* samplerRefDatas, size_t numSamplers, double worldTimeStep)
{
  typedef UsdBridgeMaterialData::DataMemberId DMI;

  // Essentially, this is an extension of UpdateShaderInput() for the case of param.Sampler
  if(Settings.EnablePreviewSurfaceShader)
  {
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::psShaderPrimPf));

    UsdShadeShader uniformShad, timeVarShad;
    GetMaterialCoreShader(this->SceneStage, materialStage, shaderPrimPath,
      uniformShad, timeVarShad);

    for(int i = 0; i < numSamplers; ++i)
    {
      const char* samplerPrimName = samplerCacheEntries[i]->Name.GetString().c_str();

      UsdShadeShader refSampler;
      GetRefSamplerPrim<true>(this->SceneStage, refSamplerPrimPaths[i], refSampler);

      ShadeGraphTypeConversionNodeContext conversionContext(
          this->SceneStage, matPrimPath, samplerPrimName);

      UpdateShaderInput_Sampler<true>(uniformShad, timeVarShad, refSampler, samplerRefDatas[i], conversionContext);

      bool affectsOpacity = samplerRefDatas[i].DataMemberId == DMI::DIFFUSE && samplerRefDatas[i].ImageNumComponents == 4;
      if(affectsOpacity)
        UpdateShaderInputColorOpacity_Sampler<true>(uniformShad, timeVarShad, refSampler, conversionContext);
    }
  }

  if(Settings.EnableMdlShader)
  {
    // Get shader prims
    SdfPath shaderPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlShaderPrimPf));
    SdfPath opMulPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlOpacityMulPrimPf));

    UsdShadeShader uniformShad, timeVarShad;
    GetMaterialCoreShader(this->SceneStage, materialStage, shaderPrimPath,
      uniformShad, timeVarShad);

    UsdShadeShader uniformOpMul = UsdShadeShader::Get(this->SceneStage, opMulPrimPath);
    UsdShadeShader timeVarOpMul = UsdShadeShader::Get(materialStage, opMulPrimPath);

    for(int i = 0; i < numSamplers; ++i)
    {
      const char* samplerPrimName = samplerCacheEntries[i]->Name.GetString().c_str();

      UsdShadeShader refSampler;
      GetRefSamplerPrim<false>(this->SceneStage, refSamplerPrimPaths[i], refSampler);

      ShadeGraphTypeConversionNodeContext conversionContext(
        this->SceneStage, matPrimPath, samplerPrimName);

      if(samplerRefDatas[i].DataMemberId != DMI::OPACITY)
      {
        UpdateShaderInput_Sampler<false>(uniformShad, timeVarShad, refSampler, samplerRefDatas[i], conversionContext);

        bool affectsOpacity = samplerRefDatas[i].DataMemberId == DMI::DIFFUSE;
        if(affectsOpacity)
        {
          if(samplerRefDatas[i].ImageNumComponents == 4)
          {
            conversionContext.ConnectionIdentifier = constring::mdlDiffuseOpacityPrimPf;
            conversionContext.ChannelSelector = 3; // Select the w channel from the float4 input

            // Connect diffuse sampler to the opacity mul node instead of the main shader; see InitializeMdlShader
            UpdateShaderInputColorOpacity_Sampler<false>(uniformOpMul, timeVarOpMul, refSampler, conversionContext);
          }
          else
            UpdateShaderInputColorOpacity_Constant<false>(this->SceneStage, matPrimPath); // Since a sampler was bound, the constant is not set during UpdateShaderInput()
        }
      }
      else
      {
        // Connect opacity sampler to the 'b' input of opacity mul node, similar to UpdateMdlShader (for attribute readers)
        UpdateShaderInput_ShadeNode<false>(uniformOpMul, timeVarOpMul, UsdBridgeTokens->b, refSampler, UsdBridgeTokens->out, conversionContext);
      }
    }
  }
}

void UsdBridgeUsdWriter::UpdateUsdMaterial(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, const UsdBridgeMaterialData& matData, const UsdGeomPrimvarsAPI& boundGeomPrimvars, double timeStep)
{
  // Update usd shader
  if(Settings.EnablePreviewSurfaceShader)
  {
    this->UpdatePsShader(timeVarStage, matPrimPath, matData, boundGeomPrimvars, timeStep);
  }

  if(Settings.EnableMdlShader)
  {
    // Update mdl shader
    this->UpdateMdlShader(timeVarStage, matPrimPath, matData, boundGeomPrimvars, timeStep);
  }
}

#define UPDATE_USD_SHADER_INPUT_MACRO(...) \
  UpdateShaderInput<true>(this, SceneStage, timeVarStage, uniformShadPrim, timeVarShadPrim, matPrimPath, boundGeomPrimvars, timeEval, __VA_ARGS__)

void UsdBridgeUsdWriter::UpdatePsShader(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, const UsdBridgeMaterialData& matData, const UsdGeomPrimvarsAPI& boundGeomPrimvars, double timeStep)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData, timeStep);
  typedef UsdBridgeMaterialData::DataMemberId DMI;

  SdfPath shadPrimPath = matPrimPath.AppendPath(SdfPath(constring::psShaderPrimPf));

  UsdShadeShader uniformShadPrim = UsdShadeShader::Get(SceneStage, shadPrimPath);
  assert(uniformShadPrim);

  UsdShadeShader timeVarShadPrim = UsdShadeShader::Get(timeVarStage, shadPrimPath);
  assert(timeVarShadPrim);

  GfVec3f difColor(GetValuePtr(matData.Diffuse));
  GfVec3f emColor(GetValuePtr(matData.Emissive));
  emColor *= matData.EmissiveIntensity.Value; // This multiplication won't translate to vcr/sampler usage

  //uniformShadPrim.GetInput(UsdBridgeTokens->useSpecularWorkflow).Set(
  //  (matData.Metallic.Value > 0.0 || matData.Metallic.SrcAttrib || matData.Metallic.Sampler) ? 0 : 1);

  UPDATE_USD_SHADER_INPUT_MACRO(DMI::DIFFUSE, matData.Diffuse, difColor);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::EMISSIVECOLOR, matData.Emissive, emColor);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::ROUGHNESS, matData.Roughness);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::OPACITY, matData.Opacity);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::METALLIC, matData.Metallic);
  UPDATE_USD_SHADER_INPUT_MACRO(DMI::IOR, matData.Ior);

  // Just a value, not connected to attribreaders or samplers
  float opacityCutoffValue = (matData.AlphaMode == UsdBridgeMaterialData::AlphaModes::MASK) ? matData.AlphaCutoff : 0.0f; // don't enable cutoff when not explicitly specified
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->opacityThreshold, DMI::OPACITY, opacityCutoffValue);
}

#define UPDATE_MDL_SHADER_INPUT_MACRO(...) \
  UpdateShaderInput<false>(this, SceneStage, timeVarStage, uniformShadPrim, timeVarShadPrim, matPrimPath, boundGeomPrimvars, timeEval, __VA_ARGS__)

void UsdBridgeUsdWriter::UpdateMdlShader(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, const UsdBridgeMaterialData& matData, const UsdGeomPrimvarsAPI& boundGeomPrimvars, double timeStep)
{
  TimeEvaluator<UsdBridgeMaterialData> timeEval(matData, timeStep);
  typedef UsdBridgeMaterialData::DataMemberId DMI;

  SdfPath shadPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlShaderPrimPf));

  UsdShadeShader uniformShadPrim = UsdShadeShader::Get(SceneStage, shadPrimPath);
  assert(uniformShadPrim);
  UsdShadeShader timeVarShadPrim = UsdShadeShader::Get(timeVarStage, shadPrimPath);
  assert(timeVarShadPrim);

  SdfPath opMulPrimPath = matPrimPath.AppendPath(SdfPath(constring::mdlOpacityMulPrimPf));

  UsdShadeShader uniformOpMulPrim = UsdShadeShader::Get(SceneStage, opMulPrimPath);
  assert(uniformOpMulPrim);
  UsdShadeShader timeVarOpMulPrim = UsdShadeShader::Get(timeVarStage, opMulPrimPath);
  assert(timeVarOpMulPrim);

  GfVec3f difColor(GetValuePtr(matData.Diffuse));
  GfVec3f emColor(GetValuePtr(matData.Emissive));

  bool enableEmission = (matData.EmissiveIntensity.Value >= 0.0 || matData.EmissiveIntensity.SrcAttrib || matData.EmissiveIntensity.Sampler);

  // Only set values on either timevar or uniform prim
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::DIFFUSE, matData.Diffuse, difColor);
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::EMISSIVECOLOR, matData.Emissive, emColor);
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::EMISSIVEINTENSITY, matData.EmissiveIntensity);
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::ROUGHNESS, matData.Roughness);
  UPDATE_MDL_SHADER_INPUT_MACRO(DMI::METALLIC, matData.Metallic);
  //UPDATE_MDL_SHADER_INPUT_MACRO(DMI::IOR, matData.Ior);
  UpdateShaderInput<false>(this, SceneStage, timeVarStage, uniformOpMulPrim, timeVarOpMulPrim, matPrimPath, boundGeomPrimvars,
    timeEval, DMI::OPACITY, matData.Opacity, UsdBridgeTokens->b, matData.Opacity.Value); // Connect to the mul shader 'b' input, instead of the opacity input directly

  // Just a value, not connected to attribreaders or samplers
  float opacityCutoffValue = (matData.AlphaMode == UsdBridgeMaterialData::AlphaModes::MASK) ? matData.AlphaCutoff : 0.0f; // don't enable cutoff when not explicitly specified
  bool enableOpacity = matData.AlphaMode != UsdBridgeMaterialData::AlphaModes::NONE;
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->opacity_threshold, DMI::OPACITY, opacityCutoffValue);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->enable_opacity, DMI::OPACITY, enableOpacity);
  SetShaderInput(uniformShadPrim, timeVarShadPrim, timeEval, UsdBridgeTokens->enable_emission, DMI::EMISSIVEINTENSITY, enableEmission);

#ifdef CUSTOM_PBR_MDL
  if (!enableOpacity)
    uniformShadPrim.SetSourceAsset(this->MdlOpaqueRelFilePath, UsdBridgeTokens->mdl);
  else
    uniformShadPrim.SetSourceAsset(this->MdlTranslucentRelFilePath, UsdBridgeTokens->mdl);
#endif
}

void UsdBridgeUsdWriter::UpdateUsdSampler(UsdStageRefPtr timeVarStage, UsdBridgePrimCache* cacheEntry, const UsdBridgeSamplerData& samplerData, double timeStep)
{
  TimeEvaluator<UsdBridgeSamplerData> timeEval(samplerData, timeStep);
  typedef UsdBridgeSamplerData::DataMemberId DMI;

  const SdfPath& samplerPrimPath = cacheEntry->PrimPath;

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
      TempImageData.resize(0);

      const void* convertedSamplerData = nullptr;
      int64_t convertedSamplerStride = samplerData.ImageStride[1];
      int numComponents = samplerData.ImageNumComponents;

      UsdBridgeType flattenedType = ubutils::UsdBridgeTypeFlatten(samplerData.DataType);
      if( !(flattenedType == UsdBridgeType::UCHAR || flattenedType == UsdBridgeType::UCHAR_SRGB_R))
      {
        // Convert to 8-bit image data
        ConvertSamplerDataToImage(samplerData, TempImageData);

        if(TempImageData.size())
        {
          convertedSamplerData = TempImageData.data();
          convertedSamplerStride = samplerData.ImageDims[0]*numComponents;
        }
      }
      else
      {
        convertedSamplerData = samplerData.Data;
      }

      if(numComponents <= 4 && convertedSamplerData)
      {
        StbWriteOutput writeOutput;

        stbi_write_png_to_func(StbWriteToBuffer, &writeOutput,
          static_cast<int>(samplerData.ImageDims[0]), static_cast<int>(samplerData.ImageDims[1]),
          numComponents, convertedSamplerData, convertedSamplerStride);

        // Filename, relative from connection working dir
        std::string wdRelFilename(SessionDirectory + imgFileName);
        Connect->WriteFile(writeOutput.imageData, writeOutput.imageSize, wdRelFilename.c_str(), true);
      }
      else
      {
        UsdBridgeLogMacro(this->LogObject, UsdBridgeLogLevel::WARNING, "Image file not written out, format not supported (should be 1-4 component unsigned char/short/int or float/double): " << samplerData.ImageName);
      }
    }
  }
}

namespace
{
  template<bool PreviewSurface>
  void UpdateAttributeReader_Impl(UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath,
    UsdBridgeMaterialData::DataMemberId dataMemberId, const TfToken& newNameToken, const UsdGeomPrimvarsAPI& boundGeomPrimvars,
    const TimeEvaluator<UsdBridgeMaterialData>& timeEval, const UsdBridgeLogObject& logObj)
  {
    typedef UsdBridgeMaterialData::DataMemberId DMI;

    if(PreviewSurface && dataMemberId == DMI::EMISSIVEINTENSITY)
      return; // Emissive intensity is not represented as a shader input in the USD preview surface model

    // Collect the various prims
    SdfPath attributeReaderPath = matPrimPath.AppendPath(GetAttributeReaderPathPf<PreviewSurface>(dataMemberId));

    UsdShadeShader uniformAttribReader = UsdShadeShader::Get(sceneStage, attributeReaderPath);
    if(!uniformAttribReader)
    {
      UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "In UpdateAttributeReader_Impl(): requesting an attribute reader that hasn't been created during fixup of name token.");
      return;
    }

    UpdateAttributeReaderName<PreviewSurface>(uniformAttribReader, newNameToken);
    UpdateAttributeReaderOutput<PreviewSurface>(uniformAttribReader, boundGeomPrimvars, newNameToken);
  }

  template<bool PreviewSurface>
  void UpdateSamplerTcReader(UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, const SdfPath& samplerPrimPath, const TfToken& newNameToken, const TimeEvaluator<UsdBridgeSamplerData>& timeEval)
  {
    typedef UsdBridgeSamplerData::DataMemberId DMI;

    // Collect the various prims
    SdfPath tcReaderPrimPath = samplerPrimPath.AppendPath(SdfPath(constring::texCoordReaderPrimPf));

    UsdShadeShader uniformTcReaderPrim = UsdShadeShader::Get(sceneStage, tcReaderPrimPath);
    assert(uniformTcReaderPrim);

    // Set the new Inattribute
    UpdateAttributeReaderName<PreviewSurface>(uniformTcReaderPrim, newNameToken);
  }
}

void UsdBridgeUsdWriter::UpdateAttributeReader(UsdStageRefPtr timeVarStage, const SdfPath& matPrimPath, MaterialDMI dataMemberId, const char* newName, const UsdGeomPrimvarsAPI& boundGeomPrimvars, double timeStep, MaterialDMI timeVarying)
{
  // Time eval with dummy data
  UsdBridgeMaterialData materialData;
  materialData.TimeVarying = timeVarying;

  TimeEvaluator<UsdBridgeMaterialData> timeEval(materialData, timeStep);

  const TfToken& newNameToken = AttributeNameToken(newName);

  if(Settings.EnablePreviewSurfaceShader)
  {
    UpdateAttributeReader_Impl<true>(SceneStage, timeVarStage, matPrimPath, dataMemberId, newNameToken, boundGeomPrimvars, timeEval, this->LogObject);
  }

  if(Settings.EnableMdlShader)
  {
    UpdateAttributeReader_Impl<false>(SceneStage, timeVarStage, matPrimPath, dataMemberId, newNameToken, boundGeomPrimvars, timeEval, this->LogObject);
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

#ifdef USE_INDEX_MATERIALS
void UsdBridgeUsdWriter::UpdateIndexVolumeMaterial(UsdStageRefPtr sceneStage, UsdStageRefPtr timeVarStage, const SdfPath& volumePath, const UsdBridgeVolumeData& volumeData, double timeStep)
{
  TimeEvaluator<UsdBridgeVolumeData> timeEval(volumeData, timeStep);
  typedef UsdBridgeVolumeData::DataMemberId DMI;

  SdfPath indexMatPath = volumePath.AppendPath(SdfPath(constring::indexMaterialPf));
  SdfPath colorMapPath = indexMatPath.AppendPath(SdfPath(constring::indexColorMapPf));

  // Renormalize the value range based on the volume data type (see CopyToGrid in the volumewriter)
  GfVec2f valueRange(GfVec2d(volumeData.TfData.TfValueRange));

  // Set the transfer function value array from opacities and colors
  assert(volumeData.TfData.TfOpacitiesType == UsdBridgeType::FLOAT);
  const float* tfOpacities = static_cast<const float*>(volumeData.TfData.TfOpacities);

  // Set domain and colormap values
  UsdPrim uniformColorMap = sceneStage->GetPrimAtPath(colorMapPath);
  assert(uniformColorMap);
  UsdPrim timeVarColorMap = timeVarStage->GetPrimAtPath(colorMapPath);
  assert(timeVarColorMap);

  UsdAttribute uniformDomainAttr = uniformColorMap.GetAttribute(UsdBridgeTokens->domain);
  UsdAttribute timeVarDomainAttr = timeVarColorMap.GetAttribute(UsdBridgeTokens->domain);
  UsdAttribute uniformTfValueAttr = uniformColorMap.GetAttribute(UsdBridgeTokens->colormapValues);
  UsdAttribute timeVarTfValueAttr = timeVarColorMap.GetAttribute(UsdBridgeTokens->colormapValues);

  // Timevarying colormaps/domains are currently not supported by index, so keep this switched off
  bool rangeTimeVarying = false;//timeEval.IsTimeVarying(DMI::TFVALUERANGE);
  bool valuesTimeVarying = false;//timeEval.IsTimeVarying(DMI::TFCOLORS) || timeEval.IsTimeVarying(DMI::TFOPACITIES);

  // Clear timevarying attributes if necessary
  ClearUsdAttributes(uniformDomainAttr, timeVarDomainAttr, rangeTimeVarying);
  ClearUsdAttributes(uniformTfValueAttr, timeVarTfValueAttr, valuesTimeVarying);

  // Set the attributes
  UsdAttribute& outAttrib = valuesTimeVarying ? timeVarTfValueAttr : uniformTfValueAttr;
  UsdTimeCode outTimeCode = valuesTimeVarying ? timeEval.TimeCode : timeEval.Default();
  VtVec4fArray* outArray = AssignColorArrayToPrimvar(LogObject, volumeData.TfData.TfColors, volumeData.TfData.TfNumColors, volumeData.TfData.TfColorsType,
    outTimeCode,
    outAttrib,
    false); // Get the pointer, set the data manually here

  for(size_t i = 0; i < outArray->size() && i < volumeData.TfData.TfNumOpacities; ++i)
    (*outArray)[i][3] = tfOpacities[i]; // Set the alpha channel

  if(outArray)
    outAttrib.Set(*outArray, outTimeCode);

  SET_TIMEVARYING_ATTRIB(rangeTimeVarying, timeVarDomainAttr, uniformDomainAttr, valueRange);
}
#endif

void ResourceCollectSampler(UsdBridgePrimCache* cache, UsdBridgeUsdWriter& usdWriter)
{
  RemoveResourceFiles(cache, usdWriter, constring::imgFolder, constring::imageExtension);
}

