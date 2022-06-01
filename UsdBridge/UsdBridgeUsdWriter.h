// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeUsdWriter_h
#define UsdBridgeUsdWriter_h

#include "usd.h"
PXR_NAMESPACE_USING_DIRECTIVE

#include "UsdBridgeData.h"
#include "UsdBridgeCaches.h"
#include "UsdBridgeVolumeWriter.h"
#include "UsdBridgeConnection.h"

#include <memory>
#include <functional>

typedef std::pair<UsdStageRefPtr, bool> StageCreatePair;

//Includes detailed usd translation interface of Usd Bridge
class UsdBridgeUsdWriter
{
public:
  typedef std::function<void(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache)> AtNewRefFunc;
  typedef std::function<void(UsdBridgePrimCache* parentCache, const std::string& childName)> AtRemoveRefFunc;
  struct RefModFuncs
  {
    AtNewRefFunc AtNewRef;
    AtRemoveRefFunc AtRemoveRef;
  };

  UsdBridgeUsdWriter(const UsdBridgeSettings& settings);
  ~UsdBridgeUsdWriter();

  void SetSceneStage(UsdStageRefPtr sceneStage);
  void SetEnableSaving(bool enableSaving);

  int FindSessionNumber();
  bool CreateDirectories();
#ifdef SUPPORT_MDL_SHADERS 
  bool CreateMdlFiles();
#endif
  bool InitializeSession();
  void ResetSession();

  bool OpenSceneStage();
  UsdStageRefPtr GetSceneStage();
  StageCreatePair GetTimeVarStage(UsdBridgePrimCache* cache
#ifdef TIME_CLIP_STAGES
    // If useClipStages, StageCreatePair.second is true when the clipStage is newly created (to signify need for prim initialization)
    , bool useClipStage = false, const char* clipPf = nullptr, double timeStep = 0.0
#endif
    );
#ifdef VALUE_CLIP_RETIMING
  void OpenPrimStage(const char* name, const char* primPostfix, UsdBridgePrimCache* cacheEntry, bool isManifest);
  void SharePrimStage(UsdBridgePrimCache* owningCache, UsdBridgePrimCache* sharingCache);
  void RemovePrimStage(const UsdBridgePrimCache* cacheEntry);
#endif
#ifdef TIME_CLIP_STAGES
  const UsdStagePair& FindOrCreatePrimClipStage(UsdBridgePrimCache* cacheEntry, const char* clipPostfix, double timeStep, bool& exists);
#endif
  void SetSceneGraphRoot(UsdBridgePrimCache* worldCache, const char* name);
  void RemoveSceneGraphRoot(UsdBridgePrimCache* worldCache);

  const std::string& CreatePrimName(const char* name, const char* category);
  bool CreatePrim(const SdfPath& path);
  void DeletePrim(const UsdBridgePrimCache* cacheEntry);
  void InitializePrimVisibility(UsdStageRefPtr stage, const SdfPath& primPath, const UsdTimeCode& timeCode);
  void SetPrimVisibility(UsdStageRefPtr stage, const SdfPath& primPath, const UsdTimeCode& timeCode, bool visible);
  void SetChildrenVisibility(UsdStageRefPtr stage, const SdfPath& parentPath, const UsdTimeCode& timeCode, bool visible);
#ifdef TIME_BASED_CACHING
  void PrimRemoveIfVisible(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, const UsdPrim& prim, bool timeVarying, const UsdTimeCode& timeCode, AtRemoveRefFunc atRemoveRef);
  void ChildrenRemoveIfVisible(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, const SdfPath& parentPath, bool timeVarying, const UsdTimeCode& timeCode, AtRemoveRefFunc atRemoveRef, const SdfPath& exceptPath = SdfPath());
#endif

#ifdef VALUE_CLIP_RETIMING
  void InitializeClipMetaData(const UsdPrim& clipPrim, UsdBridgePrimCache* childCache, double parentTimeStep, double childTimeStep, bool clipStages, const char* clipPostfix);
  void UpdateClipMetaData(const UsdPrim& clipPrim, UsdBridgePrimCache* childCache, double parentTimeStep, double childTimeStep, bool clipStages, const char* clipPostfix);
#endif

  SdfPath AddRef_NoClip(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
    bool timeVarying, double parentTimeStep, double childTimeStep,
    const RefModFuncs& refModCallbacks);
  SdfPath AddRef_NoClip(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
    bool timeVarying, double parentTimeStep, double childTimeStep,
    const RefModFuncs& refModCallbacks);
  SdfPath AddRef(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
    bool timeVarying, bool valueClip, bool clipStages, const char* clipPostFix,
    double parentTimeStep, double childTimeStep,
    const RefModFuncs& refModCallbacks);
  // Timevarying means that the existence of a reference between prim A and B can differ over time, valueClip denotes the addition of clip metadata for re-timing, 
  // and clipStages will carve up the referenced clip asset files into one for each timestep (stages are created as necessary). clipPostFix is irrelevant if !clipStages.
  // Returns the path to the parent's subprim which represents/holds the reference to the child's main class prim.
  SdfPath AddRef_Impl(UsdStageRefPtr parentStage, UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
    bool timeVarying, bool valueClip, bool clipStages, const char* clipPostFix,
    double parentTimeStep, double childTimeStep,
    const RefModFuncs& refModCallbacks);
  void RemoveAllRefs(UsdBridgePrimCache* parentCache, const char* refPathExt, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef);
  void RemoveAllRefs(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, SdfPath childBasePath, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef);
  void ManageUnusedRefs(UsdBridgePrimCache* parentCache, const UsdBridgePrimCacheList& newChildren, const char* refPathExt, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef);
  void ManageUnusedRefs(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, const UsdBridgePrimCacheList& newChildren, const char* refPathExt, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef);

  void InitializeUsdTransform(const UsdBridgePrimCache* cacheEntry);
  UsdPrim InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeMeshData& meshData, bool uniformPrim);
  UsdPrim InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeInstancerData& instancerData, bool uniformPrim);
  UsdPrim InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeCurveData& curveData, bool uniformPrim);
  UsdPrim InitializeUsdVolume(UsdStageRefPtr volumeStage, const SdfPath& volumePath, bool uniformPrim);
  UsdShadeMaterial InitializeUsdMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, bool uniformPrim);
  UsdShadeOutput InitializeUsdShader(UsdStageRefPtr shaderStage, const SdfPath& shadPrimPath, bool uniformPrim);
#ifdef SUPPORT_MDL_SHADERS  
  UsdShadeOutput InitializeMdlShader(UsdStageRefPtr shaderStage, const SdfPath& shadPrimPath, bool uniformPrim);
#endif
  void InitializeUsdSampler(const UsdBridgePrimCache* cacheEntry);

#ifdef TIME_CLIP_STAGES
  void CreateUsdGeometryManifest(const char* name, const UsdBridgePrimCache* cacheEntry, const UsdBridgeMeshData& meshData);
  void CreateUsdGeometryManifest(const char* name, const UsdBridgePrimCache* cacheEntry, const UsdBridgeInstancerData& instancerData);
  void CreateUsdGeometryManifest(const char* name, const UsdBridgePrimCache* cacheEntry, const UsdBridgeCurveData& curveData);
#endif

  void BindMaterialToGeom(const SdfPath& refGeomPath, const SdfPath& refMatPath);
  void BindShaderToMaterial(const UsdShadeMaterial& matPrim, const UsdShadeOutput& shadOut, TfToken* renderContext);
  void BindSamplerToMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, const SdfPath& refSamplerPrimPath, const char* texFileName);

  void UnBindSamplerFromMaterial(const SdfPath& matPrimPath);

  void UpdateUsdTransform(const SdfPath& transPrimPath, float* transform, bool timeVarying, double timeStep);
  void UpdateUsdGeometry(const UsdStagePtr& timeVarStage, const SdfPath& meshPath, const UsdBridgeMeshData& geomData, double timeStep);
  void UpdateUsdGeometry(const UsdStagePtr& timeVarStage, const SdfPath& instancerPath, const UsdBridgeInstancerData& geomData, double timeStep);
  void UpdateUsdGeometry(const UsdStagePtr& timeVarStage, const SdfPath& curvePath, const UsdBridgeCurveData& geomData, double timeStep);
  void UpdateUsdMaterial(UsdStageRefPtr materialStage, const SdfPath& matPrimPath, const UsdBridgeMaterialData& matData, double timeStep);
  void UpdateUsdShader(UsdStageRefPtr shaderStage, const SdfPath& matPrimPath, const SdfPath& shadPrimPath, const UsdBridgeMaterialData& matData, double timeStep);
#ifdef SUPPORT_MDL_SHADERS 
  void UpdateMdlShader(UsdStageRefPtr shaderStage, const SdfPath& shadPrimPath, const UsdBridgeMaterialData& matData, double timeStep);
#endif
  void UpdateUsdVolume(UsdStageRefPtr volumeStage, const SdfPath& volPrimPath, const std::string& name, const UsdBridgeVolumeData& volumeData, double timeStep);
  void UpdateUsdSampler(const SdfPath& samplerPrimPath, const UsdBridgeSamplerData& samplerData, double timeStep);
  void UpdateBeginEndTime(double timeStep);

  void* LogUserData;
  UsdBridgeLogCallback LogCallback;

  friend void ResourceCollectVolume(const UsdBridgePrimCache* cache, const UsdBridgeUsdWriter& usdWriter);
  friend void ResourceCollectSampler(const UsdBridgePrimCache* cache, const UsdBridgeUsdWriter& sceneStage);

  VtIntArray TempIndexArray;
  VtVec3fArray TempScalesArray;

  // Settings 
  UsdBridgeSettings Settings;
  UsdBridgeConnectionSettings ConnectionSettings;

protected:

  // Connect
  std::unique_ptr<UsdBridgeConnection> Connect = nullptr;

  // Volume writer
  std::shared_ptr<UsdBridgeVolumeWriterI> VolumeWriter; // shared - requires custom deleter

  // Session specific info
  int SessionNumber = -1;
  UsdStageRefPtr SceneStage;
  bool EnableSaving = true;
  std::string SceneFileName;
  std::string RelativeSceneFile; // relative from Asset Folders
  std::string SessionDirectory;
  std::string RootName;
  std::string RootClassName;
#ifdef SUPPORT_MDL_SHADERS
  SdfAssetPath MdlOpaqueRelFilePath; // relative from Scene Folder
  SdfAssetPath MdlTranslucentRelFilePath; // relative from Scene Folder
#endif

  double StartTime = 0.0;
  double EndTime = 0.0;

  std::string TempNameStr;
};


void ResourceCollectVolume(const UsdBridgePrimCache* cache, const UsdBridgeUsdWriter& usdWriter);
void ResourceCollectSampler(const UsdBridgePrimCache* cache, const UsdBridgeUsdWriter& usdWriter);

#endif