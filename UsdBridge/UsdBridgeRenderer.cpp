// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeRenderer.h"
#include "UsdBridgeUsdWriter.h"
#include "UsdBridgeUsdWriter_Common.h"

#ifdef USD_DEVICE_RENDERING_ENABLED

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
    RendererPlugin = nullptr;
    SetDelegatesAndIndex(nullptr);
  }

  void SetDelegatesAndIndex(UsdStageRefPtr stage)
  {
    delete SceneDelegate;
    delete RenderDelegate;
    delete RenderIndex;
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

  void CreateRenderSettings(UsdStageRefPtr stage)
  {
    stage->SetMetadata(pxr::UsdRenderTokens->renderSettingsPrimPath, RenderSettingsPath);
    RenderSettings = GetOrDefinePrim<pxr::UsdRenderSettings>(stage, RenderSettingsPath);
    RenderSettings.CreateResolutionAttr();
  }

  void ChangeResolution(uint32_t width, uint32_t height, UsdBridgeUsdWriter& usdWriter)
  {
    if(width != CachedWidth || height != CachedHeight)
    {
      RenderSettings.GetResolutionAttr().Set(pxr::GfVec2i((int)width, (int)height));
      usdWriter.SaveScene();

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

  pxr::SdfPath RenderTaskId { "UsdDeviceRender" };
  pxr::SdfPath RenderSettingsPath { "/Render/PrimarySettings" };
  pxr::UsdRenderSettings RenderSettings;

  const char* CachedRendererName = nullptr;
  uint32_t CachedWidth = 0;
  uint32_t CachedHeight = 0;
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

  UsdStageRefPtr stage = UsdWriter.GetSceneStage();

  Internals->RendererPlugin =
    pxr::HdRendererPluginRegistry::GetInstance().GetOrCreateRendererPlugin(TfToken(rendererName));
  if(stage && Internals->RendererPlugin)
  {
    Internals->CreateRenderSettings(stage); // This could be sit a more overarching initialization phase, but it's ok here as well
    Internals->SetDelegatesAndIndex(stage);
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
  auto& taskId = Internals->RenderTaskId;

  pxr::HdReprSelector reprSelector(pxr::HdReprTokens->smoothHull);
  pxr::HdRprimCollection collection(pxr::HdTokens->geometry, reprSelector);

  Internals->SceneDelegate->SetTime(pxr::UsdTimeCode(timeStep));
  
  auto renderPass = Internals->RenderDelegate->CreateRenderPass(Internals->RenderIndex, collection);
  auto renderPassState = Internals->RenderDelegate->CreateRenderPassState();
  
  //pxr::CameraUtilFraming framing(GfRect2i(GfVec2i(0, 0), 1024, 768))
  //renderPassState->SetFraming(framing);
  Internals->ChangeResolution(width, height, UsdWriter);

  renderPass->Sync();
  renderPass->Execute(renderPassState, {});

  // Rendertasks require hdx, which require opengl, so this cannot be used yet
  //auto renderTask = std::make_shared<pxr::HdxRenderTask>(Internals->SceneDelegate, taskId);
  //std::vector<pxr::HdTaskSharedPtr> tasks = {renderTask};

  //pxr::HdEngine engine;
  //engine.Execute(Internals->RenderIndex, &tasks);

  //auto renderBuffer = std::make_shared<pxr::HdRenderBuffer>();
  //renderBuffer->Allocate(pxr::GfVec3i(width, height, 1), pxr::HdFormatUNorm8Vec4, false);

  // Access pixel data
  //void* pixelData = renderBuffer->Map();
  //if (pixelData) 
  //{
  //    // Process pixel data here (e.g., copy it into an image buffer)
  //    renderBuffer->Unmap();
  //}
}

#else
UsdBridgeRenderer::UsdBridgeRenderer(const UsdBridgeUsdWriter& usdWriter)
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
#endif
