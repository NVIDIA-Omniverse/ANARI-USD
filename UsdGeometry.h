// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"

#include <memory>

class UsdDataArray;
struct UsdBridgeMeshData;

static constexpr int MAX_ATTRIBS = 16;

struct UsdGeometryData
{
  const char* name = nullptr;
  const char* usdName = nullptr;

  double timeStep = 0.0;
  int timeVarying = 0xFFFFFFFF; // TimeVarying bits: 0: position, 1: normal, 2: color, 3: index, 4: radius, 5: ids, 6: attribute0, 7: attribute1, etc.  

  const UsdDataArray* vertexPositions = nullptr;
  const UsdDataArray* vertexNormals = nullptr;
  const UsdDataArray* vertexColors = nullptr;
  const UsdDataArray* vertexAttributes[MAX_ATTRIBS] = { nullptr };
  const UsdDataArray* primitiveNormals = nullptr;
  const UsdDataArray* primitiveColors = nullptr;
  const UsdDataArray* primitiveAttributes[MAX_ATTRIBS] = { nullptr };
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
    typedef std::vector<UsdBridgeAttribute> AttributeArray;
    typedef std::vector<std::vector<char>> AttributeDataArraysType;
    
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
      std::vector<float> ColorsArray;
      std::vector<float> ScalesArray;
      std::vector<float> OrientationsArray;
      std::vector<int64_t> IdsArray;
      std::vector<int64_t> InvisIdsArray;
      AttributeDataArraysType AttributeDataArrays;
    };

  protected:

    void initializeGeomData(UsdBridgeMeshData& geomData);
    void initializeGeomData(UsdBridgeInstancerData& geomData);
    void initializeGeomData(UsdBridgeCurveData& geomData);

    bool checkArrayConstraints(const UsdDataArray* vertexArray, const UsdDataArray* primArray,
      const char* paramName, UsdDevice* device, const char* debugName, int attribIndex = -1);
    bool checkGeomParams(UsdDevice* device);

    void updateGeomData(UsdDevice* device, UsdBridgeMeshData& meshData);
    void updateGeomData(UsdDevice* device, UsdBridgeInstancerData& instancerData);
    void updateGeomData(UsdDevice* device, UsdBridgeCurveData& curveData);

    template<typename UsdGeomType>
    void commitTemplate(UsdDevice* device);

    template<typename GeomDataType>
    void setAttributeTimeVarying(typename GeomDataType::DataMemberId& timeVarying);

    void syncAttributeArrays();

    template<typename GeomDataType>
    void copyAttributeArraysToData(GeomDataType& geomData);

    void assignTempDataToAttributes(bool perPrimInterpolation);

    GeomType geomType;

    std::unique_ptr<TempArrays> tempArrays;

    AttributeArray attributeArray;
};