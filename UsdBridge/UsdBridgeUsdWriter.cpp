// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeUsdWriter.h"

#include "UsdBridgeCaches.h"
#include "UsdBridgeMdlStrings.h"
#include "UsdBridgeUsdWriter_Common.h"

TF_DEFINE_PUBLIC_TOKENS(
  UsdBridgeTokens,

  ATTRIB_TOKEN_SEQ
  USDPREVSURF_TOKEN_SEQ
  VOLUME_TOKEN_SEQ
  MDL_TOKEN_SEQ
  SAMPLER_TOKEN_SEQ
  MISC_TOKEN_SEQ
);

namespace constring
{
  const char* const sessionPf = "Session_";
  const char* const rootClassName = "/RootClass";
  const char* const rootPrimName = "/Root";

  const char* const manifestFolder = "manifests/";
  const char* const clipFolder = "clips/";
  const char* const primStageFolder = "primstages/";
  const char* const mdlFolder = "mdls/";
  const char* const imgFolder = "images/";
  const char* const volFolder = "volumes/";

  const char* const texCoordReaderPrimPf = "texcoordreader";
  const char* const shaderPrimPf = "shader";
  const char* const mdlShaderPrimPf = "mdlshader";
  const char* const openVDBPrimPf = "ovdbfield";

  const char* const imageExtension = ".png";
  const char* const vdbExtension = ".vdb";

  const char* const opaqueMaterialFile = "PBR_Opaque.mdl";
  const char* const transparentMaterialFile = "PBR_Transparent.mdl";
  const char* const fullSceneNameBin = "FullScene.usd";
  const char* const fullSceneNameAscii = "FullScene.usda";

}
  
#define ATTRIB_TOKENS_ADD(r, data, elem) AttributeTokens.push_back(UsdBridgeTokens->elem);

UsdBridgeUsdWriter::UsdBridgeUsdWriter(const UsdBridgeSettings& settings)
  : Settings(settings)
  , VolumeWriter(Create_VolumeWriter(), std::mem_fn(&UsdBridgeVolumeWriterI::Release))
{
  // Initialize AttributeTokens with common known attribute names
  BOOST_PP_SEQ_FOR_EACH(ATTRIB_TOKENS_ADD, ~, ATTRIB_TOKEN_SEQ)

  if(Settings.HostName)
    ConnectionSettings.HostName = Settings.HostName;
  if(Settings.OutputPath)
    ConnectionSettings.WorkingDirectory = Settings.OutputPath;
  FormatDirName(ConnectionSettings.WorkingDirectory);
}

UsdBridgeUsdWriter::~UsdBridgeUsdWriter()
{
}

void UsdBridgeUsdWriter::SetSceneStage(UsdStageRefPtr sceneStage)
{
  this->SceneStage = sceneStage;
}

void UsdBridgeUsdWriter::SetEnableSaving(bool enableSaving)
{
  this->EnableSaving = enableSaving;
}

int UsdBridgeUsdWriter::FindSessionNumber()
{
  int sessionNr = Connect->MaxSessionNr();

  sessionNr = std::max(0, sessionNr + Settings.CreateNewSession);

  return sessionNr;
}

bool UsdBridgeUsdWriter::CreateDirectories()
{
  bool valid = true;

  valid = Connect->CreateFolder("", true, true);

  //Connect->RemoveFolder(SessionDirectory.c_str(), true);
  bool folderMayExist = !Settings.CreateNewSession;
  
  valid = valid && Connect->CreateFolder(SessionDirectory.c_str(), true, folderMayExist);

#ifdef VALUE_CLIP_RETIMING
  valid = valid && Connect->CreateFolder((SessionDirectory + constring::manifestFolder).c_str(), true, folderMayExist);
  valid = valid && Connect->CreateFolder((SessionDirectory + constring::primStageFolder).c_str(), true, folderMayExist);
#endif
#ifdef TIME_CLIP_STAGES
  valid = valid && Connect->CreateFolder((SessionDirectory + constring::clipFolder).c_str(), true, folderMayExist);
#endif
#ifdef SUPPORT_MDL_SHADERS
  if(Settings.EnableMdlShader)
  {
    valid = valid && Connect->CreateFolder((SessionDirectory + constring::mdlFolder).c_str(), true, folderMayExist);
  }
#endif
  valid = valid && Connect->CreateFolder((SessionDirectory + constring::volFolder).c_str(), true, folderMayExist);

  if (!valid)
  {
    UsdBridgeLogMacro(this, UsdBridgeLogLevel::ERR, "Something went wrong in the filesystem creating the required output folders (permissions?).");
  }

  return valid;
}

#ifdef SUPPORT_MDL_SHADERS 
namespace
{
  void WriteMdlFromStrings(const char* string0, const char* string1, const char* fileName, const UsdBridgeConnection* Connect)
  {
    size_t strLen0 = std::strlen(string0);
    size_t strLen1 = std::strlen(string1);
    size_t totalStrLen = strLen0 + strLen1;
    char* Mdl_Contents = new char[totalStrLen];
    std::memcpy(Mdl_Contents, string0, strLen0);
    std::memcpy(Mdl_Contents + strLen0, string1, strLen1);

    Connect->WriteFile(Mdl_Contents, totalStrLen, fileName, true, false);

    delete[] Mdl_Contents;
  }
}

bool UsdBridgeUsdWriter::CreateMdlFiles()
{
  //Write PBR opacity file contents
  {
    std::string relMdlPath = constring::mdlFolder + std::string(constring::opaqueMaterialFile);
    this->MdlOpaqueRelFilePath = SdfAssetPath(relMdlPath);
    std::string mdlFileName = SessionDirectory + relMdlPath;

    WriteMdlFromStrings(Mdl_PBRBase_string, Mdl_PBRBase_string_opaque, mdlFileName.c_str(), Connect.get());
  }

  {
    std::string relMdlPath = constring::mdlFolder + std::string(constring::transparentMaterialFile);
    this->MdlTranslucentRelFilePath = SdfAssetPath(relMdlPath);
    std::string mdlFileName = SessionDirectory + relMdlPath;

    WriteMdlFromStrings(Mdl_PBRBase_string, Mdl_PBRBase_string_translucent, mdlFileName.c_str(), Connect.get());
  }

  return true;
}
#endif

bool UsdBridgeUsdWriter::InitializeSession()
{
  if (ConnectionSettings.HostName.empty())
  {
    if(ConnectionSettings.WorkingDirectory.compare("void") == 0)
      Connect = std::make_unique<UsdBridgeVoidConnection>();
    else
      Connect = std::make_unique<UsdBridgeLocalConnection>();
  }
  else
    Connect = std::make_unique<UsdBridgeRemoteConnection>();

  Connect->Initialize(ConnectionSettings, this->LogCallback, this->LogUserData);

  SessionNumber = FindSessionNumber();
  SessionDirectory = constring::sessionPf + std::to_string(SessionNumber) + "/";

  bool valid = true;

  valid = CreateDirectories();

  valid = valid && VolumeWriter->Initialize(this->LogCallback, this->LogUserData);

#ifdef SUPPORT_MDL_SHADERS 
  if(Settings.EnableMdlShader)
  {
    valid = valid && CreateMdlFiles();
  }
#endif

  return valid;
}

void UsdBridgeUsdWriter::ResetSession()
{
  this->SessionNumber = -1;
  this->SceneStage = nullptr;
}

bool UsdBridgeUsdWriter::OpenSceneStage()
{
  bool binary = this->Settings.BinaryOutput;

  this->SceneFileName = this->SessionDirectory; 
  this->SceneFileName += (binary ? constring::fullSceneNameBin : constring::fullSceneNameAscii);

  const char* absSceneFile = Connect->GetUrl(this->SceneFileName.c_str());
  if (!this->SceneStage && !Settings.CreateNewSession)
      this->SceneStage = UsdStage::Open(absSceneFile);
  if (!this->SceneStage)
    this->SceneStage = UsdStage::CreateNew(absSceneFile);
  
  if (!this->SceneStage)
  {
    UsdBridgeLogMacro(this, UsdBridgeLogLevel::ERR, "Scene UsdStage cannot be created or opened. Maybe a filesystem issue?");
    return false;
  }

  this->RootClassName = constring::rootClassName;
  UsdPrim rootClassPrim = this->SceneStage->CreateClassPrim(SdfPath(this->RootClassName));
  assert(rootClassPrim);
  this->RootName = constring::rootPrimName;
  UsdPrim rootPrim = this->SceneStage->DefinePrim(SdfPath(this->RootName));
  assert(rootPrim);

  this->SceneStage->SetDefaultPrim(rootPrim);
  UsdModelAPI(rootPrim).SetKind(KindTokens->component);

  if(!this->SceneStage->HasAuthoredTimeCodeRange())
  {
    this->SceneStage->SetStartTimeCode(StartTime);
    this->SceneStage->SetEndTimeCode(EndTime);
  }
  else
  {
    StartTime = this->SceneStage->GetStartTimeCode();
    EndTime = this->SceneStage->GetEndTimeCode();
  }

  if(this->EnableSaving)
    this->SceneStage->Save();

  return true;
}

UsdStageRefPtr UsdBridgeUsdWriter::GetSceneStage()
{
  return this->SceneStage;
}

UsdStageRefPtr UsdBridgeUsdWriter::GetTimeVarStage(UsdBridgePrimCache* cache
#ifdef TIME_CLIP_STAGES
  , bool useClipStage, const char* clipPf, double timeStep
  , std::function<void (UsdStageRefPtr)> initFunc
#endif
  ) const
{
#ifdef VALUE_CLIP_RETIMING
#ifdef TIME_CLIP_STAGES
  if (useClipStage)
  {
    bool exists;
    UsdStageRefPtr clipStage = this->FindOrCreateClipStage(cache, clipPf, timeStep, exists).second;

    if(!exists)
      initFunc(clipStage);

    return clipStage;
  }
  else
    return cache->GetPrimStagePair().second;
#else
  return cache->GetPrimStagePair().second;
#endif
#else
  return this->SceneStage;
#endif
}

#ifdef VALUE_CLIP_RETIMING
void UsdBridgeUsdWriter::CreateManifestStage(const char* name, const char* primPostfix, UsdBridgePrimCache* cacheEntry)
{
  bool binary = this->Settings.BinaryOutput;

  cacheEntry->ManifestStage.first = constring::manifestFolder + std::string(name) + primPostfix + (binary ? ".usd" : ".usda");

  std::string absoluteFileName = Connect->GetUrl((this->SessionDirectory + cacheEntry->ManifestStage.first).c_str());

  cacheEntry->ManifestStage.second = UsdStage::CreateNew(absoluteFileName);
  if (!cacheEntry->ManifestStage.second)
    cacheEntry->ManifestStage.second = UsdStage::Open(absoluteFileName);

  assert(cacheEntry->ManifestStage.second);

  cacheEntry->ManifestStage.second->DefinePrim(SdfPath(this->RootClassName));
}

void UsdBridgeUsdWriter::RemoveManifestAndClipStages(const UsdBridgePrimCache* cacheEntry)
{
  // May be superfluous
  assert(cacheEntry->ManifestStage.second);
  cacheEntry->ManifestStage.second->RemovePrim(SdfPath(RootClassName));

  // Remove ManifestStage file itself
  assert(!cacheEntry->ManifestStage.first.empty());
  Connect->RemoveFile((SessionDirectory + cacheEntry->ManifestStage.first).c_str(), true);

  // remove all clipstage files
  for (auto& x : cacheEntry->ClipStages)
  {
    Connect->RemoveFile((SessionDirectory + x.second.first).c_str(), true);
  }
}

const UsdStagePair& UsdBridgeUsdWriter::FindOrCreatePrimStage(UsdBridgePrimCache* cacheEntry, const char* namePostfix) const
{
  bool exists;
  return FindOrCreatePrimClipStage(cacheEntry, namePostfix, false, UsdBridgePrimCache::PrimStageTimeCode, exists);
}

const UsdStagePair& UsdBridgeUsdWriter::FindOrCreateClipStage(UsdBridgePrimCache* cacheEntry, const char* namePostfix, double timeStep, bool& exists) const
{
  return FindOrCreatePrimClipStage(cacheEntry, namePostfix, true, timeStep, exists);
}

const UsdStagePair& UsdBridgeUsdWriter::FindOrCreatePrimClipStage(UsdBridgePrimCache* cacheEntry, const char* namePostfix, bool isClip, double timeStep, bool& exists) const
{
  exists = true;
  bool binary = this->Settings.BinaryOutput;

  auto it = cacheEntry->ClipStages.find(timeStep);
  if (it == cacheEntry->ClipStages.end())
  {
    // Create a new Clipstage
    const char* folder = constring::primStageFolder;
    std::string fullNamePostfix(namePostfix); 
    if(isClip) 
    {
      folder = constring::clipFolder;
      fullNamePostfix += std::to_string(timeStep); 
    }
    std::string relativeFileName = folder + cacheEntry->Name.GetString() + fullNamePostfix + (binary ? ".usd" : ".usda");
    std::string absoluteFileName = Connect->GetUrl((this->SessionDirectory + relativeFileName).c_str());

    UsdStageRefPtr primClipStage = UsdStage::CreateNew(absoluteFileName);
    exists = !primClipStage;

    SdfPath rootPrimPath(this->RootClassName);
    if (exists)
    {
      primClipStage = UsdStage::Open(absoluteFileName); //Could happen if written folder is reused 
      assert(primClipStage->GetPrimAtPath(rootPrimPath));
    }
    else
      primClipStage->DefinePrim(rootPrimPath);

    it = cacheEntry->ClipStages.emplace(timeStep, UsdStagePair(std::move(relativeFileName), primClipStage)).first;
  }
  return it->second;
}
#endif

void UsdBridgeUsdWriter::SetSceneGraphRoot(UsdBridgePrimCache* worldCache, const char* name)
{
  // Also add concrete scenegraph prim with reference to world class
  SdfPath sceneGraphPath(this->RootName + "/" + name);

  UsdPrim sceneGraphPrim = this->SceneStage->DefinePrim(sceneGraphPath);
  assert(sceneGraphPrim);
  sceneGraphPrim.GetReferences().AddInternalReference(worldCache->PrimPath); //local and one-one, so no difference between ref or inherit
}

void UsdBridgeUsdWriter::RemoveSceneGraphRoot(UsdBridgePrimCache* worldCache)
{
  // Delete concrete scenegraph prim with reference to world class
  SdfPath sceneGraphPath(this->RootName);
  sceneGraphPath = sceneGraphPath.AppendPath(worldCache->Name);
  this->SceneStage->RemovePrim(sceneGraphPath);
}

const std::string& UsdBridgeUsdWriter::CreatePrimName(const char * name, const char * category)
{
  (this->TempNameStr = this->RootClassName).append("/").append(category).append("/").append(name);
  return this->TempNameStr;
}

const std::string& UsdBridgeUsdWriter::GetResourceFileName(const std::string& basePath, double timeStep, const char* fileExtension)
{
  this->TempNameStr = basePath;
#ifdef TIME_BASED_CACHING
  this->TempNameStr += "_";
  this->TempNameStr += std::to_string(timeStep);
#endif
  this->TempNameStr += fileExtension; 

  return this->TempNameStr;
}

const std::string& UsdBridgeUsdWriter::GetResourceFileName(const char* folderName, const std::string& objectName, double timeStep, const char* fileExtension)
{
  return GetResourceFileName(folderName + objectName, timeStep, fileExtension);
}

const std::string& UsdBridgeUsdWriter::GetResourceFileName(const char* folderName, const char* optionalObjectName, const std::string& defaultObjectName, double timeStep, const char* fileExtension)
{
  return GetResourceFileName(folderName, (optionalObjectName ? std::string(optionalObjectName) : defaultObjectName), timeStep, fileExtension);
}

bool UsdBridgeUsdWriter::CreatePrim(const SdfPath& path)
{
  UsdPrim classPrim = SceneStage->GetPrimAtPath(path);
  if(!classPrim)
  {
    classPrim = SceneStage->DefinePrim(path);
    assert(classPrim);
    return true;
  }
  return false;
}

void UsdBridgeUsdWriter::DeletePrim(const UsdBridgePrimCache* cacheEntry)
{
  SceneStage->RemovePrim(cacheEntry->PrimPath);

#ifdef VALUE_CLIP_RETIMING
  if (cacheEntry->ManifestStage.second)
  {
    RemoveManifestAndClipStages(cacheEntry);
  }
#endif
}

void UsdBridgeUsdWriter::InitializePrimVisibility(UsdStageRefPtr stage, const SdfPath& primPath, const UsdTimeCode& timeCode)
{
  UsdGeomImageable imageable = UsdGeomImageable::Get(stage, primPath);

  if (imageable)
  {
    imageable.CreateVisibilityAttr(VtValue(UsdGeomTokens->invisible)); // default is invisible
    UsdAttribute visAttrib = imageable.GetVisibilityAttr(); // in case MakeVisible fails

    double startTime = stage->GetStartTimeCode();
    double endTime = stage->GetEndTimeCode();
    if (startTime < timeCode)
      visAttrib.Set(VtValue(UsdGeomTokens->invisible), startTime);//imageable.MakeInvisible(startTime);
    if (endTime > timeCode)
      visAttrib.Set(VtValue(UsdGeomTokens->invisible), endTime);//imageable.MakeInvisible(endTime);
    visAttrib.Set(VtValue(UsdGeomTokens->inherited), timeCode);//imageable.MakeVisible(timeCode);
  }
}

void UsdBridgeUsdWriter::SetPrimVisibility(UsdStageRefPtr stage, const SdfPath& primPath, const UsdTimeCode& timeCode, bool visible)
{
  UsdGeomImageable imageable = UsdGeomImageable::Get(stage, primPath);
  if (imageable)
  {
    UsdAttribute visAttrib = imageable.GetVisibilityAttr();
    assert(visAttrib);

    if (visible)
      visAttrib.Set(VtValue(UsdGeomTokens->inherited), timeCode);//imageable.MakeVisible(timeCode);
    else
      visAttrib.Set(VtValue(UsdGeomTokens->invisible), timeCode);//imageable.MakeInvisible(timeCode);
  }
}

void UsdBridgeUsdWriter::SetChildrenVisibility(UsdStageRefPtr stage, const SdfPath& parentPath, const UsdTimeCode& timeCode, bool visible)
{
  UsdPrim parentPrim = stage->GetPrimAtPath(parentPath);
  if (parentPrim)
  {
    UsdPrimSiblingRange children = parentPrim.GetAllChildren();
    for (UsdPrim child : children)
    {
      SetPrimVisibility(stage, child.GetPath(), timeCode, visible);
    }
  }
}

#ifdef TIME_BASED_CACHING
void UsdBridgeUsdWriter::PrimRemoveIfVisible(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, const UsdPrim& prim, bool timeVarying, const UsdTimeCode& timeCode, AtRemoveRefFunc atRemoveRef)
{
  const SdfPath& primPath = prim.GetPath();

  bool removePrim = true;

  if (timeVarying)
  {
    UsdGeomImageable imageable = UsdGeomImageable::Get(stage, primPath);
    if (imageable)
    {
      TfToken visibilityToken;
      UsdAttribute visAttrib = imageable.GetVisibilityAttr();
      if (visAttrib)
      {
        visAttrib.Get(&visibilityToken, timeCode);
        std::vector<double> times;
        visAttrib.GetTimeSamplesInInterval(GfInterval(timeCode.GetValue()), &times);

        if (times.size() == 0 || visibilityToken == UsdGeomTokens->invisible)
        {
          // Do not remove prim if timevarying and invisible, but make sure
          // that invisibility is explicitly set (non-default)
          removePrim = false;
          visAttrib.Set(UsdGeomTokens->invisible, timeCode);
        }
      }
    }
  }

  if (removePrim)
  {
    // Decrease the ref on the representing cache entry
    const std::string& childName = prim.GetName().GetString();
    
    atRemoveRef(parentCache, childName);
    
    // Remove the prim
    stage->RemovePrim(primPath);
  }
}

void UsdBridgeUsdWriter::ChildrenRemoveIfVisible(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, const SdfPath& parentPath, bool timeVarying, const UsdTimeCode& timeCode, AtRemoveRefFunc atRemoveRef, const SdfPath& exceptPath)
{
  UsdPrim parentPrim = stage->GetPrimAtPath(parentPath);
  if (parentPrim)
  {
    UsdPrimSiblingRange children = parentPrim.GetAllChildren();
    for (UsdPrim child : children)
    {
      if (child.GetPath() != exceptPath)
        PrimRemoveIfVisible(stage, parentCache, child, timeVarying, timeCode, atRemoveRef);
    }
  }
}

#endif


#ifdef VALUE_CLIP_RETIMING
void UsdBridgeUsdWriter::InitializeClipMetaData(const UsdPrim& clipPrim, UsdBridgePrimCache* childCache, double parentTimeStep, double childTimeStep, bool clipStages, const char* clipPostfix)
{
  UsdClipsAPI clipsApi(clipPrim);

  clipsApi.SetClipPrimPath(childCache->PrimPath.GetString());

  const std::string& manifestPath = childCache->ManifestStage.first;
  const std::string* refStagePath;
#ifdef TIME_CLIP_STAGES
  if (clipStages)
  {
    //set interpolatemissingclipvalues?

    bool exists;
    const UsdStagePair& childStagePair = FindOrCreateClipStage(childCache, clipPostfix, childTimeStep, exists);
    assert(exists);

    refStagePath = &childStagePair.first;
  }
  else
#endif
  {
    refStagePath = &childCache->GetPrimStagePair().first;
  }

  clipsApi.SetClipManifestAssetPath(SdfAssetPath(manifestPath));

  VtArray<SdfAssetPath> assetPaths;
  assetPaths.push_back(SdfAssetPath(*refStagePath));
  clipsApi.SetClipAssetPaths(assetPaths);

  VtVec2dArray clipActives;
  clipActives.push_back(GfVec2d(parentTimeStep, 0));
  clipsApi.SetClipActive(clipActives);

  VtVec2dArray clipTimes;
  clipTimes.push_back(GfVec2d(parentTimeStep, childTimeStep));
  clipsApi.SetClipTimes(clipTimes);
}

void UsdBridgeUsdWriter::UpdateClipMetaData(const UsdPrim& clipPrim, UsdBridgePrimCache* childCache, double parentTimeStep, double childTimeStep, bool clipStages, const char* clipPostfix)
{
  // Add parent-child timestep or update existing relationship
  UsdClipsAPI clipsApi(clipPrim);

#ifdef TIME_CLIP_STAGES
  if (clipStages)
  {
    bool exists;
    const UsdStagePair& childStagePair = FindOrCreateClipStage(childCache, clipPostfix, childTimeStep, exists);
    // At this point, exists should be true, but if clip stage creation failed earlier due to user error, 
    // exists will be false and we'll just link to the empty new stage created by FindOrCreatePrimClipStage()

    const std::string& refStagePath = childStagePair.first;

    VtVec2dArray clipActives;
    clipsApi.GetClipActive(&clipActives);
    VtArray<SdfAssetPath> assetPaths;
    clipsApi.GetClipAssetPaths(&assetPaths);

    // Find the asset path
    SdfAssetPath refStageSdf(refStagePath);
    auto assetIt = std::find_if(assetPaths.begin(), assetPaths.end(), [&refStageSdf](const SdfAssetPath& entry) -> bool { return refStageSdf.GetAssetPath() == entry.GetAssetPath(); });
    int assetIndex = int(assetIt - assetPaths.begin());
    bool newAsset = (assetIndex == assetPaths.size()); // Gives the opportunity to garbage collect unused asset references

    {
      // Find the parentTimeStep
      int timeFindIdx = 0;
      for (; timeFindIdx < clipActives.size() && clipActives[timeFindIdx][0] != parentTimeStep; ++timeFindIdx)
      {}

      bool replaceAsset = false;

      // If timestep not found, just add (time, asset ref idx) to actives
      if (timeFindIdx == clipActives.size())
        clipActives.push_back(GfVec2d(parentTimeStep, assetIndex));
      else
      {
        // Find out whether to update existing active entry with new asset ref idx, or let the entry unchanged and replace the asset itself
        double prevAssetIndex = clipActives[timeFindIdx][1];
       
        if (newAsset)
        {
          //Find prev asset index
          int assetIdxFindIdx = 0;
          for (; assetIdxFindIdx < clipActives.size() && 
            (assetIdxFindIdx == timeFindIdx || clipActives[assetIdxFindIdx][1] != prevAssetIndex); 
            ++assetIdxFindIdx)
          {}

          // replacement occurs when prevAssetIndex hasn't been found in other entries
          replaceAsset = (assetIdxFindIdx == clipActives.size());
        }

        if(replaceAsset)
          assetPaths[int(prevAssetIndex)] = refStageSdf;
        else
          clipActives[timeFindIdx][1] = assetIndex;
      }

      // If new asset and not put in place of an old asset, add to assetPaths
      if (newAsset && !replaceAsset)
        assetPaths.push_back(refStageSdf);

      // Send the result through to usd
      clipsApi.SetClipAssetPaths(assetPaths);
      clipsApi.SetClipActive(clipActives);
    }
  }
#endif

  // Find the parentTimeStep, and change its child (or add the pair if nonexistent)
  VtVec2dArray clipTimes;
  clipsApi.GetClipTimes(&clipTimes);
  {
    int findIdx = 0;
    for (; findIdx < clipTimes.size(); ++findIdx)
    {
      if (clipTimes[findIdx][0] == parentTimeStep)
      {
        clipTimes[findIdx][1] = childTimeStep;
        break;
      }
    }
    if (findIdx == clipTimes.size())
      clipTimes.push_back(GfVec2d(parentTimeStep, childTimeStep));
    clipsApi.SetClipTimes(clipTimes);
  }
}

#endif

SdfPath UsdBridgeUsdWriter::AddRef_NoClip(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
  bool timeVarying, double parentTimeStep, double childTimeStep,
  const RefModFuncs& refModCallbacks)
{
  return AddRef_Impl(parentCache, childCache, refPathExt, timeVarying, false, false, nullptr, parentTimeStep, childTimeStep, refModCallbacks);
}

SdfPath UsdBridgeUsdWriter::AddRef(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
  bool timeVarying, bool valueClip, bool clipStages, const char* clipPostfix,
  double parentTimeStep, double childTimeStep,
  const RefModFuncs& refModCallbacks)
{
  // Value clip-enabled references have to be defined on the scenestage, as usd does not re-time recursively.
  return AddRef_Impl(parentCache, childCache, refPathExt, timeVarying, valueClip, clipStages, clipPostfix, parentTimeStep, childTimeStep, refModCallbacks);
}

SdfPath UsdBridgeUsdWriter::AddRef_Impl(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
  bool timeVarying, // Timevarying existence of the reference itself
  bool valueClip,   // Retiming through a value clip
  bool clipStages,  // Separate stages for separate time slots (can only exist in usd if valueClip enabled)
  const char* clipPostfix, double parentTimeStep, double childTimeStep,
  const RefModFuncs& refModCallbacks)
{
  UsdTimeCode parentTimeCode(parentTimeStep);

  SdfPath childBasePath = parentCache->PrimPath;
  if (refPathExt)
    childBasePath = parentCache->PrimPath.AppendPath(SdfPath(refPathExt));
  
  SdfPath referencingPrimPath = childBasePath.AppendPath(childCache->Name);
  UsdPrim referencingPrim = SceneStage->GetPrimAtPath(referencingPrimPath);

  if (!referencingPrim)
  {
    referencingPrim = SceneStage->DefinePrim(referencingPrimPath);
    assert(referencingPrim);

#ifdef VALUE_CLIP_RETIMING
    if (valueClip)
      InitializeClipMetaData(referencingPrim, childCache, parentTimeStep, childTimeStep, clipStages, clipPostfix);
#endif

    {
      UsdReferences references = referencingPrim.GetReferences(); //references or inherits?
      references.ClearReferences();
      references.AddInternalReference(childCache->PrimPath);
      //referencingPrim.SetInstanceable(true);
    }

#ifdef TIME_BASED_CACHING
    // If time domain of the stage extends beyond timestep in either direction, set visibility false for extremes.
    if (timeVarying)
      InitializePrimVisibility(SceneStage, referencingPrimPath, parentTimeCode);
#endif

    refModCallbacks.AtNewRef(parentCache, childCache);
  }
  else
  {
#ifdef TIME_BASED_CACHING
    if (timeVarying)
      SetPrimVisibility(SceneStage, referencingPrimPath, parentTimeCode, true);

#ifdef VALUE_CLIP_RETIMING
    // Cliptimes are added as additional info, not actively removed (visibility values remain leading in defining existing relationships over timesteps)
    // Also, clip stages at childTimeSteps which are not referenced anymore, are not removed; they could still be referenced from other parents!
    if (valueClip)
      UpdateClipMetaData(referencingPrim, childCache, parentTimeStep, childTimeStep, clipStages, clipPostfix);
#endif
#endif
  }

  return referencingPrimPath;
}

void UsdBridgeUsdWriter::RemoveAllRefs(UsdBridgePrimCache* parentCache, const char* refPathExt, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef)
{
  SdfPath childBasePath = parentCache->PrimPath;
  if (refPathExt)
    childBasePath = parentCache->PrimPath.AppendPath(SdfPath(refPathExt));

  RemoveAllRefs(SceneStage, parentCache, childBasePath, timeVarying, timeStep, atRemoveRef);
}

void UsdBridgeUsdWriter::RemoveAllRefs(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, SdfPath childBasePath, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef)
{
#ifdef TIME_BASED_CACHING
  UsdTimeCode timeCode(timeStep);

  // Remove refs just for this timecode, so leave invisible refs (could still be visible in other timecodes) intact.
  //SetChildrenVisibility(stage, childBasePath, timeCode, false);
  ChildrenRemoveIfVisible(stage, parentCache, childBasePath, timeVarying, timeCode, atRemoveRef);
#else
  UsdPrim parentPrim = stage->GetPrimAtPath(childBasePath);
  if(parentPrim)
  {
    UsdPrimSiblingRange children = parentPrim.GetAllChildren();
    for (UsdPrim child : children)
    {
      const std::string& childName = child.GetName().GetString();
      atRemoveRef(parentCache, childName); // Decrease reference count in caches
      stage->RemovePrim(child.GetPath()); // Remove reference prim
    }
  }
#endif
}

void UsdBridgeUsdWriter::ManageUnusedRefs(UsdBridgePrimCache* parentCache, const UsdBridgePrimCacheList& newChildren, const char* refPathExt, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef)
{
  ManageUnusedRefs(SceneStage, parentCache, newChildren, refPathExt, timeVarying, timeStep, atRemoveRef);
}

void UsdBridgeUsdWriter::ManageUnusedRefs(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, const UsdBridgePrimCacheList& newChildren, const char* refPathExt, bool timeVarying, double timeStep, AtRemoveRefFunc atRemoveRef)
{
  UsdTimeCode timeCode(timeStep);

  UsdPrim basePrim;
  if (refPathExt)
  {
    SdfPath childBasePath = parentCache->PrimPath.AppendPath(SdfPath(refPathExt));
    basePrim = stage->GetPrimAtPath(childBasePath);
  }
  else
    basePrim = stage->GetPrimAtPath(parentCache->PrimPath);

  if (basePrim)
  {
    UsdPrimSiblingRange children = basePrim.GetAllChildren();
    for (UsdPrim oldChild : children)
    {
      const std::string& oldChildName = oldChild.GetPrimPath().GetName();

      bool found = false;
      for (size_t newChildIdx = 0; newChildIdx < newChildren.size() && !found; ++newChildIdx)
      {
        const std::string& newChildName = newChildren[newChildIdx]->Name.GetString();
        found = (newChildName == oldChildName);
      }

      if (!found)
#ifdef TIME_BASED_CACHING
      {
        // make *referencing* prim invisible at timestep
        //SetPrimVisibility(oldChild.GetPath(), timeCode, false);

        // Remove *referencing* prim if visible
        PrimRemoveIfVisible(stage, parentCache, oldChild, timeVarying, timeCode, atRemoveRef); 
      }
#else
      {// remove the whole referencing prim
        atRemoveRef(parentCache, oldChildName);
        stage->RemovePrim(oldChild.GetPath());
      }
#endif
    }
  }
}

void UsdBridgeUsdWriter::InitializeUsdTransform(const UsdBridgePrimCache* cacheEntry)
{
  SdfPath transformPath = cacheEntry->PrimPath;
  UsdGeomXform transform = UsdGeomXform::Define(SceneStage, transformPath);
  assert(transform);
}

void UsdBridgeUsdWriter::BindMaterialToGeom(const SdfPath & refGeomPath, const SdfPath & refMatPath)
{
  UsdPrim refGeomPrim = this->SceneStage->GetPrimAtPath(refGeomPath);
  assert(refGeomPrim);

  // Bind the material to the mesh (use paths existing fully within the surface class definition, not the inherited geometry/material)
  UsdShadeMaterial refMatPrim = UsdShadeMaterial::Get(this->SceneStage, refMatPath);
  assert(refMatPrim);

  UsdShadeMaterialBindingAPI(refGeomPrim).Bind(refMatPrim);
}

void UsdBridgeUsdWriter::UnbindMaterialFromGeom(const SdfPath & refGeomPath)
{
  UsdPrim refGeomPrim = this->SceneStage->GetPrimAtPath(refGeomPath);
  assert(refGeomPrim);

  UsdShadeMaterialBindingAPI(refGeomPrim).UnbindDirectBinding();
}

void UsdBridgeUsdWriter::UpdateUsdTransform(const SdfPath& transPrimPath, float* transform, bool timeVarying, double timeStep)
{
  TimeEvaluator<bool> timeEval(timeVarying, timeStep);

  // Note: USD employs left-multiplication (vector in row-space)
  GfMatrix4d transMat; // Transforms can only be double, see UsdGeomXform::AddTransformOp (UsdGeomXformable)
  transMat.SetRow(0, GfVec4d(GfVec4f(&transform[0])));
  transMat.SetRow(1, GfVec4d(GfVec4f(&transform[4])));
  transMat.SetRow(2, GfVec4d(GfVec4f(&transform[8])));
  transMat.SetRow(3, GfVec4d(GfVec4f(&transform[12])));

  //Note that instance transform nodes have already been created.
  UsdGeomXform tfPrim = UsdGeomXform::Get(this->SceneStage, transPrimPath);
  assert(tfPrim);
  tfPrim.ClearXformOpOrder();
  tfPrim.AddTransformOp().Set(transMat, timeEval.Eval());
}

void UsdBridgeUsdWriter::UpdateBeginEndTime(double timeStep)
{
  if (timeStep < StartTime)
  {
    StartTime = timeStep;
    SceneStage->SetStartTimeCode(timeStep);
  }
  if (timeStep > EndTime)
  {
    EndTime = timeStep;
    SceneStage->SetEndTimeCode(timeStep);
  }
}

TfToken& UsdBridgeUsdWriter::AttributeNameToken(const char* attribName)
{
  int i = 0;
  for(; i < AttributeTokens.size(); ++i)
  {
    if(AttributeTokens[i] == attribName) // Overloaded == operator on TfToken
      break;
  }
  if(i == AttributeTokens.size())
    AttributeTokens.emplace_back(TfToken(attribName));
  return AttributeTokens[i];
}

void UsdBridgeUsdWriter::AddSharedResourceRef(const UsdBridgeResourceKey& key)
{
  bool found = false;
  for(auto& entry : SharedResourceCache)
  {
    if(entry.first == key)
    {
      ++entry.second.first;
      found = true;
    }
  }
  if(!found)
    SharedResourceCache.push_back(SharedResourceKV(key, SharedResourceValue(1, false)));
}

bool UsdBridgeUsdWriter::RemoveSharedResourceRef(const UsdBridgeResourceKey& key)
{
  bool removed = false;
  SharedResourceContainer::iterator it = SharedResourceCache.begin();
  while(it != SharedResourceCache.end())
  {
    if(it->first == key)
      --it->second.first;

    if(it->second.first == 0)
    {
      it = SharedResourceCache.erase(it);
      removed = true;
    }
    else
      ++it;
  }
  return removed;
}

bool UsdBridgeUsdWriter::IsSharedResourceModified(const UsdBridgeResourceKey& key)
{
  bool modified = false;
  for(auto& entry : SharedResourceCache)
  {
    if(entry.first == key)
    {
      modified = entry.second.second;
      entry.second.second = true;
    }
  }
  return modified;
}

void UsdBridgeUsdWriter::ResetSharedResourceModified()
{
  for(auto& entry : SharedResourceCache)
  {
    entry.second.second = false;
  }
}

void RemoveResourceFiles(UsdBridgePrimCache* cache, UsdBridgeUsdWriter& usdWriter, 
  const char* resourceFolder, const char* fileExtension)
{
  // Directly from usd is inaccurate, as timesteps may have been cleared without file removal
  //// Assuming prim without clip stages.
  //const UsdStageRefPtr volumeStage = usdWriter.GetTimeVarStage(cache);
  //const UsdStageRefPtr volumeStage = usdWriter.GetTimeVarStage(cache);
  //const SdfPath& volPrimPath = cache->PrimPath;
  //
  //UsdVolVolume volume = UsdVolVolume::Get(volumeStage, volPrimPath);
  //assert(volume);
  //
  //SdfPath ovdbFieldPath = volPrimPath.AppendPath(SdfPath(constring::openVDBPrimPf));
  //UsdVolOpenVDBAsset ovdbField = UsdVolOpenVDBAsset::Get(volumeStage, ovdbFieldPath);
  //
  //UsdAttribute fileAttr = ovdbField.GetFilePathAttr();
  //
  //std::vector<double> fileTimes;
  //fileAttr.GetTimeSamples(&fileTimes);

  assert(cache->ResourceKeys);
  UsdBridgePrimCache::ResourceContainer& keys = *(cache->ResourceKeys);

  std::string basePath = usdWriter.SessionDirectory; basePath.append(resourceFolder);
  for (const UsdBridgeResourceKey& key : keys)
  {
    bool removeFile = true;
    if(key.name)
      removeFile = usdWriter.RemoveSharedResourceRef(key);

    if(removeFile)
    {
      double timeStep = 
#ifdef TIME_BASED_CACHING
        key.timeStep;
#else
        0.0;
#endif
      const std::string& volFileName = usdWriter.GetResourceFileName(basePath.c_str(), key.name, timeStep, fileExtension);
      usdWriter.Connect->RemoveFile(volFileName.c_str(), true);
    }
  }
  keys.resize(0);
}

void ResourceCollectVolume(UsdBridgePrimCache* cache, UsdBridgeUsdWriter& usdWriter)
{
  RemoveResourceFiles(cache, usdWriter, constring::volFolder, constring::vdbExtension);
}

void ResourceCollectSampler(UsdBridgePrimCache* cache, UsdBridgeUsdWriter& usdWriter)
{
  RemoveResourceFiles(cache, usdWriter, constring::imgFolder, constring::imageExtension);
}