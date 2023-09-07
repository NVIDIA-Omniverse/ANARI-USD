// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

#include <memory>

class UsdDataArray;
struct UsdBridgeMeshData;

static constexpr int MAX_ATTRIBS = 16;

struct UsdGeometryData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  double timeStep = 0.0;
  int timeVarying = 0xFFFFFFFF; // TimeVarying bits: 0: position, 1: normal, 2: color, 3: index, 4: radius, 5: ids, 6: attribute0, 7: attribute1, etc.
  static constexpr int TIMEVAR_ATTRIBUTE_START_BIT = 6;

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

struct UsdGeometryTempArrays;

class UsdGeometry : public UsdBridgedBaseObject<UsdGeometry, UsdGeometryData, UsdGeometryHandle>
{
  protected:
    enum GeomType
    {
      GEOM_UNKNOWN = 0,
      GEOM_TRIANGLE,
      GEOM_QUAD,
      GEOM_SPHERE,
      GEOM_CYLINDER,
      GEOM_CONE,
      GEOM_CURVE
    };

  public:
    typedef std::vector<UsdBridgeAttribute> AttributeArray;
    typedef std::vector<std::vector<char>> AttributeDataArraysType;
    
    UsdGeometry(const char* name, const char* type, UsdDevice* device);
    ~UsdGeometry();

    bool isInstanced() const 
    { 
      return type == GEOM_SPHERE 
        || type == GEOM_CONE 
        || type == GEOM_CYLINDER; 
    }

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override {}

    void initializeGeomData(UsdBridgeMeshData& geomData);
    void initializeGeomData(UsdBridgeInstancerData& geomData);
    void initializeGeomData(UsdBridgeCurveData& geomData);

    bool checkArrayConstraints(const UsdDataArray* vertexArray, const UsdDataArray* primArray,
      const char* paramName, UsdDevice* device, const char* debugName, int attribIndex = -1);
    bool checkGeomParams(UsdDevice* device);

    void updateGeomData(UsdDevice* device, UsdBridge* usdBridge, UsdBridgeMeshData& meshData);
    void updateGeomData(UsdDevice* device, UsdBridge* usdBridge, UsdBridgeInstancerData& instancerData);
    void updateGeomData(UsdDevice* device, UsdBridge* usdBridge, UsdBridgeCurveData& curveData);

    template<typename UsdGeomType>
    void commitTemplate(UsdDevice* device);

    template<typename GeomDataType>
    void setAttributeTimeVarying(typename GeomDataType::DataMemberId& timeVarying);

    void syncAttributeArrays();

    template<typename GeomDataType>
    void copyAttributeArraysToData(GeomDataType& geomData);

    void assignTempDataToAttributes(bool perPrimInterpolation);

    GeomType geomType = GEOM_UNKNOWN;

    std::unique_ptr<UsdGeometryTempArrays> tempArrays;

    AttributeArray attributeArray;
};