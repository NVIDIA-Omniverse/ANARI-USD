// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeRenderer.h"
#include "UsdBridgeUsdWriter.h"
#include "UsdBridgeUsdWriter_Common.h"

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
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/usd/usdRender/settings.h>
#include <pxr/usd/usdRender/tokens.h>
#include <pxr/usdImaging/usdImaging/delegate.h>

#include <vector>
#include <memory>
#include <string>

class UsdBridgeRendererInternals
{
public:
  ~UsdBridgeRendererInternals()
  {
    DestroyRenderBuffers();
    RendererPlugin = nullptr;
    SetDelegatesAndIndex(nullptr);
  }

  void SetDelegatesAndIndex(pxr::UsdStageRefPtr stage)
  {
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

      SceneDelegate = new pxr::UsdImagingDelegate(RenderIndex, pxr::SdfPath::AbsoluteRootPath());
      SceneDelegate->Populate(stage->GetPseudoRoot());
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
    pxr::SdfPath renderBufferPath("/renderBuffer");
    RenderIndex->InsertBprim(
      pxr::HdPrimTypeTokens->renderBuffer, SceneDelegate, renderBufferPath);
    pxr::HdBprim* bprim = RenderIndex->GetBprim(pxr::HdPrimTypeTokens->renderBuffer, renderBufferPath);
    RenderBuffer = dynamic_cast<pxr::HdRenderBuffer*>(bprim);
    if(!RenderBuffer)
    {
      //Throw error
    }
  }

  void DestroyRenderBuffers()
  {
    RenderBuffer = nullptr;
  }

  void ChangeResolution(uint32_t width, uint32_t height, UsdBridgeUsdWriter& usdWriter)
  {
    if(width != CachedWidth || height != CachedHeight)
    {
      RenderSettings.GetResolutionAttr().Set(pxr::GfVec2i((int)width, (int)height));
      usdWriter.SaveScene();

      // Create a renderbuffer if necessary
      if(!RenderBuffer)
        CreateRenderBuffers();

      // Allocate the buffer with desired dimensions and format
      pxr::GfVec3i dimensions(width, height, 1);
      pxr::HdFormat format = pxr::HdFormatUNorm8Vec4;
      bool multiSampled = false;
      RenderBuffer->Allocate(dimensions, format, multiSampled);

      CachedWidth = width;
      CachedHeight = height;
    }
  }

  pxr::HdRendererPluginHandle RendererPlugin = nullptr;
  pxr::HgiUniquePtr RenderHgi = nullptr;

  pxr::HdDriver* RenderDriver = nullptr;
  pxr::HdRenderIndex* RenderIndex = nullptr;
  pxr::HdRenderDelegate* RenderDelegate = nullptr;
  pxr::UsdImagingDelegate* SceneDelegate = nullptr;

  pxr::HdRenderBuffer* RenderBuffer = nullptr;

  pxr::SdfPath RenderTaskId { "UsdDeviceRender" };
  std::string RenderSettingsPath { "/Render/PrimarySettings" };
  pxr::UsdRenderSettings RenderSettings;

  const char* CachedRendererName = nullptr;
  uint32_t CachedWidth = 0;
  uint32_t CachedHeight = 0;

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
  if(Internals->CachedRendererName == rendererName)
    return;

  Internals->CachedRendererName = rendererName;

  pxr::UsdStageRefPtr stage = UsdWriter.GetSceneStage();

  if(stage)
  {
    Internals->CreateRenderSettings(stage); // This could be sit a more overarching initialization phase, but it's ok here as well

    Internals->RendererPlugin =
      pxr::HdRendererPluginRegistry::GetInstance().GetOrCreateRendererPlugin(TfToken("HdStormRendererPlugin"));
    if(Internals->RendererPlugin)
    {
      Internals->SetDelegatesAndIndex(stage);

      Internals->Initialized = true;
    }
    else
    {
      // throw error
    }
  }
}

void UsdBridgeRenderer::SetCameraPath(const SdfPath& cameraPath)
{
  Internals->RenderSettings.GetCameraRel().ClearTargets(false);
  Internals->RenderSettings.GetCameraRel().AddTarget(cameraPath);

  UsdWriter.SaveScene();
}

void UsdBridgeRenderer::Render(uint32_t width, uint32_t height, double timeStep)
{
  if(!Internals->Initialized)
    return;

  auto& taskId = Internals->RenderTaskId;

  pxr::HdReprSelector reprSelector(pxr::HdReprTokens->refined);
  pxr::HdRprimCollection collection(pxr::HdTokens->geometry, reprSelector);

  Internals->SceneDelegate->SetTime(pxr::UsdTimeCode(timeStep));
  
  auto renderPass = Internals->RenderDelegate->CreateRenderPass(Internals->RenderIndex, collection);
  auto renderPassState = Internals->RenderDelegate->CreateRenderPassState();
  
  //pxr::CameraUtilFraming framing(GfRect2i(GfVec2i(0, 0), 1024, 768))
  //renderPassState->SetFraming(framing);
  Internals->ChangeResolution(width, height, UsdWriter);

  // Set up offscreen rendering
  if(Internals->RenderBuffer)
  {
    pxr::HdRenderPassAovBindingVector aovBindings;
    pxr::HdRenderPassAovBinding colorBinding;
    colorBinding.aovName = pxr::HdAovTokens->color; // Standard color AOV
    colorBinding.renderBuffer = Internals->RenderBuffer; // HdRenderBuffer pointer
    colorBinding.clearValue = pxr::VtValue(GfVec4f(0.0f, 0.0f, 0.0f, 1.0f));
    aovBindings.push_back(colorBinding);
    renderPassState->SetAovBindings(aovBindings);
  }

  renderPass->Sync();
  renderPass->Execute(renderPassState, {});

  // Rendertasks require hdx, which require opengl, so this cannot be used yet
  //auto renderTask = std::make_shared<pxr::HdxRenderTask>(Internals->SceneDelegate, taskId);
  //std::vector<pxr::HdTaskSharedPtr> tasks = {renderTask};

  //pxr::HdEngine engine;
  //engine.Execute(Internals->RenderIndex, &tasks);
}

bool UsdBridgeRenderer::FrameReady(bool wait)
{
  if(!Internals->RenderBuffer)
    return true;

  bool isConverged = Internals->RenderBuffer->IsConverged();
  if(wait)
  {
    while(!isConverged)
      isConverged = Internals->RenderBuffer->IsConverged();

    Internals->RenderBuffer->Resolve();
  }
  return isConverged;
}

void* UsdBridgeRenderer::MapFrame()
{
  if(!Internals->RenderBuffer)
    return nullptr;

  size_t width = Internals->RenderBuffer->GetWidth();
  size_t height = Internals->RenderBuffer->GetHeight();
  return Internals->RenderBuffer->Map();
}

void UsdBridgeRenderer::UnmapFrame()
{
  if(Internals->RenderBuffer)
    Internals->RenderBuffer->Unmap();
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
