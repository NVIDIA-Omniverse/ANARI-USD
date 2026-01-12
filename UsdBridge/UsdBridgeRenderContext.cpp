// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeRenderContext.h"
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
#include <pxr/imaging/hd/pluginRenderDelegateUniqueHandle.h>
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
#include <pxr/usdImaging/usdImagingGL/engine.h>

#include <vector>
#include <memory>
#include <string>

#define USDBRIDGE_RENDERER_USE_COLORTEXTURE



// =============================================================================
// Common helper functions and types
// =============================================================================

namespace
{
    UsdBridgeType ConvertRenderBufferFormat(HdFormat hdFormat)
    {
        switch (hdFormat)
        {
        case HdFormatUNorm8Vec4:
            return UsdBridgeType::UCHAR4;
        case HdFormatSNorm8Vec4:
            return UsdBridgeType::CHAR4;
        case HdFormatFloat32Vec4:
            return UsdBridgeType::FLOAT4;
        default:
            return UsdBridgeType::UNDEFINED;
        }
    }

    UsdBridgeType ConvertTextureFormat(HgiFormat hgiFormat)
    {
        switch (hgiFormat)
        {
        case HgiFormatUNorm8Vec4:
            return UsdBridgeType::UCHAR4;
        case HgiFormatSNorm8Vec4:
            return UsdBridgeType::UCHAR_SRGB_RGBA;
        case HgiFormatFloat32Vec4:
            return UsdBridgeType::FLOAT4;
        default:
            return UsdBridgeType::UNDEFINED;
        }
    }
}

// Struct to hold mapped frame state (used by both context types)
struct MappedFrameState
{
#ifdef USDBRIDGE_RENDERER_USE_COLORTEXTURE
    HdStTextureUtils::AlignedBuffer<uint8_t> MappedColorTextureBuffer;
    HgiTextureDesc MappedTextureDesc;
    size_t MappedSize = 0;
#endif
};

// Struct to manage render settings prims (used by both context types)
struct RenderSettingsState
{
    std::string SettingsPath;
    std::string ProductPath;
    std::string VarPath;

    UsdRenderSettings Settings;
    UsdRenderProduct Product;

    void InitializePaths(const SdfPath& contextId)
    {
        std::string contextName = contextId.GetName();
        SettingsPath = "/Render/" + contextName + "/Settings";
        ProductPath = "/Render/" + contextName + "/Product";
        VarPath = "/Render/" + contextName + "/Vars/LdrColor";
    }

    void CreatePrims(UsdStageRefPtr stage)
    {
        if (!stage)
            return;

        // Create RenderSettings prim
        Settings = GetOrDefinePrim<UsdRenderSettings>(stage, SdfPath(SettingsPath));
        Settings.CreateResolutionAttr();
        Settings.CreateCameraRel();

        // Create RenderProduct prim
        Product = GetOrDefinePrim<UsdRenderProduct>(stage, SdfPath(ProductPath));
        Product.CreateResolutionAttr();
        Product.CreateCameraRel();
        Product.CreateOrderedVarsRel();

        // Create RenderVar prim and link to product
        SdfPath renderVarSdf(VarPath);
        UsdRenderVar renderVarPrim = GetOrDefinePrim<UsdRenderVar>(stage, renderVarSdf);
        renderVarPrim.CreateSourceNameAttr(VtValue(std::string("LdrColor")));
        Product.GetOrderedVarsRel().AddTarget(renderVarSdf);
    }

    void SetResolution(uint32_t width, uint32_t height)
    {
        if (Settings)
        {
            Settings.GetResolutionAttr().Set(GfVec2i((int)width, (int)height));
        }
        if (Product)
        {
            Product.GetResolutionAttr().Set(GfVec2i((int)width, (int)height));
        }
    }

    void SetCameraPath(const SdfPath& cameraPath)
    {
        if (Settings)
        {
            Settings.GetCameraRel().ClearTargets(false);
            Settings.GetCameraRel().AddTarget(cameraPath);
        }
        if (Product)
        {
            Product.GetCameraRel().ClearTargets(false);
            Product.GetCameraRel().AddTarget(cameraPath);
        }
    }
};

// Common base data for render context internals
struct RenderContextInternalsBase
{
    SdfPath CameraPath;
    SdfPath WorldPath;
    RenderSettingsState RenderSettingsData;
    uint32_t CachedWidth = 0;
    uint32_t CachedHeight = 0;
    MappedFrameState FrameState;
    bool Initialized = false;

    void InitializeBase(const SdfPath& contextId)
    {
        RenderSettingsData.InitializePaths(contextId);
    }

    // Check if resolution changed and update cache
    bool ResolutionChanged(uint32_t width, uint32_t height) const
    {
        return width != CachedWidth || height != CachedHeight;
    }

    // Configure AOV settings on a task controller
    void ConfigureAovSettings(HdxTaskController* taskController)
    {
        HdAovDescriptor colorSettings = taskController->GetRenderOutputSettings(HdAovTokens->color);
        colorSettings.multiSampled = false;
        colorSettings.clearValue = GfVec4f(0.0f, 1.0f, 0.0f, 1.0f);
        if (colorSettings.format != HdFormatUNorm8Vec4 &&
            colorSettings.format != HdFormatFloat32Vec4 &&
            colorSettings.format != HdFormatSNorm8Vec4)
        {
            colorSettings.format = HdFormatUNorm8Vec4;
        }
        taskController->SetRenderOutputSettings(HdAovTokens->color, colorSettings);

        HdAovDescriptor depthSettings = taskController->GetRenderOutputSettings(HdAovTokens->depth);
        depthSettings.multiSampled = false;
        taskController->SetRenderOutputSettings(HdAovTokens->depth, depthSettings);
    }

    // Update resolution and save - call after setting buffer size on engine/controller
    void UpdateResolutionState(uint32_t width, uint32_t height, UsdBridgeUsdWriter& usdWriter)
    {
        RenderSettingsData.SetResolution(width, height);
        usdWriter.SaveScene();
        CachedWidth = width;
        CachedHeight = height;
    }
};

// Common frame operations that work with any source of render buffer / texture
namespace FrameOps
{
    bool FrameReady(HdRenderBuffer* renderBuffer)
    {
        if (!renderBuffer)
            return true;
        bool isConverged =  renderBuffer->IsConverged();
        return true;
    }

#ifdef USDBRIDGE_RENDERER_USE_COLORTEXTURE
    void* MapFrameTexture(
        Hgi* hgi,
        HgiTextureHandle colorTextureHandle,
        MappedFrameState& state,
        UsdBridgeType& returnFormat)
    {
        if (!colorTextureHandle)
            return nullptr;

        state.MappedTextureDesc = colorTextureHandle->GetDescriptor();
        state.MappedSize = 0;
        state.MappedColorTextureBuffer = HdStTextureUtils::HgiTextureReadback(
            hgi, colorTextureHandle, &state.MappedSize);

        returnFormat = ConvertTextureFormat(state.MappedTextureDesc.format);

        GLF_POST_PENDING_GL_ERRORS();
        return state.MappedColorTextureBuffer.get();
    }
#else
    void* MapFrameBuffer(HdRenderBuffer* renderBuffer, UsdBridgeType& returnFormat)
    {
        if (!renderBuffer)
            return nullptr;

        renderBuffer->Resolve();
        returnFormat = ConvertRenderBufferFormat(renderBuffer->GetFormat());

        void* retValue = renderBuffer->Map();
        GLF_POST_PENDING_GL_ERRORS();
        return retValue;
    }

    void UnmapFrameBuffer(HdRenderBuffer* renderBuffer)
    {
        if (renderBuffer)
            renderBuffer->Unmap();
    }
#endif
}

// =============================================================================
// UsdBridgeRendererCore::Internals
// =============================================================================

class UsdBridgeRendererCore::Internals
{
public:
    ~Internals()
    {
        delete RenderEngine;
        delete SceneDelegate;
        if (RenderIndex)
        {
            delete RenderIndex;
        }
        delete RenderDriver;
        // RenderDelegateHandle automatically destroys delegate and keeps plugin alive
        RenderDelegateHandle = nullptr;
    }

    void Initialize(const char* rendererPluginName, UsdStageRefPtr stage)
    {
        HdRendererPluginHandle rendererPlugin = 
            HdRendererPluginRegistry::GetInstance().GetOrCreateRendererPlugin(
                TfToken(rendererPluginName));

        if (rendererPlugin)
        {
            RenderDelegateHandle = rendererPlugin->CreateDelegate();

            if (RenderDelegateHandle)
            {
                RenderHgi = Hgi::CreatePlatformDefaultHgi();
                RenderDriver = new HdDriver{HgiTokens->renderDriver, VtValue(RenderHgi.get())};
                HdDriverVector drivers{RenderDriver};
                RenderIndex = HdRenderIndex::New(RenderDelegateHandle.Get(), drivers);

                SceneDelegate = new UsdImagingDelegate(RenderIndex, SdfPath::AbsoluteRootPath());
                SceneDelegate->Populate(stage->GetPseudoRoot());

                RenderEngine = new HdEngine();

                Initialized = true;
            }
        }
    }

    HdPluginRenderDelegateUniqueHandle RenderDelegateHandle;
    HgiUniquePtr RenderHgi = nullptr;
    HdDriver* RenderDriver = nullptr;
    HdRenderIndex* RenderIndex = nullptr;
    UsdImagingDelegate* SceneDelegate = nullptr;
    HdEngine* RenderEngine = nullptr;

    bool Initialized = false;
};

// =============================================================================
// UsdBridgeRendererCore
// =============================================================================

UsdBridgeRendererCore::UsdBridgeRendererCore(UsdBridgeUsdWriter& usdWriter)
    : UsdWriter(usdWriter)
    , Data(std::make_unique<Internals>())
{
}

UsdBridgeRendererCore::~UsdBridgeRendererCore() = default;

void UsdBridgeRendererCore::Initialize(const char* rendererPluginName)
{
    if (Data->Initialized)
        return;

    UsdStageRefPtr stage = UsdWriter.GetSceneStage();
    if (stage)
    {
        Data->Initialize(rendererPluginName, stage);
    }
}

bool UsdBridgeRendererCore::IsInitialized() const
{
    return Data->Initialized;
}

HdRenderIndex* UsdBridgeRendererCore::GetRenderIndex()
{
    return Data->RenderIndex;
}

HdEngine* UsdBridgeRendererCore::GetEngine()
{
    return Data->RenderEngine;
}

Hgi* UsdBridgeRendererCore::GetHgi()
{
    return Data->RenderHgi.get();
}

UsdImagingDelegate* UsdBridgeRendererCore::GetSceneDelegate()
{
    return Data->SceneDelegate;
}

UsdStageRefPtr UsdBridgeRendererCore::GetStage()
{
    return UsdWriter.GetSceneStage();
}

UsdBridgeUsdWriter& UsdBridgeRendererCore::GetUsdWriter()
{
    return UsdWriter;
}

// =============================================================================
// UsdBridgeRenderContextShared (internal implementation class)
// =============================================================================

class UsdBridgeRenderContextShared : public UsdBridgeRenderContext
{
public:
    UsdBridgeRenderContextShared(UsdBridgeRendererCore& core, const SdfPath& contextId);
    ~UsdBridgeRenderContextShared() override;

    void SetCameraPath(const SdfPath& cameraPath) override;
    void SetWorldPath(const SdfPath& worldPath) override;
    void Render(uint32_t width, uint32_t height, double timeStep) override;
    bool FrameReady(bool wait) override;
    void* MapFrame(UsdBridgeType& returnFormat) override;
    void UnmapFrame() override;
    bool IsInitialized() const override;

private:
    class Internals;
    std::unique_ptr<Internals> ContextData;
    UsdBridgeRendererCore& Core;
};

class UsdBridgeRenderContextShared::Internals : public RenderContextInternalsBase
{
public:
    Internals(const SdfPath& contextId)
        : TaskControllerId(contextId)
    {
        InitializeBase(contextId);
    }

    ~Internals()
    {
        delete TaskController;
    }

    void Initialize(HdRenderIndex* renderIndex, UsdStageRefPtr stage)
    {
        TaskController = new HdxTaskController(renderIndex, TaskControllerId);
        TaskController->SetRenderOutputs({HdAovTokens->color});
        LightingContext = GlfSimpleLightingContext::New();

        RenderSettingsData.CreatePrims(stage);

        Initialized = true;
    }

    void SetResolution(uint32_t width, uint32_t height, UsdBridgeUsdWriter& usdWriter)
    {
        if (!ResolutionChanged(width, height))
            return;

        TaskController->SetRenderBufferSize(GfVec2i(width, height));
        ConfigureAovSettings(TaskController);
        UpdateResolutionState(width, height, usdWriter);
    }

    SdfPath TaskControllerId;
    HdxTaskController* TaskController = nullptr;
    GlfSimpleLightingContextRefPtr LightingContext;
};

UsdBridgeRenderContextShared::UsdBridgeRenderContextShared(UsdBridgeRendererCore& core, const SdfPath& contextId)
    : Core(core)
    , ContextData(std::make_unique<Internals>(contextId))
{
    if (Core.IsInitialized())
    {
        ContextData->Initialize(Core.GetRenderIndex(), Core.GetStage());
    }
}

UsdBridgeRenderContextShared::~UsdBridgeRenderContextShared() = default;

void UsdBridgeRenderContextShared::SetCameraPath(const SdfPath& cameraPath)
{
    ContextData->CameraPath = cameraPath;
    ContextData->RenderSettingsData.SetCameraPath(cameraPath);
    Core.GetUsdWriter().SaveScene();

    if (ContextData->TaskController)
    {
        ContextData->TaskController->SetCameraPath(cameraPath);
    }
}

void UsdBridgeRenderContextShared::SetWorldPath(const SdfPath& worldPath)
{
    ContextData->WorldPath = worldPath;
}

void UsdBridgeRenderContextShared::Render(uint32_t width, uint32_t height, double timeStep)
{
    if (!ContextData->Initialized)
        return;

    Core.GetSceneDelegate()->SetTime(UsdTimeCode(timeStep));

    ContextData->SetResolution(width, height, Core.GetUsdWriter());

    HdReprSelector reprSelector(HdReprTokens->smoothHull);
    HdRprimCollection collection(HdTokens->geometry, reprSelector);

    // Filter to specific world if set
    if (!ContextData->WorldPath.IsEmpty())
    {
        collection.SetRootPath(ContextData->WorldPath);
    }

    CameraUtilFraming framing(GfRect2i(GfVec2i(0, 0), width, height));

    ContextData->TaskController->SetCollection(collection);
    ContextData->TaskController->SetFraming(framing);

    HdxRenderTaskParams taskParams;
    taskParams.enableLighting = true;
    taskParams.enableIdRender = false;
    taskParams.depthBiasUseDefault = true;
    taskParams.depthFunc = HdCmpFuncLess;
    taskParams.cullStyle = HdCullStyleNothing;
    taskParams.alphaThreshold = 0.1f;
    taskParams.enableSceneMaterials = true;
    taskParams.enableSceneLights = true;

    ContextData->TaskController->SetRenderParams(taskParams);
    ContextData->TaskController->SetEnableSelection(false);

    HdxColorCorrectionTaskParams hdParams;
    hdParams.colorCorrectionMode = HdxColorCorrectionTokens->sRGB;
    ContextData->TaskController->SetColorCorrectionParams(hdParams);

    HdxSelectionTrackerSharedPtr emptySelection = std::make_shared<HdxSelectionTracker>();
    Core.GetEngine()->SetTaskContextData(HdxTokens->selectionState, VtValue(emptySelection));
    Core.GetEngine()->SetTaskContextData(HdxTokens->lightingContext, VtValue(ContextData->LightingContext));

    HdTaskSharedPtrVector tasks = ContextData->TaskController->GetRenderingTasks();
    Core.GetEngine()->Execute(Core.GetRenderIndex(), &tasks);
}

bool UsdBridgeRenderContextShared::FrameReady(bool wait)
{
    if (!ContextData->Initialized)
        return true;

    return FrameOps::FrameReady(ContextData->TaskController->GetRenderOutput(HdAovTokens->color));
}

void* UsdBridgeRenderContextShared::MapFrame(UsdBridgeType& returnFormat)
{
    if (!ContextData->Initialized)
        return nullptr;

#ifdef USDBRIDGE_RENDERER_USE_COLORTEXTURE
    VtValue aov;
    HgiTextureHandle colorTextureHandle;

    if (Core.GetEngine()->GetTaskContextData(HdAovTokens->color, &aov))
    {
        if (aov.IsHolding<HgiTextureHandle>())
        {
            colorTextureHandle = aov.Get<HgiTextureHandle>();
        }
    }

    return FrameOps::MapFrameTexture(Core.GetHgi(), colorTextureHandle, ContextData->FrameState, returnFormat);
#else
    return FrameOps::MapFrameBuffer(ContextData->TaskController->GetRenderOutput(HdAovTokens->color), returnFormat);
#endif
}

void UsdBridgeRenderContextShared::UnmapFrame()
{
    if (!ContextData->Initialized)
        return;

    GLF_POST_PENDING_GL_ERRORS();

#ifndef USDBRIDGE_RENDERER_USE_COLORTEXTURE
    FrameOps::UnmapFrameBuffer(ContextData->TaskController->GetRenderOutput(HdAovTokens->color));
#endif
    // When using COLORTEXTURE, the buffer is owned by FrameState, nothing to unmap
}

bool UsdBridgeRenderContextShared::IsInitialized() const
{
    return ContextData->Initialized;
}

// =============================================================================
// UsdBridgeRenderContextStandalone (internal implementation class - debug fallback)
// =============================================================================

// Extended UsdImagingGLEngine to expose task controller
class UsdBridgeStandaloneEngine : public UsdImagingGLEngine
{
public:
    UsdBridgeStandaloneEngine(UsdImagingGLEngine::Parameters params)
        : UsdImagingGLEngine(params)
    {
    }

    HdxTaskController* GetTaskController()
    {
        return _GetTaskController();
    }
};

class UsdBridgeRenderContextStandalone : public UsdBridgeRenderContext
{
public:
    UsdBridgeRenderContextStandalone(UsdBridgeUsdWriter& usdWriter, const char* rendererPluginName, const SdfPath& contextId);
    ~UsdBridgeRenderContextStandalone() override;

    void SetCameraPath(const SdfPath& cameraPath) override;
    void SetWorldPath(const SdfPath& worldPath) override;
    void Render(uint32_t width, uint32_t height, double timeStep) override;
    bool FrameReady(bool wait) override;
    void* MapFrame(UsdBridgeType& returnFormat) override;
    void UnmapFrame() override;
    bool IsInitialized() const override;

private:
    class Internals;
    std::unique_ptr<Internals> ContextData;
    UsdBridgeUsdWriter& UsdWriter;
};

class UsdBridgeRenderContextStandalone::Internals : public RenderContextInternalsBase
{
public:
    Internals(const SdfPath& contextId)
    {
        InitializeBase(contextId);
    }

    ~Internals()
    {
        delete RenderEngineGL;
    }

    void Initialize(const char* rendererPluginName, UsdStageRefPtr stage)
    {
        UsdImagingGLEngine::Parameters initParams;
        initParams.rendererPluginId = TfToken(rendererPluginName);
        RenderEngineGL = new UsdBridgeStandaloneEngine(initParams);
        RenderEngineGL->SetRendererAov(HdAovTokens->color);
        RenderEngineGL->SetOverrideWindowPolicy(CameraUtilMatchVertically);

        RenderSettingsData.CreatePrims(stage);

        Initialized = true;
    }

    void SetResolution(uint32_t width, uint32_t height, UsdBridgeUsdWriter& usdWriter)
    {
        if (!ResolutionChanged(width, height))
            return;

        RenderEngineGL->SetRenderBufferSize(GfVec2i(width, height));
        ConfigureAovSettings(RenderEngineGL->GetTaskController());
        UpdateResolutionState(width, height, usdWriter);
    }

    UsdBridgeStandaloneEngine* RenderEngineGL = nullptr;
};

UsdBridgeRenderContextStandalone::UsdBridgeRenderContextStandalone(
    UsdBridgeUsdWriter& usdWriter, const char* rendererPluginName, const SdfPath& contextId)
    : UsdWriter(usdWriter)
    , ContextData(std::make_unique<Internals>(contextId))
{
    UsdStageRefPtr stage = UsdWriter.GetSceneStage();
    if (stage)
    {
        ContextData->Initialize(rendererPluginName, stage);
    }
}

UsdBridgeRenderContextStandalone::~UsdBridgeRenderContextStandalone() = default;

void UsdBridgeRenderContextStandalone::SetCameraPath(const SdfPath& cameraPath)
{
    ContextData->CameraPath = cameraPath;
    ContextData->RenderSettingsData.SetCameraPath(cameraPath);
    UsdWriter.SaveScene();

    if (ContextData->RenderEngineGL)
    {
        ContextData->RenderEngineGL->SetCameraPath(cameraPath);
    }
}

void UsdBridgeRenderContextStandalone::SetWorldPath(const SdfPath& worldPath)
{
    ContextData->WorldPath = worldPath;
}

void UsdBridgeRenderContextStandalone::Render(uint32_t width, uint32_t height, double timeStep)
{
    if (!ContextData->Initialized)
        return;

    ContextData->SetResolution(width, height, UsdWriter);

    CameraUtilFraming framing(GfRect2i(GfVec2i(0, 0), width, height));

    UsdImagingGLRenderParams glRenderParams;
    glRenderParams.frame = UsdTimeCode(timeStep);
    glRenderParams.clearColor = GfVec4f(0.0f, 1.0f, 0.0f, 1.0f);
    glRenderParams.gammaCorrectColors = false;
    glRenderParams.enableLighting = true;
    glRenderParams.enableSceneLights = true;
    glRenderParams.enableSceneMaterials = true;
    glRenderParams.enableSampleAlphaToCoverage = true;
    glRenderParams.enableIdRender = false;
    glRenderParams.applyRenderState = true;
    glRenderParams.highlight = false;
    glRenderParams.colorCorrectionMode = HdxColorCorrectionTokens->sRGB;

    ContextData->RenderEngineGL->SetFraming(framing);

    // Determine root prim for rendering - use world path if set, otherwise stage root
    UsdStageRefPtr stage = UsdWriter.GetSceneStage();
    UsdPrim rootPrim;
    if (!ContextData->WorldPath.IsEmpty())
    {
        rootPrim = stage->GetPrimAtPath(ContextData->WorldPath);
    }
    if (!rootPrim)
    {
        rootPrim = stage->GetPseudoRoot();
    }

    ContextData->RenderEngineGL->Render(rootPrim, glRenderParams);
}

bool UsdBridgeRenderContextStandalone::FrameReady(bool wait)
{
    if (!ContextData->Initialized)
        return true;

    return FrameOps::FrameReady(ContextData->RenderEngineGL->GetAovRenderBuffer(HdAovTokens->color));
}

void* UsdBridgeRenderContextStandalone::MapFrame(UsdBridgeType& returnFormat)
{
    if (!ContextData->Initialized)
        return nullptr;

#ifdef USDBRIDGE_RENDERER_USE_COLORTEXTURE
    return FrameOps::MapFrameTexture(
        ContextData->RenderEngineGL->GetHgi(),
        ContextData->RenderEngineGL->GetAovTexture(HdAovTokens->color),
        ContextData->FrameState,
        returnFormat);
#else
    return FrameOps::MapFrameBuffer(ContextData->RenderEngineGL->GetAovRenderBuffer(HdAovTokens->color), returnFormat);
#endif
}

void UsdBridgeRenderContextStandalone::UnmapFrame()
{
    if (!ContextData->Initialized)
        return;

    GLF_POST_PENDING_GL_ERRORS();

#ifndef USDBRIDGE_RENDERER_USE_COLORTEXTURE
    FrameOps::UnmapFrameBuffer(ContextData->RenderEngineGL->GetAovRenderBuffer(HdAovTokens->color));
#endif
    // When using COLORTEXTURE, the buffer is owned by FrameState, nothing to unmap
}

bool UsdBridgeRenderContextStandalone::IsInitialized() const
{
    return ContextData->Initialized;
}

// =============================================================================
// Factory function
// =============================================================================

std::unique_ptr<UsdBridgeRenderContext> CreateRenderContext(
    RenderContextMode mode,
    UsdBridgeRendererCore* core,
    UsdBridgeUsdWriter& usdWriter,
    const char* rendererPluginName,
    const SdfPath& contextId)
{
    switch (mode)
    {
    case RenderContextMode::Shared:
        if (core && core->IsInitialized())
        {
            return std::make_unique<UsdBridgeRenderContextShared>(*core, contextId);
        }
        // Fall through to standalone if core not available
        [[fallthrough]];

    case RenderContextMode::Standalone:
    default:
        return std::make_unique<UsdBridgeRenderContextStandalone>(usdWriter, rendererPluginName, contextId);
    }
}

#else

// =============================================================================
// Stub implementations when USD_DEVICE_RENDERING_ENABLED is not defined
// =============================================================================

class UsdBridgeRendererCore::Internals
{
public:
    bool Initialized = false;
};

UsdBridgeRendererCore::UsdBridgeRendererCore(UsdBridgeUsdWriter& usdWriter)
    : UsdWriter(usdWriter)
    , Data(std::make_unique<Internals>())
{
}

UsdBridgeRendererCore::~UsdBridgeRendererCore() = default;

void UsdBridgeRendererCore::Initialize(const char*) {}

bool UsdBridgeRendererCore::IsInitialized() const { return false; }

pxr::UsdStageRefPtr UsdBridgeRendererCore::GetStage()
{
    return UsdWriter.GetSceneStage();
}

// Stub render context that does nothing
class UsdBridgeRenderContextStub : public UsdBridgeRenderContext
{
public:
    void SetCameraPath(const pxr::SdfPath&) override {}
    void SetWorldPath(const pxr::SdfPath&) override {}
    void Render(uint32_t, uint32_t, double) override {}
    bool FrameReady(bool) override { return true; }
    void* MapFrame(UsdBridgeType&) override { return nullptr; }
    void UnmapFrame() override {}
    bool IsInitialized() const override { return false; }
};

std::unique_ptr<UsdBridgeRenderContext> CreateRenderContext(
    RenderContextMode,
    UsdBridgeRendererCore*,
    UsdBridgeUsdWriter&,
    const char*,
    const pxr::SdfPath&)
{
    return std::make_unique<UsdBridgeRenderContextStub>();
}

#endif
