// Copyright 2021 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0

// MPI parallel USD output example.
// Each MPI rank creates a triangle mesh at a unique position.
// The USD device writes each rank's output into its own subdirectory
// and rank 0 produces an encapsulating ParallelScene.usda that
// references all ranks.
//
// Build:  cmake -DUSD_DEVICE_BUILD_EXAMPLES=ON -DUSD_DEVICE_MPI_ENABLED=ON ..
// Run:    mpirun -np 4 ./anariTutorialUsdMpi

#include <cstdio>
#include <cstdlib>
#include <array>
#include <mpi.h>

#define ANARI_EXTENSION_UTILITY_IMPL
#include "anari/anari_cpp.hpp"
#include "anari/anari_cpp/ext/std.h"

using uvec2 = std::array<unsigned int, 2>;
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
  if (severity == ANARI_SEVERITY_FATAL_ERROR)
    fprintf(stderr, "[FATAL] %s\n", message);
  else if (severity == ANARI_SEVERITY_ERROR)
    fprintf(stderr, "[ERROR] %s\n", message);
  else if (severity == ANARI_SEVERITY_WARNING)
    fprintf(stderr, "[WARN ] %s\n", message);
  else if (severity == ANARI_SEVERITY_PERFORMANCE_WARNING)
    fprintf(stderr, "[PERF ] %s\n", message);
  else if (severity == ANARI_SEVERITY_INFO)
    fprintf(stderr, "[INFO ] %s\n", message);
}

int main(int argc, char **argv)
{
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  printf("[rank %d/%d] Starting MPI USD tutorial\n", rank, size);

  // image size
  uvec2 imgSize = {1024, 768};

  // camera (shared across ranks, but only matters for rendering)
  vec3 cam_pos = {(float)size, 0.f, -5.f};
  vec3 cam_up = {0.f, 1.f, 0.f};
  vec3 cam_view = {0.f, 0.f, 1.f};

  // Each rank creates a triangle mesh offset along X by its rank index.
  float xOffset = rank * 3.0f;

  vec3 vertex[] = {
    {-1.0f + xOffset, -1.0f, 3.0f},
    {-1.0f + xOffset,  1.0f, 3.0f},
    { 1.0f + xOffset, -1.0f, 3.0f},
    { 0.1f + xOffset,  0.1f, 0.3f}
  };

  // Color varies per rank
  float r = (rank % 3 == 0) ? 0.9f : 0.2f;
  float g = (rank % 3 == 1) ? 0.9f : 0.2f;
  float b = (rank % 3 == 2) ? 0.9f : 0.2f;

  vec4 color[] = {
    {r, g, b, 1.0f},
    {0.8f, 0.8f, 0.8f, 1.0f},
    {0.8f, 0.8f, 0.8f, 1.0f},
    {g, b, r, 1.0f}
  };

  ivec3 index[] = {{0, 1, 2}, {1, 2, 3}};

  // --- ANARI setup ---

  anari::Library lib = anari::loadLibrary("usd", statusFunc);

  anari::Extensions extensions =
    anari::extension::getDeviceExtensionStruct(lib, "default");

  if (!extensions.ANARI_KHR_DATA_PARALLEL_MPI)
    printf("[rank %d] WARNING: device doesn't report ANARI_KHR_DATA_PARALLEL_MPI\n", rank);

  ANARIDevice d = anariNewDevice(lib, "default");

  // Set output path (same for all ranks — the device handles rank subdirs)
  anari::setParameter(d, d, "usd::serialize.location", "./mpi_output");

  // Set the MPI communicator — this is the key KHR_DATA_PARALLEL_MPI parameter.
  // The device receives a pointer to the MPI_Comm via ANARI_VOID_POINTER.
  MPI_Comm comm = MPI_COMM_WORLD;
  void* commPtr = &comm;
  anariSetParameter(d, d, "mpiCommunicator", ANARI_VOID_POINTER, &commPtr);

  // Commit device parameters (triggers bridge initialization with MPI sync)
  anari::commitParameters(d, d);

  printf("[rank %d] Device committed, setting up scene...\n", rank);

  // camera
  auto camera = anari::newObject<anari::Camera>(d, "perspective");
  anari::setParameter(d, camera, "aspect", (float)imgSize[0] / (float)imgSize[1]);
  anari::setParameter(d, camera, "position", cam_pos);
  anari::setParameter(d, camera, "direction", cam_view);
  anari::setParameter(d, camera, "up", cam_up);
  anari::commitParameters(d, camera);

  // world
  auto world = anari::newObject<anari::World>(d);

  // geometry — each rank creates its own mesh at a different position
  auto mesh = anari::newObject<anari::Geometry>(d, "triangle");
  anari::setAndReleaseParameter(
      d, mesh, "vertex.position", anari::newArray1D(d, vertex, 4));
  anari::setAndReleaseParameter(
      d, mesh, "vertex.color", anari::newArray1D(d, color, 4));
  anari::setAndReleaseParameter(
      d, mesh, "primitive.index", anari::newArray1D(d, index, 2));
  anari::commitParameters(d, mesh);

  // material
  auto mat = anari::newObject<anari::Material>(d, "matte");
  anari::setParameter(d, mat, "color", "color");
  anari::commitParameters(d, mat);

  // surface
  auto surface = anari::newObject<anari::Surface>(d);
  anari::setAndReleaseParameter(d, surface, "geometry", mesh);
  anari::setAndReleaseParameter(d, surface, "material", mat);
  anari::commitParameters(d, surface);

  // add surface to world
  anari::setAndReleaseParameter(
      d, world, "surface", anari::newArray1D(d, &surface));
  anari::release(d, surface);

  // light
  auto light = anari::newObject<anari::Light>(d, "directional");
  anari::commitParameters(d, light);
  anari::setAndReleaseParameter(
      d, world, "light", anari::newArray1D(d, &light));
  anari::release(d, light);

  anari::commitParameters(d, world);

  // renderer
  auto renderer = anari::newObject<anari::Renderer>(d, "default");
  anari::setParameter(d, renderer, "name", "MainRenderer");
  vec4 bgColor = {1.f, 1.f, 1.f, 1.f};
  anari::setParameter(d, renderer, "backgroundColor", bgColor);
  anari::commitParameters(d, renderer);

  // frame
  auto frame = anari::newObject<anari::Frame>(d);
  anari::setParameter(d, frame, "size", imgSize);
  anari::setParameter(d, frame, "channel.color", ANARI_UFIXED8_RGBA_SRGB);
  anari::setAndReleaseParameter(d, frame, "renderer", renderer);
  anari::setAndReleaseParameter(d, frame, "camera", camera);
  anari::setAndReleaseParameter(d, frame, "world", world);
  anari::commitParameters(d, frame);

  printf("[rank %d] Rendering...\n", rank);

  // render one frame — this triggers USD output
  anari::render(d, frame);
  anari::wait(d, frame);

  printf("[rank %d] Done rendering, cleaning up...\n", rank);

  // cleanup
  anari::release(d, frame);
  anari::release(d, d);
  anari::unloadLibrary(lib);

  MPI_Finalize();

  printf("[rank %d] Finished.\n", rank);
  return 0;
}
