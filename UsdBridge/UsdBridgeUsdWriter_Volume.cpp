// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeUsdWriter.h"

#include "UsdBridgeUsdWriter_Common.h"
#include "UsdBridgeUsdWriter_Arrays.h"

namespace
{
  void InitializeUsdVolumeTimeVar(UsdVolVolume& volume, const TimeEvaluator<UsdBridgeVolumeData>* timeEval = nullptr)
  {
    typedef UsdBridgeVolumeData::DataMemberId DMI;

  }

  void InitializeUsdVolumeAssetTimeVar(UsdVolOpenVDBAsset& volAsset, const TimeEvaluator<UsdBridgeVolumeData>* timeEval = nullptr)
  {
    typedef UsdBridgeVolumeData::DataMemberId DMI;

    UsdVolOpenVDBAsset& attribCreatePrim = volAsset;
    UsdPrim attribRemovePrim = volAsset.GetPrim();
    
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::DATA, CreateFilePathAttr, UsdBridgeTokens->filePath);
  }

  UsdPrim InitializeUsdVolume_Impl(UsdStageRefPtr volumeStage, const SdfPath & volumePath, bool uniformPrim,
    TimeEvaluator<UsdBridgeVolumeData>* timeEval = nullptr)
  {
    UsdVolVolume volume = GetOrDefinePrim<UsdVolVolume>(volumeStage, volumePath);

    SdfPath ovdbFieldPath = volumePath.AppendPath(SdfPath(constring::openVDBPrimPf));
    UsdVolOpenVDBAsset volAsset = GetOrDefinePrim<UsdVolOpenVDBAsset>(volumeStage, ovdbFieldPath);

    if (uniformPrim)
    {
      volume.CreateFieldRelationship(UsdBridgeTokens->density, ovdbFieldPath);
      volAsset.CreateFieldNameAttr(VtValue(UsdBridgeTokens->density));
    }

    InitializeUsdVolumeTimeVar(volume, timeEval);
    InitializeUsdVolumeAssetTimeVar(volAsset, timeEval);

    return volume.GetPrim();
  } 

  void UpdateUsdVolumeAttributes(UsdVolVolume& uniformVolume, UsdVolVolume& timeVarVolume, UsdVolOpenVDBAsset& uniformField, UsdVolOpenVDBAsset& timeVarField,
    const UsdBridgeVolumeData& volumeData, double timeStep, const std::string& relVolPath)
  {
    TimeEvaluator<UsdBridgeVolumeData> timeEval(volumeData, timeStep);
    typedef UsdBridgeVolumeData::DataMemberId DMI;

    SdfAssetPath volAsset(relVolPath);

    // Set extents in usd
    float minX = volumeData.Origin[0];
    float minY = volumeData.Origin[1];
    float minZ = volumeData.Origin[2];
    float maxX = ((float)volumeData.NumElements[0] * volumeData.CellDimensions[0]) + minX;
    float maxY = ((float)volumeData.NumElements[1] * volumeData.CellDimensions[1]) + minY;
    float maxZ = ((float)volumeData.NumElements[2] * volumeData.CellDimensions[2]) + minZ;

    VtVec3fArray extentArray(2);
    extentArray[0].Set(minX, minY, minZ);
    extentArray[1].Set(maxX, maxY, maxZ);

    UsdAttribute uniformFileAttr = uniformField.GetFilePathAttr();
    UsdAttribute timeVarFileAttr = timeVarField.GetFilePathAttr();
    UsdAttribute uniformExtentAttr = uniformVolume.GetExtentAttr();
    UsdAttribute timeVarExtentAttr = timeVarVolume.GetExtentAttr();

    bool dataTimeVarying = timeEval.IsTimeVarying(DMI::DATA);

    // Clear timevarying attributes if necessary
    ClearUsdAttributes(uniformFileAttr, timeVarFileAttr, dataTimeVarying);
    ClearUsdAttributes(uniformExtentAttr, timeVarExtentAttr, dataTimeVarying);

    // Set the attributes
    SET_TIMEVARYING_ATTRIB(dataTimeVarying, timeVarFileAttr, uniformFileAttr, volAsset);
    SET_TIMEVARYING_ATTRIB(dataTimeVarying, timeVarExtentAttr, uniformExtentAttr, extentArray);
  }
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdVolume(UsdStageRefPtr volumeStage, const SdfPath & volumePath, bool uniformPrim) const
{ 
  UsdPrim volumePrim = InitializeUsdVolume_Impl(volumeStage, volumePath, uniformPrim);
  
#ifdef USE_INDEX_MATERIALS
  UsdShadeMaterial matPrim = InitializeIndexVolumeMaterial_Impl(volumeStage, volumePath, uniformPrim);
  UsdShadeMaterialBindingAPI(volumePrim).Bind(matPrim);
#endif

  return volumePrim;
}

#ifdef VALUE_CLIP_RETIMING
void UsdBridgeUsdWriter::UpdateUsdVolumeManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeVolumeData& volumeData)
{
  const SdfPath & volumePath = cacheEntry->PrimPath;

  UsdStageRefPtr volumeStage = cacheEntry->ManifestStage.second;
  TimeEvaluator<UsdBridgeVolumeData> timeEval(volumeData);

  InitializeUsdVolume_Impl(volumeStage, volumePath, 
    false, &timeEval);

#ifdef USE_INDEX_MATERIALS
  InitializeIndexVolumeMaterial_Impl(volumeStage, volumePath, false, &timeEval);
#endif

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}
#endif

void UsdBridgeUsdWriter::UpdateUsdVolume(UsdStageRefPtr timeVarStage, UsdBridgePrimCache* cacheEntry, const UsdBridgeVolumeData& volumeData, double timeStep)
{
  const SdfPath& volPrimPath = cacheEntry->PrimPath;
  
  // Get the volume and ovdb field prims
  UsdVolVolume uniformVolume = UsdVolVolume::Get(SceneStage, volPrimPath);
  assert(uniformVolume);
  UsdVolVolume timeVarVolume = UsdVolVolume::Get(timeVarStage, volPrimPath);
  assert(timeVarVolume);

  SdfPath ovdbFieldPath = volPrimPath.AppendPath(SdfPath(constring::openVDBPrimPf));

  UsdVolOpenVDBAsset uniformField = UsdVolOpenVDBAsset::Get(SceneStage, ovdbFieldPath);
  assert(uniformField);
  UsdVolOpenVDBAsset timeVarField = UsdVolOpenVDBAsset::Get(timeVarStage, ovdbFieldPath);
  assert(timeVarField);

  // Set the file path reference in usd
  const std::string& relVolPath = GetResourceFileName(constring::volFolder, cacheEntry->Name.GetString(), timeStep, constring::vdbExtension);

  UpdateUsdVolumeAttributes(uniformVolume, timeVarVolume, uniformField, timeVarField, volumeData, timeStep, relVolPath);

#ifdef USE_INDEX_MATERIALS
  UpdateIndexVolumeMaterial(SceneStage, timeVarStage, volPrimPath, volumeData, timeStep);
#endif

  // Output stream path (relative from connection working dir)
  std::string wdRelVolPath(SessionDirectory + relVolPath);

  // Write VDB data to stream
  VolumeWriter->ToVDB(volumeData);

  // Flush stream out to storage
  const char* volumeStreamData; size_t volumeStreamDataSize;
  VolumeWriter->GetSerializedVolumeData(volumeStreamData, volumeStreamDataSize);
  Connect->WriteFile(volumeStreamData, volumeStreamDataSize, wdRelVolPath.c_str(), true);
  // Record file write for timestep
  cacheEntry->AddResourceKey(UsdBridgeResourceKey(nullptr, timeStep));
}

void ResourceCollectVolume(UsdBridgePrimCache* cache, UsdBridgeUsdWriter& usdWriter)
{
  RemoveResourceFiles(cache, usdWriter, constring::volFolder, constring::vdbExtension);
}