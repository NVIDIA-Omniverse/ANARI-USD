// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeUsdWriter.h"
#include "UsdBridgeCaches.h"
#include "UsdBridgeMdlStrings.h"
#include "UsdBridgeUsdWriter_Common.h"
#include "UsdBridgeDiagnosticMgrDelegate.h"

#include <filesystem>
#include <typeinfo>

#define PROCESS_PREFIX

TF_DEFINE_PUBLIC_TOKENS(
  UsdBridgeTokens,

  ATTRIB_TOKEN_SEQ
  USDPREVSURF_TOKEN_SEQ
  USDPREVSURF_INPUT_TOKEN_SEQ
  MDL_TOKEN_SEQ
  MDL_INPUT_TOKEN_SEQ
  VOLUME_TOKEN_SEQ
  INDEX_TOKEN_SEQ
  MISC_TOKEN_SEQ
);

#undef PROCESS_PREFIX

namespace constring
{
  const char* const sessionPf = "Session_";
  const char* const rootClassName = "/RootClass";
  const char* const rootPrimName = "/Root";

  const char* const manifestFolder = "manifests/";
  const char* const clipFolder = "clips/";
  const char* const primStageFolder = "primstages/";
  const char* const imgFolder = "images/";
  const char* const volFolder = "volumes/";

  const char* const texCoordReaderPrimPf = "texcoordreader";
  const char* const psShaderPrimPf = "psshader";
  const char* const mdlShaderPrimPf = "mdlshader";
  const char* const psSamplerPrimPf = "pssampler";
  const char* const mdlSamplerPrimPf = "mdlsampler";
  const char* const mdlOpacityMulPrimPf = "opacitymul_mdl";
  const char* const mdlDiffuseOpacityPrimPf = "diffuseopacity_mdl";
  const char* const mdlGraphXYZPrimPf = "_xyz_f";
  const char* const mdlGraphColorPrimPf = "_ftocolor";
  const char* const mdlGraphXPrimPf = "_x";
  const char* const mdlGraphYPrimPf = "_y";
  const char* const mdlGraphZPrimPf = "_z";
  const char* const mdlGraphWPrimPf = "_w";
  const char* const openVDBPrimPf = "ovdbfield";
  const char* const protoShapePf = "proto_";

  const char* const imageExtension = ".png";
  const char* const vdbExtension = ".vdb";

  const char* const fullSceneNameBin = "FullScene.usd";
  const char* const fullSceneNameAscii = "FullScene.usda";

  const char* const mdlShaderAssetName = "OmniPBR.mdl";
  const char* const mdlSupportAssetName = "nvidia/support_definitions.mdl";
  const char* const mdlAuxAssetName = "nvidia/aux_definitions.mdl";

#ifdef CUSTOM_PBR_MDL
  const char* const mdlFolder = "mdls/";

  const char* const opaqueMaterialFile = "PBR_Opaque.mdl";
  const char* const transparentMaterialFile = "PBR_Transparent.mdl";
#endif

#ifdef USE_INDEX_MATERIALS
  const char* const indexMaterialPf = "indexmaterial";
  const char* const indexShaderPf = "indexshader";
  const char* const indexColorMapPf = "indexcolormap";
#endif
}

#ifdef OMNIVERSE_CONNECTION_ENABLE
namespace
{
  // Initialize USD plugins, particularly for registering the Omni USD Resolver
  void InitializeUsdPlugins(const UsdBridgeLogObject& logObj)
  {
    // Try to load Omniverse USD Resolver plugin by finding USD's plugin directory
    PlugRegistry& registry = PlugRegistry::GetInstance();

    // Find a known USD plugin to determine the plugin directory structure
    PlugPluginPtrVector allPlugins = registry.GetAllPlugins();
    std::string usdPluginPath;

    // Look for a core USD plugin (like usd) to find the plugin directory
    for (auto& plugin : allPlugins)
    {
      std::string name = plugin->GetName();
      std::string path = plugin->GetPath();

      // Look for core USD plugins that should always be present
      if (name == "usd")
      {
        usdPluginPath = path;
        UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::STATUS, "Found USD plugin reference: " << name << " at " << path);
        break;
      }
    }

    if (!usdPluginPath.empty())
    {
      // Parse the USD plugin path to find the plugin directory structure
      std::filesystem::path usdPath(usdPluginPath);
      std::filesystem::path installDir = usdPath.parent_path().parent_path();
      std::filesystem::path resolverPluginDir = installDir / "plugin" / "omni_usd_resolver" / "resources";

      std::string resolverPluginPath = resolverPluginDir.string();
      UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::STATUS, "Attempting to register plugins from: " << resolverPluginPath);

      registry.RegisterPlugins(resolverPluginPath);
    }
    else
    {
      UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::WARNING, "Could not find USD plugin to determine plugin directory structure");
    }

#ifndef NDEBUG
    // DEBUG: List all loaded plugins (refresh after potential registration)
    PlugPluginPtrVector plugins = registry.GetAllPlugins();
    UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::STATUS, "=== USD Plugins (Total: " << plugins.size() << ") ===");

    for (auto& plugin : plugins)
    {
      std::string pluginName = plugin->GetName();
      std::string pluginPath = plugin->GetPath();
      bool isLoaded = plugin->IsLoaded();
      UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::STATUS, "Plugin: " << pluginName << " | Path: " << pluginPath << " | Loaded: " << (isLoaded ? "YES" : "NO"));
    }

    // DEBUG: Check active resolver
    ArResolver& resolver = ArGetResolver();
    std::string resolverType = typeid(resolver).name();
    UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::STATUS, "Active resolver type: " << resolverType);
#endif
  }
}
#endif

#define PROCESS_PREFIX(elem) AttributeTokens.push_back(UsdBridgeTokens->elem); // Converts any token sequence macro to add all tokens to list

UsdBridgeUsdWriter::UsdBridgeUsdWriter(const UsdBridgeSettings& settings)
  : Settings(settings)
  , VolumeWriter(Create_VolumeWriter(), std::mem_fn(&UsdBridgeVolumeWriterI::Release))
{
  // Initialize AttributeTokens with common known attribute names
  ATTRIB_TOKEN_SEQ

  if(Settings.HostName)
    ConnectionSettings.HostName = Settings.HostName;
  if(Settings.OutputPath)
    ConnectionSettings.WorkingDirectory = Settings.OutputPath;
  FormatDirName(ConnectionSettings.WorkingDirectory);
}

#undef PROCESS_PREFIX // Reset the process prefix on the token sequence

UsdBridgeUsdWriter::~UsdBridgeUsdWriter()
{
}

void UsdBridgeUsdWriter::SetExternalSceneStage(UsdStageRefPtr sceneStage)
{
  this->ExternalSceneStage = sceneStage;
}

void UsdBridgeUsdWriter::SetEnableSaving(bool enableSaving)
{
  this->EnableSaving = enableSaving;
}

void UsdBridgeUsdWriter::SaveScene()
{
  if(this->EnableSaving)
    this->SceneStage->Save();
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

  //Connect->RemoveFolder(SessionDirectory.c_str(), true, true);
  bool folderMayExist = !Settings.CreateNewSession;
  
  valid = valid && Connect->CreateFolder(SessionDirectory.c_str(), true, folderMayExist);

#ifdef VALUE_CLIP_RETIMING
  valid = valid && Connect->CreateFolder((SessionDirectory + constring::manifestFolder).c_str(), true, folderMayExist);
  valid = valid && Connect->CreateFolder((SessionDirectory + constring::primStageFolder).c_str(), true, folderMayExist);
#endif
#ifdef TIME_CLIP_STAGES
  valid = valid && Connect->CreateFolder((SessionDirectory + constring::clipFolder).c_str(), true, folderMayExist);
#endif
#ifdef CUSTOM_PBR_MDL
  if(Settings.EnableMdlShader)
  {
    valid = valid && Connect->CreateFolder((SessionDirectory + constring::mdlFolder).c_str(), true, folderMayExist);
  }
#endif
  valid = valid && Connect->CreateFolder((SessionDirectory + constring::imgFolder).c_str(), true, folderMayExist);
  valid = valid && Connect->CreateFolder((SessionDirectory + constring::volFolder).c_str(), true, folderMayExist);

  if (!valid)
  {
    UsdBridgeLogMacro(this->LogObject, UsdBridgeLogLevel::ERR, "Something went wrong in the filesystem creating the required output folders (permissions?).");
  }

  return valid;
}

#ifdef CUSTOM_PBR_MDL
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
#ifdef OMNIVERSE_CONNECTION_ENABLE
  // Initialize USD plugins for Omni USD Resolver support before any connection is made
  InitializeUsdPlugins(this->LogObject);
#endif

  if (ConnectionSettings.HostName.empty())
  {
    if(ConnectionSettings.WorkingDirectory.compare("void") == 0)
      Connect = std::make_unique<UsdBridgeVoidConnection>();
    else
      Connect = std::make_unique<UsdBridgeLocalConnection>();
  }
  else
    Connect = std::make_unique<UsdBridgeRemoteConnection>();

  Connect->Initialize(ConnectionSettings, this->LogObject);

  SessionNumber = FindSessionNumber();
  SessionDirectory = constring::sessionPf + std::to_string(SessionNumber) + "/";

  bool valid = true;

  valid = CreateDirectories();

  valid = valid && VolumeWriter->Initialize(this->LogObject);

#ifdef CUSTOM_PBR_MDL
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

#ifdef REPLACE_SCENE_BY_EXTERNAL_STAGE
  if (this->ExternalSceneStage)
  {
    this->SceneStage = this->ExternalSceneStage;
  }
#endif

  const char* absSceneFile = Connect->GetUrl(this->SceneFileName.c_str());
  if (!this->SceneStage && !Settings.CreateNewSession)
      this->SceneStage = UsdStage::Open(absSceneFile);
  if (!this->SceneStage)
    this->SceneStage = UsdStage::CreateNew(absSceneFile);
  
  if (!this->SceneStage)
  {
    UsdBridgeLogMacro(this->LogObject, UsdBridgeLogLevel::ERR, "Scene UsdStage cannot be created or opened. Maybe a filesystem issue?");
    return false;
  }
#ifndef REPLACE_SCENE_BY_EXTERNAL_STAGE
  else if (this->ExternalSceneStage)
  {
    // Set the scene stage as a sublayer of the external stage
    ExternalSceneStage->GetRootLayer()->InsertSubLayerPath(
      this->SceneStage->GetRootLayer()->GetIdentifier());
  }
#endif

  this->RootClassName = constring::rootClassName;
  UsdPrim rootClassPrim = this->SceneStage->CreateClassPrim(SdfPath(this->RootClassName));
  assert(rootClassPrim);
  this->RootName = constring::rootPrimName;
  UsdPrim rootPrim = this->SceneStage->DefinePrim(SdfPath(this->RootName));
  assert(rootPrim);

#ifndef REPLACE_SCENE_BY_EXTERNAL_STAGE
  if (!this->ExternalSceneStage)
#endif
  {
    this->SceneStage->SetDefaultPrim(rootPrim);
    UsdModelAPI(rootPrim).SetKind(KindTokens->assembly);
  }

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

  this->SaveScene();

  return true;
}

UsdStageRefPtr UsdBridgeUsdWriter::GetSceneStage() const
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

  UsdBridgeDiagnosticMgrDelegate::SetOutputEnabled(false);
  cacheEntry->ManifestStage.second = UsdStage::CreateNew(absoluteFileName);
  UsdBridgeDiagnosticMgrDelegate::SetOutputEnabled(true);

  if (!cacheEntry->ManifestStage.second)
    cacheEntry->ManifestStage.second = UsdStage::Open(absoluteFileName);

  assert(cacheEntry->ManifestStage.second);

  cacheEntry->ManifestStage.second->DefinePrim(SdfPath(this->RootClassName));
}

void UsdBridgeUsdWriter::RemoveManifestAndClipStages(const UsdBridgePrimCache* cacheEntry)
{
  // May be superfluous
  if(cacheEntry->ManifestStage.second)
  {
    cacheEntry->ManifestStage.second->RemovePrim(SdfPath(RootClassName));

    // Remove ManifestStage file itself
    assert(!cacheEntry->ManifestStage.first.empty());
    Connect->RemoveFile((SessionDirectory + cacheEntry->ManifestStage.first).c_str(), true);
  }

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

    UsdBridgeDiagnosticMgrDelegate::SetOutputEnabled(false);
    UsdStageRefPtr primClipStage = UsdStage::CreateNew(absoluteFileName);
    UsdBridgeDiagnosticMgrDelegate::SetOutputEnabled(true);

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


void UsdBridgeUsdWriter::GetRootPrimPath(const SdfPath& name, const char* primPathCp, SdfPath& rootPrimPath)
{
  rootPrimPath = SdfPath(this->RootName);
  rootPrimPath = rootPrimPath.AppendPath(SdfPath(primPathCp));
  rootPrimPath = rootPrimPath.AppendPath(name);
}

void UsdBridgeUsdWriter::AddRootPrim(UsdBridgePrimCache* primCache, const char* primPathCp, const char* layerId)
{
  SdfPath primPath;
  GetRootPrimPath(primCache->Name, primPathCp, primPath);

  UsdPrim sceneGraphPrim = SceneStage->DefinePrim(primPath);
  assert(sceneGraphPrim);

  UsdReferences primRefs = sceneGraphPrim.GetReferences();
  primRefs.ClearReferences();
#ifdef VALUE_CLIP_RETIMING
  if(layerId)
  {
    primRefs.AddReference(layerId, primCache->PrimPath);
  }
#endif

  // Always add a ref to the internal prim, which is represented by the cache
  primRefs.AddInternalReference(primCache->PrimPath);
}

void UsdBridgeUsdWriter::RemoveRootPrim(UsdBridgePrimCache* primCache, const char* primPathCp)
{
  SdfPath primPath;
  GetRootPrimPath(primCache->Name, primPathCp, primPath);

  this->SceneStage->RemovePrim(primPath);
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
  if(SceneStage->GetPrimAtPath(cacheEntry->PrimPath))
    SceneStage->RemovePrim(cacheEntry->PrimPath);

#ifdef VALUE_CLIP_RETIMING
  RemoveManifestAndClipStages(cacheEntry);
#endif
}

#ifdef TIME_BASED_CACHING
void UsdBridgeUsdWriter::InitializePrimVisibility(UsdStageRefPtr stage, const SdfPath& primPath, const UsdTimeCode& timeCode,
  UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache)
{
  UsdGeomImageable imageable = UsdGeomImageable::Get(stage, primPath);

  if (imageable)
  {
    UsdAttribute visAttrib = imageable.GetVisibilityAttr(); // in case MakeVisible fails
    if(!visAttrib)
      visAttrib = imageable.CreateVisibilityAttr(VtValue(UsdGeomTokens->invisible)); // default is invisible
    
    double startTime = stage->GetStartTimeCode();
    double endTime = stage->GetEndTimeCode();
    if (startTime < timeCode)
      visAttrib.Set(VtValue(UsdGeomTokens->invisible), startTime);//imageable.MakeInvisible(startTime);
    if (endTime > timeCode)
      visAttrib.Set(VtValue(UsdGeomTokens->invisible), endTime);//imageable.MakeInvisible(endTime);
    visAttrib.Set(VtValue(UsdGeomTokens->inherited), timeCode);//imageable.MakeVisible(timeCode);

    parentCache->SetChildVisibleAtTime(childCache, timeCode.GetValue());
  }
}

void UsdBridgeUsdWriter::SetPrimVisible(UsdStageRefPtr stage, const SdfPath& primPath, const UsdTimeCode& timeCode,
  UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache)
{
  UsdGeomImageable imageable = UsdGeomImageable::Get(stage, primPath);
  if (imageable)
  {
    UsdAttribute visAttrib = imageable.GetVisibilityAttr();
    assert(visAttrib);

    visAttrib.Set(VtValue(UsdGeomTokens->inherited), timeCode);//imageable.MakeVisible(timeCode);
    parentCache->SetChildVisibleAtTime(childCache, timeCode.GetValue());
  }
}

void UsdBridgeUsdWriter::PrimRemoveIfInvisibleAnytime(UsdStageRefPtr stage, const UsdPrim& prim, bool timeVarying, const UsdTimeCode& timeCode, AtRemoveRefFunc atRemoveRef,
  UsdBridgePrimCache* parentCache, UsdBridgePrimCache* primCache)
{
  const SdfPath& primPath = prim.GetPath();

  bool removePrim = true; // TimeVarying is not automatically removed 

  if (timeVarying)
  {
    if(primCache)
    {
      // Remove prim only if there are no more visible children AND the timecode has actually been removed
      // (so if the timecode wasn't found in the cache, do not remove the prim)
      removePrim = parentCache->SetChildInvisibleAtTime(primCache, timeCode.GetValue());

      // If prim is still visible at other times, make sure to explicitly set this timeCode as invisible
      if(!removePrim)
      {
        UsdGeomImageable imageable = UsdGeomImageable::Get(stage, primPath);
        if (imageable)
        {
          UsdAttribute visAttrib = imageable.GetVisibilityAttr();
          if (visAttrib)
            visAttrib.Set(UsdGeomTokens->invisible, timeCode);
        }
      }
    }
    else
      removePrim = false; // If no cache is available, just don't remove, for possibility of previous timesteps (opposite from when !timeVarying)
  }

  if (removePrim)
  {
    // Decrease the ref on the representing cache entry
    if(primCache)
      atRemoveRef(parentCache, primCache);
    
    // Remove the prim
    stage->RemovePrim(primPath);
  }
}

void UsdBridgeUsdWriter::ChildrenRemoveIfInvisibleAnytime(UsdStageRefPtr stage, UsdBridgePrimCache* parentCache, const SdfPath& parentPath, bool timeVarying, const UsdTimeCode& timeCode, AtRemoveRefFunc atRemoveRef, const SdfPath& exceptPath)
{
  UsdPrim parentPrim = stage->GetPrimAtPath(parentPath);
  if (parentPrim)
  {
    UsdPrimSiblingRange children = parentPrim.GetAllChildren();
    for (UsdPrim child : children)
    {
      UsdBridgePrimCache* childCache = parentCache->GetChildCache(child.GetName());

      if (child.GetPath() != exceptPath)
        PrimRemoveIfInvisibleAnytime(stage, child, timeVarying, timeCode, atRemoveRef,
          parentCache, childCache);
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
    //assert(exists); // In case prim creation succeeds but an update is not attempted, no clip stage is generated, so exists will be false
    if(!exists)
      UsdBridgeLogMacro(this->LogObject, UsdBridgeLogLevel::WARNING, "Child clip stage not found while setting clip metadata, using generated stage instead. Probably the child data has not been properly updated.");

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
  bool timeVarying, double parentTimeStep, bool instanceable,
  const RefModFuncs& refModCallbacks)
{
  return AddRef_Impl(parentCache, childCache, refPathExt, timeVarying, false, false, nullptr, parentTimeStep, parentTimeStep, instanceable, refModCallbacks);
}

SdfPath UsdBridgeUsdWriter::AddRef(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
  bool timeVarying, bool valueClip, bool clipStages, const char* clipPostfix,
  double parentTimeStep, double childTimeStep, bool instanceable,
  const RefModFuncs& refModCallbacks)
{
  // Value clip-enabled references have to be defined on the scenestage, as usd does not re-time recursively.
  return AddRef_Impl(parentCache, childCache, refPathExt, timeVarying, valueClip, clipStages, clipPostfix, parentTimeStep, childTimeStep, instanceable, refModCallbacks);
}

SdfPath UsdBridgeUsdWriter::AddRef_Impl(UsdBridgePrimCache* parentCache, UsdBridgePrimCache* childCache, const char* refPathExt,
  bool timeVarying, // Timevarying existence (visible or not) of the reference itself
  bool valueClip,   // Retiming through a value clip
  bool clipStages,  // Separate stages for separate time slots (can only exist in usd if valueClip enabled)
  const char* clipPostfix, double parentTimeStep, double childTimeStep, bool instanceable,
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

    refModCallbacks.AtNewRef(parentCache, childCache);

#ifdef TIME_BASED_CACHING
    // If time domain of the stage extends beyond timestep in either direction, set visibility false for extremes.
    if (timeVarying)
      InitializePrimVisibility(SceneStage, referencingPrimPath, parentTimeCode,
        parentCache, childCache);
#endif
  }
  else
  {
#ifdef TIME_BASED_CACHING
    if (timeVarying)
      SetPrimVisible(SceneStage, referencingPrimPath, parentTimeCode, 
        parentCache, childCache);

#ifdef VALUE_CLIP_RETIMING
    // Cliptimes are added as additional info, not actively removed (visibility values remain leading in defining existing relationships over timesteps)
    // Also, clip stages at childTimeSteps which are not referenced anymore, are not removed; they could still be referenced from other parents!
    if (valueClip)
      UpdateClipMetaData(referencingPrim, childCache, parentTimeStep, childTimeStep, clipStages, clipPostfix);
#endif
#endif
  }

  if(instanceable || referencingPrim.HasAuthoredInstanceable())
    referencingPrim.SetInstanceable(instanceable);

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

  // Make refs just for this timecode invisible and possibly remove,
  // but leave refs which are still visible in other timecodes intact.
  ChildrenRemoveIfInvisibleAnytime(stage, parentCache, childBasePath, timeVarying, timeCode, atRemoveRef);
#else
  UsdPrim parentPrim = stage->GetPrimAtPath(childBasePath);
  if(parentPrim)
  {
    UsdPrimSiblingRange children = parentPrim.GetAllChildren();
    for (UsdPrim child : children)
    {
      UsdBridgePrimCache* childCache = parentCache->GetChildCache(child.GetName());

      if(childCache)
      {
        atRemoveRef(parentCache, childCache); // Decrease reference count in caches
        stage->RemovePrim(child.GetPath()); // Remove reference prim
      }
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
    // For each old (referencing) child prim, find it among the new ones, otherwise
    // possibly delete the referencing prim.
    UsdPrimSiblingRange children = basePrim.GetAllChildren();
    for (UsdPrim oldChild : children)
    {
      bool found = false;
      for (size_t newChildIdx = 0; newChildIdx < newChildren.size() && !found; ++newChildIdx)
      {
        found = (oldChild.GetName() == newChildren[newChildIdx]->PrimPath.GetNameToken());
      }

      UsdBridgePrimCache* oldChildCache = parentCache->GetChildCache(oldChild.GetName());

      // Not an assert: allow the case where child prims in a stage aren't cached, ie. when the bridge is destroyed and recreated
      if (!found)
#ifdef TIME_BASED_CACHING
      {
        // Remove *referencing* prim if no visible timecode exists anymore
        PrimRemoveIfInvisibleAnytime(stage, oldChild, timeVarying, timeCode, atRemoveRef,
          parentCache, oldChildCache);
      }
#else
      {// remove the whole referencing prim
        if(oldChildCache)
          atRemoveRef(parentCache, oldChildCache);
        stage->RemovePrim(oldChild.GetPath());
      }
#endif
    }
  }
}

void UsdBridgeUsdWriter::InitializeUsdTransform(const UsdBridgePrimCache* cacheEntry)
{
  SdfPath transformPath = cacheEntry->PrimPath;
  UsdGeomXform transform = GetOrDefinePrim<UsdGeomXform>(SceneStage, transformPath);
  assert(transform);
}

void UsdBridgeUsdWriter::InitializeUsdCamera(UsdStageRefPtr cameraStage, const SdfPath& cameraPath)
{
  UsdGeomCamera cameraPrim = GetOrDefinePrim<UsdGeomCamera>(cameraStage, cameraPath);
  assert(cameraPrim);

  cameraPrim.CreateProjectionAttr();
  cameraPrim.CreateHorizontalApertureAttr();
  cameraPrim.CreateVerticalApertureAttr();
  cameraPrim.CreateFocalLengthAttr();
  cameraPrim.CreateClippingRangeAttr();
  cameraPrim.CreateFocusDistanceAttr();
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

void UsdBridgeUsdWriter::UpdateUsdTransform(const SdfPath& transPrimPath, const float* transform, bool timeVarying, double timeStep)
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
  UsdGeomXformOp xformOp = tfPrim.AddTransformOp();
  ClearAndSetUsdAttribute(xformOp.GetAttr(), transMat, timeEval.Eval(), !timeEval.TimeVarying);
}

void UsdBridgeUsdWriter::UpdateUsdCamera(UsdStageRefPtr timeVarStage, const SdfPath& cameraPrimPath, 
  const UsdBridgeCameraData& cameraData, double timeStep, bool timeVarHasChanged)
{
  const TimeEvaluator<UsdBridgeCameraData> timeEval(cameraData, timeStep);
  typedef UsdBridgeCameraData::DataMemberId DMI;

  UsdGeomCamera cameraPrim = UsdGeomCamera::Get(timeVarStage, cameraPrimPath);
  assert(cameraPrim);

  // Set the view matrix
  GfVec3d position(cameraData.Position.Data);
  GfVec3d fwdDir(cameraData.Direction.Data);
  GfVec3d worldY(cameraData.Up.Data);
  GfVec3d worldZ(-fwdDir);

  GfVec3d worldX = GfCross(worldY, worldZ);// Keep righthand (ccw) order while making sure the +z direction is -fwdDir
  worldX.Normalize();
  worldY = GfCross(worldZ, worldX);
  worldY.Normalize();

  // Row-major order, with pre-multiply rules (vector components scale the rows in multiply)
  GfMatrix4d camMatrix(
    worldX[0], worldX[1], worldX[2], 0.0,
    worldY[0], worldY[1], worldY[2], 0.0,
    worldZ[0], worldZ[1], worldZ[2], 0.0,
    position[0], position[1], position[2], 1.0
  );

  cameraPrim.ClearXformOpOrder();
  UsdGeomXformOp xformOp = cameraPrim.AddTransformOp();
  ClearAndSetUsdAttribute(xformOp.GetAttr(), camMatrix, timeEval.Eval(DMI::VIEW),
    timeVarHasChanged && !timeEval.IsTimeVarying(DMI::VIEW));

  const GfVec3f camScaling(0.01f); // Add a default cam scaling for UI purposes
  UsdGeomXformOp scaleOp = cameraPrim.AddScaleOp();
  scaleOp.GetAttr().Set(camScaling, UsdTimeCode::Default());
  
  // Helper function for the projection matrix (takes fov in degrees)
  GfCamera gfCam;
  gfCam.SetPerspectiveFromAspectRatioAndFieldOfView(cameraData.Aspect, GfRadiansToDegrees(cameraData.Fovy), GfCamera::FOVVertical);

  // Update all attributes affected by SetPerspectiveFromAspectRatioAndFieldOfView (see implementation)
  UsdTimeCode projectTime = timeEval.Eval(DMI::PROJECTION);
  bool clearProjAttrib = timeVarHasChanged && !timeEval.IsTimeVarying(DMI::PROJECTION);
  //float focusDistance = 400.0f;

  ClearAndSetUsdAttribute(cameraPrim.GetProjectionAttr(), 
    (gfCam.GetProjection() == GfCamera::Perspective) ? UsdGeomTokens->perspective : UsdGeomTokens->orthographic, 
    projectTime, clearProjAttrib);
  ClearAndSetUsdAttribute(cameraPrim.GetHorizontalApertureAttr(), gfCam.GetHorizontalAperture(), projectTime, clearProjAttrib);
  ClearAndSetUsdAttribute(cameraPrim.GetVerticalApertureAttr(), gfCam.GetVerticalAperture(), projectTime, clearProjAttrib);
  ClearAndSetUsdAttribute(cameraPrim.GetFocalLengthAttr(), gfCam.GetFocalLength(), projectTime, clearProjAttrib);
  ClearAndSetUsdAttribute(cameraPrim.GetClippingRangeAttr(), GfVec2f(cameraData.Near, cameraData.Far), projectTime, clearProjAttrib);
  //ClearAndSetUsdAttribute(cameraPrim.GetFocusDistanceAttr(), focusDistance, projectTime, clearProjAttrib);
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

bool UsdBridgeUsdWriter::SetSharedResourceModified(const UsdBridgeResourceKey& key)
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
      const std::string& resFileName = usdWriter.GetResourceFileName(basePath.c_str(), key.name, timeStep, fileExtension);
      usdWriter.Connect->RemoveFile(resFileName.c_str(), true);
    }
  }
  keys.resize(0);
}