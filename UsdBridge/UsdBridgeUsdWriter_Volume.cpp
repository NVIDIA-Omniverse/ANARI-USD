// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeUsdWriter.h"

#include "UsdBridgeUsdWriter_Common.h"

namespace
{
  void BlockFieldRelationships(UsdVolVolume& volume, UsdBridgeVolumeFieldType* exception = nullptr)
  {
    UsdBridgeVolumeFieldType fieldTypes[] = {
      UsdBridgeVolumeFieldType::DENSITY,
      UsdBridgeVolumeFieldType::COLOR
    };
    for (int i = 0; i < sizeof(fieldTypes) / sizeof(UsdBridgeVolumeFieldType); ++i)
    {
      if (exception && fieldTypes[i] == *exception)
        continue;
      TfToken fieldToken = VolumeFieldTypeToken(fieldTypes[i]);
      if (volume.HasFieldRelationship(fieldToken))
        volume.BlockFieldRelationship(fieldToken);
    }
  }

  void InitializeUsdVolumeTimeVar(UsdVolVolume& volume, const TimeEvaluator<UsdBridgeVolumeData>* timeEval = nullptr)
  {
    typedef UsdBridgeVolumeData::DataMemberId DMI;

    volume.ClearXformOpOrder();

    if (!timeEval || timeEval->IsTimeVarying(DMI::TRANSFORM))
    {
      volume.AddTranslateOp();
      volume.AddScaleOp();
    }
  }

  void InitializeUsdVolumeAssetTimeVar(UsdVolOpenVDBAsset& volAsset, const TimeEvaluator<UsdBridgeVolumeData>* timeEval = nullptr)
  {
    typedef UsdBridgeVolumeData::DataMemberId DMI;

    if (!timeEval || timeEval->IsTimeVarying(DMI::DATA))
    {
      volAsset.CreateFilePathAttr();
    }
    else
    {
      volAsset.GetPrim().RemoveProperty(UsdBridgeTokens->filePath);
    }
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
    VtVec3fArray extentArray(2);
    extentArray[0].Set(0.0f, 0.0f, 0.0f);
    extentArray[1].Set((float)volumeData.NumElements[0], (float)volumeData.NumElements[1], (float)volumeData.NumElements[2]); //approx extents

    UsdAttribute uniformFileAttr = uniformField.GetFilePathAttr();
    UsdAttribute timeVarFileAttr = timeVarField.GetFilePathAttr();
    UsdAttribute uniformExtentAttr = uniformVolume.GetExtentAttr();
    UsdAttribute timeVarExtentAttr = timeVarVolume.GetExtentAttr();

    bool dataTimeVarying = timeEval.IsTimeVarying(DMI::DATA);
    bool transformTimeVarying = timeEval.IsTimeVarying(DMI::TRANSFORM);

    // Clear timevarying attributes if necessary
    ClearUsdAttributes(uniformFileAttr, timeVarFileAttr, dataTimeVarying);
    ClearUsdAttributes(uniformExtentAttr, timeVarExtentAttr, dataTimeVarying);

    // Set the attributes
    if(dataTimeVarying)
    {
      timeVarFileAttr.Set(volAsset, timeEval.Eval(DMI::DATA));
      timeVarExtentAttr.Set(extentArray, timeEval.Eval(DMI::DATA));
    }
    else
    { 
      uniformFileAttr.Set(volAsset, timeEval.Eval(DMI::DATA));
      uniformExtentAttr.Set(extentArray, timeEval.Eval(DMI::DATA));
    }

    // Translate-scale in usd (slightly different from normal attribute setting)
    UsdGeomXformOp transOp, scaleOp;
    if(transformTimeVarying)
    {
  #ifdef VALUE_CLIP_RETIMING
  #ifdef OMNIVERSE_CREATE_WORKAROUNDS
      uniformVolume.ClearXformOpOrder();
  #endif
  #endif
      timeVarVolume.ClearXformOpOrder();
      transOp = timeVarVolume.AddTranslateOp();
      scaleOp = timeVarVolume.AddScaleOp();
    }
    else
    {
      uniformVolume.ClearXformOpOrder();
      transOp = uniformVolume.AddTranslateOp();
      scaleOp = uniformVolume.AddScaleOp();
    }

    GfVec3d trans(volumeData.Origin[0], volumeData.Origin[1], volumeData.Origin[2]);
    transOp.Set(trans, timeEval.Eval(DMI::TRANSFORM));

    GfVec3f scale(volumeData.CellDimensions[0], volumeData.CellDimensions[1], volumeData.CellDimensions[2]);
    scaleOp.Set(scale, timeEval.Eval(DMI::TRANSFORM));
  }
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdVolume(UsdStageRefPtr volumeStage, const SdfPath & volumePath, bool uniformPrim) const
{ 
  return InitializeUsdVolume_Impl(volumeStage, volumePath, uniformPrim);
}

#ifdef VALUE_CLIP_RETIMING
void UsdBridgeUsdWriter::UpdateUsdVolumeManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeVolumeData& volumeData)
{
  TimeEvaluator<UsdBridgeVolumeData> timeEval(volumeData);
  InitializeUsdVolume_Impl(cacheEntry->ManifestStage.second, cacheEntry->PrimPath, false,
    &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}
#endif

void UsdBridgeUsdWriter::UpdateUsdVolume(UsdStageRefPtr timeVarStage, const SdfPath& volPrimPath, const UsdBridgeVolumeData& volumeData, double timeStep, UsdBridgePrimCache* cacheEntry)
{
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

  // Set field relationships?
  //BlockFieldRelationships() :Todo

  // Set the file path reference in usd
  const std::string& relVolPath = GetResourceFileName(constring::volFolder, cacheEntry->Name.GetString(), timeStep, constring::vdbExtension);

  UpdateUsdVolumeAttributes(uniformVolume, timeVarVolume, uniformField, timeVarField, volumeData, timeStep, relVolPath);

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