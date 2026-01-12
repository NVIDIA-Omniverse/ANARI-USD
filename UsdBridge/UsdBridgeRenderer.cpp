// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeRenderer.h"
#include "UsdBridgeUsdWriter.h"
#include "UsdBridgeUsdWriter_Common.h"
#include "UsdBridgeDiagnosticMgrDelegate.h"

#ifdef USD_DEVICE_RENDERING_ENABLED

#include <pxr/imaging/hd/aov.h>
#include <pxr/imaging/hd/driver.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/rendererPluginHandle.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/imaging/hd/tokens.h>
#include "pxr/imaging/hd/unitTestDelegate.h"
#include "pxr/imaging/hdSt/textureUtils.h"
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/imaging/hgi/texture.h>
#include <pxr/usd/usdRender/settings.h>
#include <pxr/usd/usdRender/product.h>
#include <pxr/usd/usdRender/var.h>
#include <pxr/usd/usdRender/tokens.h>
#include <pxr/usdImaging/usdImaging/delegate.h>
#include <pxr/imaging/hdx/colorCorrectionTask.h>
#include <pxr/imaging/hdx/renderSetupTask.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/imaging/hdx/selectionTracker.h>
#include <pxr/imaging/hdx/taskController.h>
#include <pxr/imaging/hdx/tokens.h>
#include <pxr/imaging/glf/simpleLightingContext.h>
#include <pxr/imaging/glf/diagnostic.h>

#include <vector>
#include <memory>
#include <string>

//#define USDBRIDGE_RENDERER_USE_ENGINEGL
#define USDBRIDGE_RENDERER_USE_COLORTEXTURE

#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL

#include <pxr/usdImaging/usdImagingGL/engine.h>

// Allow access to the task controller for AOV control
class UsdBridgeImagingGLEngine : public pxr::UsdImagingGLEngine
{
  public:
    UsdBridgeImagingGLEngine(pxr::UsdImagingGLEngine::Parameters params)
      : pxr::UsdImagingGLEngine(params)
    {}

    pxr::HdxTaskController* GetTaskController()
    {
      return _GetTaskController();
    }
};

#endif

class UsdBridgeRendererInternals
{
public:
  ~UsdBridgeRendererInternals()
  {
#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
    delete RenderEngineGL;
#else
    RendererPlugin = nullptr;
    SetDelegatesAndIndex(nullptr);
#endif
  }

  void Initialize(const char* rendererName, pxr::UsdStageRefPtr stage)
  {
#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
    pxr::UsdImagingGLEngine::Parameters initParams;
    initParams.rendererPluginId = TfToken(rendererName);
    RenderEngineGL = new UsdBridgeImagingGLEngine(initParams);
    RenderEngineGL->SetRendererAov(pxr::HdAovTokens->color);
    RenderEngineGL->SetOverrideWindowPolicy(pxr::CameraUtilMatchVertically);
    Initialized = true;
#else
    RendererPlugin =
      pxr::HdRendererPluginRegistry::GetInstance().GetOrCreateRendererPlugin(TfToken(rendererName));
    if(RendererPlugin)
    {
      SetDelegatesAndIndex(stage);

      Initialized = true;
    }
    else
    {
      // throw error
    }
#endif
  }

#ifndef USDBRIDGE_RENDERER_USE_ENGINEGL
  void SetDelegatesAndIndex(pxr::UsdStageRefPtr stage)
  {
    delete RenderEngine;
    delete TaskController;
    delete SceneDelegate;
    delete RenderIndex;
    delete RenderDelegate;
    delete RenderDriver;

    RenderDelegate =
      RendererPlugin ?
      RendererPlugin->CreateRenderDelegate() : nullptr;

    if(RenderDelegate)
    {
      RenderHgi = pxr::Hgi::CreatePlatformDefaultHgi();
      RenderDriver = new pxr::HdDriver{pxr::HgiTokens->renderDriver, pxr::VtValue(RenderHgi.get())};
      pxr::HdDriverVector drivers { RenderDriver };
      RenderIndex = pxr::HdRenderIndex::New(RenderDelegate, drivers);

      SceneDelegate = new pxr::UsdImagingDelegate(RenderIndex, SdfPath::AbsoluteRootPath());
      SceneDelegate->Populate(stage->GetPseudoRoot());
      //SceneDelegate->AddCube(SdfPath("/MyCube1"), GfMatrix4f(1));

      TaskController = new pxr::HdxTaskController(RenderIndex, TaskControllerId);

      RenderEngine = new pxr::HdEngine();

      LightingContextForOpenGLState = GlfSimpleLightingContext::New();
    }
    else
    {
      SceneDelegate = nullptr;
      RenderIndex = nullptr;
      RenderDriver = nullptr;
    }
  }
#endif

  void CreateRenderSettings(pxr::UsdStageRefPtr stage)
  {
    stage->SetMetadata(pxr::UsdRenderTokens->renderSettingsPrimPath, pxr::VtValue(RenderSettingsPath));
    RenderSettings = GetOrDefinePrim<pxr::UsdRenderSettings>(stage, pxr::SdfPath(RenderSettingsPath));
    RenderSettings.CreateResolutionAttr();
    RenderSettings.CreateCameraRel();

    RenderProduct = GetOrDefinePrim<pxr::UsdRenderProduct>(stage, pxr::SdfPath(RenderProductPath));
    RenderProduct.CreateResolutionAttr();
    RenderProduct.CreateCameraRel();
    RenderProduct.CreateOrderedVarsRel();

    pxr::SdfPath renderVarSdf(RenderVarPath);
    pxr::UsdRenderVar renderVarPrim = GetOrDefinePrim<pxr::UsdRenderVar>(stage, renderVarSdf);
    renderVarPrim.CreateSourceNameAttr(pxr::VtValue(RenderVarSource));
    RenderProduct.GetOrderedVarsRel().AddTarget(renderVarSdf);
  }

  void ChangeResolution(uint32_t width, uint32_t height, UsdBridgeUsdWriter& usdWriter)
  {
    if(width != CachedWidth || height != CachedHeight)
    {
      RenderSettings.GetResolutionAttr().Set(pxr::GfVec2i((int)width, (int)height));
      RenderProduct.GetResolutionAttr().Set(pxr::GfVec2i((int)width, (int)height));
      usdWriter.SaveScene();

#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
      RenderEngineGL->SetRenderBufferSize(pxr::GfVec2i(width, height));
      pxr::HdxTaskController* taskController = RenderEngineGL->GetTaskController();
#else
      TaskController->SetRenderOutputs({pxr::HdAovTokens->color});
      TaskController->SetRenderBufferSize(pxr::GfVec2i(width, height));

      pxr::HdxTaskController* taskController = TaskController;
#endif

      pxr::HdAovDescriptor colorSettings = taskController->GetRenderOutputSettings(pxr::HdAovTokens->color);
      colorSettings.multiSampled = false;
      colorSettings.clearValue = GfVec4f(0.0f, 1.0f, 0.0f, 1.0f);
      if(colorSettings.format != pxr::HdFormatUNorm8Vec4 &&
        colorSettings.format != pxr::HdFormatFloat32Vec4 &&
        colorSettings.format != pxr::HdFormatSNorm8Vec4)
        colorSettings.format = pxr::HdFormatUNorm8Vec4;
      taskController->SetRenderOutputSettings(pxr::HdAovTokens->color, colorSettings);
      pxr::HdAovDescriptor depthSettings = taskController->GetRenderOutputSettings(pxr::HdAovTokens->depth);
      depthSettings.multiSampled = false;
      taskController->SetRenderOutputSettings(pxr::HdAovTokens->depth, depthSettings);

      CachedWidth = width;
      CachedHeight = height;
    }
  }

  void SetFixedLights()
  {
    pxr::GfVec4f sceneAmbient(0.01f, 0.01f, 0.01f, 1.0f);
    pxr::GlfSimpleMaterial material;
    std::vector<pxr::GlfSimpleLight> lights;

    // Check if the render mode requires lights
    if (true) {
        // Ambient light located at the camera
        if (true) {
            pxr::GlfSimpleLight ambientLight;
            ambientLight.SetAmbient(GfVec4f(0.0f, 0.0f, 0.0f, 0.0f));
            ambientLight.SetPosition(GfVec4f(1.14013255, 0.592924118, -3.45756054, 1.00000000));
            lights.push_back(ambientLight);
        }

        // Set material properties
        float kA = 0.2f;
        float kS = 0.1f;
        material.SetAmbient(GfVec4f(kA, kA, kA, 1.0f));
        material.SetSpecular(GfVec4f(kS, kS, kS, 1.0f));
        material.SetShininess(32.0f);
    }

  #ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
    RenderEngineGL->SetLightingState(lights, material, sceneAmbient);
  #else
    SetLightingState(lights, material, sceneAmbient);
  #endif
  }

  void SetLightingState(
    GlfSimpleLightVector const &lights,
    GlfSimpleMaterial const &material,
    GfVec4f const &sceneAmbient)
  {
    LightingContextForOpenGLState->SetLights(lights);
    LightingContextForOpenGLState->SetMaterial(material);
    LightingContextForOpenGLState->SetSceneAmbient(sceneAmbient);
    LightingContextForOpenGLState->SetUseLighting(lights.size() > 0);

    TaskController->SetLightingState(LightingContextForOpenGLState);
  }

  UsdBridgeType ConvertTextureFormat(pxr::HgiFormat hgiFormat)
  {
    switch(hgiFormat)
    {
      case pxr::HgiFormatUNorm8Vec4:
        return UsdBridgeType::UCHAR4;
      case pxr::HgiFormatSNorm8Vec4:
        return UsdBridgeType::UCHAR_SRGB_RGBA;
      case pxr::HgiFormatFloat32Vec4:
        return UsdBridgeType::FLOAT4;
      default:
        return UsdBridgeType::UNDEFINED;
    }
  }

  UsdBridgeType ConvertRenderBufferFormat(pxr::HdFormat hdFormat)
  {
    switch(hdFormat)
    {
      case pxr::HdFormatUNorm8Vec4:
        return UsdBridgeType::UCHAR4;
      case pxr::HdFormatSNorm8Vec4:
        return UsdBridgeType::CHAR4;
      case pxr::HdFormatFloat32Vec4:
        return UsdBridgeType::FLOAT4;
      default:
        return UsdBridgeType::UNDEFINED;
    }
  }

  pxr::SdfPath CameraPath;
  pxr::SdfPath WorldPath;

  pxr::HdRendererPluginHandle RendererPlugin = nullptr;
  pxr::TfToken RendererPluginId;
  pxr::HgiUniquePtr RenderHgi = nullptr;

  pxr::HdDriver* RenderDriver = nullptr;
  pxr::HdRenderIndex* RenderIndex = nullptr;
  pxr::HdRenderDelegate* RenderDelegate = nullptr;
  pxr::UsdImagingDelegate* SceneDelegate = nullptr;

  pxr::HdxTaskController* TaskController = nullptr;

  pxr::HdRenderBuffer* RenderBuffer = nullptr;

  pxr::HdEngine* RenderEngine = nullptr;

#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
  UsdBridgeImagingGLEngine* RenderEngineGL = nullptr;
#endif

  pxr::SdfPath RenderBufferId {"/UsdDeviceRenderBuffer"};
  pxr::SdfPath TaskControllerId {"/UsdDeviceTaskController"};
  //pxr::SdfPath SceneDelegateId { "/UsdDeviceSceneDelegate" };
  pxr::SdfPath RenderSetupTaskId { "/UsdDeviceRenderSetupTask" };
  pxr::SdfPath RenderTaskId { "/UsdDeviceRenderTask" };
  std::string RenderSettingsPath { "/Render/PrimarySettings" };
  std::string RenderProductPath { "/Render/PrimaryProduct" };
  std::string RenderVarPath { "/Render/Vars/LdrColor" };
  std::string RenderVarSource { "LdrColor" };
  pxr::UsdRenderSettings RenderSettings;
  pxr::UsdRenderProduct RenderProduct;

  pxr::HdStTextureUtils::AlignedBuffer<uint8_t> MappedColorTextureBuffer;

  pxr::GlfSimpleLightingContextRefPtr LightingContextForOpenGLState;

  const char* CachedRendererName = nullptr;
  uint32_t CachedWidth = 0;
  uint32_t CachedHeight = 0;

  pxr::HgiTextureDesc MappedTextureDesc;
  size_t MappedSize = 0;

  bool Initialized = false;
};

UsdBridgeRenderer::UsdBridgeRenderer(UsdBridgeUsdWriter& usdWriter)
  : UsdWriter(usdWriter)
  , Internals(new UsdBridgeRendererInternals())
{
}

UsdBridgeRenderer::~UsdBridgeRenderer()
{
  delete Internals;
}

void UsdBridgeRenderer::Initialize(const char* rendererName)
{
  if(Internals->Initialized)
    return;

  if(Internals->CachedRendererName == rendererName)
    return;

  Internals->CachedRendererName = rendererName;

  pxr::UsdStageRefPtr stage = UsdWriter.GetSceneStage();

  if(stage)
  {
    Internals->CreateRenderSettings(stage); // This could be sit a more overarching initialization phase, but it's ok here as well

    Internals->RendererPluginId = pxr::HdRendererPluginRegistry::GetInstance().GetDefaultPluginId(true);
    Internals->Initialize(rendererName, stage);

    if(!Internals->CameraPath.IsEmpty())
      SetCameraPath(Internals->CameraPath);
  }
}

void UsdBridgeRenderer::SetCameraPath(const SdfPath& cameraPath)
{
  Internals->CameraPath = cameraPath;

  if(!Internals->Initialized)
    return;

  Internals->RenderSettings.GetCameraRel().ClearTargets(false);
  Internals->RenderSettings.GetCameraRel().AddTarget(cameraPath);
  Internals->RenderProduct.GetCameraRel().ClearTargets(false);
  Internals->RenderProduct.GetCameraRel().AddTarget(cameraPath);

#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
  Internals->RenderEngineGL->SetCameraPath(cameraPath);
#else
  Internals->TaskController->SetCameraPath(cameraPath);
  Internals->SceneDelegate->SetCameraForSampling(cameraPath);
  UsdBridgeLogMacro(UsdWriter.LogObject, UsdBridgeLogLevel::WARNING, "CAMERA " << cameraPath.GetString());
#endif

  UsdWriter.SaveScene();
}

void UsdBridgeRenderer::SetWorldPath(const SdfPath& worldPath)
{
  Internals->WorldPath = worldPath;
}

void UsdBridgeRenderer::Render(uint32_t width, uint32_t height, double timeStep)
{
  if(!Internals->Initialized)
    return;

  pxr::HdReprSelector reprSelector(pxr::HdReprTokens->smoothHull);
  pxr::HdRprimCollection collection(pxr::HdTokens->geometry, reprSelector);

  pxr::CameraUtilFraming framing(GfRect2i(GfVec2i(0, 0), width, height));

  //auto renderPass = Internals->RenderDelegate->CreateRenderPass(Internals->RenderIndex, collection);
  //auto renderPassState = Internals->RenderDelegate->CreateRenderPassState();

  //renderPassState->SetFraming(framing);
  Internals->ChangeResolution(width, height, UsdWriter);

  //Internals->SetFixedLights();

#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL

  pxr::UsdImagingGLRenderParams glRenderParams;
  glRenderParams.frame = pxr::UsdTimeCode(0.0f);
  glRenderParams.clearColor = pxr::GfVec4f(0.0f, 1.0f, 0.0f, 1.0f);
  glRenderParams.gammaCorrectColors = false;
  glRenderParams.enableLighting = true;
  glRenderParams.enableSceneLights = true;
  glRenderParams.enableSceneMaterials = true;

  // Corrections
  glRenderParams.enableSampleAlphaToCoverage = true;
  glRenderParams.enableIdRender =false;
  glRenderParams.applyRenderState = true;
  glRenderParams.highlight = false; //taskController->SetEnableSelection
  glRenderParams.clearColor = pxr::GfVec4f(0.0, 1.0, 0.0, 1.00000000);
  glRenderParams.colorCorrectionMode = pxr::HdxColorCorrectionTokens->sRGB;

  //glRenderParams.bboxes.push_back(pxr::GfBBox3d(pxr::GfRange3d(pxr::GfVec3d(-1,-1,-1), pxr::GfVec3d(1,1,1))));
  //glRenderParams.bboxes.push_back(pxr::GfBBox3d(pxr::GfRange3d(pxr::GfVec3d(-1,-1,-1), pxr::GfVec3d(1,1,1))));
  //glRenderParams.bboxLineColor = pxr::GfVec4f(0.670740008, 0.670740008, 0.670740008, 1.00000000);

  Internals->RenderEngineGL->SetFraming(framing);

  Internals->RenderEngineGL->Render(UsdWriter.GetSceneStage()->GetPseudoRoot(), glRenderParams);

#else

  auto& taskController = Internals->TaskController;

  Internals->SceneDelegate->SetTime(pxr::UsdTimeCode(0.0f));

  pxr::HdxRenderTaskParams taskParams;
  taskParams.overrideColor = pxr::GfVec4f(.0f, .0f, .0f, .0f);
  taskParams.wireframeColor = pxr::GfVec4f(.0f, .0f, .0f, .0f);
  taskParams.enableLighting = true;
  taskParams.enableIdRender = false;
  taskParams.depthBiasUseDefault = true;
  taskParams.depthFunc = pxr::HdCmpFuncLess;
  taskParams.cullStyle = pxr::HdCullStyleNothing;
  taskParams.alphaThreshold = 0.1f;
  taskParams.enableSceneMaterials = true;
  taskParams.enableSceneLights = true;

  taskController->SetRenderParams(taskParams);
  taskController->SetEnableSelection(false);

  pxr::HdxColorCorrectionTaskParams hdParams;
  hdParams.colorCorrectionMode = pxr::HdxColorCorrectionTokens->sRGB;
  taskController->SetColorCorrectionParams(hdParams);

  taskController->SetCollection(collection);
  taskController->SetFraming(framing);

  //Internals->RenderEngine->SetTaskContextData(pxr::HdxTokens->renderPassState, pxr::VtValue(renderPassState)); // This is done by HdxRenderSetupTask::Execute
  pxr::HdxSelectionTrackerSharedPtr emptySelection = std::make_shared<pxr::HdxSelectionTracker>();
  Internals->RenderEngine->SetTaskContextData(pxr::HdxTokens->selectionState, VtValue(emptySelection));
  Internals->RenderEngine->SetTaskContextData(pxr::HdxTokens->lightingContext, VtValue(Internals->LightingContextForOpenGLState));

  HdTaskSharedPtrVector tasks = taskController->GetRenderingTasks();
  Internals->RenderEngine->Execute(Internals->RenderIndex,
    &tasks);

#endif
}

bool UsdBridgeRenderer::FrameReady(bool wait)
{
  if(!Internals->Initialized)
    return true;

  pxr::HdRenderBuffer* renderBuffer =
#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
    Internals->RenderEngineGL->GetAovRenderBuffer(pxr::HdAovTokens->color);
#else
    Internals->TaskController->GetRenderOutput(pxr::HdAovTokens->color);
#endif

  if(!renderBuffer)
    return true;

  bool isConverged = renderBuffer->IsConverged();
  if(wait)
  {
    //while(!isConverged)
    //  isConverged = renderBuffer->IsConverged();
  }
  return true;
}

void* UsdBridgeRenderer::MapFrame(UsdBridgeType& returnFormat)
{
  if(!Internals->Initialized)
    return nullptr;

#ifdef USDBRIDGE_RENDERER_USE_COLORTEXTURE

#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
  auto colorTextureHandle = Internals->RenderEngineGL->GetAovTexture(pxr::HdAovTokens->color);
  auto hgi = Internals->RenderEngineGL->GetHgi();
#else
  auto hgi = Internals->RenderHgi.get();
  pxr::VtValue aov;
  pxr::HgiTextureHandle colorTextureHandle;

  if (Internals->RenderEngine->GetTaskContextData(pxr::HdAovTokens->color, &aov)) {
      if (aov.IsHolding<pxr::HgiTextureHandle>()) {
        colorTextureHandle = aov.Get<pxr::HgiTextureHandle>();
      }
  }
#endif

  if (!colorTextureHandle)
      return nullptr;

  Internals->MappedTextureDesc = colorTextureHandle->GetDescriptor();
  Internals->MappedSize = 0;
  Internals->MappedColorTextureBuffer = pxr::HdStTextureUtils::HgiTextureReadback(
    hgi, colorTextureHandle, &Internals->MappedSize);

  returnFormat = Internals->ConvertTextureFormat(Internals->MappedTextureDesc.format);

  pxr::GLF_POST_PENDING_GL_ERRORS();
  return Internals->MappedColorTextureBuffer.get();

#else

#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
  pxr::HdRenderBuffer* renderBuffer =
    Internals->RenderEngineGL->GetAovRenderBuffer(pxr::HdAovTokens->color);
#else
  pxr::HdRenderBuffer* renderBuffer =
    Internals->TaskController->GetRenderOutput(pxr::HdAovTokens->color);
#endif

  renderBuffer->Resolve();

  returnFormat = Internals->ConvertRenderBufferFormat(renderBuffer->GetFormat());

  void* retValue = renderBuffer->Map();

  pxr::GLF_POST_PENDING_GL_ERRORS();
  return retValue;

#endif

}

void UsdBridgeRenderer::UnmapFrame()
{
  if(!Internals->Initialized)
    return;

  pxr::GLF_POST_PENDING_GL_ERRORS();

#ifndef USDBRIDGE_RENDERER_USE_COLORTEXTURE
#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
  pxr::HdRenderBuffer* renderBuffer =
    Internals->RenderEngineGL->GetAovRenderBuffer(pxr::HdAovTokens->color);
#else
  pxr::HdRenderBuffer* renderBuffer =
      Internals->TaskController->GetRenderOutput(pxr::HdAovTokens->color);
#endif
  if(renderBuffer)
    renderBuffer->Unmap();
#endif
}


#else
UsdBridgeRenderer::UsdBridgeRenderer(UsdBridgeUsdWriter& usdWriter)
  : UsdWriter(usdWriter)
{
}

UsdBridgeRenderer::~UsdBridgeRenderer()
{
}

void UsdBridgeRenderer::Initialize(const char* rendererName)
{
}

void UsdBridgeRenderer::SetCameraPath(const pxr::SdfPath& cameraPath)
{
}

void UsdBridgeRenderer::SetWorldPath(const pxr::SdfPath& worldPath)
{
}

void UsdBridgeRenderer::Render(uint32_t width, uint32_t height, double time)
{
}

bool UsdBridgeRenderer::FrameReady(bool wait)
{
  return true;
}

void* UsdBridgeRenderer::MapFrame(UsdBridgeType& returnFormat)
{
  return nullptr;
}

void UsdBridgeRenderer::UnmapFrame()
{
}
#endif
