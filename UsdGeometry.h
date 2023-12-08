// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBridgedBaseObject.h"

#include <memory>
#include <limits>

class UsdDataArray;
struct UsdBridgeMeshData;

static constexpr int MAX_ATTRIBS = 16;

enum class UsdGeometryComponents
{
  POSITION = 0,
  NORMAL,
  COLOR,
  INDEX,
  SCALE,
  ORIENTATION,
  ID,
  ATTRIBUTE0,
  ATTRIBUTE1,
  ATTRIBUTE2,
  ATTRIBUTE3
};

struct UsdGeometryData
{
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;

  double timeStep = 0.0;
  int timeVarying = 0xFFFFFFFF; // TimeVarying bits

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
  bool UseUsdGeomPoints = 
#ifdef USE_USD_GEOM_POINTS
    true;
#else
    false;
#endif

  // Cylinders
  const UsdDataArray* primitiveRadii = nullptr;

  // Curves

  // Glyphs
  const UsdDataArray* vertexScales = nullptr;
  const UsdDataArray* primitiveScales = nullptr;
  const UsdDataArray* vertexOrientations = nullptr;
  const UsdDataArray* primitiveOrientations = nullptr;

  UsdFloat3 scaleConstant = {1.0f, 1.0f, 1.0f};
  UsdQuaternion orientationConstant;

  UsdSharedString* shapeType = nullptr;
  UsdGeometry* shapeGeometry = nullptr;

  UsdFloatMat4 shapeTransform;

  double shapeGeometryRefTimeStep = std::numeric_limits<float>::quiet_NaN();
};

struct UsdGeometryTempArrays;

class UsdGeometry : public UsdBridgedBaseObject<UsdGeometry, UsdGeometryData, UsdGeometryHandle, UsdGeometryComponents>
{
  public:
    enum GeomType
    {
      GEOM_UNKNOWN = 0,
      GEOM_TRIANGLE,
      GEOM_QUAD,
      GEOM_SPHERE,
      GEOM_CYLINDER,
      GEOM_CONE,
      GEOM_CURVE,
      GEOM_GLYPH
    };

    typedef std::vector<UsdBridgeAttribute> AttributeArray;
    typedef std::vector<std::vector<char>> AttributeDataArraysType;
    
    UsdGeometry(const char* name, const char* type, UsdDevice* device);
    ~UsdGeometry();

    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    bool isInstanced() const 
    { 
      return geomType == GEOM_SPHERE
        || geomType == GEOM_CONE
        || geomType == GEOM_CYLINDER
        || geomType == GEOM_GLYPH;
    }

    static constexpr ComponentPair componentParamNames[] = {
      ComponentPair(UsdGeometryComponents::POSITION, "position"),
      ComponentPair(UsdGeometryComponents::NORMAL, "normal"),
      ComponentPair(UsdGeometryComponents::COLOR, "color"),
      ComponentPair(UsdGeometryComponents::INDEX, "index"),
      ComponentPair(UsdGeometryComponents::SCALE, "scale"),
      ComponentPair(UsdGeometryComponents::SCALE, "radius"),
      ComponentPair(UsdGeometryComponents::ORIENTATION, "orientation"),
      ComponentPair(UsdGeometryComponents::ID, "id"),
      ComponentPair(UsdGeometryComponents::ATTRIBUTE0, "attribute0"),
      ComponentPair(UsdGeometryComponents::ATTRIBUTE1, "attribute1"),
      ComponentPair(UsdGeometryComponents::ATTRIBUTE2, "attribute2"),
      ComponentPair(UsdGeometryComponents::ATTRIBUTE3, "attribute3")};

  protected:
    bool deferCommit(UsdDevice* device) override;
    bool doCommitData(UsdDevice* device) override;
    void doCommitRefs(UsdDevice* device) override;

    void initializeGeomData(UsdBridgeMeshData& geomData);
    void initializeGeomData(UsdBridgeInstancerData& geomData);
    void initializeGeomData(UsdBridgeCurveData& geomData);

    void initializeGeomRefData(UsdBridgeInstancerRefData& geomRefData);

    bool checkArrayConstraints(const UsdDataArray* vertexArray, const UsdDataArray* primArray,
      const char* paramName, UsdDevice* device, const char* debugName, int attribIndex = -1);
    bool checkGeomParams(UsdDevice* device);

    void updateGeomData(UsdDevice* device, UsdBridge* usdBridge, UsdBridgeMeshData& meshData, bool isNew);
    void updateGeomData(UsdDevice* device, UsdBridge* usdBridge, UsdBridgeInstancerData& instancerData, bool isNew);
    void updateGeomData(UsdDevice* device, UsdBridge* usdBridge, UsdBridgeCurveData& curveData, bool isNew);

    template<typename UsdGeomType>
    bool commitTemplate(UsdDevice* device);

    void commitPrototypes(UsdBridge* usdBridge);

    template<typename GeomDataType>
    void setAttributeTimeVarying(typename GeomDataType::DataMemberId& timeVarying);

    void syncAttributeArrays();

    template<typename GeomDataType>
    void copyAttributeArraysToData(GeomDataType& geomData);

    void assignTempDataToAttributes(bool perPrimInterpolation);

    GeomType geomType = GEOM_UNKNOWN;
    bool protoShapeChanged = false; // Do not automatically commit shapes (the object may have been recreated onto an already existing USD prim)

    std::unique_ptr<UsdGeometryTempArrays> tempArrays;

    AttributeArray attributeArray;
};