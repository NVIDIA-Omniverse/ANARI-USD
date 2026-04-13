// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeRenderContext.h"
#include "UsdBridgeGLContext.h"
#include "UsdBridgeUsdWriter.h"
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

//#define USDBRIDGE_RENDERER_USE_COLORTEXTURE



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

// Common base data for render context internals (Hydra only; USD render settings live in UsdRenderManager)
struct RenderContextInternalsBase
{
    SdfPath CameraPath;
    SdfPath WorldPath;
    uint32_t CachedWidth = 0;
    uint32_t CachedHeight = 0;
    MappedFrameState FrameState;
    bool Initialized = false;

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

    void CommitResolutionCache(uint32_t width, uint32_t height)
    {
        CachedWidth = width;
        CachedHeight = height;
    }

    uint32_t ResolutionWidth() const { return CachedWidth; }
    uint32_t ResolutionHeight() const { return CachedHeight; }
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

        GLF_POST_PENDING_GL_ERRORS();
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

    void Initialize(const char* rendererPluginName, UsdStageRefPtr stage, const UsdBridgeLogObject& logObj)
    {
        if (!EnsureOpenGLContext())
        {
            UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "Failed to create OpenGL context for Hydra renderer '" << rendererPluginName << "'");
            return;
        }

        HdRendererPluginHandle rendererPlugin = 
            HdRendererPluginRegistry::GetInstance().GetOrCreateRendererPlugin(
                TfToken(rendererPluginName));

        if (!rendererPlugin)
        {
            UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "Failed to find Hydra renderer plugin '" << rendererPluginName << "'");
            return;
        }

        RenderDelegateHandle = rendererPlugin->CreateDelegate();

        if (!RenderDelegateHandle)
        {
            UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "Failed to create render delegate for Hydra renderer '" << rendererPluginName << "'");
            return;
        }

        RenderHgi = Hgi::CreatePlatformDefaultHgi();
        if (!RenderHgi)
        {
            UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "Failed to create Hgi (GPU interface) for Hydra renderer '" << rendererPluginName << "'");
            RenderDelegateHandle = nullptr;
            return;
        }

        RenderDriver = new HdDriver{HgiTokens->renderDriver, VtValue(RenderHgi.get())};
        HdDriverVector drivers{RenderDriver};
        RenderIndex = HdRenderIndex::New(RenderDelegateHandle.Get(), drivers);

        if (!RenderIndex)
        {
            UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "Failed to create HdRenderIndex for Hydra renderer '" << rendererPluginName << "'");
            RenderDelegateHandle = nullptr;
            return;
        }

        SceneDelegate = new UsdImagingDelegate(RenderIndex, SdfPath::AbsoluteRootPath());
        SceneDelegate->Populate(stage->GetPseudoRoot());

        RenderEngine = new HdEngine();

        Initialized = true;
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
        Data->Initialize(rendererPluginName, stage, UsdWriter.LogObject);
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
    void SetRenderBufferSize(uint32_t width, uint32_t height) override;
    void Render(double timeStep) override;
    bool FrameReady(bool wait) override;
    void* MapFrame(UsdBridgeType& returnFormat) override;
    void UnmapFrame() override;
    bool IsInitialized() const override;

private:
    class InternalData;
    std::unique_ptr<InternalData> Internals;
    UsdBridgeRendererCore& Core;
};

class UsdBridgeRenderContextShared::InternalData : public RenderContextInternalsBase
{
public:
    InternalData(const SdfPath& contextId)
        : TaskControllerId(contextId)
    {
    }

    ~InternalData()
    {
        delete TaskController;
    }

    void Initialize(HdRenderIndex* renderIndex)
    {
        TaskController = new HdxTaskController(renderIndex, TaskControllerId);
        TaskController->SetRenderOutputs({HdAovTokens->color});
        TaskController->SetEnablePresentation(false);
        LightingContext = GlfSimpleLightingContext::New();

        Initialized = true;
    }

    void SetResolution(uint32_t width, uint32_t height)
    {
        if (!ResolutionChanged(width, height))
            return;

        TaskController->SetRenderBufferSize(GfVec2i(width, height));
        ConfigureAovSettings(TaskController);
        CommitResolutionCache(width, height);
    }

    SdfPath TaskControllerId;
    HdxTaskController* TaskController = nullptr;
    GlfSimpleLightingContextRefPtr LightingContext;
};

UsdBridgeRenderContextShared::UsdBridgeRenderContextShared(UsdBridgeRendererCore& core, const SdfPath& contextId)
    : Core(core)
    , Internals(std::make_unique<InternalData>(contextId))
{
    if (Core.IsInitialized())
    {
        Internals->Initialize(Core.GetRenderIndex());
    }
}

UsdBridgeRenderContextShared::~UsdBridgeRenderContextShared() = default;

void UsdBridgeRenderContextShared::SetCameraPath(const SdfPath& cameraPath)
{
    Internals->CameraPath = cameraPath;

    if (Internals->TaskController)
        Internals->TaskController->SetCameraPath(cameraPath);
}

void UsdBridgeRenderContextShared::SetWorldPath(const SdfPath& worldPath)
{
    Internals->WorldPath = worldPath;
}

void UsdBridgeRenderContextShared::SetRenderBufferSize(uint32_t width, uint32_t height)
{
    if (Internals->Initialized)
        Internals->SetResolution(width, height);
}

void UsdBridgeRenderContextShared::Render(double timeStep)
{
    if (!Internals->Initialized)
        return;

    UsdImagingDelegate* sceneDelegate = Core.GetSceneDelegate();
    sceneDelegate->ApplyPendingUpdates();
    sceneDelegate->SetTime(UsdTimeCode(timeStep));

    // Not sure if required, play around with this if issues arise
    //sceneDelegate->SetSceneMaterialsEnabled(true);
    //sceneDelegate->SetSceneLightsEnabled(true);

    HdReprSelector reprSelector(HdReprTokens->smoothHull);
    HdRprimCollection collection(HdTokens->geometry, reprSelector);

    // Filter to specific world if set
    if (!Internals->WorldPath.IsEmpty())
    {
        collection.SetRootPath(Internals->WorldPath);
    }

    CameraUtilFraming framing(GfRect2i(GfVec2i(0, 0), Internals->ResolutionWidth(), Internals->ResolutionHeight()));

    Internals->TaskController->SetCollection(collection);
    Internals->TaskController->SetFraming(framing);

    HdxRenderTaskParams taskParams;
    taskParams.enableLighting = true;
    taskParams.enableIdRender = false;
    taskParams.depthBiasUseDefault = true;
    taskParams.depthFunc = HdCmpFuncLess;
    taskParams.cullStyle = HdCullStyleNothing;
    taskParams.alphaThreshold = 0.1f;
    taskParams.enableSceneMaterials = true;
    taskParams.enableSceneLights = true;

    Internals->TaskController->SetRenderParams(taskParams);
    Internals->TaskController->SetEnableSelection(false);

    HdxColorCorrectionTaskParams hdParams;
    hdParams.colorCorrectionMode = HdxColorCorrectionTokens->sRGB;
    Internals->TaskController->SetColorCorrectionParams(hdParams);

    HdxSelectionTrackerSharedPtr emptySelection = std::make_shared<HdxSelectionTracker>();
    Core.GetEngine()->SetTaskContextData(HdxTokens->selectionState, VtValue(emptySelection));
    Core.GetEngine()->SetTaskContextData(HdxTokens->lightingContext, VtValue(Internals->LightingContext));

    GLF_POST_PENDING_GL_ERRORS();

    HdTaskSharedPtrVector tasks = Internals->TaskController->GetRenderingTasks();
    Core.GetEngine()->Execute(Core.GetRenderIndex(), &tasks);
}

bool UsdBridgeRenderContextShared::FrameReady(bool wait)
{
    if (!Internals->Initialized)
        return true;

    return FrameOps::FrameReady(Internals->TaskController->GetRenderOutput(HdAovTokens->color));
}

void* UsdBridgeRenderContextShared::MapFrame(UsdBridgeType& returnFormat)
{
    if (!Internals->Initialized)
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

    return FrameOps::MapFrameTexture(Core.GetHgi(), colorTextureHandle, Internals->FrameState, returnFormat);
#else
    return FrameOps::MapFrameBuffer(Internals->TaskController->GetRenderOutput(HdAovTokens->color), returnFormat);
#endif
}

void UsdBridgeRenderContextShared::UnmapFrame()
{
    if (!Internals->Initialized)
        return;

    GLF_POST_PENDING_GL_ERRORS();

#ifndef USDBRIDGE_RENDERER_USE_COLORTEXTURE
    FrameOps::UnmapFrameBuffer(Internals->TaskController->GetRenderOutput(HdAovTokens->color));
#endif
    // When using COLORTEXTURE, the buffer is owned by FrameState, nothing to unmap
}

bool UsdBridgeRenderContextShared::IsInitialized() const
{
    return Internals->Initialized;
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
    UsdBridgeRenderContextStandalone(
        UsdBridgeUsdWriter& usdWriter, const char* rendererPluginName, const SdfPath& contextId);
    ~UsdBridgeRenderContextStandalone() override;

    void SetCameraPath(const SdfPath& cameraPath) override;
    void SetWorldPath(const SdfPath& worldPath) override;
    void SetRenderBufferSize(uint32_t width, uint32_t height) override;
    void Render(double timeStep) override;
    bool FrameReady(bool wait) override;
    void* MapFrame(UsdBridgeType& returnFormat) override;
    void UnmapFrame() override;
    bool IsInitialized() const override;

private:
    class InternalData;
    std::unique_ptr<InternalData> Internals;
    UsdBridgeUsdWriter& UsdWriter;
};

class UsdBridgeRenderContextStandalone::InternalData : public RenderContextInternalsBase
{
public:
    InternalData(const SdfPath& contextId)
    {
        (void)contextId;
    }

    ~InternalData()
    {
        delete RenderEngineGL;
    }

    void Initialize(const char* rendererPluginName, const UsdBridgeLogObject& logObj)
    {
        if (!EnsureOpenGLContext())
        {
            UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "Failed to create OpenGL context for standalone Hydra renderer '" << rendererPluginName << "'");
            return;
        }

        UsdImagingGLEngine::Parameters initParams;
        initParams.rendererPluginId = TfToken(rendererPluginName);
        RenderEngineGL = new UsdBridgeStandaloneEngine(initParams);
        RenderEngineGL->SetRendererAov(HdAovTokens->color);
        RenderEngineGL->SetOverrideWindowPolicy(CameraUtilMatchVertically);

        Initialized = true;
    }

    void SetResolution(uint32_t width, uint32_t height)
    {
        if (!ResolutionChanged(width, height))
            return;

        RenderEngineGL->SetRenderBufferSize(GfVec2i(width, height));
        ConfigureAovSettings(RenderEngineGL->GetTaskController());
        CommitResolutionCache(width, height);
    }

    UsdBridgeStandaloneEngine* RenderEngineGL = nullptr;
};

UsdBridgeRenderContextStandalone::UsdBridgeRenderContextStandalone(
    UsdBridgeUsdWriter& usdWriter, const char* rendererPluginName, const SdfPath& contextId)
    : UsdWriter(usdWriter)
    , Internals(std::make_unique<InternalData>(contextId))
{
    Internals->Initialize(rendererPluginName, UsdWriter.LogObject);
}

UsdBridgeRenderContextStandalone::~UsdBridgeRenderContextStandalone() = default;

void UsdBridgeRenderContextStandalone::SetCameraPath(const SdfPath& cameraPath)
{
    Internals->CameraPath = cameraPath;

    if (Internals->RenderEngineGL)
        Internals->RenderEngineGL->SetCameraPath(cameraPath);
}

void UsdBridgeRenderContextStandalone::SetWorldPath(const SdfPath& worldPath)
{
    Internals->WorldPath = worldPath;
}

void UsdBridgeRenderContextStandalone::SetRenderBufferSize(uint32_t width, uint32_t height)
{
    if (Internals->Initialized)
        Internals->SetResolution(width, height);
}

void UsdBridgeRenderContextStandalone::Render(double timeStep)
{
    if (!Internals->Initialized)
        return;

    CameraUtilFraming framing(GfRect2i(GfVec2i(0, 0), Internals->ResolutionWidth(), Internals->ResolutionHeight()));

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

    Internals->RenderEngineGL->SetFraming(framing);

    // Determine root prim for rendering - use world path if set, otherwise stage root
    UsdStageRefPtr stage = UsdWriter.GetSceneStage();
    UsdPrim rootPrim;
    if (!Internals->WorldPath.IsEmpty())
    {
        rootPrim = stage->GetPrimAtPath(Internals->WorldPath);
    }
    if (!rootPrim)
    {
        rootPrim = stage->GetPseudoRoot();
    }

    Internals->RenderEngineGL->Render(rootPrim, glRenderParams);
}

bool UsdBridgeRenderContextStandalone::FrameReady(bool wait)
{
    if (!Internals->Initialized)
        return true;

    return FrameOps::FrameReady(Internals->RenderEngineGL->GetAovRenderBuffer(HdAovTokens->color));
}

void* UsdBridgeRenderContextStandalone::MapFrame(UsdBridgeType& returnFormat)
{
    if (!Internals->Initialized)
        return nullptr;

#ifdef USDBRIDGE_RENDERER_USE_COLORTEXTURE
    return FrameOps::MapFrameTexture(
        Internals->RenderEngineGL->GetHgi(),
        Internals->RenderEngineGL->GetAovTexture(HdAovTokens->color),
        Internals->FrameState,
        returnFormat);
#else
    return FrameOps::MapFrameBuffer(Internals->RenderEngineGL->GetAovRenderBuffer(HdAovTokens->color), returnFormat);
#endif
}

void UsdBridgeRenderContextStandalone::UnmapFrame()
{
    if (!Internals->Initialized)
        return;

    GLF_POST_PENDING_GL_ERRORS();

#ifndef USDBRIDGE_RENDERER_USE_COLORTEXTURE
    FrameOps::UnmapFrameBuffer(Internals->RenderEngineGL->GetAovRenderBuffer(HdAovTokens->color));
#endif
    // When using COLORTEXTURE, the buffer is owned by FrameState, nothing to unmap
}

bool UsdBridgeRenderContextStandalone::IsInitialized() const
{
    return Internals->Initialized;
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
    void SetRenderBufferSize(uint32_t, uint32_t) override {}
    void Render(double) override {}
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
