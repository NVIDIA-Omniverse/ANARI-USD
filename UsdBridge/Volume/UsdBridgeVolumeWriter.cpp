// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBridgeVolumeWriter.h"
#include "UsdBridgeUtils.h"

#include <memory>

class UsdBridgeVolumeWriterInternals;

class UsdBridgeVolumeWriter : public UsdBridgeVolumeWriterI
{
  public:
    UsdBridgeVolumeWriter();
    ~UsdBridgeVolumeWriter();

    bool Initialize(UsdBridgeLogCallback logCallback, void* logUserData) override;

    void ToVDB(const UsdBridgeVolumeData& volumeData) override;

    void GetSerializedVolumeData(const char*& data, size_t& size) override;

    void Release() override;

    static UsdBridgeLogCallback LogCallback;
    static void* LogUserData;

  protected:

    std::unique_ptr<UsdBridgeVolumeWriterInternals> Internals;
};

extern "C" UsdBridgeVolumeWriterI* __cdecl Create_VolumeWriter()
{
  return new UsdBridgeVolumeWriter();
}

void UsdBridgeVolumeWriter::Release()
{
  delete this;
}


#ifdef USE_OPENVDB

// GridTransformer.h->LevelSetRebuild.h->MeshToVolume.h->tbb/enumerable_thread_specific.h->windows_api.h
#ifdef WIN32
#define NOMINMAX
#define _USE_MATH_DEFINES // for C
#include <math.h>
#endif

#include "openvdb/openvdb.h"
#include "openvdb/io/Stream.h"
#include "openvdb/tools/Dense.h"
#include "openvdb/tools/GridTransformer.h"
#include "openvdb/tree/ValueAccessor.h"

#include <assert.h>
#include <limits>
#include <sstream>

#define UsdBridgeLogMacro(level, message) \
  { std::stringstream logStream; \
    logStream << message; \
    std::string logString = logStream.str(); \
    UsdBridgeVolumeWriter::LogCallback(level, UsdBridgeVolumeWriter::LogUserData, logString.c_str()); } 

UsdBridgeLogCallback UsdBridgeVolumeWriter::LogCallback = nullptr;
void* UsdBridgeVolumeWriter::LogUserData = nullptr;

#ifdef USDBRIDGE_VOL_FLOAT1_OUTPUT
using ColorGridOutType = openvdb::FloatGrid;
#else
using ColorGridOutType = openvdb::Vec3fGrid;
#endif
using OpacityGridOutType = openvdb::FloatGrid;

class UsdBridgeVolumeWriterInternals
{
  public:
    UsdBridgeVolumeWriterInternals()
      : GridStream(std::ios::in | std::ios::out)
    {}
    ~UsdBridgeVolumeWriterInternals()
    {}

    std::ostream& ResetStream()
    {
      GridStream.clear();
      return GridStream;
    }

    const char* GetStreamData()
    {
      StreamData = GridStream.str(); //copy, should be move
      return StreamData.c_str();
    }

    size_t GetStreamDataSize()
    {
      return StreamData.length();
    }

  protected:
    std::stringstream GridStream;
    std::string StreamData;
};

struct TfTransformInput
{
  ColorGridOutType::Ptr colorGrid;
  OpacityGridOutType::Ptr opacityGrid;
  const UsdBridgeVolumeData& volumeData;
  const openvdb::CoordBBox& bBox;
};

template<typename DataType, typename OpType, typename TransformerType>
struct TfTransform
{
public:
  TfTransform(const UsdBridgeVolumeData& volumeData, const openvdb::CoordBBox& bBox, const TransformerType& transformer)
    : Transformer(transformer)
    , VolData(static_cast<const DataType*>(volumeData.Data))
    , Dims(bBox.max() + openvdb::math::Coord(1, 1, 1)) //Bbox is inclusive, dims are exclusive
    , InvValueRangeMag(OpType(1.0) / (OpType)(volumeData.TfValueRange[1] - volumeData.TfValueRange[0]))
    , ValueRangeMin((OpType)(volumeData.TfValueRange[0]))
  {
  }

  inline void operator()(const typename TransformerType::Iter& iter) const
  {
    openvdb::math::Coord coord = iter.getCoord();

    assert(coord.x() >= 0 && coord.x() < Dims.x() &&
      coord.y() >= 0 && coord.y() < Dims.y() &&
      coord.z() >= 0 && coord.z() < Dims.z());
    size_t linearIndex = Dims.y() * Dims.x() * coord.z() + Dims.x() * coord.y() + coord.x();
    const DataType* curVal = VolData + linearIndex;

    OpType ucVal = (((OpType)(*curVal)) - this->ValueRangeMin) * this->InvValueRangeMag;
    float normValue = (float)((ucVal < (OpType)0.0) ? (OpType)0.0 : ((ucVal > (OpType)1.0) ? (OpType)1.0 : ucVal));

    Transformer.Transform(normValue, iter);
  }

  /*
  inline void operator()(
    const openvdb::FloatGrid::ValueOnCIter& iter,
    openvdb::tree::ValueAccessor<TreeOutType>& accessor)
  {
    openvdb::Vec4f transformedColor;
    transformColor(*iter, transformedColor);

    if (iter.isVoxelValue())
    { // set a single voxel
      accessor.setValue(iter.getCoord(), transformedColor);
    }
    else
    { // fill an entire tile
      openvdb::CoordBBox bbox;
      iter.getBoundingBox(bbox);
      accessor.getTree()->fill(bbox, transformedColor);
    }
  }
  */

  const TransformerType& Transformer;
  const DataType* VolData;
  openvdb::math::Coord Dims;
  OpType InvValueRangeMag;
  OpType ValueRangeMin;
};

struct TfColorTransformer
{
public:
  typedef ColorGridOutType::ValueOnIter Iter;

  TfColorTransformer(const UsdBridgeVolumeData& volumeData)
    : TfColors(static_cast<const float*>(volumeData.TfColors))
    , NumTfColors(volumeData.TfNumColors)
  {
  }

  inline void Transform(float normValue, const Iter& iter) const
  {
    openvdb::Vec3f transformedColor;

    float colorIndexF = normValue * (this->NumTfColors - 1);
    float floorColor;
    float fracColor = std::modf(colorIndexF, &floorColor);
    int colIdx0 = int(floorColor);
    int colIdx1 = colIdx0 + (colIdx0 != (this->NumTfColors - 1));

    const float* color0 = this->TfColors + 3 * colIdx0;
    const float* color1 = this->TfColors + 3 * colIdx1;
    float OneMinFracColor = 1.0f - fracColor;

    transformedColor[0] = fracColor * color1[0] + OneMinFracColor * color0[0];
    transformedColor[1] = fracColor * color1[1] + OneMinFracColor * color0[1];
    transformedColor[2] = fracColor * color1[2] + OneMinFracColor * color0[2];

#ifdef FLOAT1_OUTPUT
    iter.setValue(transformedColor.length());
#else
    iter.setValue(transformedColor);
#endif
  }

  const float* TfColors;
  int NumTfColors;
};

struct TfOpacityTransformer
{
public:
  typedef OpacityGridOutType::ValueOnIter Iter;

  TfOpacityTransformer(const UsdBridgeVolumeData& volumeData)
    : TfOpacities(static_cast<const float*>(volumeData.TfOpacities))
    , NumTfOpacities(volumeData.TfNumOpacities)
  {
  }

  inline void Transform(float normValue, const Iter& iter) const
  {
    float opacityIndexF = normValue * (this->NumTfOpacities - 1);
    float floorOpacity;
    float fracOpacity = std::modf(opacityIndexF, &floorOpacity);
    int opacityIdx0 = int(floorOpacity);
    int opacityIdx1 = opacityIdx0 + (opacityIdx0 != (this->NumTfOpacities - 1));

    float finalOpacity = fracOpacity * this->TfOpacities[opacityIdx1] +
      (1.0f - fracOpacity) * this->TfOpacities[opacityIdx0];

    iter.setValue(finalOpacity);
  }

  const float* TfOpacities;
  int NumTfOpacities;
};

template<typename DataType, typename OpType>
void TfTransformCall(TfTransformInput& tfTransformInput)
{
  TfColorTransformer colorTransformer(tfTransformInput.volumeData);
  TfTransform<DataType, OpType, TfColorTransformer> tfColorTransform(tfTransformInput.volumeData, tfTransformInput.bBox, colorTransformer);
  openvdb::tools::foreach(tfTransformInput.colorGrid->beginValueOn(), tfColorTransform);

  TfOpacityTransformer opacityTransformer(tfTransformInput.volumeData);
  TfTransform<DataType, OpType, TfOpacityTransformer> tfOpacityTransform(tfTransformInput.volumeData, tfTransformInput.bBox, opacityTransformer);
  openvdb::tools::foreach(tfTransformInput.opacityGrid->beginValueOn(), tfOpacityTransform);
}

static void SelectTfTransform(TfTransformInput& tfTransformInput)
{
  // Transform the float data to color data
  switch (tfTransformInput.volumeData.DataType)
  {
  case UsdBridgeType::CHAR:
    TfTransformCall<char, float>(tfTransformInput);
    break;
  case UsdBridgeType::UCHAR:
    TfTransformCall<unsigned char, float>(tfTransformInput);
    break;
  case UsdBridgeType::SHORT:
    TfTransformCall<short, float>(tfTransformInput);
    break;
  case UsdBridgeType::USHORT:
    TfTransformCall<unsigned short, float>(tfTransformInput);
    break;
  case UsdBridgeType::INT:
    TfTransformCall<int, double>(tfTransformInput);
    break;
  case UsdBridgeType::UINT:
    TfTransformCall<unsigned int, double>(tfTransformInput);
    break;
  case UsdBridgeType::LONG:
    TfTransformCall<long long, double>(tfTransformInput);
    break;
  case UsdBridgeType::ULONG:
    TfTransformCall<unsigned long long, double>(tfTransformInput);
    break;
  case UsdBridgeType::FLOAT:
    TfTransformCall<float, float>(tfTransformInput);
    break;
  case UsdBridgeType::DOUBLE:
    TfTransformCall<double, double>(tfTransformInput);
    break;
  default:
    {
      const char* typeStr = UsdBridgeTypeToString(tfTransformInput.volumeData.DataType);
      UsdBridgeLogMacro(UsdBridgeLogLevel::ERR, "Volume writer preclassified copy does not support source data type: " << typeStr);
      break;
    }
  }
  //openvdb::tools::transformValues(valArray->cbeginValueOn(), *colorGrid, transformOp);
}

struct CopyToGridInput
{
  const UsdBridgeVolumeData& volumeData;
  const openvdb::CoordBBox& bBox;
};

template<typename DataType>
struct NormalizedToGridConvert
{
  NormalizedToGridConvert(const UsdBridgeVolumeData& volumeData, const openvdb::CoordBBox& bBox)
    : VolData(static_cast<const DataType*>(volumeData.Data))
    , Dims(bBox.max() + openvdb::math::Coord(1, 1, 1)) //Bbox is inclusive, dims are exclusive
    , MaxValue(std::numeric_limits<DataType>::max())
    , MinValue((float)(std::numeric_limits<DataType>::min()))
  {
  }

  inline void operator()(const openvdb::FloatGrid::ValueOnIter& iter) const
  {
    openvdb::math::Coord coord = iter.getCoord();

    assert(coord.x() >= 0 && coord.x() < Dims.x() &&
      coord.y() >= 0 && coord.y() < Dims.y() &&
      coord.z() >= 0 && coord.z() < Dims.z());
    size_t linearIndex = Dims.y() * Dims.x() * coord.z() + Dims.x() * coord.y() + coord.x();
    const DataType* curVal = VolData + linearIndex;

    iter.setValue((((float)(*curVal)) - MinValue) / (MaxValue - MinValue));
  }

  const DataType* VolData;
  openvdb::math::Coord Dims;
  float MaxValue;
  float MinValue;
};

template<typename DataType>
openvdb::GridBase::Ptr NormalizedCopyToGridTemplate(const CopyToGridInput& copyInput)
{
  openvdb::FloatGrid::Ptr floatGrid = openvdb::FloatGrid::create();
  floatGrid->denseFill(copyInput.bBox, 0.0f, true);

  NormalizedToGridConvert<DataType> gridConverter(copyInput.volumeData, copyInput.bBox);
  openvdb::tools::foreach(floatGrid->beginValueOn(), gridConverter);

  return floatGrid;
}

template<typename DataType, typename GridType>
openvdb::GridBase::Ptr CopyToGridTemplate(const CopyToGridInput& copyInput)
{
  typename GridType::Ptr scalarGrid = GridType::create();

  openvdb::tools::Dense<const DataType> valArray(copyInput.bBox, static_cast<const DataType*>(copyInput.volumeData.Data));
  openvdb::tools::copyFromDense(valArray, *scalarGrid, (DataType)0); // No tolerance set to clamp values to background value for sparsity.

  return scalarGrid;
}

static openvdb::GridBase::Ptr CopyToGrid(const CopyToGridInput& copyInput)
{
  openvdb::GridBase::Ptr scalarGrid;
  // Transform the float data to color data
  switch (copyInput.volumeData.DataType)
  {
  case UsdBridgeType::CHAR:
    scalarGrid = NormalizedCopyToGridTemplate<char>(copyInput);
    break;
  case UsdBridgeType::UCHAR:
    scalarGrid = NormalizedCopyToGridTemplate<unsigned char>(copyInput);
    break;
  case UsdBridgeType::SHORT:
    scalarGrid = NormalizedCopyToGridTemplate<short>(copyInput);
    break;
  case UsdBridgeType::USHORT:
    scalarGrid = NormalizedCopyToGridTemplate<unsigned short>(copyInput);
    break;
  case UsdBridgeType::INT:
    scalarGrid = CopyToGridTemplate<int, openvdb::Int32Grid>(copyInput);
    break;
  case UsdBridgeType::UINT:
    scalarGrid = CopyToGridTemplate<unsigned int, openvdb::Int32Grid>(copyInput);
    break;
  case UsdBridgeType::LONG:
    scalarGrid = CopyToGridTemplate<long long, openvdb::Int64Grid>(copyInput);
    break;
  case UsdBridgeType::ULONG:
    scalarGrid = CopyToGridTemplate<unsigned long long, openvdb::Int64Grid>(copyInput);
    break;
  case UsdBridgeType::FLOAT:
    scalarGrid = CopyToGridTemplate<float, openvdb::FloatGrid>(copyInput);
    break;
  case UsdBridgeType::DOUBLE:
    scalarGrid = CopyToGridTemplate<double, openvdb::DoubleGrid>(copyInput);
    break;
  case UsdBridgeType::FLOAT3:
    scalarGrid = CopyToGridTemplate<openvdb::Vec3f, openvdb::Vec3fGrid>(copyInput);
    break;
  case UsdBridgeType::DOUBLE3:
    scalarGrid = CopyToGridTemplate<openvdb::Vec3d, openvdb::Vec3dGrid>(copyInput);
    break;
  default:
    {
      const char* typeStr = UsdBridgeTypeToString(copyInput.volumeData.DataType);
      UsdBridgeLogMacro(UsdBridgeLogLevel::ERR, "Volume writer source data copy does not support source data type: " << typeStr);
    }
    break;
  }
  return scalarGrid;
}

UsdBridgeVolumeWriter::UsdBridgeVolumeWriter()
  : Internals(std::make_unique<UsdBridgeVolumeWriterInternals>())
{
  
}

UsdBridgeVolumeWriter::~UsdBridgeVolumeWriter()
{
}

bool UsdBridgeVolumeWriter::Initialize(UsdBridgeLogCallback logCallback, void* logUserData)
{
  openvdb::initialize();

  UsdBridgeVolumeWriter::LogCallback = logCallback;
  UsdBridgeVolumeWriter::LogUserData = logUserData;

  return true;
}

void UsdBridgeVolumeWriter::ToVDB(const UsdBridgeVolumeData& volumeData)
{
  const char* densityGridName = "density";
  const char* colorGridName = "diffuse";

  // Keep the grid/tree transform at identity, and set coord bounding box to element dimensions. 
  // This will correspond to a worldspace size of element dimensions too, with rest of scaling handled outside of openvdb.
  const size_t* coordDims = volumeData.NumElements;
  size_t maxInt = std::numeric_limits<int>::max();
  assert(coordDims[0] <= maxInt && coordDims[1] <= maxInt && coordDims[2] <= maxInt);

  //openvdb::Mat4R mat; mat.setToScale(openvdb::Vec3f(1.0f, 1.0f, 1.0f));
  //colorGrid->setTransform(openvdb::math::Transform::createLinearTransform(mat));

  // Wrap data in a Dense and copy to color grid
  openvdb::CoordBBox bBox(0, 0, 0, int(coordDims[0] - 1), int(coordDims[1] - 1), int(coordDims[2] - 1)); //Fill is inclusive

  // Prepare output grids
  openvdb::GridPtrVecPtr grids(new openvdb::GridPtrVec);

  if(volumeData.preClassified)
  {
    OpacityGridOutType::Ptr opacityGrid = OpacityGridOutType::create();
    ColorGridOutType::Ptr colorGrid = ColorGridOutType::create();

    opacityGrid->denseFill(bBox, 0.0f, true);
    colorGrid->denseFill(bBox,
#ifdef FLOAT1_OUTPUT
      0.0f,
#else
      openvdb::Vec3f(0, 0, 0),
#endif
      true);

    // Transform the volumedata and output into color grid
    TfTransformInput tfTransformInput = { colorGrid, opacityGrid, volumeData, bBox };
    SelectTfTransform(tfTransformInput);

    // Set grid names
    opacityGrid->setName(densityGridName);
    colorGrid->setName(colorGridName);

    // Push color and opacity grid into grid container
    grids->push_back(opacityGrid);
    grids->push_back(colorGrid);
  }
  else
  {
    CopyToGridInput copyToGridInput = { volumeData, bBox };
    openvdb::GridBase::Ptr outGrid = CopyToGrid(copyToGridInput);

    if (outGrid)
    {
      outGrid->setName(densityGridName);

      // Push density grid into grid container
      grids->push_back(outGrid);
    }
  }

  // Must write all grids at once
  openvdb::io::Stream(Internals->ResetStream()).write(*grids);
}

void UsdBridgeVolumeWriter::GetSerializedVolumeData(const char*& data, size_t& size)
{
  data = Internals->GetStreamData();
  size = Internals->GetStreamDataSize();
}

#else //USE_OPENVDB

UsdBridgeVolumeWriter::UsdBridgeVolumeWriter()
{

}

UsdBridgeVolumeWriter::~UsdBridgeVolumeWriter()
{

}

bool UsdBridgeVolumeWriter::Initialize(UsdBridgeLogCallback logCallback, void* logUserData)
{
  return true;
}

void UsdBridgeVolumeWriter::ToVDB(const UsdBridgeVolumeData & volumeData)
{

}

void UsdBridgeVolumeWriter::GetSerializedVolumeData(const char*& data, size_t& size)
{
  data = nullptr;
  size = 0;
}

#endif //USE_OPENVDB

