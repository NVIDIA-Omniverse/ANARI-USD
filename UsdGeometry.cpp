// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdGeometry.h"
#include "UsdAnari.h"
#include "UsdDataArray.h"
#include "UsdDevice.h"
#include "UsdBridgeUtils.h"
#include "anari/frontend/type_utility.h"

#include <cmath>

DEFINE_PARAMETER_MAP(UsdGeometry,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::time", ANARI_FLOAT64, timeStep)
  REGISTER_PARAMETER_MACRO("usd::timeVarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("usd::time.shapeGeometry", ANARI_FLOAT64, shapeGeometryRefTimeStep)
  REGISTER_PARAMETER_MACRO("usd::useUsdGeomPoints", ANARI_BOOL, UseUsdGeomPoints)
  REGISTER_PARAMETER_MACRO("primitive.index", ANARI_ARRAY, indices)
  REGISTER_PARAMETER_MACRO("primitive.normal", ANARI_ARRAY, primitiveNormals)
  REGISTER_PARAMETER_MACRO("primitive.color", ANARI_ARRAY, primitiveColors)
  REGISTER_PARAMETER_MACRO("primitive.radius", ANARI_ARRAY, primitiveRadii)
  REGISTER_PARAMETER_MACRO("primitive.scale", ANARI_ARRAY, primitiveScales)
  REGISTER_PARAMETER_MACRO("primitive.orientation", ANARI_ARRAY, primitiveOrientations)
  REGISTER_PARAMETER_ARRAY_MACRO("primitive.attribute", "", ANARI_ARRAY, primitiveAttributes, MAX_ATTRIBS)
  REGISTER_PARAMETER_MACRO("primitive.id", ANARI_ARRAY, primitiveIds)
  REGISTER_PARAMETER_MACRO("vertex.position", ANARI_ARRAY, vertexPositions)
  REGISTER_PARAMETER_MACRO("vertex.normal", ANARI_ARRAY, vertexNormals)
  REGISTER_PARAMETER_MACRO("vertex.color", ANARI_ARRAY, vertexColors)
  REGISTER_PARAMETER_MACRO("vertex.radius", ANARI_ARRAY, vertexRadii)
  REGISTER_PARAMETER_MACRO("vertex.scale", ANARI_ARRAY, vertexScales)
  REGISTER_PARAMETER_MACRO("vertex.orientation", ANARI_ARRAY, vertexOrientations)
  REGISTER_PARAMETER_ARRAY_MACRO("vertex.attribute", "", ANARI_ARRAY, vertexAttributes, MAX_ATTRIBS)
  REGISTER_PARAMETER_ARRAY_MACRO("usd::attribute", ".name", ANARI_STRING, attributeNames, MAX_ATTRIBS)
  REGISTER_PARAMETER_MACRO("radius", ANARI_FLOAT32, radiusConstant)
  REGISTER_PARAMETER_MACRO("scale", ANARI_FLOAT32_VEC3, scaleConstant)
  REGISTER_PARAMETER_MACRO("orientation", ANARI_FLOAT32_QUAT_IJKW, orientationConstant)
  REGISTER_PARAMETER_MACRO("shapeType", ANARI_STRING, shapeType)
  REGISTER_PARAMETER_MACRO("shapeGeometry", ANARI_GEOMETRY, shapeGeometry)
  REGISTER_PARAMETER_MACRO("shapeTransform", ANARI_FLOAT32_MAT4, shapeTransform)
) // See .h for usage.

constexpr UsdGeometry::ComponentPair UsdGeometry::componentParamNames[]; // Workaround for C++14's lack of inlining constexpr arrays

struct UsdGeometryTempArrays
{
  UsdGeometryTempArrays(const UsdGeometry::AttributeArray& attributes)
    : Attributes(attributes)
  {}

  std::vector<int> CurveLengths;
  std::vector<float> PointsArray;
  std::vector<float> NormalsArray;
  std::vector<float> RadiiArray;
  std::vector<float> ScalesArray;
  std::vector<float> OrientationsArray;
  std::vector<int64_t> IdsArray;
  std::vector<int64_t> InvisIdsArray;
  std::vector<char> ColorsArray; // generic byte array
  ANARIDataType ColorsArrayType;
  UsdGeometry::AttributeDataArraysType AttributeDataArrays;

  const UsdGeometry::AttributeArray& Attributes;

  void resetColorsArray(size_t numElements, ANARIDataType type)
  {
    ColorsArray.resize(numElements*anari::sizeOf(type));
    ColorsArrayType = type;
  }

  void reserveColorsArray(size_t numElements)
  {
    ColorsArray.reserve(numElements*anari::sizeOf(ColorsArrayType));
  }

  size_t expandColorsArray(size_t numElements)
  {
    size_t startByte = ColorsArray.size();
    size_t typeSize = anari::sizeOf(ColorsArrayType);
    ColorsArray.resize(startByte+numElements*typeSize);
    return startByte/typeSize;
  }

  void copyToColorsArray(const void* source, size_t srcIdx, size_t destIdx, size_t numElements)
  {
    size_t typeSize = anari::sizeOf(ColorsArrayType);
    size_t srcStart = srcIdx*typeSize;
    size_t dstStart = destIdx*typeSize;
    size_t numBytes = numElements*typeSize;
    assert(dstStart+numBytes <= ColorsArray.size());
    memcpy(ColorsArray.data()+dstStart, reinterpret_cast<const char*>(source)+srcStart, numBytes);
  }

  void resetAttributeDataArray(size_t attribIdx, size_t numElements)
  {
    if(Attributes[attribIdx].Data)
    {
      uint32_t eltSize = Attributes[attribIdx].EltSize;
      AttributeDataArrays[attribIdx].resize(numElements*eltSize);
    }
    else
      AttributeDataArrays[attribIdx].resize(0);
  }

  void reserveAttributeDataArray(size_t attribIdx, size_t numElements)
  {
    if(Attributes[attribIdx].Data)
    {
      uint32_t eltSize = Attributes[attribIdx].EltSize;
      AttributeDataArrays[attribIdx].reserve(numElements*eltSize);
    }
  }

  size_t expandAttributeDataArray(size_t attribIdx, size_t numElements)
  {
    if(Attributes[attribIdx].Data)
    {
      uint32_t eltSize = Attributes[attribIdx].EltSize;
      size_t startByte = AttributeDataArrays[attribIdx].size();
      AttributeDataArrays[attribIdx].resize(startByte+numElements*eltSize);
      return startByte/eltSize;
    }
    return 0;
  }

  void copyToAttributeDataArray(size_t attribIdx, size_t srcIdx, size_t destIdx, size_t numElements)
  {
    if(Attributes[attribIdx].Data)
    {
      uint32_t eltSize = Attributes[attribIdx].EltSize;
      const void* attribSrc = reinterpret_cast<const char*>(Attributes[attribIdx].Data) + srcIdx*eltSize;
      size_t dstStart = destIdx*eltSize;
      size_t numBytes = numElements*eltSize;
      assert(dstStart+numBytes <= AttributeDataArrays[attribIdx].size());
      void* attribDest = &AttributeDataArrays[attribIdx][dstStart];
      memcpy(attribDest, attribSrc, numElements*eltSize);
    }
  }
};

namespace
{
  struct UsdGeometryDebugData
  {
    UsdDevice* device = nullptr;
    UsdGeometry* geometry = nullptr;
    const char* debugName = nullptr;
  };

  UsdGeometry::GeomType GetGeomType(const char* type)
  {
    UsdGeometry::GeomType geomType;

    if (strEquals(type, "sphere"))
      geomType = UsdGeometry::GEOM_SPHERE;
    else if (strEquals(type, "cylinder"))
      geomType = UsdGeometry::GEOM_CYLINDER;
    else if (strEquals(type, "cone"))
      geomType = UsdGeometry::GEOM_CONE;
    else if (strEquals(type, "curve"))
      geomType = UsdGeometry::GEOM_CURVE;
    else if(strEquals(type, "triangle"))
      geomType = UsdGeometry::GEOM_TRIANGLE;
    else if (strEquals(type, "quad"))
      geomType = UsdGeometry::GEOM_QUAD;
    else if (strEquals(type, "glyph"))
      geomType = UsdGeometry::GEOM_GLYPH;
    else
      geomType = UsdGeometry::GEOM_UNKNOWN;

    return geomType;
  }

  uint64_t GetNumberOfPrims(bool hasIndices, const UsdDataLayout& indexLayout, UsdGeometry::GeomType geomType)
  {
    if(geomType == UsdGeometry::GEOM_CURVE)
      return indexLayout.numItems1 - 1;
    else if(hasIndices)
      return indexLayout.numItems1;

    int perPrimVertexCount = 1;
    switch(geomType)
    {
    case UsdGeometry::GEOM_CYLINDER:
    case UsdGeometry::GEOM_CONE:
      perPrimVertexCount = 2;
      break;
    case UsdGeometry::GEOM_TRIANGLE:
      perPrimVertexCount = 3;
      break;
    case UsdGeometry::GEOM_QUAD:
      perPrimVertexCount = 4;
      break;
    default: break;
    };

    return indexLayout.numItems1 / perPrimVertexCount;
  }

  bool isBitSet(int value, int bit)
  {
    return (bool)(value & (1 << bit));
  }

  size_t getIndex(const void* indices, ANARIDataType type, size_t elt)
  {
    size_t result;
    switch (type)
    {
      case ANARI_INT32:
      case ANARI_INT32_VEC2:
        result = (reinterpret_cast<const int*>(indices))[elt];
        break;
      case ANARI_UINT32:
      case ANARI_UINT32_VEC2:
        result = (reinterpret_cast<const uint32_t*>(indices))[elt];
        break;
      case ANARI_INT64:
      case ANARI_INT64_VEC2:
        result = (reinterpret_cast<const int64_t*>(indices))[elt];
        break;
      case ANARI_UINT64:
      case ANARI_UINT64_VEC2:
        result = (reinterpret_cast<const uint64_t*>(indices))[elt];
        break;
      default:
        result = 0;
        break;
    }
    return result;
  }

  void getValues1(const void* vertices, ANARIDataType type, size_t idx, float* result)
  {
    if (type == ANARI_FLOAT32)
    {
      const float* vertf = reinterpret_cast<const float*>(vertices);
      result[0] = vertf[idx];
    }
    else if (type == ANARI_FLOAT64)
    {
      const double* vertd = reinterpret_cast<const double*>(vertices);
      result[0] = (float)vertd[idx];
    }
  }

  void getValues2(const void* vertices, ANARIDataType type, size_t idx, float* result)
  {
    if (type == ANARI_FLOAT32_VEC2)
    {
      const float* vertf = reinterpret_cast<const float*>(vertices);
      result[0] = vertf[idx * 2];
      result[1] = vertf[idx * 2 + 1];
    }
    else if (type == ANARI_FLOAT64_VEC2)
    {
      const double* vertd = reinterpret_cast<const double*>(vertices);
      result[0] = (float)vertd[idx * 2];
      result[1] = (float)vertd[idx * 2 + 1];
    }
  }

  void getValues3(const void* vertices, ANARIDataType type, size_t idx, float* result)
  {
    if (type == ANARI_FLOAT32_VEC3)
    {
      const float* vertf = reinterpret_cast<const float*>(vertices);
      result[0] = vertf[idx * 3];
      result[1] = vertf[idx * 3 + 1];
      result[2] = vertf[idx * 3 + 2];
    }
    else if (type == ANARI_FLOAT64_VEC3)
    {
      const double* vertd = reinterpret_cast<const double*>(vertices);
      result[0] = (float)vertd[idx * 3];
      result[1] = (float)vertd[idx * 3 + 1];
      result[2] = (float)vertd[idx * 3 + 2];
    }
  }

  void getValues4(const void* vertices, ANARIDataType type, size_t idx, float* result)
  {
    if (type == ANARI_FLOAT32_VEC4)
    {
      const float* vertf = reinterpret_cast<const float*>(vertices);
      result[0] = vertf[idx * 4];
      result[1] = vertf[idx * 4 + 1];
      result[2] = vertf[idx * 4 + 2];
      result[3] = vertf[idx * 4 + 3];
    }
    else if (type == ANARI_FLOAT64_VEC4)
    {
      const double* vertd = reinterpret_cast<const double*>(vertices);
      result[0] = (float)vertd[idx * 4];
      result[1] = (float)vertd[idx * 4 + 1];
      result[2] = (float)vertd[idx * 4 + 2];
      result[3] = (float)vertd[idx * 4 + 3];
    }
  }

  void generateIndexedSphereData(const UsdGeometryData& paramData, const UsdGeometry::AttributeArray& attributeArray, UsdGeometryTempArrays* tempArrays)
  {
    if (paramData.indices)
    {
      auto& attribDataArrays = tempArrays->AttributeDataArrays;
      assert(attribDataArrays.size() == attributeArray.size());

      uint64_t numVertices = paramData.vertexPositions->getLayout().numItems1;

      ANARIDataType scaleType = paramData.vertexScales ? paramData.vertexScales->getType()
        : (paramData.primitiveScales ? paramData.primitiveScales->getType() : ANARI_UNKNOWN);
      size_t scaleComps = anari::componentsOf(scaleType);

      bool perPrimNormals = !paramData.vertexNormals && paramData.primitiveNormals;
      bool perPrimRadii = !paramData.vertexRadii && paramData.primitiveRadii;
      bool perPrimScales = !paramData.vertexScales && paramData.primitiveScales;
      bool perPrimColors = !paramData.vertexColors && paramData.primitiveColors;
      bool perPrimOrientations = !paramData.vertexOrientations && paramData.primitiveOrientations;

      ANARIDataType colorType = perPrimColors ? paramData.primitiveColors->getType() : ANARI_UINT8; // Vertex colors aren't reordered

      // Effectively only has to reorder if the source array is perPrim, otherwise this function effectively falls through and the source array is assigned directly at parent scope.
      tempArrays->NormalsArray.resize(perPrimNormals ? numVertices*3 : 0);
      tempArrays->RadiiArray.resize(perPrimScales ?  numVertices : 0);
      tempArrays->ScalesArray.resize(perPrimScales ?  numVertices*scaleComps : 0);
      tempArrays->OrientationsArray.resize(perPrimOrientations ? numVertices*4 : 0);
      tempArrays->IdsArray.resize(numVertices, -1); // Always filled, since indices implies necessity for invisibleIds, and therefore also an Id array
      tempArrays->resetColorsArray(perPrimColors ?  numVertices : 0, colorType);
      for(size_t attribIdx = 0; attribIdx < attribDataArrays.size(); ++attribIdx)
      {
        tempArrays->resetAttributeDataArray(attribIdx, attributeArray[attribIdx].PerPrimData ? numVertices : 0);
      }

      const void* indices = paramData.indices->getData();
      uint64_t numIndices = paramData.indices->getLayout().numItems1;
      ANARIDataType indexType = paramData.indices->getType();

      int64_t maxId = -1;
      for (uint64_t primIdx = 0; primIdx < numIndices; ++primIdx)
      {
        size_t vertIdx = getIndex(indices, indexType, primIdx);

        // Normals
        if (perPrimNormals)
        {
          float* normalsDest = &tempArrays->NormalsArray[vertIdx * 3];
          getValues3(paramData.primitiveNormals->getData(), paramData.primitiveNormals->getType(), primIdx, normalsDest);
        }

        // Orientations
        if (perPrimOrientations)
        {
          float* orientsDest = &tempArrays->OrientationsArray[vertIdx*4];
          getValues4(paramData.primitiveOrientations->getData(), paramData.primitiveOrientations->getType(), primIdx, orientsDest);
        }

        // Radii
        if (perPrimRadii)
        {
          float* radiiDest = &tempArrays->RadiiArray[vertIdx];
          getValues1(paramData.primitiveRadii->getData(), paramData.primitiveRadii->getType(), primIdx, radiiDest);
        }

        // Scales
        if (perPrimScales)
        {
          float* scalesDest = &tempArrays->ScalesArray[vertIdx*scaleComps];
          if(scaleComps == 1)
            getValues1(paramData.primitiveScales->getData(), paramData.primitiveScales->getType(), primIdx, scalesDest);
          else if(scaleComps == 3)
            getValues3(paramData.primitiveScales->getData(), paramData.primitiveScales->getType(), primIdx, scalesDest);
        }

        // Colors
        if (perPrimColors)
        {
          assert(primIdx < paramData.primitiveColors->getLayout().numItems1);
          tempArrays->copyToColorsArray(paramData.primitiveColors->getData(), primIdx, vertIdx, 1);
        }

        // Attributes
        for(size_t attribIdx = 0; attribIdx < attribDataArrays.size(); ++attribIdx)
        {
          if(attributeArray[attribIdx].PerPrimData)
          {
            tempArrays->copyToAttributeDataArray(attribIdx, primIdx, vertIdx, 1);
          }
        }

        // Ids
        if (paramData.primitiveIds)
        {
          int64_t id = static_cast<int64_t>(getIndex(paramData.primitiveIds->getData(), paramData.primitiveIds->getType(), primIdx));
          tempArrays->IdsArray[vertIdx] = id;
          if (id > maxId)
            maxId = id;
        }
        else
        {
          int64_t id = static_cast<int64_t>(vertIdx);
          maxId = tempArrays->IdsArray[vertIdx] = id;
        }
      }

      // Assign unused ids to untouched vertices, then add those ids to invisible array
      tempArrays->InvisIdsArray.resize(0);
      tempArrays->InvisIdsArray.reserve(numVertices);

      for (uint64_t vertIdx = 0; vertIdx < numVertices; ++vertIdx)
      {
        if (tempArrays->IdsArray[vertIdx] == -1)
        {
          tempArrays->IdsArray[vertIdx] = ++maxId;
          tempArrays->InvisIdsArray.push_back(maxId);
        }
      }
    }
  }

  void convertLinesToSticks(const UsdGeometryData& paramData, const UsdGeometry::AttributeArray& attributeArray, UsdGeometryTempArrays* tempArrays)
  {
    // Converts arrays of vertex endpoint 2-tuples (optionally obtained via index 2-tuples) into center vertices with correct seglengths.

    auto& attribDataArrays = tempArrays->AttributeDataArrays;
    assert(attribDataArrays.size() == attributeArray.size());

    const UsdDataArray* vertexArray = paramData.vertexPositions;
    uint64_t numVertices = vertexArray->getLayout().numItems1;
    const void* vertices = vertexArray->getData();
    ANARIDataType vertexType = vertexArray->getType();

    const UsdDataArray* indexArray = paramData.indices;
    uint64_t numSticks = indexArray ? indexArray->getLayout().numItems1 : numVertices/2;
    uint64_t numIndices = numSticks * 2; // Indices are 2-element vectors in ANARI
    const void* indices = indexArray ? indexArray->getData() : nullptr;
    ANARIDataType indexType = indexArray ? indexArray->getType() : ANARI_UINT32;

    tempArrays->PointsArray.resize(numSticks * 3);
    tempArrays->ScalesArray.resize(numSticks * 3); // Scales are always present
    tempArrays->OrientationsArray.resize(numSticks * 4);
    tempArrays->IdsArray.resize(paramData.primitiveIds ? numSticks : 0);
    // Only reorder per-vertex arrays, per-prim is already in order of the output stick center vertices
    ANARIDataType colorType = paramData.vertexColors ? paramData.vertexColors->getType() : ANARI_UINT8;
    tempArrays->resetColorsArray(paramData.vertexColors ?  numSticks : 0,  colorType);
    for(size_t attribIdx = 0; attribIdx < attribDataArrays.size(); ++attribIdx)
    {
      tempArrays->resetAttributeDataArray(attribIdx, !attributeArray[attribIdx].PerPrimData ? numSticks : 0);
    }

    for (size_t i = 0; i < numIndices; i += 2)
    {
      size_t primIdx = i / 2;
      size_t vertIdx0 = indices ? getIndex(indices, indexType, i) : i;
      size_t vertIdx1 = indices ? getIndex(indices, indexType, i + 1) : i + 1;
      assert(vertIdx0 < numVertices);
      assert(vertIdx1 < numVertices);

      float point0[3], point1[3];
      getValues3(vertices, vertexType, vertIdx0, point0);
      getValues3(vertices, vertexType, vertIdx1, point1);

      tempArrays->PointsArray[primIdx * 3] = (point0[0] + point1[0]) * 0.5f;
      tempArrays->PointsArray[primIdx * 3 + 1] = (point0[1] + point1[1]) * 0.5f;
      tempArrays->PointsArray[primIdx * 3 + 2] = (point0[2] + point1[2]) * 0.5f;

      float scaleVal = paramData.radiusConstant;
      if (paramData.vertexRadii)
      {
        getValues1(paramData.vertexRadii->getData(), paramData.vertexRadii->getType(), vertIdx0, &scaleVal);
      }
      else if (paramData.primitiveRadii)
      {
        getValues1(paramData.primitiveRadii->getData(), paramData.primitiveRadii->getType(), primIdx, &scaleVal);
      }

      float segDir[3] = {
        point1[0] - point0[0],
        point1[1] - point0[1],
        point1[2] - point0[2],
      };
      float segLength = sqrtf(segDir[0] * segDir[0] + segDir[1] * segDir[1] + segDir[2] * segDir[2]);
      tempArrays->ScalesArray[primIdx * 3] = scaleVal;
      tempArrays->ScalesArray[primIdx * 3 + 1] = scaleVal;
      tempArrays->ScalesArray[primIdx * 3 + 2] = segLength * 0.5f;

      // Rotation
      // USD shapes are always lengthwise-oriented along the z axis
      usdbridgenumerics::DirectionToQuaternionZ(segDir, segLength, tempArrays->OrientationsArray.data() + primIdx*4);

      //Colors
      if (paramData.vertexColors)
      {
        assert(vertIdx0 < paramData.vertexColors->getLayout().numItems1);
        tempArrays->copyToColorsArray(paramData.vertexColors->getData(), vertIdx0, primIdx, 1);
      }

      // Attributes
      for(size_t attribIdx = 0; attribIdx < attribDataArrays.size(); ++attribIdx)
      {
        if(!attributeArray[attribIdx].PerPrimData)
        {
          tempArrays->copyToAttributeDataArray(attribIdx, vertIdx0, primIdx, 1);
        }
      }

      // Ids
      if (paramData.primitiveIds)
      {
        tempArrays->IdsArray[primIdx] = (int64_t)getIndex(paramData.primitiveIds->getData(), paramData.primitiveIds->getType(), primIdx);
      }
    }
  }

  void pushVertex(const UsdGeometryData& paramData, const UsdGeometry::AttributeArray& attributeArray, UsdGeometryTempArrays* tempArrays,
    const void* vertices, ANARIDataType vertexType,
    bool hasNormals, bool hasColors, bool hasRadii,
    size_t segStart, size_t primIdx)
  {
    auto& attribDataArrays = tempArrays->AttributeDataArrays;

    float point[3];
    getValues3(vertices, vertexType, segStart, point);
    tempArrays->PointsArray.push_back(point[0]);
    tempArrays->PointsArray.push_back(point[1]);
    tempArrays->PointsArray.push_back(point[2]);

    // Normals
    if (hasNormals)
    {
      float normals[3];
      if (paramData.vertexNormals)
      {
        getValues3(paramData.vertexNormals->getData(), paramData.vertexNormals->getType(), segStart, normals);
      }
      else if (paramData.primitiveNormals)
      {
        getValues3(paramData.primitiveNormals->getData(), paramData.primitiveNormals->getType(), primIdx, normals);
      }

      tempArrays->NormalsArray.push_back(normals[0]);
      tempArrays->NormalsArray.push_back(normals[1]);
      tempArrays->NormalsArray.push_back(normals[2]);
    }

    // Radii
    if (hasRadii)
    {
      float radii;
      if (paramData.vertexRadii)
      {
        getValues1(paramData.vertexRadii->getData(), paramData.vertexRadii->getType(), segStart, &radii);
      }
      else if (paramData.primitiveRadii)
      {
        getValues1(paramData.primitiveRadii->getData(), paramData.primitiveRadii->getType(), primIdx, &radii);
      }

      tempArrays->ScalesArray.push_back(radii);
    }

    // Colors
    if (hasColors)
    {
      size_t destIdx = tempArrays->expandColorsArray(1);
      if (paramData.vertexColors)
      {
        tempArrays->copyToColorsArray(paramData.vertexColors->getData(), segStart, destIdx, 1);
      }
      else if (paramData.primitiveColors)
      {
        tempArrays->copyToColorsArray(paramData.primitiveColors->getData(), primIdx, destIdx, 1);
      }
    }

    // Attributes
    for(size_t attribIdx = 0; attribIdx < attribDataArrays.size(); ++attribIdx)
    {
      size_t srcIdx = attributeArray[attribIdx].PerPrimData ? primIdx : segStart;
      size_t destIdx = tempArrays->expandAttributeDataArray(attribIdx, 1);
      tempArrays->copyToAttributeDataArray(attribIdx, srcIdx, destIdx, 1);
    }
  }

#define PUSH_VERTEX(x, y) \
  pushVertex(paramData, attributeArray, tempArrays, \
    vertices, vertexType, \
    hasNormals, hasColors, hasRadii, \
    x, y)

  void reorderCurveGeometry(const UsdGeometryData& paramData, const UsdGeometry::AttributeArray& attributeArray, UsdGeometryTempArrays* tempArrays)
  {
    auto& attribDataArrays = tempArrays->AttributeDataArrays;
    assert(attribDataArrays.size() == attributeArray.size());

    const UsdDataArray* vertexArray = paramData.vertexPositions;
    uint64_t numVertices = vertexArray->getLayout().numItems1;
    const void* vertices = vertexArray->getData();
    ANARIDataType vertexType = vertexArray->getType();

    const UsdDataArray* indexArray = paramData.indices;
    uint64_t numSegments = indexArray ? indexArray->getLayout().numItems1 : numVertices-1;
    const void* indices = indexArray ? indexArray->getData() : nullptr;
    ANARIDataType indexType = indexArray ? indexArray->getType() : ANARI_UINT32;

    uint64_t maxNumVerts = numSegments*2;
    tempArrays->CurveLengths.resize(0);
    tempArrays->PointsArray.resize(0);
    tempArrays->PointsArray.reserve(maxNumVerts * 3); // Conservative max number of points
    bool hasNormals = paramData.vertexNormals || paramData.primitiveNormals;
    if (hasNormals)
    {
      tempArrays->NormalsArray.resize(0);
      tempArrays->NormalsArray.reserve(maxNumVerts * 3);
    }
    bool hasColors = paramData.vertexColors || paramData.primitiveColors;
    if (hasColors)
    {
      tempArrays->resetColorsArray(0, paramData.vertexColors ? paramData.vertexColors->getType() : paramData.primitiveColors->getType());
      tempArrays->reserveColorsArray(maxNumVerts);
    }
    bool hasRadii = paramData.vertexRadii || paramData.primitiveRadii;
    if (hasRadii)
    {
      tempArrays->ScalesArray.resize(0);
      tempArrays->ScalesArray.reserve(maxNumVerts);
    }
    for(size_t attribIdx = 0; attribIdx < attribDataArrays.size(); ++attribIdx)
    {
      tempArrays->resetAttributeDataArray(attribIdx, 0);
      tempArrays->reserveAttributeDataArray(attribIdx, maxNumVerts);
    }

    size_t prevSegEnd = 0;
    int curveLength = 0;
    for (size_t primIdx = 0; primIdx < numSegments; ++primIdx)
    {
      size_t segStart = indices ? getIndex(indices, indexType, primIdx) : primIdx;

      if (primIdx != 0 && prevSegEnd != segStart)
      {
        PUSH_VERTEX(prevSegEnd, primIdx - 1);
        curveLength += 1;
        tempArrays->CurveLengths.push_back(curveLength);
        curveLength = 0;
      }

      assert(segStart+1 < numVertices); // begin and end vertex should be in range

      PUSH_VERTEX(segStart, primIdx);
      curveLength += 1;

      prevSegEnd = segStart + 1;
    }
    if (curveLength != 0)
    {
      PUSH_VERTEX(prevSegEnd, numSegments - 1);
      curveLength += 1;
      tempArrays->CurveLengths.push_back(curveLength);
    }
  }

  template<typename T>
  void setInstancerDataArray(const char* arrayName, const UsdDataArray* vertArray, const UsdDataArray* primArray, const std::vector<T>& tmpArray, UsdBridgeType tmpArrayType,
    void const*& instancerDataArray, UsdBridgeType& instancerDataArrayType, const UsdGeometryData& paramData, const UsdGeometryDebugData& dbgData)
  {
    // Normals
    if (paramData.indices && tmpArray.size())
    {
      instancerDataArray = tmpArray.data();
      instancerDataArrayType = tmpArrayType;
    }
    else
    {
      const UsdDataArray* normals = vertArray;
      if (normals)
      {
        instancerDataArray = normals->getData();
        instancerDataArrayType = AnariToUsdBridgeType(normals->getType());
      }
      else if(primArray)
      {
        dbgData.device->reportStatus(dbgData.geometry, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
          "UsdGeometry '%s' primitive.%s not transferred: per-primitive arrays provided without setting primitive.index", dbgData.debugName, arrayName);
      }
    }
  }
}

UsdGeometry::UsdGeometry(const char* name, const char* type, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_GEOMETRY, name, device)
{
  bool createTempArrays = false;

  geomType = GetGeomType(type);

  if(isInstanced() || geomType == GEOM_CURVE)
    createTempArrays = true;

  if(geomType == GEOM_UNKNOWN)
    device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' construction failed: type %s not supported", getName(), type);

  if(createTempArrays)
    tempArrays = std::make_unique<UsdGeometryTempArrays>(attributeArray);
}

UsdGeometry::~UsdGeometry()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(cachedBridge)
    cachedBridge->DeleteGeometry(usdHandle);
#endif
}

void UsdGeometry::remove(UsdDevice* device)
{
  applyRemoveFunc(device, &UsdBridge::DeleteGeometry);
}

void UsdGeometry::filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device)
{
  if(geomType == GEOM_GLYPH && strEquals(name, "shapeType") || strEquals(name, "shapeGeometry") || strEquals(name, "shapeTransform"))
    protoShapeChanged = true;

  if(usdHandle.value && strEquals(name, "usd::useUsdGeomPoints"))
  {
    device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_WARNING, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' filterSetParam failed: 'usd::useUsdGeomPoints' parameter cannot be changed after the first commit", getName());
    return;
  }

  // In case of an attribute name change, notify observers (such as the surface, to change material/sampler bindings)
  static const char* attribNamePrefix = "usd::attribute";
  if(strcmp(name, attribNamePrefix) > 0)
  {
    static const char* attribNamePostfix = ".name";
    size_t paramOffset = strlen(name) - strlen(attribNamePostfix);
    if(strcmp(name+paramOffset, attribNamePostfix) == 0)
      hasNewAttribName = true; // Only notify once the commit takes place
  }

  BridgedBaseObjectType::filterSetParam(name, type, mem, device);
}

void UsdGeometry::commit(UsdDevice* device)
{
  // Make sure to notify observers of attrib array name changes before the actual commit (doCommitData may have a locked commitlist)
  if(hasNewAttribName)
  {
    notify(this, device);
    hasNewAttribName = false;
  }

  // Continue with the normal commit, ie. adding to the commit list or immediate execution of doCommitData
  BridgedBaseObjectType::commit(device);
}

template<typename GeomDataType>
void UsdGeometry::setAttributeTimeVarying(typename GeomDataType::DataMemberId& timeVarying)
{
  typedef typename GeomDataType::DataMemberId DMI;
  const UsdGeometryData& paramData = getReadParams();

  static constexpr int attribStartBit = static_cast<int>(UsdGeometryComponents::ATTRIBUTE0);

  for(size_t attribIdx = 0; attribIdx < attributeArray.size(); ++attribIdx)
  {
    DMI dmiAttribBit = ubutils::GetAttribBit<DMI>(attribIdx);

    timeVarying = timeVarying &
      (isBitSet(paramData.timeVarying, attribStartBit+(int)attribIdx) ? DMI::ALL : ~dmiAttribBit);
  }
}

void UsdGeometry::syncAttributeArrays()
{
  const UsdGeometryData& paramData = getReadParams();

  // Find the max index of the last attribute that still contains an array
  int attribCount = 0;
  for(int i = 0; i < MAX_ATTRIBS; ++i)
  {
    if(paramData.primitiveAttributes[i] != nullptr || paramData.vertexAttributes[i] != nullptr)
      attribCount = i+1;
  }

  // Set the attribute arrays and related info, resize temporary attribute array data for reordering
  if(attribCount)
  {
    attributeArray.resize(attribCount);
    for(int i = 0; i < attribCount; ++i)
    {
      const UsdDataArray* attribArray = paramData.vertexAttributes[i] ? paramData.vertexAttributes[i] : paramData.primitiveAttributes[i];
      if (attribArray)
      {
        attributeArray[i].Data = attribArray->getData();
        attributeArray[i].DataType = AnariToUsdBridgeType(attribArray->getType());
        attributeArray[i].PerPrimData = paramData.vertexAttributes[i] ? false : true;
        attributeArray[i].EltSize = static_cast<uint32_t>(anari::sizeOf(attribArray->getType()));
        attributeArray[i].Name = UsdSharedString::c_str(paramData.attributeNames[i]);
      }
      else
      {
        attributeArray[i].Data = nullptr;
        attributeArray[i].DataType = UsdBridgeType::UNDEFINED;
      }
    }

    if(tempArrays)
      tempArrays->AttributeDataArrays.resize(attribCount);
  }
}

template<typename GeomDataType>
void UsdGeometry::copyAttributeArraysToData(GeomDataType& geomData)
{
  geomData.Attributes = attributeArray.data();
  geomData.NumAttributes = static_cast<uint32_t>(attributeArray.size());
}

void UsdGeometry::assignTempDataToAttributes(bool perPrimInterpolation)
{
  const AttributeDataArraysType& attribDataArrays = tempArrays->AttributeDataArrays;
  assert(attributeArray.size() == attribDataArrays.size());
  for(size_t attribIdx = 0; attribIdx < attribDataArrays.size(); ++attribIdx)
  {
    if(attribDataArrays[attribIdx].size()) // Always > 0 if attributeArray[attribIdx].Data is set
      attributeArray[attribIdx].Data = attribDataArrays[attribIdx].data();

    attributeArray[attribIdx].PerPrimData = perPrimInterpolation; // Already converted to per-vertex (or per-prim)
  }
}

void UsdGeometry::initializeGeomData(UsdBridgeMeshData& geomData)
{
  typedef UsdBridgeMeshData::DataMemberId DMI;
  const UsdGeometryData& paramData = getReadParams();

  geomData.TimeVarying = DMI::ALL
    & (isTimeVarying(CType::POSITION) ? DMI::ALL : ~DMI::POINTS)
    & (isTimeVarying(CType::NORMAL) ? DMI::ALL : ~DMI::NORMALS)
    & (isTimeVarying(CType::COLOR) ? DMI::ALL : ~DMI::COLORS)
    & (isTimeVarying(CType::INDEX) ? DMI::ALL : ~DMI::INDICES);
  setAttributeTimeVarying<UsdBridgeMeshData>(geomData.TimeVarying);

  geomData.FaceVertexCount = geomType == GEOM_QUAD ? 4 : 3;
}

void UsdGeometry::initializeGeomData(UsdBridgeInstancerData& geomData)
{
  typedef UsdBridgeInstancerData::DataMemberId DMI;
  const UsdGeometryData& paramData = getReadParams();

  geomData.TimeVarying = DMI::ALL
    & (isTimeVarying(CType::POSITION) ? DMI::ALL : ~DMI::POINTS)
    & (( ((geomType == GEOM_CYLINDER || geomType == GEOM_CONE)
            && (isTimeVarying(CType::NORMAL) || isTimeVarying(CType::POSITION) || isTimeVarying(CType::INDEX)))
        || ((geomType == GEOM_GLYPH) && isTimeVarying(CType::ORIENTATION))
      ) ? DMI::ALL : ~DMI::ORIENTATIONS)
    & (isTimeVarying(CType::SCALE) ? DMI::ALL : ~DMI::SCALES)
    & (isTimeVarying(CType::INDEX) ? DMI::ALL : ~DMI::INVISIBLEIDS)
    & (isTimeVarying(CType::COLOR) ? DMI::ALL : ~DMI::COLORS)
    & (isTimeVarying(CType::ID) ? DMI::ALL : ~DMI::INSTANCEIDS)
    & ~DMI::SHAPEINDICES; // Shapeindices are always the same, and USD clients typically do not support timevarying shapes
  setAttributeTimeVarying<UsdBridgeInstancerData>(geomData.TimeVarying);

  geomData.UseUsdGeomPoints = geomType == GEOM_SPHERE && paramData.UseUsdGeomPoints;
}

void UsdGeometry::initializeGeomData(UsdBridgeCurveData& geomData)
{
  typedef UsdBridgeCurveData::DataMemberId DMI;
  const UsdGeometryData& paramData = getReadParams();

  // Turn off what is not timeVarying
  geomData.TimeVarying = DMI::ALL
    & (isTimeVarying(CType::POSITION) ? DMI::ALL : ~DMI::POINTS)
    & (isTimeVarying(CType::NORMAL) ? DMI::ALL : ~DMI::NORMALS)
    & (isTimeVarying(CType::SCALE) ? DMI::ALL : ~DMI::SCALES)
    & (isTimeVarying(CType::COLOR) ? DMI::ALL : ~DMI::COLORS)
    & ((isTimeVarying(CType::POSITION) || isTimeVarying(CType::INDEX)) ? DMI::ALL : ~DMI::CURVELENGTHS);
  setAttributeTimeVarying<UsdBridgeCurveData>(geomData.TimeVarying);
}

void UsdGeometry::initializeGeomRefData(UsdBridgeInstancerRefData& geomRefData)
{
  const UsdGeometryData& paramData = getReadParams();

  // The anari side currently only supports only one shape, so just set DefaultShape
  bool isGlyph = geomType == GEOM_GLYPH;
  if(isGlyph && paramData.shapeGeometry)
    geomRefData.DefaultShape = UsdBridgeInstancerRefData::SHAPE_MESH;
  else
  {
    UsdGeometry::GeomType defaultShape = (isGlyph && paramData.shapeType) ? GetGeomType(paramData.shapeType->c_str()) : geomType;
    switch (defaultShape)
    {
      case GEOM_CYLINDER:
        geomRefData.DefaultShape = UsdBridgeInstancerRefData::SHAPE_CYLINDER;
        break;
      case GEOM_CONE:
        geomRefData.DefaultShape = UsdBridgeInstancerRefData::SHAPE_CONE;
        break;
      default:
        geomRefData.DefaultShape = UsdBridgeInstancerRefData::SHAPE_SPHERE;
        break;
    };
  }

  geomRefData.ShapeTransform = paramData.shapeTransform;
}

bool UsdGeometry::checkArrayConstraints(const UsdDataArray* vertexArray, const UsdDataArray* primArray,
  const char* paramName, UsdDevice* device, const char* debugName, int attribIndex)
{
  const UsdGeometryData& paramData = getReadParams();

  UsdLogInfo logInfo(device, this, ANARI_GEOMETRY, debugName);

  const UsdDataArray* vertices = paramData.vertexPositions;
  const UsdDataLayout& vertLayout = vertices->getLayout();

  const UsdDataArray* indices = paramData.indices;
  const UsdDataLayout& indexLayout = indices ? indices->getLayout() : vertLayout;

  const UsdDataLayout& perVertLayout = vertexArray ? vertexArray->getLayout() : vertLayout;
  const UsdDataLayout& perPrimLayout = primArray ? primArray->getLayout() : indexLayout;

  const UsdDataLayout& attrLayout = vertexArray ? perVertLayout : perPrimLayout;

  if (!AssertOneDimensional(attrLayout, logInfo, paramName)
    || !AssertNoStride(attrLayout, logInfo, paramName)
    )
  {
    return false;
  }

  if (vertexArray && vertexArray->getLayout().numItems1 < vertLayout.numItems1)
  {
    if(attribIndex == -1)
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: all 'vertex.X' array elements should at least be the size of vertex.positions", debugName);
    else
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: all 'vertex.attribute%i' array elements should at least be the size of vertex.positions", debugName, attribIndex);
    return false;
  }

  uint64_t numPrims = GetNumberOfPrims(indices, indexLayout, geomType);
  if (primArray && primArray->getLayout().numItems1 < numPrims)
  {
    if(attribIndex == -1)
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: size of 'primitive.X' array too small", debugName);
    else
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: size of 'primitive.attribute%i' array too small", debugName, attribIndex);
    return false;
  }

  return true;
}

bool UsdGeometry::checkGeomParams(UsdDevice* device)
{
  const UsdGeometryData& paramData = getReadParams();

  const char* debugName = getName();

  bool success = true;

  success = success && checkArrayConstraints(paramData.vertexPositions, nullptr, "vertex.position", device, debugName);
  success = success && checkArrayConstraints(nullptr, paramData.indices, "primitive.index", device, debugName);

  success = success && checkArrayConstraints(paramData.vertexNormals, paramData.primitiveNormals, "vertex/primitive.normal", device, debugName);
  for(int i = 0; i < MAX_ATTRIBS; ++i)
    success = success && checkArrayConstraints(paramData.vertexAttributes[i], paramData.primitiveAttributes[i], "vertex/primitive.attribute", device, debugName, i);
  success = success && checkArrayConstraints(paramData.vertexColors, paramData.primitiveColors, "vertex/primitive.color", device, debugName);
  success = success && checkArrayConstraints(paramData.vertexRadii, paramData.primitiveRadii, "vertex/primitive.radius", device, debugName);
  success = success && checkArrayConstraints(paramData.vertexScales, paramData.primitiveScales, "vertex/primitive.scale", device, debugName);
  success = success && checkArrayConstraints(paramData.vertexOrientations, paramData.primitiveOrientations, "vertex/primitive.orientation", device, debugName);
  success = success && checkArrayConstraints(nullptr, paramData.primitiveIds, "primitive.id", device, debugName);

  if (!success)
    return false;

  ANARIDataType vertType = paramData.vertexPositions->getType();
  if (vertType != ANARI_FLOAT32_VEC3 && vertType != ANARI_FLOAT64_VEC3)
  {
    device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'vertex.position' parameter should be of type ANARI_FLOAT32_VEC3 or ANARI_FLOAT64_VEC3.", debugName);
    return false;
  }

  if (paramData.indices)
  {
    ANARIDataType indexType = paramData.indices->getType();

    UsdBridgeType flattenedType = AnariToUsdBridgeType_Flattened(indexType);
    if( (geomType == GEOM_TRIANGLE || geomType == GEOM_QUAD) &&
      (flattenedType == UsdBridgeType::UINT || flattenedType == UsdBridgeType::ULONG || flattenedType == UsdBridgeType::LONG))
    {
      static bool reported = false; // Hardcode this to show only once to make sure developers get to see it, without spamming the console.
      if(!reported)
      {
        device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_WARNING, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' has 'primitive.index' of type other than ANARI_INT32, which may result in an overflow for FaceVertexIndicesAttr of UsdGeomMesh.", debugName);
        reported = true;
      }
    }

    if (geomType == GEOM_SPHERE || geomType == GEOM_CURVE || geomType == GEOM_GLYPH)
    {
      if(geomType == GEOM_SPHERE && paramData.UseUsdGeomPoints)
        device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_WARNING, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' is a sphere geometry with indices, but the usd::useUsdGeomPoints parameter is not set, so all vertices will show as spheres.", debugName);

      if (indexType != ANARI_INT32 && indexType != ANARI_UINT32 && indexType != ANARI_INT64 && indexType != ANARI_UINT64)
      {
        device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'primitive.index' parameter should be of type ANARI_(U)INT32/64.", debugName);
        return false;
      }
    }
    else if (geomType == GEOM_CYLINDER || geomType == GEOM_CONE)
    {
      if (indexType != ANARI_UINT32_VEC2 && indexType != ANARI_INT32_VEC2 && indexType != ANARI_UINT64_VEC2 && indexType != ANARI_INT64_VEC2)
      {
        device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'primitive.index' parameter should be of type ANARI_(U)INT_VEC2.", debugName);
        return false;
      }
    }
    else if (geomType == GEOM_TRIANGLE)
    {
      if (indexType != ANARI_UINT32_VEC3 && indexType != ANARI_INT32_VEC3 && indexType != ANARI_UINT64_VEC3 && indexType != ANARI_INT64_VEC3)
      {
        device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'primitive.index' parameter should be of type ANARI_(U)INT_VEC3.", debugName);
        return false;
      }
    }
    else if (geomType == GEOM_QUAD)
    {
      if (indexType != ANARI_UINT32_VEC4 && indexType != ANARI_INT32_VEC4 && indexType != ANARI_UINT64_VEC4 && indexType != ANARI_INT64_VEC4)
      {
        device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'primitive.index' parameter should be of type ANARI_(U)INT_VEC4.", debugName);
        return false;
      }
    }
  }

  const UsdDataArray* normals = paramData.vertexNormals ? paramData.vertexNormals : paramData.primitiveNormals;
  if (normals)
  {
    ANARIDataType arrayType = normals->getType();
    if (arrayType != ANARI_FLOAT32_VEC3 && arrayType != ANARI_FLOAT64_VEC3)
    {
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'vertex/primitive.normal' parameter should be of type ANARI_FLOAT32_VEC3 or ANARI_FLOAT64_VEC3.", debugName);
      return false;
    }
  }

  const UsdDataArray* colors = paramData.vertexColors ? paramData.vertexColors : paramData.primitiveColors;
  if (colors)
  {
    ANARIDataType arrayType = colors->getType();
    if ((int)arrayType < (int)ANARI_INT8 || (int)arrayType > (int)ANARI_UFIXED8_R_SRGB)
    {
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'vertex/primitive.color' parameter should be of Color type (see ANARI standard)", debugName);
      return false;
    }
  }

  const UsdDataArray* radii = paramData.vertexRadii ? paramData.vertexRadii : paramData.primitiveRadii;
  if (radii)
  {
    ANARIDataType arrayType = radii->getType();
    if (arrayType != ANARI_FLOAT32 && arrayType != ANARI_FLOAT64)
    {
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'vertex/primitive.radius' parameter should be of type ANARI_FLOAT32 or ANARI_FLOAT64.", debugName);
      return false;
    }
  }

  const UsdDataArray* scales = paramData.vertexScales ? paramData.vertexScales : paramData.primitiveScales;
  if (scales)
  {
    ANARIDataType arrayType = scales->getType();
    if (arrayType != ANARI_FLOAT32 && arrayType != ANARI_FLOAT64 && arrayType != ANARI_FLOAT32_VEC3 && arrayType != ANARI_FLOAT64_VEC3)
    {
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'vertex/primitive.scale' parameter should be of type ANARI_FLOAT32(_VEC3) or ANARI_FLOAT64(_VEC3).", debugName);
      return false;
    }
  }

  const UsdDataArray* orientations = paramData.vertexOrientations ? paramData.vertexOrientations : paramData.primitiveOrientations;
  if (orientations)
  {
    ANARIDataType arrayType = orientations->getType();
    if (arrayType != ANARI_FLOAT32_QUAT_IJKW)
    {
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'vertex/primitive.orientation' parameter should be of type ANARI_FLOAT32_QUAT_IJKW.", debugName);
      return false;
    }
  }

  if (paramData.primitiveIds)
  {
    ANARIDataType idType = paramData.primitiveIds->getType();
    if (idType != ANARI_INT32 && idType != ANARI_UINT32 && idType != ANARI_INT64 && idType != ANARI_UINT64)
    {
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'primitive.id' parameter should be of type ANARI_(U)INT or ANARI_(U)LONG.", debugName);
      return false;
    }
  }

  return true;
}

void UsdGeometry::updateGeomData(UsdDevice* device, UsdBridge* usdBridge, UsdBridgeMeshData& meshData, bool isNew)
{
  const UsdGeometryData& paramData = getReadParams();

  const UsdDataArray* vertices = paramData.vertexPositions;
  meshData.NumPoints = vertices->getLayout().numItems1;
  meshData.Points = vertices->getData();
  meshData.PointsType = AnariToUsdBridgeType(vertices->getType());

  const UsdDataArray* normals = paramData.vertexNormals ? paramData.vertexNormals : paramData.primitiveNormals;
  if (normals)
  {
    meshData.Normals = normals->getData();
    meshData.NormalsType = AnariToUsdBridgeType(normals->getType());
    meshData.PerPrimNormals = paramData.vertexNormals ? false : true;
  }
  const UsdDataArray* colors = paramData.vertexColors ? paramData.vertexColors : paramData.primitiveColors;
  if (colors)
  {
    meshData.Colors = colors->getData();
    meshData.ColorsType = AnariToUsdBridgeType(colors->getType());
    meshData.PerPrimColors = paramData.vertexColors ? false : true;
  }

  const UsdDataArray* indices = paramData.indices;
  if (indices)
  {
    ANARIDataType indexType = indices->getType();
    meshData.NumIndices = indices->getLayout().numItems1 * anari::componentsOf(indexType);
    meshData.Indices = indices->getData();
    meshData.IndicesType = AnariToUsdBridgeType_Flattened(indexType);
  }
  else
  {
    meshData.NumIndices = meshData.NumPoints; // Vertices are implicitly indexed consecutively (FaceVertexCount determines how many prims)
  }

  //meshData.UpdatesToPerform = Still to be implemented

  double worldTimeStep = device->getReadParams().timeStep;
  double dataTimeStep = selectObjTime(paramData.timeStep, worldTimeStep);
  usdBridge->SetGeometryData(usdHandle, meshData, dataTimeStep);
}

void UsdGeometry::updateGeomData(UsdDevice* device, UsdBridge* usdBridge, UsdBridgeInstancerData& instancerData, bool isNew)
{
  const UsdGeometryData& paramData = getReadParams();
  const char* debugName = getName();

  if (geomType == GEOM_SPHERE || geomType == GEOM_GLYPH)
  {
    // A paramData.indices (primitive-indexed spheres) array, is not supported in USD, also duplicate spheres make no sense.
    // Instead, Ids/InvisibleIds are assigned to emulate sparsely indexed spheres (sourced from paramData.primitiveIds if available),
    // with the per-vertex arrays remaining intact. Any per-prim arrays are explicitly converted to per-vertex via the tempArrays.
    generateIndexedSphereData(paramData, attributeArray, tempArrays.get());

    const UsdDataArray* vertices = paramData.vertexPositions;
    instancerData.NumPoints = vertices->getLayout().numItems1;
    instancerData.Points = vertices->getData();
    instancerData.PointsType = AnariToUsdBridgeType(vertices->getType());

    UsdGeometryDebugData dbgData = { device, this, debugName };

    // Orientations
    // are a bit extended beyond the spec: even spheres can set them, and the use of normals is also supported
    if(paramData.vertexOrientations || paramData.primitiveOrientations)
    {
      setInstancerDataArray("orientation", paramData.vertexOrientations, paramData.primitiveOrientations, tempArrays->OrientationsArray, UsdBridgeType::FLOAT4,
        instancerData.Orientations, instancerData.OrientationsType,
        paramData, dbgData);
    }
    else
    {
      setInstancerDataArray("normal", paramData.vertexNormals, paramData.primitiveNormals, tempArrays->NormalsArray, UsdBridgeType::FLOAT3,
        instancerData.Orientations, instancerData.OrientationsType,
        paramData, dbgData);
    }

    // Scales
    if(geomType == GEOM_SPHERE)
    {
      setInstancerDataArray("radius", paramData.vertexRadii, paramData.primitiveRadii,
        tempArrays->RadiiArray, UsdBridgeType::FLOAT,
        instancerData.Scales, instancerData.ScalesType,
        paramData, dbgData);
    }
    else
    {
      size_t numTmpScales = tempArrays->ScalesArray.size();
      bool scalarScale = (numTmpScales == instancerData.NumPoints);
      assert(!numTmpScales || scalarScale || numTmpScales == instancerData.NumPoints*3);

      setInstancerDataArray("scale", paramData.vertexScales, paramData.primitiveScales,
        tempArrays->ScalesArray, scalarScale ? UsdBridgeType::FLOAT : UsdBridgeType::FLOAT3,
        instancerData.Scales, instancerData.ScalesType,
        paramData, dbgData);
    }

    // Colors
    setInstancerDataArray("color", paramData.vertexColors, paramData.primitiveColors,
      tempArrays->ColorsArray, AnariToUsdBridgeType(tempArrays->ColorsArrayType),
      instancerData.Colors, instancerData.ColorsType,
      paramData, dbgData);

    // Attributes
    // By default, syncAttributeArrays and initializeGeomData already set up instancerData.Attributes
    // Just set attributeArray's data to tempArrays where necessary
    if(paramData.indices)
    {
      // Type remains the same, everything per-vertex (as explained above)
      assignTempDataToAttributes(false);
    }
    else
    {
      // Check whether any perprim attributes exist
      for(size_t attribIdx = 0; attribIdx < attributeArray.size(); ++attribIdx)
      {
        if(attributeArray[attribIdx].Data && attributeArray[attribIdx].PerPrimData)
        {
          attributeArray[attribIdx].Data = nullptr;
          device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
            "UsdGeometry '%s' primitive.attribute%i not transferred: per-primitive arrays provided without setting primitive.index", debugName, static_cast<int>(attribIdx));
        }
      }
    }

    // Ids
    if (paramData.indices && tempArrays->IdsArray.size())
    {
      instancerData.InstanceIds = tempArrays->IdsArray.data();
      instancerData.InstanceIdsType = UsdBridgeType::LONG;
    }
    else
    {
      const UsdDataArray* ids = paramData.primitiveIds;
      if (ids)
      {
        instancerData.InstanceIds = ids->getData();
        instancerData.InstanceIdsType = AnariToUsdBridgeType(ids->getType());
      }
    }

    // Invisible Ids
    if (paramData.indices && tempArrays->InvisIdsArray.size())
    {
      instancerData.InvisibleIds = tempArrays->InvisIdsArray.data();
      instancerData.InvisibleIdsType = UsdBridgeType::LONG;
      instancerData.NumInvisibleIds = tempArrays->InvisIdsArray.size();
    }

    for(int i = 0; i < 3; ++i)
      instancerData.Scale.Data[i] = (geomType == GEOM_SPHERE) ? paramData.radiusConstant : paramData.scaleConstant.Data[i];

    instancerData.Orientation = paramData.orientationConstant;
  }
  else
  {
    convertLinesToSticks(paramData, attributeArray, tempArrays.get());

    instancerData.NumPoints = tempArrays->PointsArray.size()/3;
    if (instancerData.NumPoints > 0)
    {
      instancerData.Points = tempArrays->PointsArray.data();
      instancerData.PointsType = UsdBridgeType::FLOAT3;
      instancerData.Scales = tempArrays->ScalesArray.data();
      instancerData.ScalesType = UsdBridgeType::FLOAT3;
      instancerData.Orientations = tempArrays->OrientationsArray.data();
      instancerData.OrientationsType = UsdBridgeType::FLOAT4;

      // Colors
      if (tempArrays->ColorsArray.size())
      {
        instancerData.Colors = tempArrays->ColorsArray.data();
        instancerData.ColorsType = AnariToUsdBridgeType(tempArrays->ColorsArrayType);
      }
      else if(const UsdDataArray* colors = paramData.primitiveColors)
      { // Per-primitive color array corresponds to per-vertex stick output
        instancerData.Colors = colors->getData();
        instancerData.ColorsType = AnariToUsdBridgeType(colors->getType());
      }

      // Attributes
      assignTempDataToAttributes(false);

      // Ids
      if (tempArrays->IdsArray.size())
      {
        instancerData.InstanceIds = tempArrays->IdsArray.data();
        instancerData.InstanceIdsType = UsdBridgeType::LONG;
      }
    }
  }

  double worldTimeStep = device->getReadParams().timeStep;
  double dataTimeStep = selectObjTime(paramData.timeStep, worldTimeStep);
  usdBridge->SetGeometryData(usdHandle, instancerData, dataTimeStep);

  if(isNew && geomType != GEOM_GLYPH && !instancerData.UseUsdGeomPoints)
    commitPrototypes(usdBridge); // Also initialize the prototype shape on the instancer geom
}

void UsdGeometry::updateGeomData(UsdDevice* device, UsdBridge* usdBridge, UsdBridgeCurveData& curveData, bool isNew)
{
  const UsdGeometryData& paramData = getReadParams();

  reorderCurveGeometry(paramData, attributeArray, tempArrays.get());

  curveData.NumPoints = tempArrays->PointsArray.size() / 3;
  if (curveData.NumPoints > 0)
  {
    curveData.Points = tempArrays->PointsArray.data();
    curveData.PointsType = UsdBridgeType::FLOAT3;
    curveData.CurveLengths = tempArrays->CurveLengths.data();
    curveData.NumCurveLengths = tempArrays->CurveLengths.size();

    if (tempArrays->NormalsArray.size())
    {
      curveData.Normals = tempArrays->NormalsArray.data();
      curveData.NormalsType = UsdBridgeType::FLOAT3;
    }
    curveData.PerPrimNormals = false;// Always vertex colored as per reorderCurveGeometry. One entry per whole curve would be useless

    // Attributes
    assignTempDataToAttributes(false);

    // Copy colors
    if (tempArrays->ColorsArray.size())
    {
      curveData.Colors = tempArrays->ColorsArray.data();
      curveData.ColorsType = AnariToUsdBridgeType(tempArrays->ColorsArrayType);
    }
    curveData.PerPrimColors = false; // Always vertex colored as per reorderCurveGeometry. One entry per whole curve would be useless

    // Assign scales
    if (tempArrays->ScalesArray.size())
    {
      curveData.Scales = tempArrays->ScalesArray.data();
      curveData.ScalesType = UsdBridgeType::FLOAT;
    }
    curveData.UniformScale = paramData.radiusConstant;
  }

  double worldTimeStep = device->getReadParams().timeStep;
  double dataTimeStep = selectObjTime(paramData.timeStep, worldTimeStep);
  usdBridge->SetGeometryData(usdHandle, curveData, dataTimeStep);
}

template<typename UsdGeomType>
bool UsdGeometry::commitTemplate(UsdDevice* device)
{
  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdGeometryData& paramData = getReadParams();
  const char* debugName = getName();

  UsdGeomType geomData;

  syncAttributeArrays();
  initializeGeomData(geomData);
  copyAttributeArraysToData(geomData);

  bool isNew = false;
  if (!usdHandle.value)
  {
    isNew = usdBridge->CreateGeometry(debugName, usdHandle, geomData);
  }

  if (paramChanged || isNew)
  {
    if (paramData.vertexPositions)
    {
      if(checkGeomParams(device))
        updateGeomData(device, usdBridge, geomData, isNew);
    }
    else
    {
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: missing 'vertex.position'.", debugName);
    }

    paramChanged = false;
  }

  return isNew;
}

void UsdGeometry::commitPrototypes(UsdBridge* usdBridge)
{
  UsdBridgeInstancerRefData instancerRefData;
  initializeGeomRefData(instancerRefData);
  usdBridge->SetPrototypeData(usdHandle, instancerRefData);
}

bool UsdGeometry::deferCommit(UsdDevice* device)
{
  return false;
}

bool UsdGeometry::doCommitData(UsdDevice* device)
{
  if(geomType == GEOM_UNKNOWN)
    return false;

  bool isNew = false;
  switch (geomType)
  {
    case GEOM_TRIANGLE: isNew = commitTemplate<UsdBridgeMeshData>(device); break;
    case GEOM_QUAD: isNew = commitTemplate<UsdBridgeMeshData>(device); break;
    case GEOM_SPHERE: isNew = commitTemplate<UsdBridgeInstancerData>(device); break;
    case GEOM_CYLINDER: isNew = commitTemplate<UsdBridgeInstancerData>(device); break;
    case GEOM_CONE: isNew = commitTemplate<UsdBridgeInstancerData>(device); break;
    case GEOM_GLYPH: isNew = commitTemplate<UsdBridgeInstancerData>(device); break;
    case GEOM_CURVE: isNew = commitTemplate<UsdBridgeCurveData>(device); break;
    default: break;
  }

  return geomType == GEOM_GLYPH && (isNew || protoShapeChanged); // Defer commit of prototypes until the geometry refs are in place
}

void UsdGeometry::doCommitRefs(UsdDevice* device)
{
  assert(geomType == GEOM_GLYPH && protoShapeChanged);

  UsdBridge* usdBridge = device->getUsdBridge();
  const UsdGeometryData& paramData = getReadParams();

  double worldTimeStep = device->getReadParams().timeStep;

  // Make sure the references are updated on the Bridge side.
  if (paramData.shapeGeometry)
  {
    double geomObjTimeStep = paramData.shapeGeometry->getReadParams().timeStep;

    UsdGeometryHandle protoGeomHandle = paramData.shapeGeometry->getUsdHandle();
    size_t numHandles = 1;

    double protoTimestep = selectRefTime(paramData.shapeGeometryRefTimeStep, geomObjTimeStep, worldTimeStep);

    usdBridge->SetPrototypeRefs(usdHandle,
      &protoGeomHandle,
      numHandles,
      worldTimeStep,
      &protoTimestep
    );
  }
  else
  {
    usdBridge->DeletePrototypeRefs(usdHandle, worldTimeStep);
  }

  // Now set the prototype relations to the reference paths (Bridge uses paths from SetPrototypeRefs)
  commitPrototypes(usdBridge);

  protoShapeChanged = false;
}
