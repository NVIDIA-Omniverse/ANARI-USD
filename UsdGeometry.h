// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"

#include <memory>

class UsdDataArray;
struct UsdBridgeMeshData;

struct UsdGeometryData
{
  const char* name = nullptr;
  const char* usdName = nullptr;

  double timeStep = 0.0;
  int timeVarying = 0xFFFFFFFF; // TimeVarying bits: 0: position, 1: normal, 2: texcoord (attribute0), 3: color, 4: index, 5: radius, 6: ids 

  const UsdDataArray* vertexPositions = nullptr;
  const UsdDataArray* vertexNormals = nullptr;
  const UsdDataArray* vertexTexCoords = nullptr;
  const UsdDataArray* vertexColors = nullptr;
  const UsdDataArray* primitiveNormals = nullptr;
  const UsdDataArray* primitiveTexCoords = nullptr;
  const UsdDataArray* primitiveColors = nullptr;
  const UsdDataArray* primitiveIds = nullptr;
  const UsdDataArray* indices = nullptr; 
  
  // Spheres
  const UsdDataArray* vertexRadii = nullptr;
  float radiusConstant = 1.0f;
  bool UseUsdGeomPointInstancer = 
#ifdef USE_USD_GEOM_POINTS
    false;
#else
    true;
#endif

  // Cylinders
  const UsdDataArray* primitiveRadii = nullptr;

  // Curves
};

class UsdGeometry : public UsdBridgedBaseObject<UsdGeometry, UsdGeometryData, UsdGeometryHandle>
{
  protected:
    enum GeomType
    {
      GEOM_TRIANGLE = 0,
      GEOM_QUAD,
      GEOM_SPHERE,
      GEOM_CYLINDER,
      GEOM_CONE,
      GEOM_CURVE
    };

  public:
    UsdGeometry(const char* name, const char* type, UsdBridge* bridge, UsdDevice* device);
    ~UsdGeometry();

    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    void filterResetParam(
      const char *name) override;

    void commit(UsdDevice* device) override;

    struct TempArrays
    {
      std::vector<int> CurveLengths;
      std::vector<float> PointsArray;
      std::vector<float> NormalsArray;
      std::vector<float> TexCoordsArray;
      std::vector<float> ColorsArray;
      std::vector<float> ScalesArray;
      std::vector<float> OrientationsArray;
      std::vector<int64_t> IdsArray;
      std::vector<int64_t> InvisIdsArray;
    };

  protected:

    void initializeGeomData(UsdBridgeMeshData& geomData);
    void initializeGeomData(UsdBridgeInstancerData& geomData);
    void initializeGeomData(UsdBridgeCurveData& geomData);

    bool checkArrayConstraints(const UsdDataArray* vertexArray, const UsdDataArray* primArray,
      const char* paramName, UsdDevice* device, const char* debugName);
    bool checkGeomParams(UsdDevice* device, const char* debugName);

    void updateGeomData(UsdBridgeMeshData& meshData);
    void updateGeomData(UsdBridgeInstancerData& instancerData);
    void updateGeomData(UsdBridgeCurveData& curveData);

    template<typename UsdGeomType>
    void commitTemplate(UsdDevice* device);

    GeomType geomType;

    std::unique_ptr<TempArrays> tempArrays;

};