// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeUsdWriter.h"

#include "UsdBridgeUsdWriter_Common.h"
#include "UsdBridgeUsdWriter_Arrays.h"
#include "UsdBridgeRt.h"

namespace
{
  template<typename GeomDataType>
  struct UsdGeomUpdateArguments
  {
    UsdBridgeRt& UsdRtData;
    const GeomDataType& GeomData;
    uint64_t NumPrims;
    UsdBridgeUpdateEvaluator<const GeomDataType>& UpdateEval;
    TimeEvaluator<GeomDataType>& TimeEval;
  };
  #define UNPACK_UPDATE_ARGS\
    UsdBridgeRt& usdRtData = updateArgs.UsdRtData;\
    const GeomDataType& geomData = updateArgs.GeomData;\
    uint64_t numPrims = updateArgs.NumPrims;\
    UsdBridgeUpdateEvaluator<const GeomDataType>& updateEval = updateArgs.UpdateEval;\
    TimeEvaluator<GeomDataType>& timeEval = updateArgs.TimeEval;

  template<typename UsdGeomType>
  struct UsdGeomUpdateAttribArgs
  {
    const UsdBridgeLogObject& LogObj;
    UsdGeomType& TimeVarGeom;
    UsdGeomType& UniformGeom;
  };
  #define UNPACK_ATTRIB_ARGS\
    const UsdBridgeLogObject& logObj = attribArgs.LogObj;\
    UsdGeomType& timeVarGeom = attribArgs.TimeVarGeom;\
    UsdGeomType& uniformGeom = attribArgs.UniformGeom;

  struct UsdGeomUpdatePrimvarArgs
  {
    UsdBridgeUsdWriter* Writer;
    UsdGeomPrimvarsAPI& TimeVarPrimvars;
    UsdGeomPrimvarsAPI& UniformPrimvars;
  };
  #define UNPACK_PRIMVAR_ARGS\
    UsdBridgeUsdWriter* writer = primvarArgs.Writer;\
    UsdGeomPrimvarsAPI& timeVarPrimvars = primvarArgs.TimeVarPrimvars;\
    UsdGeomPrimvarsAPI& uniformPrimvars = primvarArgs.UniformPrimvars;


  template<typename ReturnEltType = UsdBridgeNoneType>
  UsdBridgeSpanI<ReturnEltType>* UpdateUsdAttribute_Safe( UsdBridgeRt& usdRtData,
    const UsdBridgeLogObject& logObj, const void* arrayData, UsdBridgeType arrayDataType, size_t arrayNumElements,
    ::PXR_NS::UsdAttribute& attrib, ::PXR_NS::UsdTimeCode& timeCode, bool writeToAttrib = true)
  {
    UsdBridgeSpanI<ReturnEltType>* rtSpan = nullptr;

    // Check if UsdRt has been able to convert the prim
    if(usdRtData.ValidPrim())
    {
      // Route the attribute assignment through UsdRt/Fabric
      rtSpan = usdRtData.UpdateUsdAttribute<ReturnEltType>(logObj, arrayData, arrayDataType, arrayNumElements, attrib,
        timeCode, writeToAttrib);
    }
    else
    {
      // Route the attribute assignment through the usual PXR_NS namespace
      UsdBridgeArrays::AttribSpanInit spanInit(arrayNumElements, attrib, timeCode);
      spanInit.SetAutoAssignToAttrib(writeToAttrib);
      rtSpan = UsdBridgeArrays::AssignArrayToAttribute<UsdBridgeArrays::AttribSpanInit, UsdBridgeArrays::AttribSpan, ReturnEltType>(
        logObj, arrayData, arrayDataType, arrayNumElements, spanInit);
    }

    return rtSpan;
  }

  template<typename UsdGeomType>
  UsdAttribute UsdGeomGetPointsAttribute(UsdGeomType& usdGeom) { return UsdAttribute(); }

  template<>
  UsdAttribute UsdGeomGetPointsAttribute(UsdGeomMesh& usdGeom) { return usdGeom.GetPointsAttr(); }

  template<>
  UsdAttribute UsdGeomGetPointsAttribute(UsdGeomPoints& usdGeom) { return usdGeom.GetPointsAttr(); }

  template<>
  UsdAttribute UsdGeomGetPointsAttribute(UsdGeomBasisCurves& usdGeom) { return usdGeom.GetPointsAttr(); }

  template<>
  UsdAttribute UsdGeomGetPointsAttribute(UsdGeomPointInstancer& usdGeom) { return usdGeom.GetPositionsAttr(); }

  template<typename GeomDataType>
  void CreateUsdGeomColorPrimvars(UsdGeomPrimvarsAPI& primvarApi, const GeomDataType& geomData, const UsdBridgeSettings& settings, const TimeEvaluator<GeomDataType>* timeEval = nullptr)
  {
    using DMI = typename GeomDataType::DataMemberId;

    bool timeVarChecked = true;
    if(timeEval)
    {
      timeVarChecked = timeEval->IsTimeVarying(DMI::COLORS);
    }

    if (timeVarChecked)
    {
      primvarApi.CreatePrimvar(UsdBridgeTokens->color, SdfValueTypeNames->Float4Array);
    }
    else
    {
      primvarApi.RemovePrimvar(UsdBridgeTokens->color);
    }
  }

  template<typename GeomDataType>
  void CreateUsdGeomTexturePrimvars(UsdGeomPrimvarsAPI& primvarApi, const GeomDataType& geomData, const UsdBridgeSettings& settings, const TimeEvaluator<GeomDataType>* timeEval = nullptr)
  {
    using DMI = typename GeomDataType::DataMemberId;

    bool timeVarChecked = true;
    if(timeEval)
    {
      timeVarChecked = timeEval->IsTimeVarying(DMI::ATTRIBUTE0);
    }

    if (timeVarChecked)
      primvarApi.CreatePrimvar(UsdBridgeTokens->st, SdfValueTypeNames->TexCoord2fArray);
    else if (timeEval)
      primvarApi.RemovePrimvar(UsdBridgeTokens->st);
  }

  template<typename GeomDataType>
  void CreateUsdGeomAttributePrimvar(UsdBridgeUsdWriter* writer, UsdGeomPrimvarsAPI& primvarApi, const GeomDataType& geomData, uint32_t attribIndex, const TimeEvaluator<GeomDataType>* timeEval = nullptr)
  {
    using DMI = typename GeomDataType::DataMemberId;

    const UsdBridgeAttribute& attrib = geomData.Attributes[attribIndex];
    if(attrib.DataType != UsdBridgeType::UNDEFINED)
    {
      bool timeVarChecked = true;
      if(timeEval)
      {
        DMI attributeId = DMI::ATTRIBUTE0 + attribIndex;
        timeVarChecked = timeEval->IsTimeVarying(attributeId);
      }

      TfToken attribToken = attrib.Name ? writer->AttributeNameToken(attrib.Name) : AttribIndexToToken(attribIndex);

      if(timeVarChecked)
      {
        SdfValueTypeName primvarType = UsdBridgeArrays::GetAttribArrayType(attrib.DataType);
        if(primvarType == SdfValueTypeNames->BoolArray)
        {
          UsdBridgeLogMacro(writer->LogObject, UsdBridgeLogLevel::WARNING, "UsdGeom Attribute<" << attribIndex << "> primvar does not support source data type: " << attrib.DataType);
        }

        // Allow for attrib primvar types to change
        UsdGeomPrimvar primvar = primvarApi.GetPrimvar(attribToken);
        if(primvar && primvar.GetTypeName() != primvarType)
        {
          primvarApi.RemovePrimvar(attribToken);
        }
        primvarApi.CreatePrimvar(attribToken, primvarType);
      }
      else if(timeEval)
      {
        primvarApi.RemovePrimvar(attribToken);
      }
    }
  }

  template<typename GeomDataType>
  void CreateUsdGeomAttributePrimvars(UsdBridgeUsdWriter* writer, UsdGeomPrimvarsAPI& primvarApi, const GeomDataType& geomData, const TimeEvaluator<GeomDataType>* timeEval = nullptr)
  {
    for(uint32_t attribIndex = 0; attribIndex < geomData.NumAttributes; ++attribIndex)
    {
      CreateUsdGeomAttributePrimvar(writer, primvarApi, geomData, attribIndex, timeEval);
    }
  }

  void InitializeUsdGeometryTimeVar(UsdBridgeUsdWriter* writer, UsdGeomMesh& meshGeom, const UsdBridgeMeshData& meshData, const UsdBridgeSettings& settings,
    const TimeEvaluator<UsdBridgeMeshData>* timeEval = nullptr)
  {
    typedef UsdBridgeMeshData::DataMemberId DMI;
    UsdGeomPrimvarsAPI primvarApi(meshGeom);

    UsdGeomMesh& attribCreatePrim = meshGeom;
    UsdPrim attribRemovePrim = meshGeom.GetPrim();

    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::POINTS, CreatePointsAttr, UsdBridgeTokens->points);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::POINTS, CreateExtentAttr, UsdBridgeTokens->extent);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::INDICES, CreateFaceVertexIndicesAttr, UsdBridgeTokens->faceVertexCounts);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::INDICES, CreateFaceVertexCountsAttr, UsdBridgeTokens->faceVertexIndices);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::NORMALS, CreateNormalsAttr, UsdBridgeTokens->normals);

    CreateUsdGeomColorPrimvars(primvarApi, meshData, settings, timeEval);

    if(settings.EnableStTexCoords)
      CreateUsdGeomTexturePrimvars(primvarApi, meshData, settings, timeEval);

    CreateUsdGeomAttributePrimvars(writer, primvarApi, meshData, timeEval);
  }

  void InitializeUsdGeometryTimeVar(UsdBridgeUsdWriter* writer, UsdGeomPoints& pointsGeom, const UsdBridgeInstancerData& instancerData, const UsdBridgeSettings& settings,
    const TimeEvaluator<UsdBridgeInstancerData>* timeEval = nullptr)
  {
    typedef UsdBridgeInstancerData::DataMemberId DMI;
    UsdGeomPrimvarsAPI primvarApi(pointsGeom);

    UsdGeomPoints& attribCreatePrim = pointsGeom;
    UsdPrim attribRemovePrim = pointsGeom.GetPrim();

    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::POINTS, CreatePointsAttr, UsdBridgeTokens->points);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::POINTS, CreateExtentAttr, UsdBridgeTokens->extent);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::INSTANCEIDS, CreateIdsAttr, UsdBridgeTokens->ids);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::ORIENTATIONS, CreateNormalsAttr, UsdBridgeTokens->normals);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::SCALES, CreateWidthsAttr, UsdBridgeTokens->widths);

    CreateUsdGeomColorPrimvars(primvarApi, instancerData, settings, timeEval);

    if(settings.EnableStTexCoords)
      CreateUsdGeomTexturePrimvars(primvarApi, instancerData, settings, timeEval);

    CreateUsdGeomAttributePrimvars(writer, primvarApi, instancerData, timeEval);
  }

  void InitializeUsdGeometryTimeVar(UsdBridgeUsdWriter* writer, UsdGeomPointInstancer& pointsGeom, const UsdBridgeInstancerData& instancerData, const UsdBridgeSettings& settings,
    const TimeEvaluator<UsdBridgeInstancerData>* timeEval = nullptr)
  {
    typedef UsdBridgeInstancerData::DataMemberId DMI;
    UsdGeomPrimvarsAPI primvarApi(pointsGeom);

    UsdGeomPointInstancer& attribCreatePrim = pointsGeom;
    UsdPrim attribRemovePrim = pointsGeom.GetPrim();

    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::POINTS, CreatePositionsAttr, UsdBridgeTokens->positions);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::POINTS, CreateExtentAttr, UsdBridgeTokens->extent);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::SHAPEINDICES, CreateProtoIndicesAttr, UsdBridgeTokens->protoIndices);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::INSTANCEIDS, CreateIdsAttr, UsdBridgeTokens->ids);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::ORIENTATIONS, CreateOrientationsAttr, UsdBridgeTokens->orientations);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::SCALES, CreateScalesAttr, UsdBridgeTokens->scales);

    CreateUsdGeomColorPrimvars(primvarApi, instancerData, settings, timeEval);

    if(settings.EnableStTexCoords)
      CreateUsdGeomTexturePrimvars(primvarApi, instancerData, settings, timeEval);

    CreateUsdGeomAttributePrimvars(writer, primvarApi, instancerData, timeEval);

    //CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::LINEARVELOCITIES, CreateVelocitiesAttr, UsdBridgeTokens->velocities);
    //CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::ANGULARVELOCITIES, CreateAngularVelocitiesAttr, UsdBridgeTokens->angularVelocities);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::INVISIBLEIDS, CreateInvisibleIdsAttr, UsdBridgeTokens->invisibleIds);
  }

  void InitializeUsdGeometryTimeVar(UsdBridgeUsdWriter* writer, UsdGeomBasisCurves& curveGeom, const UsdBridgeCurveData& curveData, const UsdBridgeSettings& settings,
    const TimeEvaluator<UsdBridgeCurveData>* timeEval = nullptr)
  {
    typedef UsdBridgeCurveData::DataMemberId DMI;
    UsdGeomPrimvarsAPI primvarApi(curveGeom);

    UsdGeomBasisCurves& attribCreatePrim = curveGeom;
    UsdPrim attribRemovePrim = curveGeom.GetPrim();

    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::POINTS, CreatePointsAttr, UsdBridgeTokens->positions);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::POINTS, CreateExtentAttr, UsdBridgeTokens->extent);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::CURVELENGTHS, CreateCurveVertexCountsAttr, UsdBridgeTokens->curveVertexCounts);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::NORMALS, CreateNormalsAttr, UsdBridgeTokens->normals);
    CREATE_REMOVE_TIMEVARYING_ATTRIB_QUALIFIED(DMI::SCALES, CreateWidthsAttr, UsdBridgeTokens->widths);

    CreateUsdGeomColorPrimvars(primvarApi, curveData, settings, timeEval);

    if(settings.EnableStTexCoords)
      CreateUsdGeomTexturePrimvars(primvarApi, curveData, settings, timeEval);

    CreateUsdGeomAttributePrimvars(writer, primvarApi, curveData, timeEval);

  }

  UsdPrim InitializeUsdGeometry_Impl(UsdBridgeUsdWriter* writer, UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeMeshData& meshData, bool uniformPrim,
    const UsdBridgeSettings& settings,
    TimeEvaluator<UsdBridgeMeshData>* timeEval = nullptr)
  {
    UsdGeomMesh geomMesh = GetOrDefinePrim<UsdGeomMesh>(geometryStage, geomPath);

    InitializeUsdGeometryTimeVar(writer, geomMesh, meshData, settings, timeEval);

    if (uniformPrim)
    {
      geomMesh.CreateDoubleSidedAttr(VtValue(true));
      geomMesh.CreateSubdivisionSchemeAttr().Set(UsdGeomTokens->none);
    }

    return geomMesh.GetPrim();
  }

  UsdPrim InitializeUsdGeometry_Impl(UsdBridgeUsdWriter* writer, UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeInstancerData& instancerData, bool uniformPrim,
    const UsdBridgeSettings& settings,
    TimeEvaluator<UsdBridgeInstancerData>* timeEval = nullptr)
  {
    if (instancerData.UseUsdGeomPoints)
    {
      UsdGeomPoints geomPoints = GetOrDefinePrim<UsdGeomPoints>(geometryStage, geomPath);

      InitializeUsdGeometryTimeVar(writer, geomPoints, instancerData, settings, timeEval);

      if (uniformPrim)
      {
        geomPoints.CreateDoubleSidedAttr(VtValue(true));
      }

      return geomPoints.GetPrim();
    }
    else
    {
      UsdGeomPointInstancer geomPoints = GetOrDefinePrim<UsdGeomPointInstancer>(geometryStage, geomPath);

      InitializeUsdGeometryTimeVar(writer, geomPoints, instancerData, settings, timeEval);

      return geomPoints.GetPrim();
    }
  }

  UsdPrim InitializeUsdGeometry_Impl(UsdBridgeUsdWriter* writer, UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeCurveData& curveData, bool uniformPrim,
    const UsdBridgeSettings& settings,
    TimeEvaluator<UsdBridgeCurveData>* timeEval = nullptr)
  {
    UsdGeomBasisCurves geomCurves = GetOrDefinePrim<UsdGeomBasisCurves>(geometryStage, geomPath);

    InitializeUsdGeometryTimeVar(writer, geomCurves, curveData, settings, timeEval);

    if (uniformPrim)
    {
      geomCurves.CreateDoubleSidedAttr(VtValue(true));
      geomCurves.GetTypeAttr().Set(UsdGeomTokens->linear);
    }

    return geomCurves.GetPrim();
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomPoints(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::POINTS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::POINTS);

    ClearUsdAttributes(UsdGeomGetPointsAttribute(uniformGeom), UsdGeomGetPointsAttribute(timeVarGeom), timeVaryingUpdate);
    ClearUsdAttributes(uniformGeom.GetExtentAttr(), timeVarGeom.GetExtentAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      if (!geomData.Points)
      {
        UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "GeomData requires points.");
      }
      else
      {
        UsdGeomType* outGeom = timeVaryingUpdate ? &timeVarGeom : &uniformGeom;
        UsdTimeCode timeCode = timeEval.Eval(DMI::POINTS);

        // Points
        UsdAttribute pointsAttr = UsdGeomGetPointsAttribute(*outGeom);

        const void* arrayData = geomData.Points;
        size_t arrayNumElements = geomData.NumPoints;
        UsdBridgeType arrayDataType = geomData.PointsType;

        UsdBridgeSpanI<GfVec3f>* pointSpan = UpdateUsdAttribute_Safe<GfVec3f>(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements, pointsAttr, timeCode);

        // Usd requires extent.
        if(pointSpan)
        {
          // Usd requires extent.
          GfRange3f extent;
          for (GfVec3f& pt : *pointSpan) {
            extent.UnionWith(pt);
          }
          VtVec3fArray extentArray(2);
          extentArray[0] = extent.GetMin();
          extentArray[1] = extent.GetMax();

          outGeom->GetExtentAttr().Set(extentArray, timeCode);
        }
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomIndices(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::INDICES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INDICES);

    ClearUsdAttributes(uniformGeom.GetFaceVertexIndicesAttr(), timeVarGeom.GetFaceVertexIndicesAttr(), timeVaryingUpdate);
    ClearUsdAttributes(uniformGeom.GetFaceVertexCountsAttr(), timeVarGeom.GetFaceVertexCountsAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdGeomType* outGeom = timeVaryingUpdate ? &timeVarGeom : &uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::INDICES);

      uint64_t numIndices = geomData.NumIndices;

      // Face Vertex counts
      UsdAttribute faceVertCountsAttr = outGeom->GetFaceVertexCountsAttr();
      int vertexCount = numIndices / numPrims;

      UsdBridgeSpanI<int>* faceVertCountSpan = UpdateUsdAttribute_Safe<int>(usdRtData, logObj, nullptr, UsdBridgeType::INT, numPrims,
        faceVertCountsAttr, timeCode); // By passing a nullptr as data, only the span will be returned

      if(faceVertCountSpan)
      {
        for(int& x : *faceVertCountSpan) x = vertexCount;
        faceVertCountSpan->AssignToAttrib();
      }

      // Vertex indices
      if(numIndices > 0)
      {
        const void* arrayData = geomData.Indices;
        UsdBridgeType arrayDataType = geomData.IndicesType;
        size_t arrayNumElements = numIndices;
        UsdAttribute indicesAttr = outGeom->GetFaceVertexIndicesAttr();

        UsdBridgeSpanI<int>* indicesSpan = UpdateUsdAttribute_Safe<int>(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements,
          indicesAttr, timeCode);

        // If arrayData was null, only the span will have been returned
        if (!arrayData && indicesSpan)
        {
          int i = 0;
          for(int& x : *indicesSpan) x = i++;
          indicesSpan->AssignToAttrib();
        }
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomNormals(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::NORMALS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::NORMALS);

    ClearUsdAttributes(uniformGeom.GetNormalsAttr(), timeVarGeom.GetNormalsAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdGeomType* outGeom = timeVaryingUpdate ? &timeVarGeom : &uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::NORMALS);

      UsdAttribute normalsAttr = outGeom->GetNormalsAttr();

      if (geomData.Normals != nullptr)
      {
        const void* arrayData = geomData.Normals;
        UsdBridgeType arrayDataType = geomData.NormalsType;
        size_t arrayNumElements = geomData.PerPrimNormals ? numPrims : geomData.NumPoints;

        UpdateUsdAttribute_Safe(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements, normalsAttr, timeCode);

        // Per face or per-vertex interpolation. This will break timesteps that have been written before.
        TfToken normalInterpolation = geomData.PerPrimNormals ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
        uniformGeom.SetNormalsInterpolation(normalInterpolation);
      }
      else
      {
        normalsAttr.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename GeomDataType>
  void UpdateUsdGeomTexCoords(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdatePrimvarArgs& primvarArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_PRIMVAR_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::ATTRIBUTE0);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ATTRIBUTE0);

    UsdGeomPrimvar uniformPrimvar = uniformPrimvars.GetPrimvar(UsdBridgeTokens->st);
    UsdGeomPrimvar timeVarPrimvar = timeVarPrimvars.GetPrimvar(UsdBridgeTokens->st);

    ClearUsdAttributes(uniformPrimvar.GetAttr(), timeVarPrimvar.GetAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdTimeCode timeCode = timeEval.Eval(DMI::ATTRIBUTE0);

      UsdAttribute texcoordPrimvar = timeVaryingUpdate ? timeVarPrimvar : uniformPrimvar;
      assert(texcoordPrimvar);

      const UsdBridgeAttribute& texCoordAttrib = geomData.Attributes[0];

      if (texCoordAttrib.Data != nullptr)
      {
        const void* arrayData = texCoordAttrib.Data;
        UsdBridgeType arrayDataType = texCoordAttrib.DataType;
        size_t arrayNumElements = texCoordAttrib.PerPrimData ? numPrims : geomData.NumPoints;

        UpdateUsdAttribute_Safe(usdRtData, writer->LogObject, arrayData, arrayDataType, arrayNumElements, texcoordPrimvar, timeCode);
  
        // Per face or per-vertex interpolation. This will break timesteps that have been written before.
        TfToken texcoordInterpolation = texCoordAttrib.PerPrimData ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
        uniformPrimvar.SetInterpolation(texcoordInterpolation);
      }
      else
      {
        texcoordPrimvar.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename GeomDataType>
  void UpdateUsdGeomAttribute(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdatePrimvarArgs& primvarArgs, uint32_t attribIndex)
  {
    UNPACK_UPDATE_ARGS UNPACK_PRIMVAR_ARGS

    assert(attribIndex < geomData.NumAttributes);
    const UsdBridgeAttribute& bridgeAttrib = geomData.Attributes[attribIndex];

    TfToken attribToken = bridgeAttrib.Name ? writer->AttributeNameToken(bridgeAttrib.Name) : AttribIndexToToken(attribIndex);
    UsdGeomPrimvar uniformPrimvar = uniformPrimvars.GetPrimvar(attribToken);
    // The uniform primvar has to exist, otherwise any timevarying data will be ignored as well
    if(!uniformPrimvar || uniformPrimvar.GetTypeName() != UsdBridgeArrays::GetAttribArrayType(bridgeAttrib.DataType))
    {
      CreateUsdGeomAttributePrimvar(writer, uniformPrimvars, geomData, attribIndex); // No timeEval, to force attribute primvar creation on the uniform api
    }

    UsdGeomPrimvar timeVarPrimvar = timeVarPrimvars.GetPrimvar(attribToken);
    if(!timeVarPrimvar || timeVarPrimvar.GetTypeName() != UsdBridgeArrays::GetAttribArrayType(bridgeAttrib.DataType)) // even though new clipstages initialize the correct primvar type/name, it may still be wrong for existing ones (or primstages if so configured)
    {
      CreateUsdGeomAttributePrimvar(writer, timeVarPrimvars, geomData, attribIndex, &timeEval);
    }

    using DMI = typename GeomDataType::DataMemberId;
    DMI attributeId = DMI::ATTRIBUTE0 + attribIndex;

    bool performsUpdate = updateEval.PerformsUpdate(attributeId);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(attributeId);

    ClearUsdAttributes(uniformPrimvar.GetAttr(), timeVarPrimvar.GetAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdTimeCode timeCode = timeEval.Eval(attributeId);

      UsdAttribute attributePrimvar = timeVaryingUpdate ? timeVarPrimvar : uniformPrimvar;

      if(!attributePrimvar)
      {
        UsdBridgeLogMacro(writer->LogObject, UsdBridgeLogLevel::ERR, "UsdGeom Attribute<Index> primvar not found, was the attribute at requested index valid during initialization of the prim? Index is " << attribIndex);
      }
      else
      {
        if (bridgeAttrib.Data != nullptr)
        {
          const void* arrayData = bridgeAttrib.Data;
          size_t arrayNumElements = bridgeAttrib.PerPrimData ? numPrims : geomData.NumPoints;

          UpdateUsdAttribute_Safe(usdRtData, writer->LogObject, arrayData, bridgeAttrib.DataType, arrayNumElements, attributePrimvar, timeCode);


          // Per face or per-vertex interpolation. This will break timesteps that have been written before.
          TfToken attribInterpolation = bridgeAttrib.PerPrimData ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
          uniformPrimvar.SetInterpolation(attribInterpolation);
        }
        else
        {
          attributePrimvar.Set(SdfValueBlock(), timeCode);
        }
      }
    }
  }

  template<typename GeomDataType>
  void UpdateUsdGeomAttributes(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdatePrimvarArgs& primvarArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_PRIMVAR_ARGS
    
    uint32_t startIdx = 0;
    for(uint32_t attribIndex = startIdx; attribIndex < geomData.NumAttributes; ++attribIndex)
    {
      const UsdBridgeAttribute& attrib = geomData.Attributes[attribIndex];
      if(attrib.DataType != UsdBridgeType::UNDEFINED)
        UpdateUsdGeomAttribute(updateArgs, primvarArgs, attribIndex);
    }
  }

  template<typename GeomDataType>
  void UpdateUsdGeomColors(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdatePrimvarArgs& primvarArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_PRIMVAR_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::COLORS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::COLORS);

    UsdGeomPrimvar uniformDispPrimvar = uniformPrimvars.GetPrimvar(UsdBridgeTokens->color);
    UsdGeomPrimvar timeVarDispPrimvar = timeVarPrimvars.GetPrimvar(UsdBridgeTokens->color);

    ClearUsdAttributes(uniformDispPrimvar.GetAttr(), timeVarDispPrimvar.GetAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdTimeCode timeCode = timeEval.Eval(DMI::COLORS);

      UsdAttribute colorAttrib = timeVaryingUpdate ? timeVarDispPrimvar : uniformDispPrimvar;

      if (geomData.Colors != nullptr)
      {
        size_t arrayNumElements = geomData.PerPrimColors ? numPrims : geomData.NumPoints;
        const void* arrayData = geomData.Colors;
        UsdBridgeType arrayDataType = geomData.ColorsType;
        assert(colorAttrib);

        // Get a span of type GfVec4f
        UsdBridgeSpanI<GfVec4f>* colorsSpan = UpdateUsdAttribute_Safe<GfVec4f>(usdRtData, writer->LogObject, nullptr, arrayDataType, arrayNumElements, colorAttrib, timeCode);

        if(colorsSpan)
        {
          // Write full color array to the span, using its type
          UsdBridgeArrays::WriteToSpanColor(writer->LogObject, *colorsSpan, arrayData, arrayNumElements, arrayDataType);
          // Assign the span to the color attribute
          colorsSpan->AssignToAttrib();
        }

        // Per face or per-vertex interpolation. This will break timesteps that have been written before.
        TfToken colorInterpolation = geomData.PerPrimColors ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
        uniformDispPrimvar.SetInterpolation(colorInterpolation);
      }
      else
      {
        colorAttrib.Set(SdfValueBlock(), timeCode);
      }
    }
  }


  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomInstanceIds(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::INSTANCEIDS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INSTANCEIDS);

    ClearUsdAttributes(uniformGeom.GetIdsAttr(), timeVarGeom.GetIdsAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdGeomType* outGeom = timeVaryingUpdate ? &timeVarGeom : &uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::INSTANCEIDS);

      UsdAttribute idsAttr = outGeom->GetIdsAttr();

      if (geomData.InstanceIds)
      {
        const void* arrayData = geomData.InstanceIds;
        size_t arrayNumElements = geomData.NumPoints;
        UsdBridgeType arrayDataType = geomData.InstanceIdsType;

        UpdateUsdAttribute_Safe(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements, idsAttr, timeCode);
      }
      else
      {
        idsAttr.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomWidths(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::SCALES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::SCALES);

    ClearUsdAttributes(uniformGeom.GetWidthsAttr(), timeVarGeom.GetWidthsAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::SCALES);

      UsdAttribute widthsAttribute = outGeom.GetWidthsAttr();
      assert(widthsAttribute);

      const void* arrayData = geomData.Scales;
      size_t arrayNumElements = geomData.NumPoints;
      UsdBridgeType arrayDataType = geomData.ScalesType;

      // Remember that widths define a diameter, so a default width (1.0) corresponds to a scale of 0.5.
      if(!arrayData && geomData.getUniformScale() == 0.5f)
      {
        widthsAttribute.Set(SdfValueBlock(), timeCode);
      }
      else
      {
        UsdBridgeSpanI<float>* widthsSpan = UpdateUsdAttribute_Safe<float>(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements,
          widthsAttribute, timeCode, false); // Don't update the attribute, just return the span (with written arrayData if applicable)

        if(widthsSpan)
        {
          if (arrayData)
          { for(float& x : *widthsSpan) { x *= 2.0f; } }
          else
          { for(float& x : *widthsSpan) { x = geomData.getUniformScale() * 2.0f; } }

          widthsSpan->AssignToAttrib();
        }
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomScales(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::SCALES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::SCALES);

    ClearUsdAttributes(uniformGeom.GetScalesAttr(), timeVarGeom.GetScalesAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::SCALES);

      UsdAttribute scalesAttribute = outGeom.GetScalesAttr();
      assert(scalesAttribute);

      const void* arrayData = geomData.Scales;
      size_t arrayNumElements = geomData.NumPoints;
      UsdBridgeType arrayDataType = geomData.ScalesType;

      if(!arrayData && usdbridgenumerics::isIdentity(geomData.Scale))
      {
        scalesAttribute.Set(SdfValueBlock(), timeCode);
      }
      else
      {
        UsdBridgeSpanI<GfVec3f>* scalesSpan = UpdateUsdAttribute_Safe<GfVec3f>(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements,
            scalesAttribute, timeCode); // By passing a nullptr as data, only the span will be returned

        if(!arrayData && scalesSpan)
        {
          GfVec3f defaultScale(geomData.Scale.Data);
          for(GfVec3f& x : *scalesSpan) x = defaultScale;
          scalesSpan->AssignToAttrib();
        }
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomOrientNormals(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::ORIENTATIONS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ORIENTATIONS);

    ClearUsdAttributes(uniformGeom.GetNormalsAttr(), timeVarGeom.GetNormalsAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::ORIENTATIONS);

      UsdAttribute normalsAttribute = outGeom.GetNormalsAttr();
      assert(normalsAttribute);

      if (geomData.Orientations)
      {
        const void* arrayData = geomData.Orientations;
        size_t arrayNumElements = geomData.NumPoints;
        UsdBridgeType arrayDataType = geomData.OrientationsType;

        UpdateUsdAttribute_Safe(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements, normalsAttribute, timeCode);
      }
      else
      {
        normalsAttribute.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomOrientations(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::ORIENTATIONS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ORIENTATIONS);

    ClearUsdAttributes(uniformGeom.GetOrientationsAttr(), timeVarGeom.GetOrientationsAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::ORIENTATIONS);

      // Orientations
      UsdAttribute orientationsAttribute = outGeom.GetOrientationsAttr();
      assert(orientationsAttribute);

      if(!geomData.Orientations && usdbridgenumerics::isIdentity(geomData.Orientation))
      {
        orientationsAttribute.Set(SdfValueBlock(), timeCode);
      }
      else
      {
        const void* arrayData = (UsdBridgeTypeNumComponents(geomData.OrientationsType) == 4) 
          ? geomData.Orientations : nullptr;
        size_t arrayNumElements = geomData.NumPoints;
        UsdBridgeType arrayDataType = geomData.OrientationsType;

        UsdBridgeSpanI<GfQuath>* orientsSpan = UpdateUsdAttribute_Safe<GfQuath>(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements,
          orientationsAttribute, timeCode);

        if(!arrayData && orientsSpan)
        {
          if (geomData.Orientations)
          {
            bool supportedType = true;
            switch (geomData.OrientationsType)
            {
            case UsdBridgeType::FLOAT3: { ConvertNormalsToQuaternions<float>(*orientsSpan, geomData.Orientations); break; }
            case UsdBridgeType::DOUBLE3: { ConvertNormalsToQuaternions<double>(*orientsSpan, geomData.Orientations); break; }
            default: { supportedType = false; break; }
            }

            if(supportedType)
              orientsSpan->AssignToAttrib();
            else
            {
              UsdBridgeLogMacro(logObj, UsdBridgeLogLevel::ERR, "UsdGeom OrientationsAttribute should be a 4-component vector, FLOAT3 or DOUBLE3.");
            }
          }
          else // => !usdbridgenumerics::isIdentity(geomData.Orientation)
          {
            // Assign a single orientation everywhere
            GfQuath defaultOrient(geomData.Orientation.Data[3], geomData.Orientation.Data[0], geomData.Orientation.Data[1], geomData.Orientation.Data[2]);
            for(GfQuath& x : *orientsSpan) x = defaultOrient;
            orientsSpan->AssignToAttrib();
          }
        }
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomProtoIndices(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::SHAPEINDICES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::SHAPEINDICES);


    if (performsUpdate)
    {
      UsdTimeCode timeCode = timeEval.Eval(DMI::SHAPEINDICES);
      UsdGeomType* outGeom = timeCode.IsDefault() ? &uniformGeom : &timeVarGeom;

      UsdAttribute protoIndexAttr = outGeom->GetProtoIndicesAttr();
      assert(protoIndexAttr);

      const void* arrayData = geomData.ShapeIndices;
      size_t arrayNumElements = geomData.NumPoints;
      UsdBridgeType arrayDataType = UsdBridgeType::INT;

      UsdBridgeSpanI<int>* protoIdxSpan = UpdateUsdAttribute_Safe<int>(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements,
          protoIndexAttr, timeCode); // By passing a nullptr as data, only the span will be returned

      if(!arrayData && protoIdxSpan)
      {
        for(int& x : *protoIdxSpan) x = 0;
        protoIdxSpan->AssignToAttrib();
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomLinearVelocities(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::LINEARVELOCITIES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::LINEARVELOCITIES);

    ClearUsdAttributes(uniformGeom.GetVelocitiesAttr(), timeVarGeom.GetVelocitiesAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::LINEARVELOCITIES);

      // Linear velocities
      UsdAttribute linearVelocitiesAttribute = outGeom.GetVelocitiesAttr();
      assert(linearVelocitiesAttribute);
      if (geomData.LinearVelocities)
      {
        const void* arrayData = geomData.LinearVelocities;
        size_t arrayNumElements = geomData.NumPoints;
        UsdBridgeType arrayDataType = UsdBridgeType::FLOAT3; // as per type and numcomponents of geomData.AngularVelocities

        UpdateUsdAttribute_Safe(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements, linearVelocitiesAttribute, timeCode);

      }
      else
      {
        linearVelocitiesAttribute.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomAngularVelocities(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::ANGULARVELOCITIES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ANGULARVELOCITIES);

    ClearUsdAttributes(uniformGeom.GetAngularVelocitiesAttr(), timeVarGeom.GetAngularVelocitiesAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::ANGULARVELOCITIES);

      // Angular velocities
      UsdAttribute angularVelocitiesAttribute = outGeom.GetAngularVelocitiesAttr();
      assert(angularVelocitiesAttribute);
      if (geomData.AngularVelocities)
      {
        const void* arrayData = geomData.AngularVelocities;
        size_t arrayNumElements = geomData.NumPoints;
        UsdBridgeType arrayDataType = UsdBridgeType::FLOAT3; // as per type and numcomponents of geomData.AngularVelocities

        UpdateUsdAttribute_Safe(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements, angularVelocitiesAttribute, timeCode);

      }
      else
      {
        angularVelocitiesAttribute.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomInvisibleIds(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename GeomDataType::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::INVISIBLEIDS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INVISIBLEIDS);

    ClearUsdAttributes(uniformGeom.GetInvisibleIdsAttr(), timeVarGeom.GetInvisibleIdsAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::INVISIBLEIDS);

      // Invisible ids
      UsdAttribute invisIdsAttr = outGeom.GetInvisibleIdsAttr();
      assert(invisIdsAttr);
      uint64_t numInvisibleIds = geomData.NumInvisibleIds;
      if (numInvisibleIds)
      {
        const void* arrayData = geomData.InvisibleIds;
        size_t arrayNumElements = numInvisibleIds;
        UsdBridgeType arrayDataType = geomData.InvisibleIdsType;

        UpdateUsdAttribute_Safe(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements, invisIdsAttr, timeCode);
      }
      else
      {
        invisIdsAttr.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomCurveLengths(UsdGeomUpdateArguments<GeomDataType>& updateArgs, UsdGeomUpdateAttribArgs<UsdGeomType>& attribArgs)
  {
    UNPACK_UPDATE_ARGS UNPACK_ATTRIB_ARGS
    using DMI = typename UsdBridgeCurveData::DataMemberId;

    // Fill geom prim and geometry layer with data.
    bool performsUpdate = updateEval.PerformsUpdate(DMI::CURVELENGTHS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::CURVELENGTHS);

    ClearUsdAttributes(uniformGeom.GetCurveVertexCountsAttr(), timeVarGeom.GetCurveVertexCountsAttr(), timeVaryingUpdate);

    if (performsUpdate)
    {
      UsdGeomBasisCurves& outGeom = timeVaryingUpdate ? timeVarGeom : uniformGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::POINTS);

      UsdAttribute vertCountAttr = outGeom.GetCurveVertexCountsAttr();
      assert(vertCountAttr);

      const void* arrayData = geomData.CurveLengths;
      size_t arrayNumElements = geomData.NumCurveLengths;
      UsdBridgeType arrayDataType = UsdBridgeType::INT; // as per type of geomData.CurveLengths

      UpdateUsdAttribute_Safe(usdRtData, logObj, arrayData, arrayDataType, arrayNumElements, vertCountAttr, timeCode);
    }
  }

  void UpdateUsdGeomPrototypes(const UsdBridgeLogObject& logObj, const UsdStagePtr& sceneStage, UsdGeomPointInstancer& uniformGeom,
    const UsdBridgeInstancerRefData& geomRefData, const SdfPrimPathList& protoGeomPaths,
    const char* protoShapePathRp)
  {
    using DMI = typename UsdBridgeInstancerData::DataMemberId;

    SdfPath protoBasePath = uniformGeom.GetPath().AppendPath(SdfPath(protoShapePathRp));
    sceneStage->RemovePrim(protoBasePath);

    UsdRelationship protoRel = uniformGeom.GetPrototypesRel();

    for(int shapeIdx = 0; shapeIdx < geomRefData.NumShapes; ++shapeIdx)
    {
      SdfPath shapePath;
      if(geomRefData.Shapes[shapeIdx] != UsdBridgeInstancerRefData::SHAPE_MESH)
      {
        std::string protoName = constring::protoShapePf + std::to_string(shapeIdx);
        shapePath = protoBasePath.AppendPath(SdfPath(protoName.c_str()));
      }
      else
      {
        int protoGeomIdx = static_cast<int>(geomRefData.Shapes[shapeIdx]); // The mesh shape value is an index into protoGeomPaths
        assert(protoGeomIdx < protoGeomPaths.size());
        shapePath = protoGeomPaths[protoGeomIdx];
      }

      UsdGeomXformable geomXformable;
      switch (geomRefData.Shapes[shapeIdx])
      {
        case UsdBridgeInstancerRefData::SHAPE_SPHERE:
        {
          geomXformable = UsdGeomSphere::Define(sceneStage, shapePath);
          break;
        }
        case UsdBridgeInstancerRefData::SHAPE_CYLINDER:
        {
          geomXformable = UsdGeomCylinder::Define(sceneStage, shapePath);
          break;
        }
        case UsdBridgeInstancerRefData::SHAPE_CONE:
        {
          geomXformable = UsdGeomCone::Define(sceneStage, shapePath);
          break;
        }
        default:
        {
          geomXformable = UsdGeomXformable::Get(sceneStage, shapePath);
          break;
        }
      }

      // Add a transform
      geomXformable.ClearXformOpOrder();
      if(!usdbridgenumerics::isIdentity(geomRefData.ShapeTransform))
      {
        const float* transform = geomRefData.ShapeTransform.Data;
        GfMatrix4d transMat;
        transMat.SetRow(0, GfVec4d(GfVec4f(&transform[0])));
        transMat.SetRow(1, GfVec4d(GfVec4f(&transform[4])));
        transMat.SetRow(2, GfVec4d(GfVec4f(&transform[8])));
        transMat.SetRow(3, GfVec4d(GfVec4f(&transform[12])));

        geomXformable.AddTransformOp().Set(transMat);
      }

      protoRel.AddTarget(shapePath);
    }
  }
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeMeshData& meshData, bool uniformPrim)
{
  return InitializeUsdGeometry_Impl(this, geometryStage, geomPath, meshData, uniformPrim, Settings);
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeInstancerData& instancerData, bool uniformPrim)
{
  return InitializeUsdGeometry_Impl(this, geometryStage, geomPath, instancerData, uniformPrim, Settings);
}

UsdPrim UsdBridgeUsdWriter::InitializeUsdGeometry(UsdStageRefPtr geometryStage, const SdfPath& geomPath, const UsdBridgeCurveData& curveData, bool uniformPrim)
{
  return InitializeUsdGeometry_Impl(this, geometryStage, geomPath, curveData, uniformPrim, Settings);
}

#ifdef VALUE_CLIP_RETIMING
void UsdBridgeUsdWriter::UpdateUsdGeometryManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeMeshData& meshData)
{
  TimeEvaluator<UsdBridgeMeshData> timeEval(meshData);
  InitializeUsdGeometry_Impl(this, cacheEntry->ManifestStage.second, cacheEntry->PrimPath, meshData, false,
    Settings, &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}

void UsdBridgeUsdWriter::UpdateUsdGeometryManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeInstancerData& instancerData)
{
  TimeEvaluator<UsdBridgeInstancerData> timeEval(instancerData);
  InitializeUsdGeometry_Impl(this, cacheEntry->ManifestStage.second, cacheEntry->PrimPath, instancerData, false,
    Settings, &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}

void UsdBridgeUsdWriter::UpdateUsdGeometryManifest(const UsdBridgePrimCache* cacheEntry, const UsdBridgeCurveData& curveData)
{
  TimeEvaluator<UsdBridgeCurveData> timeEval(curveData);
  InitializeUsdGeometry_Impl(this, cacheEntry->ManifestStage.second, cacheEntry->PrimPath, curveData, false,
    Settings, &timeEval);

  if(this->EnableSaving)
    cacheEntry->ManifestStage.second->Save();
}
#endif

#define UPDATE_USDGEOM_ATTRIB_ARRAYS(FuncDef) \
  FuncDef(updateArgs, attribArgs)

#define UPDATE_USDGEOM_PRIMVAR_ARRAYS(FuncDef) \
  FuncDef(updateArgs, primvarArgs)

void UsdBridgeUsdWriter::UpdateUsdGeometry(const UsdStagePtr& timeVarStage, const SdfPath& meshPath, const UsdBridgeMeshData& geomData, double timeStep)
{
  // To avoid data duplication when using of clip stages, we need to potentially use the scenestage prim for time-uniform data.
  UsdGeomMesh uniformGeom = UsdGeomMesh::Get(this->SceneStage, meshPath);
  assert(uniformGeom);
  UsdGeomPrimvarsAPI uniformPrimvars(uniformGeom);

  UsdGeomMesh timeVarGeom = UsdGeomMesh::Get(timeVarStage, meshPath);
  assert(timeVarGeom);
  UsdGeomPrimvarsAPI timeVarPrimvars(timeVarGeom);

  // Update the mesh
  UsdBridgeUpdateEvaluator<const UsdBridgeMeshData> updateEval(geomData);
  TimeEvaluator<UsdBridgeMeshData> timeEval(geomData, timeStep);

  assert((geomData.NumIndices % geomData.FaceVertexCount) == 0);
  uint64_t numPrims = int(geomData.NumIndices) / geomData.FaceVertexCount;

  UsdBridgeRt usdRtData(this->SceneStage, meshPath);

  UsdGeomUpdateArguments<UsdBridgeMeshData> updateArgs = { usdRtData, geomData, numPrims, updateEval, timeEval };
  UsdGeomUpdateAttribArgs<UsdGeomMesh> attribArgs = { this->LogObject, timeVarGeom, uniformGeom };
  UsdGeomUpdatePrimvarArgs primvarArgs = { this, timeVarPrimvars, uniformPrimvars };

  UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomPoints);
  UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomNormals);
  if( Settings.EnableStTexCoords && UsdGeomDataHasTexCoords(geomData) )
    { UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomTexCoords); }
  UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomAttributes);
  UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomColors);
  UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomIndices);
}

void UsdBridgeUsdWriter::UpdateUsdGeometry(const UsdStagePtr& timeVarStage, const SdfPath& instancerPath, const UsdBridgeInstancerData& geomData, double timeStep)
{
  UsdBridgeUpdateEvaluator<const UsdBridgeInstancerData> updateEval(geomData);
  TimeEvaluator<UsdBridgeInstancerData> timeEval(geomData, timeStep);

  UsdBridgeRt usdRtData(this->SceneStage, instancerPath);

  bool useGeomPoints = geomData.UseUsdGeomPoints;

  uint64_t numPrims = geomData.NumPoints;

  if (useGeomPoints)
  {
    UsdGeomPoints uniformGeom = UsdGeomPoints::Get(this->SceneStage, instancerPath);
    assert(uniformGeom);
    UsdGeomPrimvarsAPI uniformPrimvars(uniformGeom);

    UsdGeomPoints timeVarGeom = UsdGeomPoints::Get(timeVarStage, instancerPath);
    assert(timeVarGeom);
    UsdGeomPrimvarsAPI timeVarPrimvars(timeVarGeom);

    UsdGeomUpdateArguments<UsdBridgeInstancerData> updateArgs = { usdRtData, geomData, numPrims, updateEval, timeEval };
    UsdGeomUpdateAttribArgs<UsdGeomPoints> attribArgs = { this->LogObject, timeVarGeom, uniformGeom };
    UsdGeomUpdatePrimvarArgs primvarArgs = { this, timeVarPrimvars, uniformPrimvars };

    UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomPoints);
    UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomInstanceIds);
    UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomWidths);
    UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomOrientNormals);
    if( Settings.EnableStTexCoords && UsdGeomDataHasTexCoords(geomData) )
      { UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomTexCoords); }
    UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomAttributes);
    UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomColors);
  }
  else
  {
    UsdGeomPointInstancer uniformGeom = UsdGeomPointInstancer::Get(this->SceneStage, instancerPath);
    assert(uniformGeom);
    UsdGeomPrimvarsAPI uniformPrimvars(uniformGeom);

    UsdGeomPointInstancer timeVarGeom = UsdGeomPointInstancer::Get(timeVarStage, instancerPath);
    assert(timeVarGeom);
    UsdGeomPrimvarsAPI timeVarPrimvars(timeVarGeom);

    UsdGeomUpdateArguments<UsdBridgeInstancerData> updateArgs = { usdRtData, geomData, numPrims, updateEval, timeEval };
    UsdGeomUpdateAttribArgs<UsdGeomPointInstancer> attribArgs = { this->LogObject, timeVarGeom, uniformGeom };
    UsdGeomUpdatePrimvarArgs primvarArgs = { this, timeVarPrimvars, uniformPrimvars };

    UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomPoints);
    UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomInstanceIds);
    UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomScales);
    UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomOrientations);
    if( Settings.EnableStTexCoords && UsdGeomDataHasTexCoords(geomData) )
      { UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomTexCoords); }
    UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomAttributes);
    UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomColors);
    UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomProtoIndices);
    //UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomLinearVelocities);
    //UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomAngularVelocities);
    UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomInvisibleIds);
  }
}

void UsdBridgeUsdWriter::UpdateUsdGeometry(const UsdStagePtr& timeVarStage, const SdfPath& curvePath, const UsdBridgeCurveData& geomData, double timeStep)
{
  // To avoid data duplication when using of clip stages, we need to potentially use the scenestage prim for time-uniform data.
  UsdGeomBasisCurves uniformGeom = UsdGeomBasisCurves::Get(this->SceneStage, curvePath);
  assert(uniformGeom);
  UsdGeomPrimvarsAPI uniformPrimvars(uniformGeom);

  UsdGeomBasisCurves timeVarGeom = UsdGeomBasisCurves::Get(timeVarStage, curvePath);
  assert(timeVarGeom);
  UsdGeomPrimvarsAPI timeVarPrimvars(timeVarGeom);

  // Update the curve
  UsdBridgeUpdateEvaluator<const UsdBridgeCurveData> updateEval(geomData);
  TimeEvaluator<UsdBridgeCurveData> timeEval(geomData, timeStep);

  uint64_t numPrims = geomData.NumCurveLengths;

  UsdBridgeRt usdRtData(this->SceneStage, curvePath);

  UsdGeomUpdateArguments<UsdBridgeCurveData> updateArgs = { usdRtData, geomData, numPrims, updateEval, timeEval };
  UsdGeomUpdateAttribArgs<UsdGeomBasisCurves> attribArgs = { this->LogObject, timeVarGeom, uniformGeom };
  UsdGeomUpdatePrimvarArgs primvarArgs = { this, timeVarPrimvars, uniformPrimvars };

  UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomPoints);
  UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomNormals);
  if( Settings.EnableStTexCoords && UsdGeomDataHasTexCoords(geomData) )
    { UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomTexCoords); }
  UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomAttributes);
  UPDATE_USDGEOM_PRIMVAR_ARRAYS(UpdateUsdGeomColors);
  UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomWidths);
  UPDATE_USDGEOM_ATTRIB_ARRAYS(UpdateUsdGeomCurveLengths);
}

void UsdBridgeUsdWriter::UpdateUsdInstancerPrototypes(const SdfPath& instancerPath, const UsdBridgeInstancerRefData& geomRefData,
  const SdfPrimPathList& refProtoGeomPrimPaths, const char* protoShapePathRp)
{
  UsdGeomPointInstancer uniformGeom = UsdGeomPointInstancer::Get(this->SceneStage, instancerPath);
  if(!uniformGeom)
  {
    UsdBridgeLogMacro(this->LogObject, UsdBridgeLogLevel::WARNING, "Attempt to perform update of prototypes on a prim that is not a UsdGeomPointInstancer.");
    return;
  }

  // Very basic rel update, without any timevarying aspects
  UpdateUsdGeomPrototypes(this->LogObject, this->SceneStage, uniformGeom, geomRefData, refProtoGeomPrimPaths, protoShapePathRp);
}