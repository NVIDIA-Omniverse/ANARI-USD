// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeData_h
#define UsdBridgeData_h

#include "UsdBridgeMacros.h"
#include "UsdBridgeNumerics.h"

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
struct UsdSamplerHandle : public UsdBridgeHandle {};
struct UsdShaderHandle : public UsdBridgeHandle {};
struct UsdMaterialHandle : public UsdBridgeHandle {};
struct UsdLightHandle : public UsdBridgeHandle {};

enum class UsdBridgeType
{
  BOOL = 0,

  UCHAR,
  UCHAR_SRGB_R,
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
  UCHAR_SRGB_RA,
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
  UCHAR_SRGB_RGB,
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
  UCHAR_SRGB_RGBA,
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

  INT_PAIR,
  INT_PAIR2,
  INT_PAIR3,
  INT_PAIR4,

  FLOAT_PAIR,
  FLOAT_PAIR2,
  FLOAT_PAIR3,
  FLOAT_PAIR4,

  DOUBLE_PAIR,
  DOUBLE_PAIR2,
  DOUBLE_PAIR3,
  DOUBLE_PAIR4,

  ULONG_PAIR,
  ULONG_PAIR2,
  ULONG_PAIR3,
  ULONG_PAIR4,

  FLOAT_MAT2,
  FLOAT_MAT3,
  FLOAT_MAT4,
  FLOAT_MAT2x3,
  FLOAT_MAT3x4,

  UNDEFINED
};
constexpr int UsdBridgeNumFundamentalTypes = (int)UsdBridgeType::UCHAR2; // Multi-component groups sizes up until 4 should be equal

inline int UsdBridgeTypeNumComponents(UsdBridgeType dataType)
{
  int typeIdent = (int)dataType;
  if(typeIdent <= (int)UsdBridgeType::DOUBLE4)
    return typeIdent / UsdBridgeNumFundamentalTypes;
  
  switch(dataType)
  {
    case UsdBridgeType::INT_PAIR: return 2;
    case UsdBridgeType::INT_PAIR2: return 4;
    case UsdBridgeType::INT_PAIR3: return 6;
    case UsdBridgeType::INT_PAIR4: return 8;

    case UsdBridgeType::FLOAT_PAIR: return 2;
    case UsdBridgeType::FLOAT_PAIR2: return 4;
    case UsdBridgeType::FLOAT_PAIR3: return 6;
    case UsdBridgeType::FLOAT_PAIR4: return 8;

    case UsdBridgeType::DOUBLE_PAIR: return 2;
    case UsdBridgeType::DOUBLE_PAIR2: return 4;
    case UsdBridgeType::DOUBLE_PAIR3: return 6;
    case UsdBridgeType::DOUBLE_PAIR4: return 8;

    case UsdBridgeType::ULONG_PAIR: return 2;
    case UsdBridgeType::ULONG_PAIR2: return 4;
    case UsdBridgeType::ULONG_PAIR3: return 6;
    case UsdBridgeType::ULONG_PAIR4: return 8;

    case UsdBridgeType::FLOAT_MAT2: return 4;
    case UsdBridgeType::FLOAT_MAT3: return 9;
    case UsdBridgeType::FLOAT_MAT4: return 16;
    case UsdBridgeType::FLOAT_MAT2x3: return 6;
    case UsdBridgeType::FLOAT_MAT3x4: return 12;

    default: return 1;
  }

  return 1;
}

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

struct UsdBridgeLogObject
{
  void* LogUserData;
  UsdBridgeLogCallback LogCallback;
};

struct UsdBridgeSettings
{
  const char* HostName;             // Name of the remote server 
  const char* OutputPath;           // Directory for output (on server if HostName is not empty) 
  bool CreateNewSession;            // Find a new session directory on creation of the bridge, or re-use the last opened one (leave contents intact). 
  bool BinaryOutput;                // Select usda or usd output.

  // Output settings
  bool EnablePreviewSurfaceShader;
  bool EnableMdlShader;

  // About to be deprecated
  static constexpr bool EnableStTexCoords = false;
};

// Generic attribute definition
struct UsdBridgeAttribute
{
  const void* Data = nullptr;
  UsdBridgeType DataType = UsdBridgeType::UNDEFINED;
  bool PerPrimData = false;
  uint32_t EltSize = 0;
  const char* Name = nullptr;
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
  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    POINTS = (1 << 0), // Goes together with extent
    SHAPEINDICES = (1 << 1),
    SCALES = (1 << 2),
    ORIENTATIONS = (1 << 3),
    LINEARVELOCITIES = (1 << 4),
    ANGULARVELOCITIES = (1 << 5),
    INSTANCEIDS = (1 << 6),
    COLORS = (1 << 7),
    INVISIBLEIDS = (1 << 8),
    ATTRIBUTE0 = (1 << 9),
    ATTRIBUTE1 = (1 << 10),
    ATTRIBUTE2 = (1 << 11),
    ATTRIBUTE3 = (1 << 12),
    ALL = (1 << 13) - 1
  };

  DataMemberId UpdatesToPerform = DataMemberId::ALL;
  DataMemberId TimeVarying = DataMemberId::ALL;

  float getUniformScale() const { return Scale.Data[0]; }

  bool UseUsdGeomPoints = true; // Shape is sphere and geomPoints is desired

  uint64_t NumPoints = 0;
  const void* Points = nullptr;
  UsdBridgeType PointsType = UsdBridgeType::UNDEFINED;
  const int* ShapeIndices = nullptr; //if set, one for every point
  const void* Scales = nullptr;// 3-vector scale
  UsdBridgeType ScalesType = UsdBridgeType::UNDEFINED;
  UsdFloat3 Scale = {1, 1, 1};// In case no scales are given
  const void* Orientations = nullptr;
  UsdBridgeType OrientationsType = UsdBridgeType::UNDEFINED;
  UsdQuaternion Orientation;// In case no orientations are given
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

struct UsdBridgeInstancerRefData
{
  enum InstanceShape
  {
    SHAPE_SPHERE = -1,
    SHAPE_CYLINDER = -2,
    SHAPE_CONE = -3,
    SHAPE_MESH = 0 // Positive values denote meshes (allows for indices into a list of meshes passed along with a list of shapes)
  };

  InstanceShape DefaultShape = SHAPE_SPHERE;
  InstanceShape* Shapes = &DefaultShape;
  int NumShapes = 1;
  UsdFloatMat4 ShapeTransform;
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
  float getUniformScale() const { return UniformScale; }

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
  float UniformScale = 1;// In case no scales are given
  const UsdBridgeAttribute* Attributes = nullptr; // Pointer to externally managed attribute array
  uint32_t NumAttributes = 0;

  const int* CurveLengths = nullptr;
  uint64_t NumCurveLengths = 0;
};

struct UsdBridgeTfData
{
  const void* TfColors = nullptr;
  UsdBridgeType TfColorsType = UsdBridgeType::UNDEFINED;
  int TfNumColors = 0;
  const void* TfOpacities = nullptr;
  UsdBridgeType TfOpacitiesType = UsdBridgeType::UNDEFINED;
  int TfNumOpacities = 0;
  double TfValueRange[2] = { 0, 1 };
};

struct UsdBridgeVolumeData
{
  static constexpr int TFDataStart = 2;
  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    DATA = (1 << 1), // Includes the extent - number of elements per dimension
    VOL_ALL = (1 << TFDataStart) - 1,
    TFCOLORS = (1 << (TFDataStart + 0)),
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

  long long BackgroundIdx = -1; // When not -1, denotes the first element in Data which contains a background value

  UsdBridgeTfData TfData;
};

struct UsdBridgeMaterialData
{
  template<typename ValueType>
  struct MaterialInput
  {
    ValueType Value;
    const char* SrcAttrib;
    bool Sampler; // Only denote whether a sampler is attached
  };

  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    DIFFUSE = (1 << 0), 
    OPACITY = (1 << 1),
    EMISSIVECOLOR = (1 << 2),
    EMISSIVEINTENSITY = (1 << 3),
    ROUGHNESS = (1 << 4),
    METALLIC = (1 << 5),
    IOR = (1 << 6),
    ALL = (1 << 7) - 1
  };
  DataMemberId TimeVarying = DataMemberId::NONE;

  bool IsPbr = true;

  enum class AlphaModes : uint32_t
  {
    NONE,
    BLEND,
    MASK
  };
  AlphaModes AlphaMode = AlphaModes::NONE;
  float AlphaCutoff = 0.5f;

  MaterialInput<UsdFloat3> Diffuse = {{ 1.0f }, nullptr};
  MaterialInput<UsdFloat3> Emissive = {{ 1.0f }, nullptr};
  MaterialInput<float> Opacity = {1.0f, nullptr};
  MaterialInput<float> EmissiveIntensity = {0.0f, nullptr};
  MaterialInput<float> Roughness = {0.5f, nullptr};
  MaterialInput<float> Metallic = {0.0, nullptr};
  MaterialInput<float> Ior = {1.0f, nullptr};
};

template<typename ValueType>
const typename ValueType::DataType* GetValuePtr(const UsdBridgeMaterialData::MaterialInput<ValueType>& input)
{
  return input.Value.Data;
}

struct UsdBridgeSamplerData
{
  enum class SamplerType : uint32_t
  {
    SAMPLER_1D = 0,
    SAMPLER_2D,
    SAMPLER_3D
  };

  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    DATA = (1 << 0), // Refers to: data(-type), image name/url
    WRAPS = (1 << 1),
    WRAPT = (1 << 2),
    WRAPR = (1 << 3),
    ALL = (1 << 4) - 1
  };
  DataMemberId TimeVarying = DataMemberId::NONE;

  enum class WrapMode
  {
    BLACK = 0,
    CLAMP,
    REPEAT,
    MIRROR
  };

  SamplerType Type = SamplerType::SAMPLER_1D;

  const char* InAttribute = nullptr;

  const char* ImageUrl = nullptr;
  const char* ImageName = nullptr;
  uint64_t ImageDims[3] = {0, 0, 0};
  int64_t ImageStride[3] = {0, 0, 0};
  int ImageNumComponents = 4;

  const void* Data = nullptr;
  UsdBridgeType DataType = UsdBridgeType::UNDEFINED;

  WrapMode WrapS = WrapMode::BLACK;
  WrapMode WrapT = WrapMode::BLACK;
  WrapMode WrapR = WrapMode::BLACK;
};

struct UsdSamplerRefData
{
  int ImageNumComponents;
  double TimeStep;
  UsdBridgeMaterialData::DataMemberId DataMemberId; // Material input parameter to connect to
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

