// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdGeometry.h"
#include "UsdBridge/UsdBridge.h"
#include "UsdDataArray.h"
#include "UsdAnari.h"
#include "UsdDevice.h"

#include <cmath>

DEFINE_PARAMETER_MAP(UsdGeometry,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
  REGISTER_PARAMETER_MACRO("usd::timestep", ANARI_FLOAT64, timeStep)
  REGISTER_PARAMETER_MACRO("usd::timevarying", ANARI_INT32, timeVarying)
  REGISTER_PARAMETER_MACRO("usd::usepointinstancer", ANARI_INT32, UseUsdGeomPointInstancer) // Use UsdGeomPointInstancer instead of UsdGeomPoints
  REGISTER_PARAMETER_MACRO("primitive.index", ANARI_ARRAY, indices)
  REGISTER_PARAMETER_MACRO("primitive.normal", ANARI_ARRAY, primitiveNormals)
  REGISTER_PARAMETER_MACRO("primitive.texcoord", ANARI_ARRAY, primitiveTexCoords)
  REGISTER_PARAMETER_MACRO("primitive.attribute0", ANARI_ARRAY, primitiveTexCoords)
  REGISTER_PARAMETER_MACRO("primitive.color", ANARI_ARRAY, primitiveColors)
  REGISTER_PARAMETER_MACRO("primitive.radius", ANARI_ARRAY, primitiveRadii)
  REGISTER_PARAMETER_MACRO("primitive.id", ANARI_ARRAY, primitiveIds)
  REGISTER_PARAMETER_MACRO("vertex.position", ANARI_ARRAY, vertexPositions)
  REGISTER_PARAMETER_MACRO("vertex.normal", ANARI_ARRAY, vertexNormals)
  REGISTER_PARAMETER_MACRO("vertex.texcoord", ANARI_ARRAY, vertexTexCoords)
  REGISTER_PARAMETER_MACRO("vertex.attribute0", ANARI_ARRAY, vertexTexCoords)
  REGISTER_PARAMETER_MACRO("vertex.color", ANARI_ARRAY, vertexColors)
  REGISTER_PARAMETER_MACRO("vertex.radius", ANARI_ARRAY, vertexRadii)
  REGISTER_PARAMETER_MACRO("radius", ANARI_FLOAT32, radiusConstant)
) // See .h for usage.


namespace
{
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
      result[0] = vertf[idx * 3];
      result[1] = vertf[idx * 3 + 1];
      result[2] = vertf[idx * 3 + 2];
      result[3] = vertf[idx * 3 + 3];
    }
    else if (type == ANARI_FLOAT64_VEC4)
    {
      const double* vertd = reinterpret_cast<const double*>(vertices);
      result[0] = (float)vertd[idx * 3];
      result[1] = (float)vertd[idx * 3 + 1];
      result[2] = (float)vertd[idx * 3 + 2];
      result[3] = (float)vertd[idx * 3 + 3];
    }
  }

  void genereteIndexedSphereData(UsdGeometryData& paramData, UsdGeometry::TempArrays* tempArrays)
  {
    if (paramData.indices)
    {
      uint64_t numVertices = paramData.vertexPositions->getLayout().numItems1;

      bool perPrimNormals = !paramData.vertexNormals && paramData.primitiveNormals;
      bool perPrimScales = !paramData.vertexRadii && paramData.primitiveRadii;
      bool perPrimColors = !paramData.vertexColors && paramData.primitiveColors;
      bool perPrimTexCoords = !paramData.vertexTexCoords && paramData.primitiveTexCoords;

      tempArrays->NormalsArray.resize(perPrimNormals ? numVertices*3 : 0);
      tempArrays->ScalesArray.resize(perPrimScales ?  numVertices : 0);
      tempArrays->ColorsArray.resize(perPrimColors ?  numVertices*4 : 0);
      tempArrays->TexCoordsArray.resize(perPrimTexCoords ?  numVertices*2 : 0);
      tempArrays->IdsArray.resize(numVertices, -1); // Always filled, since indices implies necessity for invisibleIds, and therefore also an Id array

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
          getValues2(paramData.primitiveNormals->getData(), paramData.primitiveNormals->getType(), primIdx, normalsDest);
        }

        // Scales
        if (perPrimScales)
        {
          float* scalesDest = &tempArrays->ScalesArray[vertIdx];
          getValues2(paramData.primitiveRadii->getData(), paramData.primitiveRadii->getType(), primIdx, scalesDest);
        }

        // Colors 
        if (perPrimColors)
        {
          float* colorsDest = &tempArrays->ColorsArray[vertIdx * 4];
          colorsDest[3] = 0.0f;
          if (paramData.vertexColors->getType() == ANARI_FLOAT32_VEC3 || paramData.vertexColors->getType() == ANARI_FLOAT64_VEC3)
            getValues3(paramData.primitiveColors->getData(), paramData.primitiveColors->getType(), primIdx, colorsDest);
          else
            getValues4(paramData.primitiveColors->getData(), paramData.primitiveColors->getType(), primIdx, colorsDest);
        }

        // Texcoords
        if (perPrimTexCoords)
        {
          float* texcoordsDest = &tempArrays->TexCoordsArray[vertIdx * 2];
          getValues2(paramData.primitiveTexCoords->getData(), paramData.primitiveTexCoords->getType(), primIdx, texcoordsDest);
        }

        // Ids
        if (paramData.primitiveIds)
        {
          int64_t id = (int64_t)getIndex(paramData.primitiveIds->getData(), paramData.primitiveIds->getType(), primIdx);
          tempArrays->IdsArray[vertIdx] = id;
          if (id > maxId)
            maxId = id;
        }
        else
        {
          int64_t id = (int64_t)vertIdx;
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

  void convertLinesToSticks(UsdGeometryData& paramData, UsdGeometry::TempArrays* tempArrays)
  {
    const UsdDataArray* vertexArray = paramData.vertexPositions;
    uint64_t numVertices = vertexArray->getLayout().numItems1;
    const void* vertices = vertexArray->getData();
    ANARIDataType vertexType = vertexArray->getType();

    const UsdDataArray* indexArray = paramData.indices;
    uint64_t numSticks = indexArray ? indexArray->getLayout().numItems1 : numVertices;
    uint64_t numIndices = numSticks * 2;
    const void* indices = indexArray ? indexArray->getData() : nullptr;
    ANARIDataType indexType = indexArray ? indexArray->getType() : ANARI_UINT32;

    tempArrays->PointsArray.resize(numSticks * 3);
    tempArrays->ScalesArray.resize(numSticks * 3); // Scales are always present
    tempArrays->OrientationsArray.resize(numSticks * 4);  
    bool hasColors = paramData.vertexColors || paramData.primitiveColors;
    tempArrays->ColorsArray.resize(hasColors ? numSticks * 4 : 0);
    bool hasTexCoords = paramData.vertexTexCoords || paramData.primitiveTexCoords;
    tempArrays->TexCoordsArray.resize(hasTexCoords ? numSticks * 2 : 0);
    tempArrays->IdsArray.resize(paramData.primitiveIds ? numSticks : 0);

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

      // (dot(|segDir|, zAxis), cross(|segDir|, zAxis)) gives (cos(th), axis*sin(th)), 
      // but rotation is represented by cos(th/2), axis*sin(th/2), ie. half the amount of rotation.
      // So calculate (dot(|halfVec|, zAxis), cross(|halfVec|, zAxis)) instead.
      float invSegLength = 1.0f / segLength;
      float halfVec[3] = {
        segDir[0] * invSegLength,
        segDir[1] * invSegLength,
        segDir[2] * invSegLength + 1.0f
      };
      float halfNorm = sqrtf(halfVec[0] * halfVec[0] + halfVec[1] * halfVec[1] + halfVec[2] * halfVec[2]);
      if (halfNorm != 0.0f)
      {
        float invHalfNorm = 1.0f / halfNorm;
        halfVec[0] *= invHalfNorm;
        halfVec[1] *= invHalfNorm;
        halfVec[2] *= invHalfNorm;
      }

      // Cross zAxis (0,0,1) with segment direction (new Z axis) to get rotation axis * sin(angle)
      float rotAxis[3] = { -halfVec[1], halfVec[0], 0.0f };
      // Dot for cos(angle)
      float cosAngle = halfVec[2];

      if (halfNorm == 0.0f) // In this case there is a 180 degree rotation
      {
        rotAxis[1] = 1.0f; //rotAxis (0,1,0)*sin(pi/2)
        // cosAngle = cos(pi/2) = 0.0f;
      }

      tempArrays->OrientationsArray[primIdx * 4] = cosAngle;
      tempArrays->OrientationsArray[primIdx * 4 + 1] = rotAxis[0];
      tempArrays->OrientationsArray[primIdx * 4 + 2] = rotAxis[1];
      tempArrays->OrientationsArray[primIdx * 4 + 3] = rotAxis[2];

      //Colors 
      if (paramData.vertexColors)
      {
        float* colorsDest = &tempArrays->ColorsArray[primIdx * 4];
        colorsDest[3] = 0.0f;
        if (paramData.vertexColors->getType() == ANARI_FLOAT32_VEC3 || paramData.vertexColors->getType() == ANARI_FLOAT64_VEC3)
          getValues3(paramData.vertexColors->getData(), paramData.vertexColors->getType(), vertIdx0, colorsDest);
        else
          getValues4(paramData.vertexColors->getData(), paramData.vertexColors->getType(), vertIdx0, colorsDest);
      }
      else if (paramData.primitiveColors)
      {
        float* colorsDest = &tempArrays->ColorsArray[primIdx * 4];
        colorsDest[3] = 0.0f;
        if (paramData.vertexColors->getType() == ANARI_FLOAT32_VEC3 || paramData.vertexColors->getType() == ANARI_FLOAT64_VEC3)
          getValues3(paramData.primitiveColors->getData(), paramData.primitiveColors->getType(), primIdx, colorsDest);
        else
          getValues4(paramData.primitiveColors->getData(), paramData.primitiveColors->getType(), primIdx, colorsDest);
      }

      // Texcoords
      if (paramData.vertexTexCoords)
      {
        float* texcoordsDest = &tempArrays->TexCoordsArray[primIdx * 2];
        getValues2(paramData.vertexTexCoords->getData(), paramData.vertexTexCoords->getType(), vertIdx0, texcoordsDest);
      }
      else if (paramData.primitiveTexCoords)
      {
        float* texcoordsDest = &tempArrays->TexCoordsArray[primIdx * 2];
        getValues2(paramData.primitiveTexCoords->getData(), paramData.primitiveTexCoords->getType(), primIdx, texcoordsDest);
      }

      // Ids
      if (paramData.primitiveIds)
      {
        tempArrays->IdsArray[primIdx] = (int64_t)getIndex(paramData.primitiveIds->getData(), paramData.primitiveIds->getType(), primIdx);
      }
    }
  }

  void pushVertex(UsdGeometryData& paramData, UsdGeometry::TempArrays* tempArrays,
    const void* vertices, ANARIDataType vertexType,
    bool hasNormals, bool hasColors, bool hasTexCoords, bool hasRadii,
    size_t segStart, size_t primIdx)
  {
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
      float colors[4];
      colors[3] = 0.0f;
      if (paramData.vertexColors)
      {
        if (paramData.vertexColors->getType() == ANARI_FLOAT32_VEC3 || paramData.vertexColors->getType() == ANARI_FLOAT64_VEC3)
          getValues3(paramData.vertexColors->getData(), paramData.vertexColors->getType(), segStart, colors);
        else
          getValues4(paramData.vertexColors->getData(), paramData.vertexColors->getType(), segStart, colors);
      }
      else if (paramData.primitiveColors)
      {
        if (paramData.vertexColors->getType() == ANARI_FLOAT32_VEC3 || paramData.vertexColors->getType() == ANARI_FLOAT64_VEC3)
          getValues3(paramData.primitiveColors->getData(), paramData.primitiveColors->getType(), primIdx, colors);
        else
          getValues4(paramData.primitiveColors->getData(), paramData.primitiveColors->getType(), primIdx, colors);
      }

      tempArrays->ColorsArray.push_back(colors[0]);
      tempArrays->ColorsArray.push_back(colors[1]);
      tempArrays->ColorsArray.push_back(colors[2]);
      tempArrays->ColorsArray.push_back(colors[3]);
    }

    // Texcoords
    if (hasTexCoords)
    {
      float texCoords[2];
      if (paramData.vertexTexCoords)
      {
        getValues2(paramData.vertexTexCoords->getData(), paramData.vertexTexCoords->getType(), segStart, texCoords);
      }
      else if (paramData.primitiveTexCoords)
      {
        getValues2(paramData.primitiveTexCoords->getData(), paramData.primitiveTexCoords->getType(), primIdx, texCoords);
      }

      tempArrays->TexCoordsArray.push_back(texCoords[0]);
      tempArrays->TexCoordsArray.push_back(texCoords[1]);
    }
  }

#define PUSH_VERTEX(x, y) \
  pushVertex(paramData, tempArrays, \
    vertices, vertexType, \
    hasNormals, hasColors, hasTexCoords, hasRadii, \
    x, y)

  void reorderCurveGeometry(UsdGeometryData& paramData, UsdGeometry::TempArrays* tempArrays)
  {
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
      tempArrays->ColorsArray.resize(0);
      tempArrays->ColorsArray.reserve(maxNumVerts * 4);
    }
    bool hasTexCoords = paramData.vertexTexCoords || paramData.primitiveTexCoords;
    if (hasTexCoords)
    {
      tempArrays->TexCoordsArray.resize(0);
      tempArrays->TexCoordsArray.reserve(maxNumVerts * 2);
    }
    bool hasRadii = paramData.vertexRadii || paramData.primitiveRadii;
    if (hasRadii)
    {
      tempArrays->ScalesArray.resize(0);
      tempArrays->ScalesArray.reserve(maxNumVerts);
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

}

UsdGeometry::UsdGeometry(const char* name, const char* type, UsdBridge* bridge, UsdDevice* device)
  : BridgedBaseObjectType(ANARI_GEOMETRY, name, bridge)
{
  bool createTempArrays = false;

  if (strcmp(type, "sphere") == 0)
    geomType = GEOM_SPHERE;
  else if (strcmp(type, "cylinder") == 0)
  {
    geomType = GEOM_CYLINDER;
    createTempArrays = true;
  }
  else if (strcmp(type, "cone") == 0)
  {
    geomType = GEOM_CONE;
    createTempArrays = true;
  }
  else if (strcmp(type, "curve") == 0)
  {
    geomType = GEOM_CURVE;
    createTempArrays = true;
  }
  else if(strcmp(type, "triangle") == 0)
    geomType = GEOM_TRIANGLE;
  else if (strcmp(type, "quad") == 0)
    geomType = GEOM_QUAD;
  else
    device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' construction failed: type %s not supported", getName(), name);

  if(createTempArrays)
    tempArrays = std::make_unique<TempArrays>();
}

UsdGeometry::~UsdGeometry()
{
#ifdef OBJECT_LIFETIME_EQUALS_USD_LIFETIME
  if(usdBridge)
    usdBridge->DeleteGeometry(usdHandle);
#endif
}

void UsdGeometry::filterSetParam(
  const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  if(filterNameParam(name, type, mem, device))
    setParam(name, type, mem, device);
}

void UsdGeometry::filterResetParam(const char *name)
{
  resetParam(name);
}

void UsdGeometry::initializeGeomData(UsdBridgeMeshData& geomData)
{
  typedef UsdBridgeMeshData::DataMemberId DMI;
  geomData.TimeVarying = DMI::ALL
    & (isBitSet(paramData.timeVarying, 0) ? DMI::ALL : ~DMI::POINTS)
    & (isBitSet(paramData.timeVarying, 1) ? DMI::ALL : ~DMI::NORMALS)
    & (isBitSet(paramData.timeVarying, 2) ? DMI::ALL : ~DMI::TEXCOORDS)
    & (isBitSet(paramData.timeVarying, 3) ? DMI::ALL : ~DMI::COLORS)
    & (isBitSet(paramData.timeVarying, 4) ? DMI::ALL : ~DMI::INDICES);

  geomData.FaceVertexCount = geomType == GEOM_QUAD ? 4 : 3;
}

void UsdGeometry::initializeGeomData(UsdBridgeInstancerData& geomData)
{
  typedef UsdBridgeInstancerData::DataMemberId DMI;
  geomData.TimeVarying = DMI::ALL
    & (isBitSet(paramData.timeVarying, 0) ? DMI::ALL : ~DMI::POINTS)
    & (  (isBitSet(paramData.timeVarying, 1) 
      || ((geomType == GEOM_CYLINDER || geomType == GEOM_CONE) 
        && (isBitSet(paramData.timeVarying, 0) || isBitSet(paramData.timeVarying, 4)))) ? DMI::ALL : ~DMI::ORIENTATIONS)
    & (isBitSet(paramData.timeVarying, 5) ? DMI::ALL : ~DMI::SCALES)
    & (isBitSet(paramData.timeVarying, 4) ? DMI::ALL : ~DMI::INVISIBLEIDS)
    & (isBitSet(paramData.timeVarying, 2) ? DMI::ALL : ~DMI::TEXCOORDS)
    & (isBitSet(paramData.timeVarying, 3) ? DMI::ALL : ~DMI::COLORS)
    & (isBitSet(paramData.timeVarying, 6) ? DMI::ALL : ~DMI::INSTANCEIDS);
  geomData.UsePointInstancer = paramData.UseUsdGeomPointInstancer;

  switch (geomType)
  {
    case GEOM_CYLINDER:
      geomData.DefaultShape = UsdBridgeInstancerData::SHAPE_CYLINDER;
      break;
    case GEOM_CONE:
      geomData.DefaultShape = UsdBridgeInstancerData::SHAPE_CONE;
      break;
    default:
      geomData.DefaultShape = UsdBridgeInstancerData::SHAPE_SPHERE;
      break;
  };
}

void UsdGeometry::initializeGeomData(UsdBridgeCurveData& geomData)
{
  typedef UsdBridgeCurveData::DataMemberId DMI;
  // Turn off what is not timeVarying
  geomData.TimeVarying = DMI::ALL
    & (isBitSet(paramData.timeVarying, 0) ? DMI::ALL : ~DMI::POINTS)
    & (isBitSet(paramData.timeVarying, 1) ? DMI::ALL : ~DMI::NORMALS)
    & (isBitSet(paramData.timeVarying, 5) ? DMI::ALL : ~DMI::SCALES)
    & (isBitSet(paramData.timeVarying, 2) ? DMI::ALL : ~DMI::TEXCOORDS)
    & (isBitSet(paramData.timeVarying, 3) ? DMI::ALL : ~DMI::COLORS)
    & ((isBitSet(paramData.timeVarying, 0) || isBitSet(paramData.timeVarying, 4)) ? DMI::ALL : ~DMI::CURVELENGTHS);
}

bool UsdGeometry::checkArrayConstraints(const UsdDataArray* vertexArray, const UsdDataArray* primArray,
  const char* paramName, UsdDevice* device, const char* debugName)
{
  const UsdDataArray* vertices = paramData.vertexPositions;
  const UsdDataLayout& vertLayout = vertices->getLayout();

  const UsdDataArray* indices = paramData.indices;
  const UsdDataLayout& indexLayout = indices ? indices->getLayout() : vertLayout;

  const UsdDataLayout& perVertLayout = vertexArray ? vertexArray->getLayout() : vertLayout;
  const UsdDataLayout& perPrimLayout = primArray ? primArray->getLayout() : indexLayout;

  const UsdDataLayout& attrLayout = vertexArray ? perVertLayout : perPrimLayout;

  if (AssertOneDimensional(attrLayout, device, debugName, paramName)
    || AssertNoStride(attrLayout, device, debugName, paramName)
    )
  {
    return false;
  }

  if (perVertLayout.numItems1 != vertLayout.numItems1)
  {
    device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: all 'vertex.X' array elements should same size as vertex.positions", debugName);
    return false;
  }

  if (perPrimLayout.numItems1 != indexLayout.numItems1)
  {
    device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: all 'primitive.X' array elements should have same size as vertex.indices (if exists) or vertex.positions (otherwise)", debugName);
    return false;
  }

  return true;
}

bool UsdGeometry::checkGeomParams(UsdDevice* device, const char* debugName)
{
  bool success = true;

  success = success && checkArrayConstraints(paramData.vertexPositions, nullptr, "vertex.position", device, debugName);
  success = success && checkArrayConstraints(nullptr, paramData.indices, "primitive.index", device, debugName);

  success = success && checkArrayConstraints(paramData.vertexNormals, paramData.primitiveNormals, "vertex/primitive.normal", device, debugName);
  success = success && checkArrayConstraints(paramData.vertexTexCoords, paramData.primitiveTexCoords, "vertex/primitive.texcoord/attribute0", device, debugName);
  success = success && checkArrayConstraints(paramData.vertexColors, paramData.primitiveColors, "vertex/primitive.color", device, debugName);
  success = success && checkArrayConstraints(paramData.vertexRadii, paramData.primitiveRadii, "vertex/primitive.radius", device, debugName);
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
    if (geomType == GEOM_SPHERE || geomType == GEOM_CURVE)
    {
      if(geomType == GEOM_SPHERE && !paramData.UseUsdGeomPointInstancer)
        device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_WARNING, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' is a sphere geometry with indices, but the usd::usepointinstancer parameter is not set, so all vertices will show as spheres.", debugName);

      if (indexType != ANARI_INT32 && indexType != ANARI_UINT32 && indexType != ANARI_INT64 && indexType != ANARI_UINT64)
      {
        device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'primitive.index' parameter should be of type ANARI_(U)INT.", debugName);
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
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'vertex/primitive.normal' parameter should be of type ANARI_FLOAT32 or ANARI_FLOAT64.", debugName);
      return false;
    }
  }

  const UsdDataArray* texCoords = paramData.vertexTexCoords ? paramData.vertexTexCoords : paramData.primitiveTexCoords;
  if (texCoords)
  {
    ANARIDataType arrayType = texCoords->getType();
    if (arrayType != ANARI_FLOAT32_VEC2 && arrayType != ANARI_FLOAT64_VEC2)
    {
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'vertex/primitive.texcoord/attribute0' parameter should be of type ANARI_FLOAT32_VEC2 or ANARI_FLOAT64_VEC2.", debugName);
      return false;
    }
  }

  const UsdDataArray* colors = paramData.vertexColors ? paramData.vertexColors : paramData.primitiveColors;
  if (colors)
  {
    ANARIDataType arrayType = colors->getType();
    if (arrayType != ANARI_FLOAT32_VEC3 && arrayType != ANARI_FLOAT64_VEC3 && arrayType != ANARI_FLOAT32_VEC4 && arrayType != ANARI_FLOAT64_VEC4)
    {
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: 'vertex/primitive.color' parameter should be of type ANARI_FLOAT32_VEC(3/4) or ANARI_FLOAT64_VEC(3/4).", debugName);
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

void UsdGeometry::updateGeomData(UsdBridgeMeshData& meshData)
{
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
  const UsdDataArray* texCoords = paramData.vertexTexCoords ? paramData.vertexTexCoords : paramData.primitiveTexCoords;
  if (texCoords)
  {
    meshData.TexCoords = texCoords->getData();
    meshData.TexCoordsType = AnariToUsdBridgeType(texCoords->getType());
    meshData.PerPrimTexCoords = paramData.vertexTexCoords ? false : true;
  }

  const UsdDataArray* indices = paramData.indices;
  if (indices)
  {
    meshData.NumIndices = indices->getLayout().numItems1;
    meshData.Indices = indices->getData();
    ANARIDataType indexType = indices->getType();
    if (indexType == ANARI_UINT32_VEC3 || indexType == ANARI_INT32_VEC3 || indexType == ANARI_UINT64_VEC3 || indexType == ANARI_INT64_VEC3)
    {
      meshData.IndicesType = AnariToUsdBridgeType_Flattened(indices->getType());
      meshData.NumIndices *= 3;
    }
    else if (indexType == ANARI_UINT32_VEC4 || indexType == ANARI_INT32_VEC4 || indexType == ANARI_UINT64_VEC4 || indexType == ANARI_INT64_VEC4)
    {
      meshData.IndicesType = AnariToUsdBridgeType_Flattened(indices->getType());
      meshData.NumIndices *= 4;
    }
    else
    {
      meshData.IndicesType = AnariToUsdBridgeType(indices->getType());
    }
  }

  //meshData.UpdatesToPerform = Still to be implemented

  double timeStep = paramData.timeStep;
  usdBridge->SetGeometryData(usdHandle, meshData, timeStep);
}

void UsdGeometry::updateGeomData(UsdBridgeInstancerData& instancerData)
{
  if (geomType == GEOM_SPHERE)
  {
    genereteIndexedSphereData(paramData, tempArrays.get());

    const UsdDataArray* vertices = paramData.vertexPositions;
    instancerData.NumPoints = vertices->getLayout().numItems1;
    instancerData.Points = vertices->getData();
    instancerData.PointsType = AnariToUsdBridgeType(vertices->getType());

    // Normals
    if (paramData.indices && tempArrays->NormalsArray.size())
    {
      instancerData.Orientations = &tempArrays->NormalsArray[0];
      instancerData.OrientationsType = UsdBridgeType::FLOAT3;
    }
    else
    {
      const UsdDataArray* normals = paramData.vertexNormals ? paramData.vertexNormals : paramData.primitiveNormals;
      if (normals)
      {
        instancerData.Orientations = normals->getData();
        instancerData.OrientationsType = AnariToUsdBridgeType(normals->getType());
      }
    }

    // Colors
    if (paramData.indices && tempArrays->ColorsArray.size())
    {
      instancerData.Colors = &tempArrays->ColorsArray[0];
      instancerData.ColorsType = UsdBridgeType::FLOAT4;
    }
    else 
    {
      const UsdDataArray* colors = paramData.vertexColors ? paramData.vertexColors : paramData.primitiveColors;
      if (colors)
      {
        instancerData.Colors = colors->getData();
        instancerData.ColorsType = AnariToUsdBridgeType(colors->getType());
      }
    }

    // Texcoords
    if (paramData.indices && tempArrays->TexCoordsArray.size())
    {
      instancerData.TexCoords = &tempArrays->TexCoordsArray[0];
      instancerData.TexCoordsType = UsdBridgeType::FLOAT2;
    }
    else
    {
      const UsdDataArray* texCoords = paramData.vertexTexCoords ? paramData.vertexTexCoords : paramData.primitiveTexCoords;
      if (texCoords)
      {
        instancerData.TexCoords = texCoords->getData();
        instancerData.TexCoordsType = AnariToUsdBridgeType(texCoords->getType());
      }
    }

    // Scales
    if (paramData.indices && tempArrays->ScalesArray.size())
    {
      instancerData.Scales = &tempArrays->ScalesArray[0];
      instancerData.ScalesType = UsdBridgeType::FLOAT;
    }
    else
    {
      const UsdDataArray* radii = paramData.vertexRadii ? paramData.vertexRadii : paramData.primitiveRadii;
      if (radii)
      {
        instancerData.Scales = radii->getData();
        instancerData.ScalesType = AnariToUsdBridgeType(radii->getType());
      }
    }

    // Ids
    if (paramData.indices && tempArrays->IdsArray.size())
    {
      instancerData.InstanceIds = &tempArrays->IdsArray[0];
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
      instancerData.InvisibleIds = &tempArrays->InvisIdsArray[0];
      instancerData.InvisibleIdsType = UsdBridgeType::LONG;
      instancerData.NumInvisibleIds = tempArrays->InvisIdsArray.size();
    }

    instancerData.UniformScale = paramData.radiusConstant;
  }
  else
  {
    convertLinesToSticks(paramData, tempArrays.get());

    instancerData.NumPoints = tempArrays->PointsArray.size()/3;
    if (instancerData.NumPoints > 0)
    {
      instancerData.Points = &tempArrays->PointsArray[0];
      instancerData.PointsType = UsdBridgeType::FLOAT3;
      instancerData.Scales = &tempArrays->ScalesArray[0];
      instancerData.ScalesType = UsdBridgeType::FLOAT3;
      instancerData.Orientations = &tempArrays->OrientationsArray[0];
      instancerData.OrientationsType = UsdBridgeType::FLOAT4;

      if (tempArrays->ColorsArray.size())
      {
        instancerData.Colors = &tempArrays->ColorsArray[0];
        instancerData.ColorsType = UsdBridgeType::FLOAT4;
      }
      if (tempArrays->TexCoordsArray.size())
      {
        instancerData.TexCoords = &tempArrays->TexCoordsArray[0];
        instancerData.TexCoordsType = UsdBridgeType::FLOAT2;
      }
      if (tempArrays->IdsArray.size())
      {
        instancerData.InstanceIds = &tempArrays->IdsArray[0];
        instancerData.InstanceIdsType = UsdBridgeType::LONG;
      }
    }
  }

  double timeStep = paramData.timeStep;
  usdBridge->SetGeometryData(usdHandle, instancerData, timeStep);
}

void UsdGeometry::updateGeomData(UsdBridgeCurveData& curveData)
{
  reorderCurveGeometry(paramData, tempArrays.get());

  curveData.NumPoints = tempArrays->PointsArray.size() / 3;
  if (curveData.NumPoints > 0)
  {
    curveData.Points = &tempArrays->PointsArray[0];
    curveData.PointsType = UsdBridgeType::FLOAT3;
    curveData.CurveLengths = &tempArrays->CurveLengths[0];
    curveData.NumCurveLengths = tempArrays->CurveLengths.size();

    if (tempArrays->NormalsArray.size())
    {
      curveData.Normals = &tempArrays->NormalsArray[0];
      curveData.NormalsType = UsdBridgeType::FLOAT3;
    }
    curveData.PerPrimNormals = false;// Always vertex colored as per reorderCurveGeometry. One entry per whole curve would be useless

    // Copy Texcoords
    if (tempArrays->TexCoordsArray.size())
    {
      curveData.TexCoords = &tempArrays->TexCoordsArray[0];
      curveData.TexCoordsType = UsdBridgeType::FLOAT2;
    }
    curveData.PerPrimTexCoords = false; // Always vertex colored as per reorderCurveGeometry. One entry per whole curve would be useless

    // Copy colors
    if (tempArrays->ColorsArray.size())
    {
      curveData.Colors = &tempArrays->ColorsArray[0];
      curveData.ColorsType = UsdBridgeType::FLOAT4;
    }
    curveData.PerPrimColors = false; // Always vertex colored as per reorderCurveGeometry. One entry per whole curve would be useless

    // Assign scales
    if (tempArrays->ScalesArray.size())
    {
      curveData.Scales = &tempArrays->ScalesArray[0];
      curveData.ScalesType = UsdBridgeType::FLOAT;
    }
    curveData.UniformScale = paramData.radiusConstant;
  }

  double timeStep = paramData.timeStep;
  usdBridge->SetGeometryData(usdHandle, curveData, timeStep);
}

template<typename UsdGeomType>
void UsdGeometry::commitTemplate(UsdDevice* device)
{
  const char* debugName = getName();

  UsdGeomType geomData;

  bool isNew = false;
  if (!usdHandle.value)
  {  
    initializeGeomData(geomData);
    isNew = usdBridge->CreateGeometry(debugName, geomData, usdHandle);
  }

  if (paramChanged || isNew)
  {
    if (paramData.vertexPositions)
    {
      if(checkGeomParams(device, debugName))
        updateGeomData(geomData);
    }
    else
    {
      device->reportStatus(this, ANARI_GEOMETRY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdGeometry '%s' commit failed: missing 'vertex.position'.", debugName);
    }
  
    paramChanged = false;
  }
}

void UsdGeometry::commit(UsdDevice* device)
{
  if(!usdBridge)
    return;

  switch (geomType)
  {
    case GEOM_TRIANGLE: commitTemplate<UsdBridgeMeshData>(device); break;
    case GEOM_QUAD: commitTemplate<UsdBridgeMeshData>(device); break;
    case GEOM_SPHERE: commitTemplate<UsdBridgeInstancerData>(device); break;
    case GEOM_CYLINDER: commitTemplate<UsdBridgeInstancerData>(device); break;
    case GEOM_CONE: commitTemplate<UsdBridgeInstancerData>(device); break;
    case GEOM_CURVE: commitTemplate<UsdBridgeCurveData>(device); break;
    default: break;
  }
}
