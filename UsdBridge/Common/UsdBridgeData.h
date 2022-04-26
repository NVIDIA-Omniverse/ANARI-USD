// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeData_h
#define UsdBridgeData_h

#include "UsdBridgeMacros.h"

#include <cstddef>
#include <functional>
#include <stdint.h>

class UsdBridge;

struct UsdBridgePrimCache;
struct UsdBridgeHandle
{
  UsdBridgePrimCache* value = nullptr;
};
struct UsdWorldHandle : public UsdBridgeHandle {};
struct UsdInstanceHandle : public UsdBridgeHandle {};
struct UsdGroupHandle : public UsdBridgeHandle {};
struct UsdSurfaceHandle : public UsdBridgeHandle {};
struct UsdGeometryHandle : public UsdBridgeHandle {};
struct UsdVolumeHandle : public UsdBridgeHandle {};
struct UsdSpatialFieldHandle : public UsdBridgeHandle {};
struct UsdTransferFunctionHandle : public UsdBridgeHandle {};
struct UsdSamplerHandle : public UsdBridgeHandle {};
struct UsdShaderHandle : public UsdBridgeHandle {};
struct UsdMaterialHandle : public UsdBridgeHandle {};

enum class UsdBridgeType
{
  BOOL = 0,

  UCHAR,
  CHAR,
  USHORT,
  SHORT,
  UINT,
  INT,
  ULONG,
  LONG,
  HALF,
  FLOAT,
  DOUBLE,

  UCHAR2,
  CHAR2,
  USHORT2,
  SHORT2,
  UINT2,
  INT2,
  ULONG2,
  LONG2,
  HALF2,
  FLOAT2,
  DOUBLE2,


  UCHAR3,
  CHAR3,
  USHORT3,
  SHORT3,
  UINT3,
  INT3,
  ULONG3,
  LONG3,
  HALF3,
  FLOAT3,
  DOUBLE3,


  UCHAR4,
  CHAR4,
  USHORT4,
  SHORT4,
  UINT4,
  INT4,
  ULONG4,
  LONG4,
  HALF4,
  FLOAT4,
  DOUBLE4,

  UNDEFINED
};
constexpr int UsdBridgeNumFundamentalTypes = (int)UsdBridgeType::UCHAR2; // Multi-component groups sizes should be equal

enum class UsdBridgeGeomType
{
  MESH = 0,
  INSTANCER,
  CURVE,
  VOLUME,
  NUM_GEOMTYPES
};

enum class UsdBridgeVolumeFieldType
{
  DENSITY,
  COLOR
};

enum class UsdBridgeLogLevel
{
  STATUS,
  WARNING,
  ERR
};
typedef void(*UsdBridgeLogCallback)(UsdBridgeLogLevel, void*, const char*);

struct UsdBridgeSettings
{
  const char* HostName;             // Name of the remote server 
  const char* OutputPath;           // Directory for output (on server if HostName is not empty) 
  bool CreateNewSession;            // Find a new session directory on creation of the bridge, or re-use the last opened one. 
  bool BinaryOutput;                // Select usda or usd output.
};

// Generic attribute definition
struct UsdBridgeAttribute
{
  const void* Data = nullptr;
  UsdBridgeType DataType = UsdBridgeType::UNDEFINED;
  bool PerPrimData = false;
  uint32_t EltSize;
};

struct UsdBridgeMeshData
{
  static const UsdBridgeGeomType GeomType = UsdBridgeGeomType::MESH;

  //Class definitions
  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    POINTS = (1 << 0), // Goes together with extent
    NORMALS = (1 << 1),
    COLORS = (1 << 2),
    INDICES = (1 << 3),
    ATTRIBUTE0 = (1 << 4),
    ATTRIBUTE1 = (1 << 5),
    ATTRIBUTE2 = (1 << 6),
    ATTRIBUTE3 = (1 << 7),
    ALL = (1 << 8) - 1
  };
  
  DataMemberId UpdatesToPerform = DataMemberId::ALL;
  DataMemberId TimeVarying = DataMemberId::ALL;

  bool isEmpty() { return Points == NULL; }

  //Mesh data
  uint64_t NumPoints = 0;

  const void* Points = nullptr;
  UsdBridgeType PointsType = UsdBridgeType::UNDEFINED;
  const void* Normals = nullptr;
  UsdBridgeType NormalsType = UsdBridgeType::UNDEFINED;
  bool PerPrimNormals = false;
  const void* Colors = nullptr;
  UsdBridgeType ColorsType = UsdBridgeType::UNDEFINED;
  bool PerPrimColors = false;
  const UsdBridgeAttribute* Attributes = nullptr; // Pointer to externally managed attribute array
  uint32_t NumAttributes = 0;

  const void* Indices = nullptr;
  UsdBridgeType IndicesType = UsdBridgeType::UNDEFINED;
  uint64_t NumIndices = 0;

  int FaceVertexCount = 0;
};

struct UsdBridgeInstancerData
{
  static const UsdBridgeGeomType GeomType = UsdBridgeGeomType::INSTANCER;

  //Class definitions
  enum InstanceShape
  {
    SHAPE_SPHERE = -1,
    SHAPE_CYLINDER = -2,
    SHAPE_CONE = -3,
    SHAPE_MESH = 0 // Positive values denote meshes (specific meshes can be referenced by id)
  };

  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    POINTS = (1 << 0), // Goes together with extent
    SHAPES = (1 << 1), // Cannot be timevarying
    SHAPEINDICES = (1 << 2),
    SCALES = (1 << 3),
    ORIENTATIONS = (1 << 4),
    LINEARVELOCITIES = (1 << 5),
    ANGULARVELOCITIES = (1 << 6),
    INSTANCEIDS = (1 << 7),
    COLORS = (1 << 8),
    INVISIBLEIDS = (1 << 9),
    ATTRIBUTE0 = (1 << 10),
    ATTRIBUTE1 = (1 << 11),
    ATTRIBUTE2 = (1 << 12),
    ATTRIBUTE3 = (1 << 13),
    ALL = (1 << 14) - 1
  };

  DataMemberId UpdatesToPerform = DataMemberId::ALL;
  DataMemberId TimeVarying = DataMemberId::ALL;

  InstanceShape DefaultShape = SHAPE_SPHERE;
  InstanceShape* Shapes = &DefaultShape; //no duplicate entries allowed.
  int NumShapes = 1;

  bool UsePointInstancer = true;

  uint64_t NumPoints = 0;
  const void* Points = nullptr;
  UsdBridgeType PointsType = UsdBridgeType::UNDEFINED;
  const int* ShapeIndices = nullptr; //if set, one for every point
  const void* Scales = nullptr;// 3-vector scale
  UsdBridgeType ScalesType = UsdBridgeType::UNDEFINED;
  double UniformScale = 1;// In case no scales are given
  const void* Orientations = nullptr;
  UsdBridgeType OrientationsType = UsdBridgeType::UNDEFINED;
  const void* Colors = nullptr;
  UsdBridgeType ColorsType = UsdBridgeType::UNDEFINED;
  static constexpr bool PerPrimColors = false; // For compatibility
  const float* LinearVelocities = nullptr;
  const float* AngularVelocities = nullptr;
  const UsdBridgeAttribute* Attributes = nullptr; // Pointer to externally managed attribute array
  uint32_t NumAttributes = 0;
  const void* InstanceIds = nullptr; 
  UsdBridgeType InstanceIdsType = UsdBridgeType::UNDEFINED;

  const void* InvisibleIds = nullptr; //Index into points
  uint64_t NumInvisibleIds = 0;
  UsdBridgeType InvisibleIdsType = UsdBridgeType::UNDEFINED;
};

struct UsdBridgeCurveData
{
  static const UsdBridgeGeomType GeomType = UsdBridgeGeomType::CURVE;

  //Class definitions
  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    POINTS = (1 << 0), // Goes together with extent and curvelengths
    NORMALS = (1 << 1),
    SCALES = (1 << 2),
    COLORS = (1 << 3),
    CURVELENGTHS = (1 << 4),
    ATTRIBUTE0 = (1 << 5),
    ATTRIBUTE1 = (1 << 6),
    ATTRIBUTE2 = (1 << 7),
    ATTRIBUTE3 = (1 << 8),
    ALL = (1 << 9) - 1
  };

  DataMemberId UpdatesToPerform = DataMemberId::ALL;
  DataMemberId TimeVarying = DataMemberId::ALL;

  bool isEmpty() { return Points == NULL; }

  uint64_t NumPoints = 0;

  const void* Points = nullptr;
  UsdBridgeType PointsType = UsdBridgeType::UNDEFINED;
  const void* Normals = nullptr;
  UsdBridgeType NormalsType = UsdBridgeType::UNDEFINED;
  bool PerPrimNormals = false;
  const void* Colors = nullptr;
  UsdBridgeType ColorsType = UsdBridgeType::UNDEFINED;
  bool PerPrimColors = false; // One prim would be a full curve
  const void* Scales = nullptr; // Used for line width, typically 1-component
  UsdBridgeType ScalesType = UsdBridgeType::UNDEFINED;
  double UniformScale = 1;// In case no scales are given
  const UsdBridgeAttribute* Attributes = nullptr; // Pointer to externally managed attribute array
  uint32_t NumAttributes = 0;

  const int* CurveLengths = nullptr;
  uint64_t NumCurveLengths = 0;
};

struct UsdBridgeVolumeData
{
  static constexpr int TFDataStart = 4;
  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    DATA = (1 << 1),
    ORIGIN = (1 << 2),
    CELLDIMENSIONS = (1 << 3),
    VOL_ALL = (1 << 4) - 1,
    TFCOLORS = (1 << (TFDataStart+0)),
    TFOPACITIES = (1 << (TFDataStart + 1)),
    TFVALUERANGE = (1 << (TFDataStart + 2)),
    ALL = (1 << (TFDataStart + 3)) - 1
  };
#if __cplusplus >= 201703L
  static_assert(DataMemberId::TFCOLORS > DataMemberId::VOL_ALL);
#endif
  
  DataMemberId UpdatesToPerform = DataMemberId::ALL;
  DataMemberId TimeVarying = DataMemberId::ALL;

  bool preClassified = false;

  const void* Data = nullptr;
  UsdBridgeType DataType = UsdBridgeType::UNDEFINED; // Same timeVarying rule as 'Data'
  size_t NumElements[3] = { 0,0,0 }; // Same timeVarying rule as 'Data'
  float Origin[3] = { 0,0,0 };
  float CellDimensions[3] = { 1,1,1 };

  const void* TfColors;
  UsdBridgeType TfColorsType = UsdBridgeType::UNDEFINED;
  int TfNumColors;
  const void* TfOpacities;
  UsdBridgeType TfOpacitiesType = UsdBridgeType::UNDEFINED;
  int TfNumOpacities;
  double TfValueRange[2] = { 0, 1 };
};

struct UsdBridgeMaterialData
{
  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    DIFFUSE = (1 << 0), 
    SPECULAR = (1 << 1),
    EMISSIVE = (1 << 2),
    OPACITY = (1 << 3),
    EMISSIVEINTENSITY = (1 << 4),
    ROUGHNESS = (1 << 5),
    METALLIC = (1 << 6),
    IOR = (1 << 7),
    ALL = (1 << 8) - 1
  };
  DataMemberId TimeVarying = DataMemberId::NONE;

  float Diffuse[3] = { 1.0f };
  float Specular[3] = { 1.0f };
  float Emissive[3] = { 1.0f };
  float Opacity = 1.0f;
  bool HasTranslucency = false;
  float EmissiveIntensity = 0.0f;
  float Roughness = 0.5f;
  float Metallic = -1.0;
  float Ior = 1.0f;
  bool UseVertexColors = false;
};

struct UsdBridgeSamplerData
{
  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    FILENAME = (1 << 0),
    WRAPS = (1 << 1),
    WRAPT = (1 << 2),
    ALL = (1 << 3) - 1
  };
  DataMemberId TimeVarying = DataMemberId::NONE;

  enum class WrapMode
  {
    BLACK = 0,
    CLAMP,
    REPEAT,
    MIRROR
  };

  const char* FileName;
  WrapMode WrapS;
  WrapMode WrapT;
};

template<class TEnum>
struct DefineBitMaskOps
{
  static const bool enable = false;
};

template<class TEnum>
struct DefineAddSubOps
{
  static const bool enable = false;
};

template<>
struct DefineBitMaskOps<UsdBridgeMeshData::DataMemberId>
{
  static const bool enable = true; 
};
template<>
struct DefineAddSubOps<UsdBridgeMeshData::DataMemberId>
{
  static const bool enable = true; 
};

template<>
struct DefineBitMaskOps<UsdBridgeInstancerData::DataMemberId>
{
  static const bool enable = true;
};
template<>
struct DefineAddSubOps<UsdBridgeInstancerData::DataMemberId>
{
  static const bool enable = true;
};

template<>
struct DefineBitMaskOps<UsdBridgeCurveData::DataMemberId>
{
  static const bool enable = true;
};
template<>
struct DefineAddSubOps<UsdBridgeCurveData::DataMemberId>
{
  static const bool enable = true;
};

template<>
struct DefineBitMaskOps<UsdBridgeVolumeData::DataMemberId>
{
  static const bool enable = true;
};

template<>
struct DefineBitMaskOps<UsdBridgeMaterialData::DataMemberId>
{
  static const bool enable = true;
};

template<>
struct DefineBitMaskOps<UsdBridgeSamplerData::DataMemberId>
{
  static const bool enable = true;
};

template<class TEnum>
typename std::enable_if<DefineBitMaskOps<TEnum>::enable, TEnum>::type operator |(TEnum lhs, TEnum rhs)
{
  using underlying = typename std::underlying_type<TEnum>::type;
  return static_cast<TEnum> (
    static_cast<underlying>(lhs) |
    static_cast<underlying>(rhs)
    );
}

template<class TEnum>
typename std::enable_if<DefineBitMaskOps<TEnum>::enable, TEnum>::type operator &(TEnum lhs, TEnum rhs)
{
  using underlying = typename std::underlying_type<TEnum>::type;
  return static_cast<TEnum> (
    static_cast<underlying>(lhs) &
    static_cast<underlying>(rhs)
    );
}

template<class TEnum>
typename std::enable_if<DefineBitMaskOps<TEnum>::enable, TEnum>::type operator ~(TEnum rhs)
{
  using underlying = typename std::underlying_type<TEnum>::type;
  return static_cast<TEnum> (~static_cast<underlying>(rhs));
}

template<class TEnum>
typename std::enable_if<DefineAddSubOps<TEnum>::enable, TEnum>::type operator +(TEnum lhs, int rhs)
{
  using underlying = typename std::underlying_type<TEnum>::type;
  return static_cast<TEnum> ( static_cast<underlying>(lhs) + rhs );
}

template<class T>
class UsdBridgeUpdateEvaluator
{
public:
  UsdBridgeUpdateEvaluator(T& data)
    : Data(data)
  {
  }

  bool PerformsUpdate(typename T::DataMemberId member) const
  {
    return ((Data.UpdatesToPerform & member) != T::DataMemberId::NONE);
  }

  void RemoveUpdate(typename T::DataMemberId member)
  { 
    Data.UpdatesToPerform = (Data.UpdatesToPerform & ~member);
  }

  void AddUpdate(typename T::DataMemberId member) 
  { 
    Data.UpdatesToPerform = (Data.UpdatesToPerform | member);
  }

  T& Data;
};

#endif

