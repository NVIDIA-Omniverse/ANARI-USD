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
#include <pxr/usd/usdRender/tokens.h>
#include <pxr/usdImaging/usdImaging/delegate.h>
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
    DestroyRenderBuffers();
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

  void CreateRenderSettings(pxr::UsdStageRefPtr stage)
  {
    stage->SetMetadata(pxr::UsdRenderTokens->renderSettingsPrimPath, pxr::VtValue(RenderSettingsPath));
    RenderSettings = GetOrDefinePrim<pxr::UsdRenderSettings>(stage, pxr::SdfPath(RenderSettingsPath));
    RenderSettings.CreateResolutionAttr();
  }

  void CreateRenderBuffers()
  {
    //RenderIndex->InsertBprim(
    //  pxr::HdPrimTypeTokens->renderBuffer, SceneDelegate, RenderBufferId);
    //pxr::HdBprim* bprim = RenderIndex->GetBprim(pxr::HdPrimTypeTokens->renderBuffer, RenderBufferId);
    //RenderBuffer = dynamic_cast<pxr::HdRenderBuffer*>(bprim);
    //if(!RenderBuffer)
    //{
    //  //Throw error
    //}
  }

  void DestroyRenderBuffers()
  {
    //if(RenderBuffer)
    //{
    //  bool isConverged = RenderBuffer->IsConverged();
    //  while(!isConverged)
    //    isConverged = RenderBuffer->IsConverged();
//
    //  RenderBuffer->Finalize(nullptr);
    //  RenderIndex->RemoveBprim(pxr::HdPrimTypeTokens->renderBuffer, RenderBufferId);
    //}
    //RenderBuffer = nullptr;
  }

  void ChangeResolution(uint32_t width, uint32_t height, UsdBridgeUsdWriter& usdWriter)
  {
    if(width != CachedWidth || height != CachedHeight)
    {
      RenderSettings.GetResolutionAttr().Set(pxr::GfVec2i((int)width, (int)height));
      usdWriter.SaveScene();

#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL

      RenderEngineGL->SetRenderBufferSize(pxr::GfVec2i(width, height));
      pxr::HdxTaskController* taskController = RenderEngineGL->GetTaskController();

#else
      TaskController->SetRenderOutputs({pxr::HdAovTokens->color});
      TaskController->SetRenderBufferSize(pxr::GfVec2i(width, height));

      pxr::HdxTaskController* taskController = TaskController;

      // Create a renderbuffer if necessary
      //if(RenderBuffer)
      //{
      //  DestroyRenderBuffers();
      //}
      //if(!RenderBuffer)
      //  CreateRenderBuffers();

      // Allocate the buffer with desired dimensions and format
      //pxr::GfVec3i dimensions(width, height, 1);
      //pxr::HdFormat format = pxr::HdFormatUNorm8Vec4;
      //bool multiSampled = false;
      //static int lala = 0;
      //if(lala == 0)
      //  RenderBuffer->Allocate(dimensions, format, multiSampled);
      //else
      //  RenderBuffer2->Allocate(dimensions, format, multiSampled);
      //lala++;
#endif
      pxr::HdAovDescriptor colorSettings(
        pxr::HdFormatUNorm8Vec4,
        false,
        pxr::VtValue(GfVec4f(0.0f, 1.0f, 0.0f, 1.0f)));
      taskController->SetRenderOutputSettings(pxr::HdAovTokens->color, colorSettings);
      pxr::HdAovDescriptor depthSettings = taskController->GetRenderOutputSettings(pxr::HdAovTokens->depth);
      depthSettings.multiSampled = false;
      taskController->SetRenderOutputSettings(pxr::HdAovTokens->depth, depthSettings);

      CachedWidth = width;
      CachedHeight = height;
    }
  }

  pxr::SdfPath CameraPath;

  pxr::HdRendererPluginHandle RendererPlugin = nullptr;
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
  pxr::UsdRenderSettings RenderSettings;

  pxr::HdStTextureUtils::AlignedBuffer<uint8_t> MappedColorTextureBuffer;

  pxr::GlfSimpleLightingContextRefPtr LightingContextForOpenGLState;

  const char* CachedRendererName = nullptr;
  uint32_t CachedWidth = 0;
  uint32_t CachedHeight = 0;

  uint32_t CheckWidth = 0;
  uint32_t CheckHeight = 0;
  pxr::HdFormat CheckFormat;
  bool CheckMSAA = false;
  pxr::HgiTextureDesc CheckTexture;
  size_t CheckSize = 0;

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

  static int lala = 0;
  //if(lala++ < 4)
  //  return;

  if(Internals->CachedRendererName == rendererName)
    return;

  Internals->CachedRendererName = rendererName;

  pxr::UsdStageRefPtr stage = UsdWriter.GetSceneStage();

  if(stage)
  {
    Internals->CreateRenderSettings(stage); // This could be sit a more overarching initialization phase, but it's ok here as well

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

#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
  Internals->RenderEngineGL->SetCameraPath(cameraPath);
#else
  Internals->TaskController->SetCameraPath(cameraPath);
  Internals->SceneDelegate->SetCameraForSampling(cameraPath);
  UsdBridgeLogMacro(UsdWriter.LogObject, UsdBridgeLogLevel::WARNING, "CAMERA " << cameraPath.GetString());
#endif

  UsdWriter.SaveScene();
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

#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL

  pxr::UsdImagingGLRenderParams glRenderParams;
  glRenderParams.frame = pxr::UsdTimeCode(0.0f);
  glRenderParams.clearColor = pxr::GfVec4f(0.0f, 1.0f, 0.0f, 1.0f);
  glRenderParams.gammaCorrectColors = false;

  Internals->RenderEngineGL->SetFraming(framing);

  Internals->RenderEngineGL->Render(UsdWriter.GetSceneStage()->GetPseudoRoot(), glRenderParams);

#else

  Internals->SceneDelegate->SetTime(pxr::UsdTimeCode(0.0f));

  //pxr::HdxRenderTaskParams taskParams;

  // Set up offscreen rendering
  //if(Internals->RenderBuffer)
  //{
  //  pxr::HdRenderPassAovBindingVector aovBindings;
  //  pxr::HdRenderPassAovBinding colorBinding;
  //  colorBinding.aovName = pxr::HdAovTokens->color; // Standard color AOV
  //  colorBinding.renderBuffer = Internals->RenderBuffer; // HdRenderBuffer pointer
  //  colorBinding.clearValue = pxr::VtValue(GfVec4f(0.0f, 0.0f, 0.0f, 1.0f));
  //  aovBindings.push_back(colorBinding);
  //  //renderPassState->SetAovBindings(aovBindings);
  //  taskParams.aovBindings.push_back(colorBinding);
  //}

  //renderPass->Sync();
  //Internals->RenderDelegate->CommitResources(&Internals->RenderIndex->GetChangeTracker());
  //renderPass->Execute(renderPassState, {});

  // Rendertasks require hdx, which require opengl, so this cannot be used yet
  //auto& setupTaskId = Internals->RenderSetupTaskId;
  //auto& taskId = Internals->RenderTaskId;
  //auto& sceneDelegate = Internals->SceneDelegate;

  //Internals->RenderIndex->InsertTask<pxr::HdxRenderSetupTask>(sceneDelegate, setupTaskId);
  //Internals->RenderIndex->InsertTask<pxr::HdxRenderTask>(sceneDelegate, taskId);
  //auto renderSetupTask = dynamic_cast<pxr::HdxRenderSetupTask*>(
  //  Internals->RenderIndex->GetTask(setupTaskId).get());
  //auto renderTask = Internals->RenderIndex->GetTask(taskId);
  //std::vector<pxr::HdTaskSharedPtr> tasks = {renderTask};

  //taskParams.useAovMultiSample = false;
  //taskParams.resolveAovMultiSample = false;
  //taskParams.framing = framing;
  //sceneDelegate->UpdateTask(taskId, HdTokens->params, pxr::VtValue(taskParams));
  //sceneDelegate->UpdateTask(taskId, HdTokens->collection, pxr::VtValue(collection));
  //renderSetupTask->SyncParams(sceneDelegate, taskParams);

  auto& taskController = Internals->TaskController;
  taskController->SetCollection(collection);
  taskController->SetFraming(framing);

  //Internals->RenderEngine->SetTaskContextData(pxr::HdxTokens->renderPassState, pxr::VtValue(renderPassState)); // This is done by HdxRenderSetupTask::Execute
  pxr::HdxSelectionTrackerSharedPtr emptySelection = std::make_shared<pxr::HdxSelectionTracker>();
  Internals->RenderEngine->SetTaskContextData(pxr::HdxTokens->selectionState, VtValue(emptySelection));

  Internals->LightingContextForOpenGLState->SetUseLighting(true);
  taskController->SetLightingState(Internals->LightingContextForOpenGLState);
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
    while(!isConverged)
      isConverged = renderBuffer->IsConverged();
  }
  return isConverged;
}

void* UsdBridgeRenderer::MapFrame()
{
  if(!Internals->Initialized)
    return nullptr;

#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
  //auto colorTextureHandle = Internals->RenderEngineGL->GetAovTexture(pxr::HdAovTokens->color);
//
  //if (!colorTextureHandle)
  //    return nullptr;
//
  //Internals->CheckTexture = colorTextureHandle->GetDescriptor();
//
  //Internals->CheckSize = 0;
  //Internals->MappedColorTextureBuffer = pxr::HdStTextureUtils::HgiTextureReadback(
  //    Internals->RenderEngineGL->GetHgi(), colorTextureHandle, &Internals->CheckSize);
  //void* retValue = Internals->MappedColorTextureBuffer.get();

  pxr::HdRenderBuffer* renderBuffer =
    Internals->RenderEngineGL->GetAovRenderBuffer(pxr::HdAovTokens->color);

#else

  pxr::HdRenderBuffer* renderBuffer =
    Internals->TaskController->GetRenderOutput(pxr::HdAovTokens->color);

#endif

  renderBuffer->Resolve();

  Internals->CheckWidth = renderBuffer->GetWidth();
  Internals->CheckHeight = renderBuffer->GetHeight();
  Internals->CheckFormat = renderBuffer->GetFormat();
  Internals->CheckMSAA = renderBuffer->IsMultiSampled();
  void* retValue = renderBuffer->Map();



  pxr::GLF_POST_PENDING_GL_ERRORS();
  return retValue;
}

void UsdBridgeRenderer::UnmapFrame()
{
  if(!Internals->Initialized)
    return;

  pxr::GLF_POST_PENDING_GL_ERRORS();
#ifdef USDBRIDGE_RENDERER_USE_ENGINEGL
  pxr::HdRenderBuffer* renderBuffer =
    Internals->RenderEngineGL->GetAovRenderBuffer(pxr::HdAovTokens->color);
#else
  pxr::HdRenderBuffer* renderBuffer =
      Internals->TaskController->GetRenderOutput(pxr::HdAovTokens->color);
#endif
  if(renderBuffer)
    renderBuffer->Unmap();
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

void UsdBridgeRenderer::Render(uint32_t width, uint32_t height, double time)
{
}

bool UsdBridgeRenderer::FrameReady(bool wait)
{
  return true;
}

void* UsdBridgeRenderer::MapFrame()
{
  return nullptr;
}

void UsdBridgeRenderer::UnmapFrame()
{
}
#endif
