// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridge.h"

#include "UsdBridgeUsdWriter.h"
#include "UsdBridgeCaches.h"
#include "UsdBridgeRenderer.h"
#include "UsdBridgeDiagnosticMgrDelegate.h"

#include <string>
#include <memory>
#include <algorithm>

#ifdef USE_USDRT
#include "carb/ClientUtils.h"
CARB_GLOBALS("anariUsdBridge")
#endif

#define BRIDGE_CACHE Internals->Cache
#define BRIDGE_USDWRITER Internals->UsdWriter
#define BRIDGE_RENDERER Internals->HydraRenderer

namespace
{
  // Parent paths for class definitions (Class path)
  const char* const worldPathCp = "worlds";
  const char* const instancePathCp = "instances";
  const char* const groupPathCp = "groups";
  const char* const surfacePathCp = "surfaces";
  const char* const volumePathCp = "volumes";
  const char* const geometryPathCp = "geometries";
  const char* const fieldPathCp = "spatialfields";
  const char* const materialPathCp = "materials";
  const char* const samplerPathCp = "samplers";
  const char* const cameraPathCp = "cameras";
  const char* const lightPathCp = "lights";

  // Parent path extensions for references in parent classes (Reference path)
  const char* const instancePathRp = "instances";
  const char* const surfacePathRp = "surfaces";
  const char* const volumePathRp = "volumes";
  const char* const geometryPathRp = "geometry"; // created in surface parent class
  const char* const fieldPathRp = "spatialfield"; // created in volume parent class
  const char* const materialPathRp = "material"; // created in surface parent class
  const char* const samplerPathRp = "samplers"; // created in material parent class (separation from other UsdShader prims in material)
  const char* const protoShapePathRp = "protoshapes"; // created in geometry parent class
  const char* const protoGeometryPathRp = "protogeometries"; // created in geometry parent class

  // Postfixes for prim stage names, also used for manifests
  const char* const geomPrimStagePf = "_Geom";
  const char* const fieldPrimStagePf = "_Field";
  const char* const materialPrimStagePf = "_Material";
  const char* const samplerPrimStagePf = "_Sampler";
  const char* const cameraPrimStagePf = "_Camera";
  const char* const lightPrimStagePf = "_Light";

  // Postfixes for clip stage names
  const char* const geomClipPf = "_Geom_";

  bool PrimIsNew(const BoolEntryPair& createResult)
  {
    return !createResult.first.first && !createResult.first.second;
  }
}

typedef UsdBridgePrimCacheManager::PrimCacheIterator PrimCacheIterator;
typedef UsdBridgePrimCacheManager::ConstPrimCacheIterator ConstPrimCacheIterator;

struct UsdBridgeInternals
{
  UsdBridgeInternals(const UsdBridgeSettings& settings)
    : UsdWriter(settings)
    , HydraRenderer(UsdWriter)
  {
    RefModCallbacks.AtNewRef = [this](UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache){
      // Increase the reference count for the child on creation of referencing prim
      this->Cache.AddChild(parentCache, childCache);
    };

    RefModCallbacks.AtRemoveRef = [this](UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache) {
      this->Cache.RemoveChild(parentCache, childCache);
    };

#ifdef USE_USDRT
    InitializeCarbSDK();
#endif
  }

  ~UsdBridgeInternals()
  {
    if(DiagRemoveFunc)
      DiagRemoveFunc(DiagnosticDelegate.get());
  }

  template<typename UsdHandleType>
  bool CreateWorldIndependentObject(const char* name,
    const char* objectPathCp, const char* objectPrimStagePf,
    const std::function<void(UsdStageRefPtr, const SdfPath&)>& func,
    UsdHandleType& handle);

  BoolEntryPair FindOrCreatePrim(const char* category, const char* name, ResourceCollectFunc collectFunc = nullptr);
  void FindAndDeletePrim(const UsdBridgeHandle& handle);

  void CreateRootPrimAndAttach(UsdBridgePrimCache* cacheEntry, const char* primPathCp, const char* layerId = nullptr);
  void RemoveRootPrimAndDetach(UsdBridgePrimCache* cacheEntry, const char* primPathCp);

  template<class T>
  const UsdBridgePrimCacheList& ExtractPrimCaches(const T* handles, uint64_t numHandles);

  const UsdBridgePrimCacheList& ToCacheList(UsdBridgePrimCache* primCache);

  UsdGeomPrimvarsAPI GetBoundGeomPrimvars(const UsdBridgeHandle& material) const;

  // Cache
  UsdBridgePrimCacheManager Cache;

  // USDWriter
  UsdBridgeUsdWriter UsdWriter;

  // USD Hydra renderer
  UsdBridgeRenderer HydraRenderer;

  // Material->geometry binding suggestion
  std::map<UsdBridgePrimCache*, SdfPath> MaterialToGeometryBinding;

  // Callbacks
  UsdBridgeUsdWriter::RefModFuncs RefModCallbacks;

  // Diagnostic Manager
  std::unique_ptr<UsdBridgeDiagnosticMgrDelegate> DiagnosticDelegate;
  std::function<void (UsdBridgeDiagnosticMgrDelegate*)> DiagRemoveFunc;

  // Camera
  UsdCameraHandle LastUsedCamera { nullptr };

  // Temp arrays
  UsdBridgePrimCacheList TempPrimCaches;
  SdfPrimPathList TempPrimPaths;
  SdfPrimPathList ProtoPrimPaths;

#ifdef USE_USDRT
  void InitializeCarbSDK();
  void CleanupCarbSDK();

  UsdBridgeCarbLogger* CarbLogObject;
#endif
};


BoolEntryPair UsdBridgeInternals::FindOrCreatePrim(const char* category, const char* name, ResourceCollectFunc collectFunc)
{
  assert(TfIsValidIdentifier(name));

  UsdBridgePrimCache* primCache = nullptr;

  // Find existing entry
  ConstPrimCacheIterator it = Cache.FindPrimCache(name);
  bool cacheExists = Cache.ValidIterator(it);
  bool stageExists = true;

  if (cacheExists)
  {
    // Get the existing entry
    primCache = it->second.get();
  }
  else
  {
    primCache = Cache.CreatePrimCache(name, UsdWriter.CreatePrimName(name, category), collectFunc)->second.get();

    stageExists = !UsdWriter.CreatePrim(primCache->PrimPath);
  }

  return BoolEntryPair(std::pair<bool,bool>(stageExists, cacheExists), primCache);
}

void UsdBridgeInternals::FindAndDeletePrim(const UsdBridgeHandle& handle)
{
  ConstPrimCacheIterator it = Cache.FindPrimCache(handle);
  assert(Cache.ValidIterator(it));
  UsdBridgePrimCache* cacheEntry = (*it).second.get();

  UsdWriter.DeletePrim(cacheEntry);

  Cache.RemovePrimCache(it, UsdWriter.LogObject);
}

void UsdBridgeInternals::CreateRootPrimAndAttach(UsdBridgePrimCache* cacheEntry, const char* primPathCp, const char* layerId)
{
  UsdWriter.AddRootPrim(cacheEntry, primPathCp, layerId);
  Cache.AttachTopLevelPrim(cacheEntry);
}

void UsdBridgeInternals::RemoveRootPrimAndDetach(UsdBridgePrimCache* cacheEntry, const char* primPathCp)
{
  Cache.DetachTopLevelPrim(cacheEntry);
  UsdWriter.RemoveRootPrim(cacheEntry, primPathCp);
}

template<class T>
const UsdBridgePrimCacheList& UsdBridgeInternals::ExtractPrimCaches(const T* handles, uint64_t numHandles)
{
  TempPrimCaches.resize(numHandles);
  for (uint64_t i = 0; i < numHandles; ++i)
  {
    TempPrimCaches[i] = this->Cache.ConvertToPrimCache(handles[i]);
  }
  return TempPrimCaches;
}

const UsdBridgePrimCacheList& UsdBridgeInternals::ToCacheList(UsdBridgePrimCache* primCache)
{
  TempPrimCaches.resize(1);
  TempPrimCaches[0] = primCache;
  return TempPrimCaches;
}

UsdGeomPrimvarsAPI UsdBridgeInternals::GetBoundGeomPrimvars(const UsdBridgeHandle& material) const
{
  auto it = MaterialToGeometryBinding.find(material.value);
  if(it != MaterialToGeometryBinding.end())
  {
    const SdfPath& boundGeomPrimPath = it->second;
    UsdPrim boundGeomPrim = UsdWriter.GetSceneStage()->GetPrimAtPath(boundGeomPrimPath);
    return UsdGeomPrimvarsAPI(boundGeomPrim);
  }
  return UsdGeomPrimvarsAPI();
}

#ifdef USE_USDRT
void UsdBridgeInternals::InitializeCarbSDK()
{
  static bool isInitialized = false;
  if(!isInitialized)
  {
    if(carb::Framework* framework = carb::acquireFrameworkAndRegisterBuiltins())
    {
      framework->registerPlugin(g_carbClientName, framework->getBuiltinLoggingDesc());
    }
  }

  CarbLogObject = new UsdBridgeCarbLogger();

  if(!isInitialized)
  {
    carb::Framework* framework = carb::getFramework();

    if(framework)
    {
      constexpr const char* const kPluginsSearchPaths[] = { ".", "plugins", "usdrt_only", "plugins/scenegraph", "plugins/omni.usd" };
      const std::vector<const char*> loadedFileWildcards{ "carb.dictionary.plugin", "carb.dictionary.serializer-*.plugin", "carb.settings.plugin",
                    "omni.gpucompute-*.plugin", "omni.fabric*.plugin", "omni.tbb.globalcontrol.plugin", "carb.tasking.plugin", "usdrt.scenegraph.plugin" };

      carb::PluginLoadingDesc desc = carb::PluginLoadingDesc::getDefault();
      desc.loadedFileWildcards = loadedFileWildcards.data();
      desc.loadedFileWildcardCount = loadedFileWildcards.size();
      desc.searchPaths = kPluginsSearchPaths;
      desc.searchPathCount = CARB_COUNTOF(kPluginsSearchPaths);
      framework->loadPlugins(desc);

      isInitialized = true;
    }
  }
}

void UsdBridgeInternals::CleanupCarbSDK()
{
  delete CarbLogObject;
  CarbLogObject = nullptr;
}
#endif

template<typename HandleType>
bool HasNullHandles(const HandleType* handles, uint64_t numHandles)
{
  for(uint64_t i = 0; i < numHandles; ++i)
    if(handles[i].value == nullptr)
      return true;
  return false;
}

UsdBridge::UsdBridge(const UsdBridgeSettings& settings) 
  : Internals(new UsdBridgeInternals(settings))
  , SessionValid(false)
{
  SetEnableSaving(true);
}

void UsdBridge::SetExternalSceneStage(SceneStagePtr sceneStage)
{
  BRIDGE_USDWRITER.SetExternalSceneStage(UsdStageRefPtr((UsdStage*)sceneStage));
}

void UsdBridge::SetEnableSaving(bool enableSaving)
{
  this->EnableSaving = enableSaving;
  BRIDGE_USDWRITER.SetEnableSaving(enableSaving);
}

bool UsdBridge::OpenSession(UsdBridgeLogCallback logCallback, void* logUserData)
{
  BRIDGE_USDWRITER.LogObject = {logUserData, logCallback};

  Internals->DiagnosticDelegate = std::make_unique<UsdBridgeDiagnosticMgrDelegate>(logUserData, logCallback);
  Internals->DiagRemoveFunc = [](UsdBridgeDiagnosticMgrDelegate* delegate)
    { TfDiagnosticMgr::GetInstance().RemoveDelegate(delegate); };
  TfDiagnosticMgr::GetInstance().AddDelegate(Internals->DiagnosticDelegate.get());

  SessionValid = BRIDGE_USDWRITER.InitializeSession();
  SessionValid = SessionValid && BRIDGE_USDWRITER.OpenSceneStage();

  return SessionValid;
}

void UsdBridge::CloseSession()
{
  BRIDGE_USDWRITER.ResetSession();
}

UsdBridge::~UsdBridge()
{
  delete Internals;
}

bool UsdBridge::CreateWorld(const char* name, UsdWorldHandle& handle)
{
  if (!SessionValid) return false;

  // Find or create a cache entry belonging to a prim located under worldPathCp in the usd.
  BoolEntryPair createResult = Internals->FindOrCreatePrim(worldPathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool cacheExists = createResult.first.second;

  if (!cacheExists)
  {
    Internals->CreateRootPrimAndAttach(cacheEntry, worldPathCp);
  }
  
  handle.value = cacheEntry;
  return PrimIsNew(createResult);
}

bool UsdBridge::CreateInstance(const char* name, UsdInstanceHandle& handle)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(instancePathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool cacheExists = createResult.first.second;

  if (!cacheExists)
  {
    BRIDGE_USDWRITER.InitializeUsdTransform(cacheEntry);
  }

  handle.value = cacheEntry;
  return PrimIsNew(createResult);
}

bool UsdBridge::CreateGroup(const char* name, UsdGroupHandle& handle)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(groupPathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool cacheExists = createResult.first.second;

  if (!cacheExists)
  {
    BRIDGE_USDWRITER.InitializeUsdTransform(cacheEntry);
  }

  handle.value = cacheEntry;
  return PrimIsNew(createResult);
}

bool UsdBridge::CreateSurface(const char* name, UsdSurfaceHandle& handle)
{
  if (!SessionValid) return false;

  // Although surface doesn't support transform operations, a transform prim supports timevarying visibility.
  BoolEntryPair createResult = Internals->FindOrCreatePrim(surfacePathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool cacheExists = createResult.first.second;

  if (!cacheExists)
  {
    BRIDGE_USDWRITER.InitializeUsdTransform(cacheEntry);
  }

  handle.value = cacheEntry;
  return PrimIsNew(createResult);
}

bool UsdBridge::CreateVolume(const char * name, UsdVolumeHandle& handle)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(volumePathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool cacheExists = createResult.first.second;

  if (!cacheExists)
  {
    BRIDGE_USDWRITER.InitializeUsdTransform(cacheEntry);
  }

  handle.value = cacheEntry;
  return PrimIsNew(createResult);
}

template<typename GeomDataType>
bool UsdBridge::CreateGeometryTemplate(const char* name, UsdGeometryHandle& handle, const GeomDataType& geomData)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(geometryPathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool cacheExists = createResult.first.second;

  if (!cacheExists)
  {
#ifdef VALUE_CLIP_RETIMING
    BRIDGE_USDWRITER.CreateManifestStage(name, geomPrimStagePf, cacheEntry);

#ifndef TIME_CLIP_STAGES
    // Default primstage created for clip assetpaths of parents. If this path is not active, individual clip stages are created lazily during data update.
    UsdStageRefPtr geomPrimStage = BRIDGE_USDWRITER.FindOrCreatePrimStage(cacheEntry, geomPrimStagePf).second;
    BRIDGE_USDWRITER.InitializeUsdGeometry(geomPrimStage, cacheEntry->PrimPath, geomData, false);
#endif
#endif

    UsdPrim geomPrim = BRIDGE_USDWRITER.InitializeUsdGeometry(BRIDGE_USDWRITER.GetSceneStage(), cacheEntry->PrimPath, geomData, true);
    geomPrim.ApplyAPI<UsdShadeMaterialBindingAPI>();
  }

  handle.value = cacheEntry;
  return PrimIsNew(createResult);
}

bool UsdBridge::CreateGeometry(const char* name, UsdGeometryHandle& handle, const UsdBridgeMeshData& meshData)
{
  return CreateGeometryTemplate<UsdBridgeMeshData>(name, handle, meshData);
}

bool UsdBridge::CreateGeometry(const char* name, UsdGeometryHandle& handle, const UsdBridgeInstancerData& instancerData)
{
  return CreateGeometryTemplate<UsdBridgeInstancerData>(name, handle, instancerData);
}

bool UsdBridge::CreateGeometry(const char* name, UsdGeometryHandle& handle, const UsdBridgeCurveData& curveData)
{
  return CreateGeometryTemplate<UsdBridgeCurveData>(name, handle, curveData);
}

bool UsdBridge::CreateSpatialField(const char * name, UsdSpatialFieldHandle& handle)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(fieldPathCp, name, &ResourceCollectVolume);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool cacheExists = createResult.first.second;

  if (!cacheExists)
  {
#ifdef VALUE_CLIP_RETIMING
    // Create manifest and primstage
    BRIDGE_USDWRITER.CreateManifestStage(name, fieldPrimStagePf, cacheEntry);

    UsdStageRefPtr volPrimStage = BRIDGE_USDWRITER.FindOrCreatePrimStage(cacheEntry, fieldPrimStagePf).second;
    BRIDGE_USDWRITER.InitializeUsdVolume(volPrimStage, cacheEntry->PrimPath, false);
#endif   

    UsdPrim volumePrim = BRIDGE_USDWRITER.InitializeUsdVolume(BRIDGE_USDWRITER.GetSceneStage(), cacheEntry->PrimPath, true);
    volumePrim.ApplyAPI<UsdShadeMaterialBindingAPI>();
  }

  handle.value = cacheEntry;
  return PrimIsNew(createResult);
}

bool UsdBridge::CreateMaterial(const char* name, UsdMaterialHandle& handle)
{
  if (!SessionValid) return false;

  // Create the material
  BoolEntryPair createResult = Internals->FindOrCreatePrim(materialPathCp, name);
  UsdBridgePrimCache* matCacheEntry = createResult.second;
  bool cacheExists = createResult.first.second;

  UsdMaterialHandle matHandle; matHandle.value = matCacheEntry;

  if (!cacheExists)
  {
#ifdef VALUE_CLIP_RETIMING
    // Create manifest and primstage
    BRIDGE_USDWRITER.CreateManifestStage(name, materialPrimStagePf, matCacheEntry);

    UsdStageRefPtr matPrimStage = BRIDGE_USDWRITER.FindOrCreatePrimStage(matCacheEntry, materialPrimStagePf).second;
    BRIDGE_USDWRITER.InitializeUsdMaterial(matPrimStage, matCacheEntry->PrimPath, false);
#endif

    UsdShadeMaterial matPrim = BRIDGE_USDWRITER.InitializeUsdMaterial(BRIDGE_USDWRITER.GetSceneStage(), matCacheEntry->PrimPath, true);
  }

  handle = matHandle;
  return PrimIsNew(createResult);
}

bool UsdBridge::CreateSampler(const char* name, UsdSamplerHandle& handle, UsdBridgeSamplerData::SamplerType type)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(samplerPathCp, name, &ResourceCollectSampler);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool cacheExists = createResult.first.second;

  if (!cacheExists)
  {
#ifdef VALUE_CLIP_RETIMING
    // Create manifest and primstage
    BRIDGE_USDWRITER.CreateManifestStage(name, samplerPrimStagePf, cacheEntry);

    UsdStageRefPtr samplerPrimStage = BRIDGE_USDWRITER.FindOrCreatePrimStage(cacheEntry, samplerPrimStagePf).second;
    BRIDGE_USDWRITER.InitializeUsdSampler(samplerPrimStage, cacheEntry->PrimPath, type, false);
#endif

    BRIDGE_USDWRITER.InitializeUsdSampler(BRIDGE_USDWRITER.GetSceneStage(), cacheEntry->PrimPath, type, true);
  }

  handle.value = cacheEntry;
  return PrimIsNew(createResult);
}

template<typename UsdHandleType>
bool UsdBridgeInternals::CreateWorldIndependentObject(
  const char* name, const char* objectPathCp, const char* objectPrimStagePf,
  const std::function<void(UsdStageRefPtr, const SdfPath&)>& initializeObjectFunc,
  UsdHandleType& handle)
{
  // Creates an object that is not (indirectly) referenced by the world,
  // so it's creating a prim directly under the root with a reference to make the class prim concrete.

  // Find or create a cache entry belonging to a prim located under objectPathCp in the usd.
  BoolEntryPair createResult = this->FindOrCreatePrim(objectPathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool cacheExists = createResult.first.second;

  // The object has no value clips and manifest, instead the primstage+path is directly referenced by the root
  // instance prim.
  if (!cacheExists)
  {
    const char* layerId = nullptr; // The path to the referenced USD layer.
#ifdef VALUE_CLIP_RETIMING
    const UsdStagePair& stagePair = UsdWriter.FindOrCreatePrimStage(cacheEntry, objectPrimStagePf);
    layerId = stagePair.first.c_str();
    UsdStageRefPtr objStage = stagePair.second;
#else
    UsdStageRefPtr objStage = UsdWriter.GetSceneStage();
#endif

    initializeObjectFunc(objStage, cacheEntry->PrimPath);

    CreateRootPrimAndAttach(cacheEntry, objectPathCp, layerId);
  }
  
  handle.value = cacheEntry;
  return PrimIsNew(createResult); 
}

bool UsdBridge::CreateLight(const char* name, UsdBridgeLightType lightType, UsdLightHandle& handle)
{
  if (!SessionValid) return false;

  auto initializeFunc = [Internals = this->Internals, lightType](UsdStageRefPtr stage, const SdfPath& path)
    { BRIDGE_USDWRITER.InitializeUsdLight(stage, path, lightType); };

  return Internals->CreateWorldIndependentObject(name, lightPathCp, lightPrimStagePf, initializeFunc, handle);
}

bool UsdBridge::CreateCamera(const char* name, UsdCameraHandle& handle)
{
  if (!SessionValid) return false;

  auto initializeFunc = [Internals = this->Internals](UsdStageRefPtr stage, const SdfPath& path)
    { BRIDGE_USDWRITER.InitializeUsdCamera(stage, path); };

  return Internals->CreateWorldIndependentObject(name, cameraPathCp, cameraPrimStagePf, initializeFunc, handle);
}

void UsdBridge::DeleteWorld(UsdWorldHandle handle)
{
  if (handle.value == nullptr) return;

  UsdBridgePrimCache* worldCache = BRIDGE_CACHE.ConvertToPrimCache(handle);

  Internals->RemoveRootPrimAndDetach(worldCache, worldPathCp);

  // Remove the abstract class
  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::DeleteInstance(UsdInstanceHandle handle)
{
  if (handle.value == nullptr) return;

  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::DeleteGroup(UsdGroupHandle handle)
{
  if (handle.value == nullptr) return;

  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::DeleteSurface(UsdSurfaceHandle handle)
{
  if (handle.value == nullptr) return;

  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::DeleteVolume(UsdVolumeHandle handle)
{
  if (handle.value == nullptr) return;

  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::DeleteGeometry(UsdGeometryHandle handle)
{
  if (handle.value == nullptr) return;

  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::DeleteSpatialField(UsdSpatialFieldHandle handle)
{
  if (handle.value == nullptr) return;

  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::DeleteMaterial(UsdMaterialHandle handle)
{
  if (handle.value == nullptr) return;

  Internals->MaterialToGeometryBinding.erase(handle.value);

  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::DeleteSampler(UsdSamplerHandle handle)
{
  if (handle.value == nullptr) return;

  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::DeleteCamera(UsdCameraHandle handle)
{
  if (handle.value == nullptr) return;

  UsdBridgePrimCache* cameraCache = BRIDGE_CACHE.ConvertToPrimCache(handle);

  Internals->RemoveRootPrimAndDetach(cameraCache, cameraPathCp);

  // Remove the abstract class 
  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::DeleteLight(UsdLightHandle handle)
{
  if (handle.value == nullptr) return;

  UsdBridgePrimCache* lightCache = BRIDGE_CACHE.ConvertToPrimCache(handle);

  Internals->RemoveRootPrimAndDetach(lightCache, lightPathCp);

  // Remove the abstract class 
  Internals->FindAndDeletePrim(handle);
}

template<typename ParentHandleType, typename ChildHandleType>
void UsdBridge::SetNoClipRefs(ParentHandleType parentHandle, const ChildHandleType* childHandles, uint64_t numChildren, 
  const char* refPathExt, bool timeVarying, double timeStep, const int* instanceableValues)
{
  if (parentHandle.value == nullptr) return;
  if(HasNullHandles(childHandles, numChildren)) return;

  UsdBridgePrimCache* parentCache = BRIDGE_CACHE.ConvertToPrimCache(parentHandle);
  const UsdBridgePrimCacheList& childCaches = Internals->ExtractPrimCaches<ChildHandleType>(childHandles, numChildren);

  BRIDGE_USDWRITER.ManageUnusedRefs(parentCache, childCaches, refPathExt, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
  for (uint64_t i = 0; i < numChildren; ++i)
  {
    BRIDGE_USDWRITER.AddRef_NoClip(parentCache, childCaches[i], refPathExt, timeVarying, timeStep,
      instanceableValues && instanceableValues[i], Internals->RefModCallbacks);
  }
}

void UsdBridge::SetInstanceRefs(UsdWorldHandle world, const UsdInstanceHandle* instances, uint64_t numInstances, bool timeVarying, double timeStep, const int* instanceableValues)
{
  SetNoClipRefs(world, instances, numInstances, instancePathRp, timeVarying, timeStep, instanceableValues);
}

void UsdBridge::SetGroupRef(UsdInstanceHandle instance, UsdGroupHandle group, bool timeVarying, double timeStep)
{
  if (instance.value == nullptr) return;
  if (group.value == nullptr) return;

  UsdBridgePrimCache* instanceCache = BRIDGE_CACHE.ConvertToPrimCache(instance);
  UsdBridgePrimCache* groupCache = BRIDGE_CACHE.ConvertToPrimCache(group);

  constexpr bool instanceable = false;

  BRIDGE_USDWRITER.ManageUnusedRefs(instanceCache, Internals->ToCacheList(groupCache), nullptr, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
  BRIDGE_USDWRITER.AddRef_NoClip(instanceCache, groupCache, nullptr, timeVarying, timeStep, instanceable, Internals->RefModCallbacks);
}

void UsdBridge::SetSurfaceRefs(UsdWorldHandle world, const UsdSurfaceHandle* surfaces, uint64_t numSurfaces, bool timeVarying, double timeStep, const int* instanceableValues)
{
  SetNoClipRefs(world, surfaces, numSurfaces, surfacePathRp, timeVarying, timeStep, instanceableValues);
}

void UsdBridge::SetSurfaceRefs(UsdGroupHandle group, const UsdSurfaceHandle* surfaces, uint64_t numSurfaces, bool timeVarying, double timeStep, const int* instanceableValues)
{
  SetNoClipRefs(group, surfaces, numSurfaces, surfacePathRp, timeVarying, timeStep, instanceableValues);
}

void UsdBridge::SetVolumeRefs(UsdWorldHandle world, const UsdVolumeHandle* volumes, uint64_t numVolumes, bool timeVarying, double timeStep, const int* instanceableValues)
{
  SetNoClipRefs(world, volumes, numVolumes, volumePathRp, timeVarying, timeStep, instanceableValues);
}

void UsdBridge::SetVolumeRefs(UsdGroupHandle group, const UsdVolumeHandle* volumes, uint64_t numVolumes, bool timeVarying, double timeStep, const int* instanceableValues)
{
  SetNoClipRefs(group, volumes, numVolumes, volumePathRp, timeVarying, timeStep, instanceableValues);
}

void UsdBridge::SetGeometryRef(UsdSurfaceHandle surface, UsdGeometryHandle geometry, double timeStep, double geomTimeStep)
{
  if (surface.value == nullptr) return;
  if (geometry.value == nullptr) return;

  UsdBridgePrimCache* surfaceCache = BRIDGE_CACHE.ConvertToPrimCache(surface);
  UsdBridgePrimCache* geometryCache = BRIDGE_CACHE.ConvertToPrimCache(geometry);

  constexpr bool timeVarying = false; // On a surface, the path to the geometry cannot change over time (ie. is uniformly set), since material is not timevarying either (due to the material binding path rel from the geometry) and the geometry contents itself are already timevarying.
  constexpr bool valueClip = true;
  constexpr bool clipStages = true;
  constexpr bool instanceable = false; // Can only make Xform prims instanceable

  BRIDGE_USDWRITER.ManageUnusedRefs(surfaceCache, Internals->ToCacheList(geometryCache), geometryPathRp, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
  BRIDGE_USDWRITER.ManageUnusedRefs(surfaceCache, UsdBridgePrimCacheList(), materialPathRp, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
  SdfPath refGeomPath = BRIDGE_USDWRITER.AddRef(surfaceCache, geometryCache, geometryPathRp, timeVarying, valueClip, clipStages, geomClipPf, timeStep, geomTimeStep, instanceable, Internals->RefModCallbacks);

  BRIDGE_USDWRITER.UnbindMaterialFromGeom(refGeomPath);
}

void UsdBridge::SetGeometryMaterialRef(UsdSurfaceHandle surface, UsdGeometryHandle geometry, UsdMaterialHandle material, double timeStep, double geomTimeStep, double matTimeStep, std::function<void()> updateBoundParamsFunc)
{
  if (surface.value == nullptr) return;
  if (geometry.value == nullptr || material.value == nullptr) return;

  UsdBridgePrimCache* surfaceCache = BRIDGE_CACHE.ConvertToPrimCache(surface);
  UsdBridgePrimCache* geometryCache = BRIDGE_CACHE.ConvertToPrimCache(geometry);
  UsdBridgePrimCache* materialCache = BRIDGE_CACHE.ConvertToPrimCache(material);

  // For updating material attribute reader output types, update the suggested material->geompath mapping.
  // Since multiple geometries can be bound to a material, only one is taken, so it is up to the user to ensure correct attribute connection types.
  SdfPath& geomPrimPath = geometryCache->PrimPath;
  Internals->MaterialToGeometryBinding[material.value] = geomPrimPath;

  if(updateBoundParamsFunc)
    updateBoundParamsFunc();

  constexpr bool timeVarying = false;
  constexpr bool valueClip = true;
  constexpr bool instanceable = false; // Can only make Xform prims instanceable

  // Remove any dangling references
  BRIDGE_USDWRITER.ManageUnusedRefs(surfaceCache, Internals->ToCacheList(geometryCache), geometryPathRp, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
  BRIDGE_USDWRITER.ManageUnusedRefs(surfaceCache, Internals->ToCacheList(materialCache), materialPathRp, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);

  // Update the references
  SdfPath refGeomPath = BRIDGE_USDWRITER.AddRef(surfaceCache, geometryCache, geometryPathRp, timeVarying, valueClip, true, geomClipPf, timeStep, geomTimeStep, instanceable, Internals->RefModCallbacks); // Can technically be timeVarying, but would be a bit confusing. Instead, timevary the surface.
  SdfPath refMatPath = BRIDGE_USDWRITER.AddRef(surfaceCache, materialCache, materialPathRp, timeVarying, valueClip, false, nullptr, timeStep, matTimeStep, instanceable, Internals->RefModCallbacks);

  // Bind the referencing material to the referencing geom prim (as they are within same scope in this usd prim)
  BRIDGE_USDWRITER.BindMaterialToGeom(refGeomPath, refMatPath);
}

void UsdBridge::SetSpatialFieldRef(UsdVolumeHandle volume, UsdSpatialFieldHandle field, double timeStep, double fieldTimeStep)
{
  if (volume.value == nullptr) return;
  if (field.value == nullptr) return;

  UsdBridgePrimCache* volumeCache = BRIDGE_CACHE.ConvertToPrimCache(volume);
  UsdBridgePrimCache* fieldCache = BRIDGE_CACHE.ConvertToPrimCache(field);

  constexpr bool timeVarying = false;
  constexpr bool valueClip = true;
  constexpr bool clipStages = false;
  constexpr bool instanceable = false;

  BRIDGE_USDWRITER.ManageUnusedRefs(volumeCache, Internals->ToCacheList(fieldCache), fieldPathRp, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);

  SdfPath refVolPath = BRIDGE_USDWRITER.AddRef(volumeCache, fieldCache, fieldPathRp, timeVarying, valueClip, clipStages, nullptr, timeStep, fieldTimeStep, instanceable, Internals->RefModCallbacks); // Can technically be timeVarying, but would be a bit confusing. Instead, timevary the volume.
}

void UsdBridge::SetSamplerRefs(UsdMaterialHandle material, const UsdSamplerHandle* samplers, size_t numSamplers, double timeStep, const UsdSamplerRefData* samplerRefData)
{
  if (material.value == nullptr) return;
  if(HasNullHandles(samplers, numSamplers)) return;

  UsdBridgePrimCache* matCache = BRIDGE_CACHE.ConvertToPrimCache(material);
  const UsdBridgePrimCacheList& samplerCaches = Internals->ExtractPrimCaches<UsdSamplerHandle>(samplers, numSamplers);

  // Sampler references cannot be time-varying (ie. path to sampler cannot change over time - connectToSource doesn't allow for that), and are set on the scenestage prim (so they inherit the type of the sampler prim)
  // However, they do allow value clip retiming.
  constexpr bool timeVarying = false;
  constexpr bool valueClip = true;
  constexpr bool clipStages = false;
  constexpr bool instanceable = false;

  BRIDGE_USDWRITER.ManageUnusedRefs(matCache, samplerCaches, samplerPathRp, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);

  // Bind sampler and material
  SdfPath& matPrimPath = matCache->PrimPath;// .AppendPath(SdfPath(materialAttribPf));
  UsdStageRefPtr materialStage = BRIDGE_USDWRITER.GetTimeVarStage(matCache);

  Internals->TempPrimPaths.resize(numSamplers);
  for (uint64_t i = 0; i < numSamplers; ++i)
  {
    Internals->TempPrimPaths[i] = BRIDGE_USDWRITER.AddRef(matCache, samplerCaches[i], samplerPathRp, timeVarying, valueClip, clipStages, nullptr, timeStep, samplerRefData[i].TimeStep, instanceable, Internals->RefModCallbacks);
  }

  BRIDGE_USDWRITER.ConnectSamplersToMaterial(materialStage, matPrimPath, Internals->TempPrimPaths, samplerCaches, samplerRefData, numSamplers, timeStep); // requires world timestep, see implementation

#ifdef VALUE_CLIP_RETIMING
  if(this->EnableSaving)
    materialStage->Save();
#endif
}

void UsdBridge::SetPrototypeRefs(UsdGeometryHandle geometry, const UsdGeometryHandle* protoGeometries, size_t numProtoGeometries, double timeStep, double* protoTimeSteps)
{
  if (geometry.value == nullptr) return;
  if(HasNullHandles(protoGeometries, numProtoGeometries)) return;

  UsdBridgePrimCache* geometryCache = BRIDGE_CACHE.ConvertToPrimCache(geometry);
  const UsdBridgePrimCacheList& protoGeomCaches = Internals->ExtractPrimCaches<UsdGeometryHandle>(protoGeometries, numProtoGeometries);

  // Due to prototype rel paths, the path to the prototype itself cannot change. Also, the contents of the prototype itself can already timevary.
  constexpr bool timeVarying = false;
  constexpr bool valueClip = true;
  constexpr bool clipStages = true;
  constexpr bool instanceable = false;

  BRIDGE_USDWRITER.ManageUnusedRefs(geometryCache, protoGeomCaches, protoGeometryPathRp, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);

  Internals->ProtoPrimPaths.resize(numProtoGeometries);
  for(uint64_t i = 0; i < numProtoGeometries; ++i)
  {
    Internals->ProtoPrimPaths[i] = BRIDGE_USDWRITER.AddRef(geometryCache, protoGeomCaches[i], protoGeometryPathRp, timeVarying, valueClip, clipStages, geomClipPf, timeStep, protoTimeSteps[i], instanceable, Internals->RefModCallbacks);
  }
}

template<typename ParentHandleType>
void UsdBridge::DeleteAllRefs(ParentHandleType parentHandle, const char* refPathExt, bool timeVarying, double timeStep)
{
  if (parentHandle.value == nullptr) return;

  UsdBridgePrimCache* parentCache = BRIDGE_CACHE.ConvertToPrimCache(parentHandle);

  BRIDGE_USDWRITER.RemoveAllRefs(parentCache, refPathExt, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
}

void UsdBridge::DeleteInstanceRefs(UsdWorldHandle world, bool timeVarying, double timeStep)
{
  DeleteAllRefs(world, instancePathRp, timeVarying, timeStep);
}

void UsdBridge::DeleteGroupRef(UsdInstanceHandle instance, bool timeVarying, double timeStep)
{
  DeleteAllRefs(instance, nullptr, timeVarying, timeStep);
}

void UsdBridge::DeleteSurfaceRefs(UsdWorldHandle world, bool timeVarying, double timeStep)
{
  DeleteAllRefs(world, surfacePathRp, timeVarying, timeStep);
}

void UsdBridge::DeleteSurfaceRefs(UsdGroupHandle group, bool timeVarying, double timeStep)
{
  DeleteAllRefs(group, surfacePathRp, timeVarying, timeStep);
}

void UsdBridge::DeleteVolumeRefs(UsdWorldHandle world, bool timeVarying, double timeStep)
{
  DeleteAllRefs(world, volumePathRp, timeVarying, timeStep);
}

void UsdBridge::DeleteVolumeRefs(UsdGroupHandle group, bool timeVarying, double timeStep)
{
  DeleteAllRefs(group, volumePathRp, timeVarying, timeStep);
}

void UsdBridge::DeleteGeometryRef(UsdSurfaceHandle surface, double timeStep)
{
  DeleteAllRefs(surface, geometryPathRp, false, timeStep);
}

void UsdBridge::DeleteSpatialFieldRef(UsdVolumeHandle volume, double timeStep)
{
  DeleteAllRefs(volume, fieldPathRp, false, timeStep);
}

void UsdBridge::DeleteMaterialRef(UsdSurfaceHandle surface, double timeStep)
{
  DeleteAllRefs(surface, materialPathRp, false, timeStep);
}

void UsdBridge::DeleteSamplerRefs(UsdMaterialHandle material, double timeStep)
{
  DeleteAllRefs(material, samplerPathRp, false, timeStep);
}

void UsdBridge::DeletePrototypeRefs(UsdGeometryHandle geometry, double timeStep)
{
  Internals->ProtoPrimPaths.resize(0);
  DeleteAllRefs(geometry, protoGeometryPathRp, false, timeStep);
}

void UsdBridge::UpdateBeginEndTime(double timeStep)
{
  if (!SessionValid) return;

  BRIDGE_USDWRITER.UpdateBeginEndTime(timeStep);
}

void UsdBridge::SetInstanceTransform(UsdInstanceHandle instance, const float* transform, bool timeVarying, double timeStep)
{
  if (instance.value == nullptr) return;

  UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(instance);

  SdfPath transformPath = cache->PrimPath;// .AppendPath(SdfPath(transformAttribPf));

  BRIDGE_USDWRITER.UpdateUsdTransform(transformPath, transform, timeVarying, timeStep);
}

template<typename GeomDataType>
void UsdBridge::SetGeometryDataTemplate(UsdGeometryHandle geometry, const GeomDataType& geomData, double timeStep)
{
  if (geometry.value == nullptr) return;

  UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(geometry);

  SdfPath& geomPath = cache->PrimPath;

#ifdef VALUE_CLIP_RETIMING
  if(cache->TimeVarBitsUpdate(geomData.TimeVarying))
    BRIDGE_USDWRITER.UpdateUsdGeometryManifest(cache, geomData);
#endif

  UsdStageRefPtr geomStage = BRIDGE_USDWRITER.GetTimeVarStage(cache
#ifdef TIME_CLIP_STAGES
    , true, geomClipPf, timeStep
    , [usdWriter=&BRIDGE_USDWRITER, &geomPath, &geomData] (UsdStageRefPtr geomStage) 
        { usdWriter->InitializeUsdGeometry(geomStage, geomPath, geomData, false); }
#endif
  );
  
  BRIDGE_USDWRITER.UpdateUsdGeometry(geomStage, geomPath, geomData, timeStep);

#ifdef VALUE_CLIP_RETIMING
  if(this->EnableSaving)
    geomStage->Save();
#endif
}

void UsdBridge::SetGeometryData(UsdGeometryHandle geometry, const UsdBridgeMeshData& meshData, double timeStep)
{
  SetGeometryDataTemplate<UsdBridgeMeshData>(geometry, meshData, timeStep);
}

void UsdBridge::SetGeometryData(UsdGeometryHandle geometry, const UsdBridgeInstancerData& instancerData, double timeStep)
{
  SetGeometryDataTemplate<UsdBridgeInstancerData>(geometry, instancerData, timeStep);
}

void UsdBridge::SetGeometryData(UsdGeometryHandle geometry, const UsdBridgeCurveData& curveData, double timeStep)
{
  SetGeometryDataTemplate<UsdBridgeCurveData>(geometry, curveData, timeStep);
}

void UsdBridge::SetSpatialFieldData(UsdSpatialFieldHandle field, const UsdBridgeVolumeData& volumeData, double timeStep)
{
  if (field.value == nullptr) return;

  UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(field);

#ifdef VALUE_CLIP_RETIMING
  if(cache->TimeVarBitsUpdate(volumeData.TimeVarying))
    BRIDGE_USDWRITER.UpdateUsdVolumeManifest(cache, volumeData);
#endif

  UsdStageRefPtr volumeStage = BRIDGE_USDWRITER.GetTimeVarStage(cache);

  // To avoid data duplication when using of clip stages, we need to potentially use the scenestage prim for time-uniform data.
  BRIDGE_USDWRITER.UpdateUsdVolume(volumeStage, cache, volumeData, timeStep);

#ifdef VALUE_CLIP_RETIMING
  if(this->EnableSaving)
    volumeStage->Save();
#endif
}

void UsdBridge::SetMaterialData(UsdMaterialHandle material, const UsdBridgeMaterialData& matData, double timeStep)
{
  if (material.value == nullptr) return;

  UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(material);
  SdfPath& matPrimPath = cache->PrimPath;
  
#ifdef VALUE_CLIP_RETIMING
  if(cache->TimeVarBitsUpdate(matData.TimeVarying))
    BRIDGE_USDWRITER.UpdateUsdMaterialManifest(cache, matData);
#endif

  UsdStageRefPtr materialStage = BRIDGE_USDWRITER.GetTimeVarStage(cache);
  UsdGeomPrimvarsAPI boundGeomPrimvars = Internals->GetBoundGeomPrimvars(material);

  BRIDGE_USDWRITER.UpdateUsdMaterial(materialStage, matPrimPath, matData, boundGeomPrimvars, timeStep);

#ifdef VALUE_CLIP_RETIMING
  if(this->EnableSaving)
    materialStage->Save();
#endif
}

void UsdBridge::SetSamplerData(UsdSamplerHandle sampler, const UsdBridgeSamplerData& samplerData, double timeStep)
{
  if (sampler.value == nullptr) return;

  UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(sampler);

#ifdef VALUE_CLIP_RETIMING
  if(cache->TimeVarBitsUpdate(samplerData.TimeVarying))
    BRIDGE_USDWRITER.UpdateUsdSamplerManifest(cache, samplerData);
#endif

  UsdStageRefPtr samplerStage = BRIDGE_USDWRITER.GetTimeVarStage(cache);
  
  BRIDGE_USDWRITER.UpdateUsdSampler(samplerStage, cache, samplerData, timeStep);

#ifdef VALUE_CLIP_RETIMING
  if(this->EnableSaving)
    samplerStage->Save();
#endif
}

template<typename LightDataType>
void UsdBridge::SetLightDataTemplate(UsdLightHandle light, const LightDataType& lightData, double timeStep)
{
  if (light.value == nullptr) return;

  UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(light);
  SdfPath& lightPath = cache->PrimPath;

  bool timeVarHasChanged =
#ifdef VALUE_CLIP_RETIMING
    cache->TimeVarBitsUpdate(lightData.TimeVarying);
#else
    false;
#endif

  UsdStageRefPtr lightStage = BRIDGE_USDWRITER.GetTimeVarStage(cache);

  BRIDGE_USDWRITER.UpdateUsdLight(lightStage, lightPath, lightData, timeStep, timeVarHasChanged);

#ifdef VALUE_CLIP_RETIMING
  if(this->EnableSaving)
    lightStage->Save();
#endif
}

void UsdBridge::SetLightData(UsdLightHandle light, const UsdBridgeDirectionalLightData& lightData, double timeStep)
{
  SetLightDataTemplate<UsdBridgeDirectionalLightData>(light, lightData, timeStep);
}

void UsdBridge::SetLightData(UsdLightHandle light, const UsdBridgePointLightData& lightData, double timeStep)
{
  SetLightDataTemplate<UsdBridgePointLightData>(light, lightData, timeStep);
}

void UsdBridge::SetLightData(UsdLightHandle light, const UsdBridgeDomeLightData& lightData, double timeStep)
{
  SetLightDataTemplate<UsdBridgeDomeLightData>(light, lightData, timeStep);
}

void UsdBridge::SetCameraData(UsdCameraHandle camera, const UsdBridgeCameraData& cameraData, double timeStep)
{
  if (camera.value == nullptr) return;

  UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(camera);
  SdfPath& cameraPath = cache->PrimPath;

  bool timeVarHasChanged =
#ifdef VALUE_CLIP_RETIMING
    cache->TimeVarBitsUpdate(cameraData.TimeVarying);
#else
    false;
#endif

  UsdStageRefPtr cameraStage = BRIDGE_USDWRITER.GetTimeVarStage(cache);

  BRIDGE_USDWRITER.UpdateUsdCamera(cameraStage, cameraPath, cameraData, timeStep, timeVarHasChanged);

#ifdef VALUE_CLIP_RETIMING
  if(this->EnableSaving)
    cameraStage->Save();
#endif
}

void UsdBridge::SetPrototypeData(UsdGeometryHandle geometry, const UsdBridgeInstancerRefData& instancerRefData)
{
  if (geometry.value == nullptr) return;

  UsdBridgePrimCache* geomCache = BRIDGE_CACHE.ConvertToPrimCache(geometry);

  SdfPath& geomPath = geomCache->PrimPath;

  BRIDGE_USDWRITER.UpdateUsdInstancerPrototypes(geomPath, instancerRefData, Internals->ProtoPrimPaths, protoShapePathRp); // The geometry reference path is already merged into ProtoPrimPaths
}

void UsdBridge::ChangeMaterialInputAttributes(UsdMaterialHandle material, const MaterialInputAttribName* inputAttribs, size_t numInputAttribs, double timeStep, MaterialDMI timeVarying)
{
  // This function only applies to materials being bound to a different (type of) geometry, which means that for example attribute connection names can change

  if (material.value == nullptr) return;

  UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(material);
  SdfPath& matPrimPath = cache->PrimPath;// .AppendPath(SdfPath(samplerAttribPf));

  UsdStageRefPtr materialStage = BRIDGE_USDWRITER.GetTimeVarStage(cache);
  UsdGeomPrimvarsAPI boundGeomPrimvars = Internals->GetBoundGeomPrimvars(material);

  for(int i = 0; i < numInputAttribs; ++i)
    BRIDGE_USDWRITER.UpdateAttributeReader(materialStage, matPrimPath, inputAttribs[i].first, inputAttribs[i].second, boundGeomPrimvars, timeStep, timeVarying);

#ifdef VALUE_CLIP_RETIMING
  //if(this->EnableSaving)
  //  materialStage->Save(); // Attrib readers do not have timevarying info at the moment
#endif
}

void UsdBridge::ChangeInAttribute(UsdSamplerHandle sampler, const char* newName, double timeStep, SamplerDMI timeVarying)
{
  if (sampler.value == nullptr) return;

  UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(sampler);
  SdfPath& samplerPrimPath = cache->PrimPath;// .AppendPath(SdfPath(samplerAttribPf));

  UsdStageRefPtr samplerStage = BRIDGE_USDWRITER.GetTimeVarStage(cache);
  
  BRIDGE_USDWRITER.UpdateInAttribute(samplerStage, samplerPrimPath, newName, timeStep, timeVarying);

#ifdef VALUE_CLIP_RETIMING
  if(this->EnableSaving)
    samplerStage->Save();
#endif
}

void UsdBridge::SaveScene()
{
  if (!SessionValid) return;

  BRIDGE_USDWRITER.SaveScene();
}

void UsdBridge::InitializeRendering()
{
  if (!SessionValid) return;

  BRIDGE_RENDERER.Initialize();
}

void UsdBridge::SetRenderCamera(UsdCameraHandle camera)
{
  if (!SessionValid) return;

  if(camera.value != Internals->LastUsedCamera.value)
  {
    UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(camera);
    SdfPath cameraPath;
    BRIDGE_USDWRITER.GetRootPrimPath(cache->Name, cameraPathCp, cameraPath);

    BRIDGE_RENDERER.SetCameraPath(cameraPath);

    Internals->LastUsedCamera = camera;
  }
}

void UsdBridge::RenderFrame(uint32_t width, uint32_t height, double timeStep)
{
  if (!SessionValid) return;

  BRIDGE_RENDERER.Render(width, height, timeStep);
}

bool UsdBridge::FrameReady(bool wait)
{
  if (!SessionValid) return true;

  return BRIDGE_RENDERER.FrameReady(wait);
}

void* UsdBridge::MapFrame(UsdBridgeType& returnFormat)
{
  if (!SessionValid) return nullptr;

  return BRIDGE_RENDERER.MapFrame(returnFormat);
}

void UsdBridge::UnmapFrame()
{
  if (!SessionValid) return;

  BRIDGE_RENDERER.UnmapFrame();
}

void UsdBridge::ResetResourceUpdateState()
{
  if (!SessionValid) return;

  BRIDGE_USDWRITER.ResetSharedResourceModified();
}

void UsdBridge::GarbageCollect()
{
  BRIDGE_CACHE.RemoveUnreferencedPrimCaches(
    [this](UsdBridgePrimCache* cacheEntry) 
    { 
      if(cacheEntry->ResourceCollect)
        cacheEntry->ResourceCollect(cacheEntry, BRIDGE_USDWRITER);

      BRIDGE_USDWRITER.DeletePrim(cacheEntry);
    }
  );
  BRIDGE_USDWRITER.SaveScene();
}

const char* UsdBridge::GetPrimPath(UsdBridgeHandle* handle)
{
  if(handle && handle->value)
  {
    return handle->value->PrimPath.GetString().c_str();
  }
  else
    return nullptr;
}

void UsdBridge::SetConnectionLogVerbosity(int logVerbosity)
{
  int logLevel = UsdBridgeRemoteConnection::GetConnectionLogLevelMax() - logVerbosity; // Just invert verbosity to get the level
  UsdBridgeRemoteConnection::SetConnectionLogLevel(logLevel);

#ifdef USE_USDRT
  UsdBridgeCarbLogger::SetCarbLogVerbosity(logVerbosity);
#endif
}
