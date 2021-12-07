// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridge.h"

#include "UsdBridgeUsdWriter.h"
#include "UsdBridgeCaches.h"

#include <string>

#define BRIDGE_CACHE Internals->Cache
#define BRIDGE_USDWRITER Internals->UsdWriter

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

  // Parent path extensions for references in parent classes (Reference path)
  const char* const surfacePathRp = "surfaces";
  const char* const volumePathRp = "volumes";
  const char* const geometryPathRp = "geometry"; // created in surfaces parent class
  const char* const fieldPathRp = "spatialfield"; // created in volumes parent class
  const char* const materialPathRp = "material"; // created in surfaces parent class
  const char* const samplerPathRp = "samplers"; // created in material parent class (separation from other UsdShader prims in material)

  // Postfixes for prim stage names
  const char* const geomPrimStagePf = "_Geom";
  const char* const fieldPrimStagePf = "_Field";
  const char* const materialPrimStagePf = "_Material";

  // Postfixes for clip stage names
  const char* const geomClipPf = "_Geom_";
}

typedef UsdBridgeTemporalCache::PrimCacheIterator PrimCacheIterator;
typedef UsdBridgeTemporalCache::ConstPrimCacheIterator ConstPrimCacheIterator;

struct UsdBridgeInternals
{
  UsdBridgeInternals(const UsdBridgeSettings& settings)
    : UsdWriter(settings)
  {
    RefModCallbacks.AtNewRef = [this](UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache){
#ifdef TIME_BASED_CACHING
      // Increase the reference count for the child on creation of referencing prim
      this->Cache.AddChild(parentCache, childCache);
#endif
    };

    RefModCallbacks.AtRemoveRef = [this](UsdBridgePrimCache* parentCache, const std::string& childName) {
#ifdef TIME_BASED_CACHING
      ConstPrimCacheIterator it = this->Cache.FindPrimCache(childName);
      assert(this->Cache.ValidIterator(it));
      this->Cache.RemoveChild(parentCache, it->second.get());
#endif
    };
  }

  BoolEntryPair FindOrCreatePrim(const char* category, const char* name, ResourceCollectFunc collectFunc = nullptr);
  void FindAndDeletePrim(const UsdBridgeHandle& handle);

  template<class T>
  const UsdBridgePrimCacheList& ExtractPrimCaches(const UsdBridgeTemporalCache& Cache, const T* handles, uint64_t numHandles);

  // Cache
  UsdBridgeTemporalCache Cache;

  // USDWriter
  UsdBridgeUsdWriter UsdWriter;

  // Callbacks
  UsdBridgeUsdWriter::RefModFuncs RefModCallbacks;

  // Temp arrays
  UsdBridgePrimCacheList TempPrimCaches;
};


BoolEntryPair UsdBridgeInternals::FindOrCreatePrim(const char* category, const char* name, ResourceCollectFunc collectFunc)
{
  assert(TfIsValidIdentifier(name));

  UsdBridgePrimCache* primCache = nullptr;

  // Find existing entry
  ConstPrimCacheIterator it = Cache.FindPrimCache(name);
  bool cacheExists = Cache.ValidIterator(it);

  if (cacheExists)
  {
    // Get the existing entry
    primCache = it->second.get();
  }
  else
  {
    primCache = Cache.CreatePrimCache(name, UsdWriter.CreatePrimName(name, category), collectFunc)->second.get();

    UsdWriter.CreatePrim(primCache->PrimPath);
  }

  return std::pair(!cacheExists, primCache);
}

void UsdBridgeInternals::FindAndDeletePrim(const UsdBridgeHandle& handle)
{
  ConstPrimCacheIterator it = Cache.FindPrimCache(handle);
  assert(Cache.ValidIterator(it));
  UsdBridgePrimCache* cacheEntry = (*it).second.get();

  UsdWriter.DeletePrim(cacheEntry);

  Cache.RemovePrimCache(it);
}

template<class T>
const UsdBridgePrimCacheList& UsdBridgeInternals::ExtractPrimCaches(const UsdBridgeTemporalCache& Cache, const T* handles, uint64_t numHandles)
{
  TempPrimCaches.resize(numHandles);
  for (uint64_t i = 0; i < numHandles; ++i)
  {
    TempPrimCaches[i] = Cache.ConvertToPrimCache(handles[i]);
  }
  return TempPrimCaches;
}


UsdBridge::UsdBridge(const UsdBridgeSettings& settings) 
  : Internals(new UsdBridgeInternals(settings))
  , SessionValid(false)
{
}

bool UsdBridge::OpenSession(UsdBridgeLogCallback logCallback, void* logUserData)
{
  BRIDGE_USDWRITER.LogUserData = logUserData;
  BRIDGE_USDWRITER.LogCallback = logCallback;

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
  bool newPrim = createResult.first;

  if (newPrim)
  {
    BRIDGE_USDWRITER.SetSceneGraphRoot(cacheEntry, name);

    BRIDGE_CACHE.InitializeWorldPrim(cacheEntry);
  }
  
  handle = UsdWorldHandle({ cacheEntry });
  return newPrim;
}

bool UsdBridge::CreateInstance(const char* name, UsdInstanceHandle& handle)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(instancePathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool newPrim = createResult.first;

  if (newPrim)
  {
    BRIDGE_USDWRITER.InitializeUsdTransform(cacheEntry);
  }

  handle = UsdInstanceHandle({ cacheEntry });
  return newPrim;
}

bool UsdBridge::CreateGroup(const char* name, UsdGroupHandle& handle)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(groupPathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool newPrim = createResult.first;

  if (newPrim)
  {
    BRIDGE_USDWRITER.InitializeUsdTransform(cacheEntry);
  }

  handle = UsdGroupHandle({ cacheEntry });
  return newPrim;
}

bool UsdBridge::CreateSurface(const char* name, UsdSurfaceHandle& handle)
{
  if (!SessionValid) return false;

  // Although surface doesn't support transform operations, a transform prim supports timevarying visibility.
  BoolEntryPair createResult = Internals->FindOrCreatePrim(surfacePathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool newPrim = createResult.first;

  if (newPrim)
  {
    BRIDGE_USDWRITER.InitializeUsdTransform(cacheEntry);
  }

  handle = UsdSurfaceHandle({ cacheEntry });
  return newPrim;
}

bool UsdBridge::CreateVolume(const char * name, UsdVolumeHandle& handle)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(volumePathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool newPrim = createResult.first;

  if (newPrim)
  {
    BRIDGE_USDWRITER.InitializeUsdTransform(cacheEntry);
  }

  handle = UsdVolumeHandle({ cacheEntry });
  return newPrim;
}

template<typename GeomDataType>
bool UsdBridge::CreateGeometryTemplate(const char* name, const GeomDataType& geomData, UsdGeometryHandle& handle)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(geometryPathCp, name);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool newPrim = createResult.first;

  if (newPrim)
  {
#ifdef VALUE_CLIP_RETIMING
    BRIDGE_USDWRITER.OpenPrimStage(name, geomPrimStagePf, cacheEntry

#ifdef TIME_CLIP_STAGES
      , true);

    BRIDGE_USDWRITER.CreateUsdGeometryManifest(name, cacheEntry, geomData);
#else
      , false);

    UsdStageRefPtr geomStage = cacheEntry->PrimStage.second;
    BRIDGE_USDWRITER.InitializeUsdGeometry(geomStage, cacheEntry->PrimPath, geomData, false);
#endif
#endif

    BRIDGE_USDWRITER.InitializeUsdGeometry(BRIDGE_USDWRITER.GetSceneStage(), cacheEntry->PrimPath, geomData, true);
  }

  handle = UsdGeometryHandle({ cacheEntry });
  return newPrim;
}

bool UsdBridge::CreateGeometry(const char* name, const UsdBridgeMeshData& meshData, UsdGeometryHandle& handle)
{
  return CreateGeometryTemplate<UsdBridgeMeshData>(name, meshData, handle);
}

bool UsdBridge::CreateGeometry(const char* name, const UsdBridgeInstancerData& instancerData, UsdGeometryHandle& handle)
{
  return CreateGeometryTemplate<UsdBridgeInstancerData>(name, instancerData, handle);
}

bool UsdBridge::CreateGeometry(const char* name, const UsdBridgeCurveData& curveData, UsdGeometryHandle& handle)
{
  return CreateGeometryTemplate<UsdBridgeCurveData>(name, curveData, handle);
}

bool UsdBridge::CreateSpatialField(const char * name, UsdSpatialFieldHandle& handle)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(fieldPathCp, name, &ResourceCollectVolume);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool newPrim = createResult.first;

  if (newPrim)
  {
#ifdef VALUE_CLIP_RETIMING
    // Create a separate stage to be referenced by the value clips
    BRIDGE_USDWRITER.OpenPrimStage(name, fieldPrimStagePf, cacheEntry, false);
    UsdStageRefPtr volumeStage = cacheEntry->PrimStage.second;
    BRIDGE_USDWRITER.InitializeUsdVolume(volumeStage, cacheEntry->PrimPath, false);
#endif   

    BRIDGE_USDWRITER.InitializeUsdVolume(BRIDGE_USDWRITER.GetSceneStage(), cacheEntry->PrimPath, true);
  }

  handle = UsdSpatialFieldHandle({ cacheEntry });
  return newPrim;
}

bool UsdBridge::CreateMaterial(const char* name, UsdMaterialHandle& handle)
{
  if (!SessionValid) return false;

  // Create the material
  BoolEntryPair matCreateResult = Internals->FindOrCreatePrim(materialPathCp, name);
  UsdBridgePrimCache* matCacheEntry = matCreateResult.second;
  bool newPrim = matCreateResult.first;

  UsdMaterialHandle matHandle({ matCacheEntry });

  if (newPrim)
  {
#ifdef VALUE_CLIP_RETIMING
    // Create a separate stage to be referenced by the value clips
    BRIDGE_USDWRITER.OpenPrimStage(name, materialPrimStagePf, matCacheEntry, false);
    UsdStageRefPtr materialStage = matCacheEntry->PrimStage.second;
    BRIDGE_USDWRITER.InitializeUsdMaterial(materialStage, matCacheEntry->PrimPath, false);
#endif

    UsdShadeMaterial matPrim = BRIDGE_USDWRITER.InitializeUsdMaterial(BRIDGE_USDWRITER.GetSceneStage(), matCacheEntry->PrimPath, true);
  }

  handle = matHandle;
  return newPrim;
}

bool UsdBridge::CreateSampler(const char* name, UsdSamplerHandle& handle)
{
  if (!SessionValid) return false;

  BoolEntryPair createResult = Internals->FindOrCreatePrim(samplerPathCp, name, &ResourceCollectSampler);
  UsdBridgePrimCache* cacheEntry = createResult.second;
  bool newPrim = createResult.first;

  if (newPrim)
  {
    BRIDGE_USDWRITER.InitializeUsdSampler(cacheEntry);
  }

  handle = UsdSamplerHandle({ cacheEntry });
  return newPrim;
}

void UsdBridge::DeleteWorld(UsdWorldHandle handle)
{
  if (handle.value == nullptr) return;

  UsdBridgePrimCache* worldCache = BRIDGE_CACHE.ConvertToPrimCache(handle);

  BRIDGE_USDWRITER.RemoveSceneGraphRoot(worldCache);

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

  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::DeleteSampler(UsdSamplerHandle handle)
{
  if (handle.value == nullptr) return;

  Internals->FindAndDeletePrim(handle);
}

void UsdBridge::SetInstanceRefs(UsdWorldHandle world, const UsdInstanceHandle* instances, uint64_t numInstances, bool timeVarying, double timeStep)
{
  if (world.value == nullptr) return;

  UsdBridgePrimCache* worldCache = BRIDGE_CACHE.ConvertToPrimCache(world);
  const UsdBridgePrimCacheList& instanceCaches = Internals->ExtractPrimCaches<UsdInstanceHandle>(BRIDGE_CACHE, instances, numInstances);

  BRIDGE_USDWRITER.ManageUnusedRefs(worldCache, instanceCaches, nullptr, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
  for (uint64_t i = 0; i < numInstances; ++i)
  {
    BRIDGE_USDWRITER.AddRef_NoClip(worldCache, instanceCaches[i], nullptr, timeVarying, timeStep, timeStep, false, Internals->RefModCallbacks);
  }
}

void UsdBridge::SetGroupRef(UsdInstanceHandle instance, UsdGroupHandle group, bool timeVarying, double timeStep)
{
  if (instance.value == nullptr) return;

  UsdBridgePrimCache* instanceCache = BRIDGE_CACHE.ConvertToPrimCache(instance);
  UsdBridgePrimCache* groupCache = BRIDGE_CACHE.ConvertToPrimCache(group);

  BRIDGE_USDWRITER.AddRef_NoClip(instanceCache, groupCache, nullptr, timeVarying, timeStep, timeStep, true, Internals->RefModCallbacks);
}

void UsdBridge::SetSurfaceRefs(UsdGroupHandle group, const UsdSurfaceHandle* surfaces, uint64_t numSurfaces, bool timeVarying, double timeStep)
{
  if (group.value == nullptr) return;

  UsdBridgePrimCache* groupCache = BRIDGE_CACHE.ConvertToPrimCache(group);
  const UsdBridgePrimCacheList& surfaceCaches = Internals->ExtractPrimCaches<UsdSurfaceHandle>(BRIDGE_CACHE, surfaces, numSurfaces);

  BRIDGE_USDWRITER.ManageUnusedRefs(groupCache, surfaceCaches, surfacePathRp, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
  for (uint64_t i = 0; i < numSurfaces; ++i)
  {
    BRIDGE_USDWRITER.AddRef_NoClip(groupCache, surfaceCaches[i], surfacePathRp, timeVarying, timeStep, timeStep, false, Internals->RefModCallbacks);
  }
}

void UsdBridge::SetVolumeRefs(UsdGroupHandle group, const UsdVolumeHandle* volumes, uint64_t numVolumes, bool timeVarying, double timeStep)
{
  if (group.value == nullptr) return;

  UsdBridgePrimCache* groupCache = BRIDGE_CACHE.ConvertToPrimCache(group);
  const UsdBridgePrimCacheList& volumeCaches = Internals->ExtractPrimCaches<UsdVolumeHandle>(BRIDGE_CACHE, volumes, numVolumes);

  BRIDGE_USDWRITER.ManageUnusedRefs(groupCache, volumeCaches, volumePathRp, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
  for (uint64_t i = 0; i < numVolumes; ++i)
  {
    BRIDGE_USDWRITER.AddRef_NoClip(groupCache, volumeCaches[i], volumePathRp, timeVarying, timeStep, timeStep, false, Internals->RefModCallbacks);
  }
}

void UsdBridge::SetGeometryMaterialRef(UsdSurfaceHandle surface, UsdGeometryHandle geometry, UsdMaterialHandle material, double timeStep, double geomTimeStep, double matTimeStep)
{
  if (surface.value == nullptr) return;

  UsdBridgePrimCache* surfaceCache = BRIDGE_CACHE.ConvertToPrimCache(surface);
  UsdBridgePrimCache* geometryCache = BRIDGE_CACHE.ConvertToPrimCache(geometry);
  UsdBridgePrimCache* materialCache = BRIDGE_CACHE.ConvertToPrimCache(material);

  // Update the references
  SdfPath refGeomPath = BRIDGE_USDWRITER.AddRef(surfaceCache, geometryCache, geometryPathRp, false, true, true, geomClipPf, timeStep, geomTimeStep, true, Internals->RefModCallbacks); // Can technically be timeVarying, but would be a bit confusing. Instead, timevary the surface.
  SdfPath refMatPath = BRIDGE_USDWRITER.AddRef(surfaceCache, materialCache, materialPathRp, false, true, false, nullptr, timeStep, matTimeStep, true, Internals->RefModCallbacks);

  // Bind the referencing material to the referencing geom prim (as they are within same scope in this usd prim)
  BRIDGE_USDWRITER.BindMaterialToGeom(refGeomPath, refMatPath);
}

void UsdBridge::SetSpatialFieldRef(UsdVolumeHandle volume, UsdSpatialFieldHandle field, double timeStep, double fieldTimeStep)
{
  if (volume.value == nullptr) return;

  UsdBridgePrimCache* volumeCache = BRIDGE_CACHE.ConvertToPrimCache(volume);
  UsdBridgePrimCache* fieldCache = BRIDGE_CACHE.ConvertToPrimCache(field);

  SdfPath refVolPath = BRIDGE_USDWRITER.AddRef(volumeCache, fieldCache, fieldPathRp, false, true, false, nullptr, timeStep, fieldTimeStep, true, Internals->RefModCallbacks); // Can technically be timeVarying, but would be a bit confusing. Instead, timevary the volume.
}

void UsdBridge::SetSamplerRef(UsdMaterialHandle material, UsdSamplerHandle sampler, const char* texfileName, double timeStep)
{
  if (material.value == nullptr) return;

  UsdBridgePrimCache* matCache = BRIDGE_CACHE.ConvertToPrimCache(material);
  SdfPath& matPrimPath = matCache->PrimPath;// .AppendPath(SdfPath(materialAttribPf));

  UsdBridgePrimCache* samplerCache = BRIDGE_CACHE.ConvertToPrimCache(sampler);

  // Sampler references cannot be time-varying (connectToSource doesn't allow for that), and are set on the scenestage prim (so they inherit the type of the sampler prim)
  SdfPath refSamplerPath = BRIDGE_USDWRITER.AddRef_NoClip(matCache, samplerCache, samplerPathRp, false, timeStep, timeStep, true, Internals->RefModCallbacks);

  UsdStageRefPtr materialStage = BRIDGE_USDWRITER.GetTimeVarStage(matCache).first;

  BRIDGE_USDWRITER.BindSamplerToMaterial(materialStage, matPrimPath, refSamplerPath, texfileName);

#ifdef VALUE_CLIP_RETIMING
  materialStage->Save();
#endif
}

void UsdBridge::DeleteInstanceRefs(UsdWorldHandle world, bool timeVarying, double timeStep)
{
  if (world.value == nullptr) return;

  UsdBridgePrimCache* worldCache = BRIDGE_CACHE.ConvertToPrimCache(world);

  BRIDGE_USDWRITER.RemoveAllRefs(worldCache, nullptr, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
}

void UsdBridge::DeleteGroupRef(UsdInstanceHandle instance, bool timeVarying, double timeStep)
{
  if (instance.value == nullptr) return;

  UsdBridgePrimCache* instanceCache = BRIDGE_CACHE.ConvertToPrimCache(instance);

  BRIDGE_USDWRITER.RemoveAllRefs(instanceCache, nullptr, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
}

void UsdBridge::DeleteSurfaceRefs(UsdGroupHandle group, bool timeVarying, double timeStep)
{
  if (group.value == nullptr) return;

  UsdBridgePrimCache* groupCache = BRIDGE_CACHE.ConvertToPrimCache(group);

  BRIDGE_USDWRITER.RemoveAllRefs(groupCache, surfacePathRp, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
}

void UsdBridge::DeleteVolumeRefs(UsdGroupHandle group, bool timeVarying, double timeStep)
{
  if (group.value == nullptr) return;

  UsdBridgePrimCache* groupCache = BRIDGE_CACHE.ConvertToPrimCache(group);

  BRIDGE_USDWRITER.RemoveAllRefs(groupCache, volumePathRp, timeVarying, timeStep, Internals->RefModCallbacks.AtRemoveRef);
}

void UsdBridge::DeleteGeometryRef(UsdSurfaceHandle surface, double timeStep)
{
  if (surface.value == nullptr) return;

  UsdBridgePrimCache* surfaceCache = BRIDGE_CACHE.ConvertToPrimCache(surface);

  BRIDGE_USDWRITER.RemoveAllRefs(surfaceCache, surfacePathRp, false, timeStep, Internals->RefModCallbacks.AtRemoveRef);
}

void UsdBridge::DeleteSpatialFieldRef(UsdVolumeHandle volume, double timeStep)
{
  if (volume.value == nullptr) return;

  UsdBridgePrimCache* volumeCache = BRIDGE_CACHE.ConvertToPrimCache(volume);

  BRIDGE_USDWRITER.RemoveAllRefs(volumeCache, fieldPathRp, false, timeStep, Internals->RefModCallbacks.AtRemoveRef);
}

void UsdBridge::DeleteMaterialRef(UsdSurfaceHandle surface, double timeStep)
{
  if (surface.value == nullptr) return;

  UsdBridgePrimCache* surfaceCache = BRIDGE_CACHE.ConvertToPrimCache(surface);

  BRIDGE_USDWRITER.RemoveAllRefs(surfaceCache, materialPathRp, false, timeStep, Internals->RefModCallbacks.AtRemoveRef);
}

void UsdBridge::DeleteSamplerRef(UsdMaterialHandle material, double timeStep)
{
  if (material.value == nullptr) return;

  UsdBridgePrimCache* matCache = BRIDGE_CACHE.ConvertToPrimCache(material);
  const SdfPath& matPrimPath = matCache->PrimPath;// .AppendPath(SdfPath(materialAttribPf));

  BRIDGE_USDWRITER.UnBindSamplerFromMaterial(matPrimPath);

  // Remove the sampler reference from the material node
  BRIDGE_USDWRITER.RemoveAllRefs(matCache, samplerPathRp, false, timeStep, Internals->RefModCallbacks.AtRemoveRef);
}

void UsdBridge::UpdateBeginEndTime(double timeStep)
{
  if (!SessionValid) return;

  BRIDGE_USDWRITER.UpdateBeginEndTime(timeStep);
}

void UsdBridge::SetInstanceTransform(UsdInstanceHandle instance, float* transform, bool timeVarying, double timeStep)
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

  std::pair<UsdStageRefPtr, bool> stageCreatePair = BRIDGE_USDWRITER.GetTimeVarStage(cache
#ifdef TIME_CLIP_STAGES
    , true, geomClipPf, timeStep
#endif
  );
  UsdStageRefPtr geomStage = stageCreatePair.first;
  if (stageCreatePair.second)
  {
    BRIDGE_USDWRITER.InitializeUsdGeometry(geomStage, geomPath, geomData, false);
  }

  BRIDGE_USDWRITER.UpdateUsdGeometry(geomStage, geomPath, geomData, timeStep);

#ifdef VALUE_CLIP_RETIMING
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

void UsdBridge::SetVolumeData(UsdSpatialFieldHandle field, const UsdBridgeVolumeData & volumeData, double timeStep)
{
  if (field.value == nullptr) return;

  UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(field);

  UsdStageRefPtr volumeStage = BRIDGE_USDWRITER.GetTimeVarStage(cache).first;

  // To avoid data duplication when using of clip stages, we need to potentially use the scenestage prim for time-uniform data.
  BRIDGE_USDWRITER.UpdateUsdVolume(volumeStage, cache->PrimPath, cache->Name.GetString(), volumeData, timeStep);

#ifdef VALUE_CLIP_RETIMING
  volumeStage->Save();
#endif
}

void UsdBridge::SetMaterialData(UsdMaterialHandle material, const UsdBridgeMaterialData& matData, double timeStep)
{
  if (material.value == nullptr) return;

  UsdBridgePrimCache* matCache = BRIDGE_CACHE.ConvertToPrimCache(material);
  SdfPath& matPrimPath = matCache->PrimPath;
  
  UsdStageRefPtr materialStage = BRIDGE_USDWRITER.GetTimeVarStage(matCache).first;

  BRIDGE_USDWRITER.UpdateUsdMaterial(materialStage, matPrimPath, matData, timeStep);

#ifdef VALUE_CLIP_RETIMING
  materialStage->Save();
#endif
}

void UsdBridge::SetSamplerData(UsdSamplerHandle sampler, const UsdBridgeSamplerData& samplerData, double timeStep)
{
  if (sampler.value == nullptr) return;

  UsdBridgePrimCache* cache = BRIDGE_CACHE.ConvertToPrimCache(sampler);

  SdfPath& samplerPrimPath = cache->PrimPath;// .AppendPath(SdfPath(samplerAttribPf));
  
  BRIDGE_USDWRITER.UpdateUsdSampler(samplerPrimPath, samplerData, timeStep);
}

void UsdBridge::SaveScene()
{
  if (!SessionValid) return;

  BRIDGE_USDWRITER.GetSceneStage()->Save();
}

void UsdBridge::GarbageCollect()
{
#ifdef TIME_BASED_CACHING
  BRIDGE_CACHE.RemoveUnreferencedPrimCaches(
    [this](ConstPrimCacheIterator it) 
    { 
      UsdBridgePrimCache* cacheEntry = (*it).second.get();

      if(cacheEntry->ResourceCollect)
        cacheEntry->ResourceCollect(cacheEntry, BRIDGE_USDWRITER);

      BRIDGE_USDWRITER.DeletePrim(cacheEntry);
    }
  );
  BRIDGE_USDWRITER.GetSceneStage()->Save();
#endif
}

void UsdBridge::SetConnectionLogVerbosity(int logVerbosity)
{
  int logLevel = UsdBridgeRemoteConnection::GetConnectionLogLevelMax() - logVerbosity; // Just invert verbosity to get the level
  UsdBridgeRemoteConnection::SetConnectionLogLevel(logLevel);
}
