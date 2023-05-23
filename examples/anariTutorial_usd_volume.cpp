// Copyright 2021 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <array>
#include <random>

// anari
#define ANARI_FEATURE_UTILITY_IMPL
#include "anari/anari_cpp.hpp"
#include "anari/anari_cpp/ext/std.h"

// stb_image
#include "stb_image_write.h"

using uvec2 = std::array<unsigned int, 2>;
using uvec3 = std::array<unsigned int, 3>;
using ivec3 = std::array<int, 3>;
using vec3 = std::array<float, 3>;
using vec4 = std::array<float, 4>;
using box3 = std::array<vec3, 2>;

void statusFunc(const void *userData,
    ANARIDevice device,
    ANARIObject source,
    ANARIDataType sourceType,
    ANARIStatusSeverity severity,
    ANARIStatusCode code,
    const char *message)
{
  (void)userData;
  (void)device;
  (void)source;
  (void)sourceType;
  (void)code;
  if (severity == ANARI_SEVERITY_FATAL_ERROR) {
    fprintf(stderr, "[FATAL] %s\n", message);
  } else if (severity == ANARI_SEVERITY_ERROR) {
    fprintf(stderr, "[ERROR] %s\n", message);
  } else if (severity == ANARI_SEVERITY_WARNING) {
    fprintf(stderr, "[WARN ] %s\n", message);
  } else if (severity == ANARI_SEVERITY_PERFORMANCE_WARNING) {
    fprintf(stderr, "[PERF ] %s\n", message);
  } else if (severity == ANARI_SEVERITY_INFO) {
    fprintf(stderr, "[INFO ] %s\n", message);
  } else if (severity == ANARI_SEVERITY_DEBUG) {
    fprintf(stderr, "[DEBUG] %s\n", message);
  }
}

struct ParameterInfo
{
  const char* m_name;
  bool m_param;
  const char* m_description;
};

struct GravityVolume
{
  GravityVolume(anari::Device d);
  ~GravityVolume();

  std::vector<ParameterInfo> parameters();

  anari::World world();

  void commit();

 private:
  anari::Device m_device{nullptr};
  anari::World m_world{nullptr};
};

struct Point
{
  vec3 center;
  float weight;
};

static std::vector<Point> generatePoints(size_t numPoints)
{
  // create random number distributions for point center and weight
  std::mt19937 gen(0);

  std::uniform_real_distribution<float> centerDistribution(-1.f, 1.f);
  std::uniform_real_distribution<float> weightDistribution(0.1f, 0.3f);

  // populate the points
  std::vector<Point> points(numPoints);

  for (auto &p : points) {
    p.center[0] = centerDistribution(gen);
    p.center[1] = centerDistribution(gen);
    p.center[2] = centerDistribution(gen);

    p.weight = weightDistribution(gen);
  }

  return points;
}

static std::vector<float> generateVoxels(
    const std::vector<Point> &points, ivec3 dims)
{
  // get world coordinate in [-1.f, 1.f] from logical coordinates in [0,
  // volumeDimension)
  auto logicalToWorldCoordinates = [&](int i, int j, int k) {
    vec3 result = {-1.f + float(i) / float(dims[0] - 1) * 2.f,
        -1.f + float(j) / float(dims[1] - 1) * 2.f,
        -1.f + float(k) / float(dims[2] - 1) * 2.f};
    return result;
  };

  // generate voxels
  std::vector<float> voxels(size_t(dims[0]) * size_t(dims[1]) * size_t(dims[2]));

  for (int k = 0; k < dims[2]; k++) {
    for (int j = 0; j < dims[1]; j++) {
      for (int i = 0; i < dims[0]; i++) {
        // index in array
        size_t index = size_t(k) * size_t(dims[2]) * size_t(dims[1])
            + size_t(j) * size_t(dims[0]) + size_t(i);

        // compute volume value
        float value = 0.f;

        for (auto &p : points) {
          vec3 pointCoordinate = logicalToWorldCoordinates(i, j, k);
          const float distanceSq = 
            std::powf(pointCoordinate[0] - p.center[0], 2.0f) +
            std::powf(pointCoordinate[1] - p.center[1], 2.0f) +
            std::powf(pointCoordinate[2] - p.center[2], 2.0f);

          // contribution proportional to weighted inverse-square distance
          // (i.e. gravity)
          value += p.weight / distanceSq;
        }

        voxels[index] = value;
      }
    }
  }

  return voxels;
}

GravityVolume::GravityVolume(anari::Device d)
{
  m_device = d;
  m_world = anari::newObject<anari::World>(m_device);
}

GravityVolume::~GravityVolume()
{
  anari::release(m_device, m_world);
}

std::vector<ParameterInfo> GravityVolume::parameters()
{
  return {
      {"withGeometry", false, "Include geometry inside the volume?"}
      //
  };
}

anari::World GravityVolume::world()
{
  return m_world;
}

void GravityVolume::commit()
{
  anari::Device d = m_device;

  const bool withGeometry = false;//getParam<bool>("withGeometry", false);
  const int volumeDims = 128;
  const size_t numPoints = 10;
  const float voxelRange[2] = {0.f, 10.f};

  ivec3 volumeDims3 = {volumeDims,volumeDims,volumeDims};
  vec3 fieldOrigin = {-1.f, -1.f, -1.f};
  vec3 fieldSpacing = {2.f / volumeDims, 2.f / volumeDims, 2.f / volumeDims};

  auto points = generatePoints(numPoints);
  auto voxels = generateVoxels(points, volumeDims3);

  auto field = anari::newObject<anari::SpatialField>(d, "structuredRegular");
  anari::setParameter(d, field, "origin", fieldOrigin);
  anari::setParameter(d, field, "spacing", fieldSpacing);
  anari::setAndReleaseParameter(d,
      field,
      "data",
      anari::newArray3D(d, voxels.data(), volumeDims, volumeDims, volumeDims));
  anari::commitParameters(d, field);

  auto volume = anari::newObject<anari::Volume>(d, "scivis");
  anari::setAndReleaseParameter(d, volume, "field", field);

  {
    std::vector<vec3> colors;
    std::vector<float> opacities;

    colors.emplace_back(vec3{0.f, 0.f, 1.f});
    colors.emplace_back(vec3{0.f, 1.f, 0.f});
    colors.emplace_back(vec3{1.f, 0.f, 0.f});

    opacities.emplace_back(0.f);
    opacities.emplace_back(0.05f);
    opacities.emplace_back(0.1f);

    anari::setAndReleaseParameter(
        d, volume, "color", anari::newArray1D(d, colors.data(), colors.size()));
    anari::setAndReleaseParameter(d,
        volume,
        "opacity",
        anari::newArray1D(d, opacities.data(), opacities.size()));
    anariSetParameter(d, volume, "valueRange", ANARI_FLOAT32_BOX1, voxelRange);
  }

  anari::commitParameters(d, volume);

  if (withGeometry) {
    std::vector<vec3> positions(numPoints);
    std::transform(
        points.begin(), points.end(), positions.begin(), [](const Point &p) {
          return p.center;
        });

    ANARIGeometry geom = anari::newObject<anari::Geometry>(d, "sphere");
    anari::setAndReleaseParameter(d,
        geom,
        "vertex.position",
        anari::newArray1D(d, positions.data(), positions.size()));
    anari::setParameter(d, geom, "radius", 0.05f);
    anari::commitParameters(d, geom);

    auto mat = anari::newObject<anari::Material>(d, "matte");
    anari::commitParameters(d, mat);

    auto surface = anari::newObject<anari::Surface>(d);
    anari::setAndReleaseParameter(d, surface, "geometry", geom);
    anari::setAndReleaseParameter(d, surface, "material", mat);
    anari::commitParameters(d, surface);

    anari::setAndReleaseParameter(
        d, m_world, "surface", anari::newArray1D(d, &surface));
    anari::release(d, surface);
  } else {
    anari::unsetParameter(d, m_world, "surface");
  }

  anari::setAndReleaseParameter(
      d, m_world, "volume", anari::newArray1D(d, &volume));
  anari::release(d, volume);

  // create and setup light
  auto light = anari::newObject<anari::Light>(d, "directional");
  anari::commitParameters(d, light);
  anari::setAndReleaseParameter(
      d, m_world, "light", anari::newArray1D(d, &light));
  anari::release(d, light);

  anari::commitParameters(d, m_world);
}


int main(int argc, const char **argv)
{
  (void)argc;
  (void)argv;
  stbi_flip_vertically_on_write(1);

  // image size
  uvec2 imgSize = {1024 /*width*/, 768 /*height*/};

  // camera
  vec3 cam_pos = {0.f, 0.f, 0.f};
  vec3 cam_up = {0.f, 1.f, 0.f};
  vec3 cam_view = {0.1f, 0.f, 1.f};

  // triangle mesh array
  vec3 vertex[] = {{-1.0f, -1.0f, 3.0f},
      {-1.0f, 1.0f, 3.0f},
      {1.0f, -1.0f, 3.0f},
      {0.1f, 0.1f, 0.3f}};
  vec4 color[] = {{0.9f, 0.5f, 0.5f, 1.0f},
      {0.8f, 0.8f, 0.8f, 1.0f},
      {0.8f, 0.8f, 0.8f, 1.0f},
      {0.5f, 0.9f, 0.5f, 1.0f}};
  uvec3 index[] = {{0, 1, 2}, {1, 2, 3}};

  printf("initialize ANARI...");

  anari::Library lib = anari::loadLibrary("usd", statusFunc);

  anari::Features features = anari::feature::getObjectFeatures(
      lib, "default", "default", ANARI_DEVICE);

  if (!features.ANARI_KHR_GEOMETRY_TRIANGLE)
    printf("WARNING: device doesn't support ANARI_KHR_GEOMETRY_TRIANGLE\n");
  if (!features.ANARI_KHR_CAMERA_PERSPECTIVE)
    printf("WARNING: device doesn't support ANARI_KHR_CAMERA_PERSPECTIVE\n");
  if (!features.ANARI_KHR_LIGHT_DIRECTIONAL)
    printf("WARNING: device doesn't support ANARI_KHR_LIGHT_DIRECTIONAL\n");
  if (!features.ANARI_KHR_MATERIAL_MATTE)
    printf("WARNING: device doesn't support ANARI_KHR_MATERIAL_MATTE\n");

  ANARIDevice d = anariNewDevice(lib, "default");

  printf("done!\n");
  printf("setting up camera...");

  // create and setup camera
  auto camera = anari::newObject<anari::Camera>(d, "perspective");
  anari::setParameter(
      d, camera, "aspect", (float)imgSize[0] / (float)imgSize[1]);
  anari::setParameter(d, camera, "position", cam_pos);
  anari::setParameter(d, camera, "direction", cam_view);
  anari::setParameter(d, camera, "up", cam_up);
  anari::commitParameters(d, camera); // commit objects to indicate setting parameters is done

  printf("done!\n");
  printf("setting up scene...");

  GravityVolume* volumeScene = new GravityVolume(d);
  volumeScene->commit();
  anari::World world = volumeScene->world();

  printf("setting up renderer...");

  // create renderer
  auto renderer = anari::newObject<anari::Renderer>(d, "default");
  // objects can be named for easier identification in debug output etc.
  anari::setParameter(d, renderer, "name", "MainRenderer");

  printf("done!\n");

  // complete setup of renderer
  vec4 bgColor = {1.f, 1.f, 1.f, 1.f};
  anari::setParameter(d, renderer, "backgroundColor", bgColor); // white
  anari::commitParameters(d, renderer);

  // create and setup frame
  auto frame = anari::newObject<anari::Frame>(d);
  anari::setParameter(d, frame, "size", imgSize);
  anari::setParameter(d, frame, "channel.color", ANARI_UFIXED8_RGBA_SRGB);

  anari::setAndReleaseParameter(d, frame, "renderer", renderer);
  anari::setAndReleaseParameter(d, frame, "camera", camera);
  anari::setParameter(d, frame, "world", world);

  anari::commitParameters(d, frame);

  printf("rendering out USD objects via anari::renderFrame()...");

  // render one frame
  anari::render(d, frame);
  anari::wait(d, frame);

  printf("done!\n");
  printf("\ncleaning up objects...");

  // final cleanups
  anari::release(d, frame);
  delete volumeScene;
  anari::release(d, d);
  anari::unloadLibrary(lib);

  printf("done!\n");
  

  return 0;
}
