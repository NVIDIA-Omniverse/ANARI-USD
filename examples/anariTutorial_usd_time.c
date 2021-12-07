// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include "anari/anari.h"
// stb_image
#include "stb_image_write.h"

const char *g_libraryType = "usd";

#ifdef _WIN32
const char* localOutputDir = "e:/usd/anari";
const char* texFile = "d:/models/texture.png";
#else
const char* localOutputDir = "/home/<username>/usd/anari";
const char* texFile = "/home/<username>/models/texture.png"; // Point this to any png
#endif

const char* wrapS = "repeat";
const char* wrapT = "repeat";

/******************************************************************/
void statusFunc(void *userData,
  ANARIDevice device,
  ANARIObject source,
  ANARIDataType sourceType,
  ANARIStatusSeverity severity,
  ANARIStatusCode code,
  const char *message)
{
  (void)userData;
  if (severity == ANARI_SEVERITY_FATAL_ERROR) {
    fprintf(stderr, "[FATAL] %s\n", message);
  }
  else if (severity == ANARI_SEVERITY_ERROR) {
    fprintf(stderr, "[ERROR] %s\n", message);
  }
  else if (severity == ANARI_SEVERITY_WARNING) {
    fprintf(stderr, "[WARN ] %s\n", message);
  }
  else if (severity == ANARI_SEVERITY_PERFORMANCE_WARNING) {
    fprintf(stderr, "[PERF ] %s\n", message);
  }
  else if (severity == ANARI_SEVERITY_INFO) {
    fprintf(stderr, "[INFO] %s\n", message);
  }
}

int main(int argc, const char **argv)
{
  stbi_flip_vertically_on_write(1);

  // image size
  int imgSize[2] = { 1024, 768 };

  // camera
  float cam_pos[] = {0.f, 0.f, 0.f};
  float cam_up[] = {0.f, 1.f, 0.f};
  float cam_view[] = {0.1f, 0.f, 1.f};

  float transform[12] = { 3.0f, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 3.0f, 2.0f, 3.0f, 4.0f };

  // triangle mesh data
  float vertex[] = {-1.0f,
      -1.0f,
      3.0f,
      -1.0f,
      1.0f,
      3.0f,
      1.0f,
      -1.0f,
      3.0f,
      0.1f,
      0.1f,
      0.3f};
  float color[] = {0.0f,
      0.0f,
      0.9f,
      1.0f,
      0.8f,
      0.8f,
      0.8f,
      1.0f,
      0.8f,
      0.8f,
      0.8f,
      1.0f,
      0.0f,
      0.9f,
      0.0f,
      1.0f};
  float texcoord[] = { 0.0f,
      0.0f,
      1.0f,
      0.0f,
      1.0f,
      1.0f,
      0.0f,
      1.0f };
  float sphereSizes[] = { 0.1f,
      2.0f,
      0.3f,
      0.05f };
  int32_t index[] = {0, 1, 2, 1, 2, 3};

  float kd[] = { 0.0f, 0.0f, 1.0f };

  printf("initialize ANARI...");

  ANARILibrary lib = anariLoadLibrary(g_libraryType, statusFunc, NULL);

  ANARIDevice dev = anariNewDevice(lib, "usd");

  if (!dev) {
    printf("\n\nERROR: could not load device '%s'\n", "usd");
    return 1;
  }

  int outputBinary = 0;
  int outputOmniverse = 0;
  int connLogVerbosity = 0;

  anariSetParameter(dev, dev, "usd::connection.logverbosity", ANARI_INT32, &connLogVerbosity);

  if (outputOmniverse)
  {
    anariSetParameter(dev, dev, "usd::serialize.hostname", ANARI_STRING, "ov-test");
    anariSetParameter(dev, dev, "usd::serialize.outputpath", ANARI_STRING, "/Users/test/anari");
  }
  else
  {
    anariSetParameter(dev, dev, "usd::serialize.outputpath", ANARI_STRING, localOutputDir);
  }
  anariSetParameter(dev, dev, "usd::serialize.outputbinary", ANARI_BOOL, &outputBinary);

  // commit device
  anariCommit(dev, dev);

  printf("done!\n");
  printf("setting up camera...");

  // create and setup camera
  ANARICamera camera = anariNewCamera(dev, "perspective");
  float aspect = imgSize[0] / (float)imgSize[1];
  anariSetParameter(dev, camera, "aspect", ANARI_FLOAT32, &aspect);
  anariSetParameter(dev, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
  anariSetParameter(dev, camera, "direction", ANARI_FLOAT32_VEC3, cam_view);
  anariSetParameter(dev, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
  anariCommit(dev, camera); // commit each object to indicate mods are done

  printf("done!\n");
  printf("setting up scene...");

  double timeValues[] = { 1, 0, 2, 3, 5, 8, 4, 6, 7, 9 };
  double geomTimeValues[] = { 3, 0, 4, 9, 7, 8, 6, 5, 1, 2 };
  double matTimeValues[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  int numTimeSteps = sizeof(timeValues) / sizeof(double);

  int useVertexColors = 1;

  // CREATE ALL TIMESTEPS:

  for (int timeIdx = 0; timeIdx < numTimeSteps; ++timeIdx)
  {
    int doubleNodes = ((timeIdx % 3) == 1);

    anariSetParameter(dev, dev, "usd::timestep", ANARI_FLOAT64, timeValues + timeIdx);

    ANARIWorld world = anariNewWorld(dev);

    // create and setup model and mesh
    ANARIGeometry mesh = anariNewGeometry(dev, "triangle");
    anariSetParameter(dev, mesh, "name", ANARI_STRING, "tutorialMesh");

    float scaledVertex[12];
    for (int v = 0; v < 12; ++v)
    {
      scaledVertex[v] = vertex[v] * (1.0f + timeIdx);
    }

    ANARIData array = anariNewArray1D(dev, scaledVertex, 0, 0, ANARI_FLOAT32_VEC3, 4, 0);
    anariCommit(dev, array);
    anariSetParameter(dev, mesh, "vertex.position", ANARI_ARRAY, &array);
    anariRelease(dev, array); // we are done using this handle

    if (useVertexColors)
    {
      array = anariNewArray1D(dev, color, 0, 0, ANARI_FLOAT32_VEC4, 4, 0);
      anariCommit(dev, array);
      anariSetParameter(dev, mesh, "vertex.color", ANARI_ARRAY, &array);
      anariRelease(dev, array);
    }

    array = anariNewArray1D(dev, texcoord, 0, 0, ANARI_FLOAT32_VEC2, 4, 0);
    anariCommit(dev, array);
    anariSetParameter(dev, mesh, "vertex.texcoord", ANARI_ARRAY, &array);
    anariRelease(dev, array);

    array = anariNewArray1D(dev, index, 0, 0, ANARI_UINT32_VEC3, 2, 0);
    anariCommit(dev, array);
    anariSetParameter(dev, mesh, "primitive.index", ANARI_ARRAY, &array);
    anariRelease(dev, array);

    int timeVarying = 0xFFFFFFFB;// Texcoords are not timeVarying
    anariSetParameter(dev, mesh, "usd::timevarying", ANARI_INT32, &timeVarying);
    anariSetParameter(dev, mesh, "usd::timestep", ANARI_FLOAT64, geomTimeValues + timeIdx);

    anariCommit(dev, mesh);

    ANARISampler sampler = anariNewSampler(dev, "texture2d");
    anariSetParameter(dev, sampler, "name", ANARI_STRING, "tutorialSampler_0");
    anariSetParameter(dev, sampler, "filename", ANARI_STRING, texFile);
    anariSetParameter(dev, sampler, "wrapMode1", ANARI_STRING, &wrapS);
    anariSetParameter(dev, sampler, "wrapMode2", ANARI_STRING, &wrapT);
    anariCommit(dev, sampler);

    ANARIMaterial mat = anariNewMaterial(dev, "matte");
    anariSetParameter(dev, mat, "name", ANARI_STRING, "tutorialMaterial_0");

    float opacity = 1.0;// -timeIdx * 0.1f;
    anariSetParameter(dev, mat, "usevertexcolors", ANARI_BOOL, &useVertexColors);
    if(!useVertexColors)
      anariSetParameter(dev, mat, "map_kd", ANARI_SAMPLER, &sampler);
    anariSetParameter(dev, mat, "color", ANARI_FLOAT32_VEC3, kd);
    anariSetParameter(dev, mat, "opacity", ANARI_FLOAT32, &opacity);
    anariSetParameter(dev, mat, "usd::timestep", ANARI_FLOAT64, matTimeValues + timeIdx);
    anariCommit(dev, mat);
    anariRelease(dev, sampler);

    // put the mesh into a model
    ANARISurface surface;
    surface = anariNewSurface(dev);
    anariSetParameter(dev, surface, "name", ANARI_STRING, "tutorialSurface_0");
    anariSetParameter(dev, surface, "geometry", ANARI_GEOMETRY, &mesh);
    anariSetParameter(dev, surface, "material", ANARI_MATERIAL, &mat);
    anariCommit(dev, surface);
    anariRelease(dev, mesh);
    anariRelease(dev, mat);

    // put the surface into a group
    ANARIGroup group;
    group = anariNewGroup(dev);
    anariSetParameter(dev, group, "name", ANARI_STRING, "tutorialGroup_0");
    array = anariNewArray1D(dev, &surface, 0, 0, ANARI_SURFACE, 1, 0);
    anariCommit(dev, array);
    anariSetParameter(dev, group, "surface", ANARI_ARRAY, &array);
    anariCommit(dev, group);
    anariRelease(dev, surface);
    anariRelease(dev, array);

    // put the group into an instance (give the group a world transform)
    ANARIInstance instance[2];
    instance[0] = anariNewInstance(dev);
    anariSetParameter(dev, instance[0], "name", ANARI_STRING, "tutorialInstance_0");
    anariSetParameter(dev, instance[0], "transform", ANARI_FLOAT32_MAT3x4, transform);
    anariSetParameter(dev, instance[0], "group", ANARI_GROUP, &group);
    anariRelease(dev, group);

    // create and setup light for Ambient Occlusion
    ANARILight light = anariNewLight(dev, "ambient");
    anariSetParameter(dev, light, "name", ANARI_STRING, "tutorialLight");
    anariCommit(dev, light);
    array = anariNewArray1D(dev, &light, 0, 0, ANARI_LIGHT, 1, 0);
    anariCommit(dev, array);
    anariSetParameter(dev, world, "light", ANARI_ARRAY, &array);
    anariRelease(dev, light);
    anariRelease(dev, array);

    anariCommit(dev, instance[0]);

    if (doubleNodes)
    {
      mesh = anariNewGeometry(dev, "sphere");
      anariSetParameter(dev, mesh, "name", ANARI_STRING, "tutorialPoints");

      array = anariNewArray1D(dev, vertex, 0, 0, ANARI_FLOAT32_VEC3, 4, 0);
      anariCommit(dev, array);
      anariSetParameter(dev, mesh, "vertex.position", ANARI_ARRAY, &array);
      anariRelease(dev, array); // we are done using this handle

      if (useVertexColors)
      {
        array = anariNewArray1D(dev, color, 0, 0, ANARI_FLOAT32_VEC4, 4, 0);
        anariCommit(dev, array);
        anariSetParameter(dev, mesh, "vertex.color", ANARI_ARRAY, &array);
        anariRelease(dev, array);
      }

      array = anariNewArray1D(dev, texcoord, 0, 0, ANARI_FLOAT32_VEC2, 4, 0);
      anariCommit(dev, array);
      anariSetParameter(dev, mesh, "vertex.texcoord", ANARI_ARRAY, &array);
      anariRelease(dev, array);

      array = anariNewArray1D(dev, sphereSizes, 0, 0, ANARI_FLOAT32, 4, 0);
      anariCommit(dev, array);
      anariSetParameter(dev, mesh, "vertex.radius", ANARI_ARRAY, &array);
      anariRelease(dev, array);

      anariSetParameter(dev, mesh, "usd::timestep", ANARI_FLOAT64, timeValues + timeIdx);

      anariCommit(dev, mesh);

      sampler = anariNewSampler(dev, "texture2d");
      anariSetParameter(dev, sampler, "name", ANARI_STRING, "tutorialSampler_1");
      anariSetParameter(dev, sampler, "filename", ANARI_STRING, texFile);
      anariSetParameter(dev, sampler, "wrapMode1", ANARI_STRING, &wrapS);
      anariSetParameter(dev, sampler, "wrapMode2", ANARI_STRING, &wrapT);
      anariCommit(dev, sampler);

      mat = anariNewMaterial(dev, "matte");
      anariSetParameter(dev, mat, "name", ANARI_STRING, "tutorialMaterial_1");
      anariSetParameter(dev, mat, "usevertexcolors", ANARI_BOOL, &useVertexColors);
      anariSetParameter(dev, mat, "color", ANARI_FLOAT32_VEC3, kd);
      anariSetParameter(dev, mat, "usd::timestep", ANARI_FLOAT64, timeValues + timeIdx);
      anariCommit(dev, mat);
      anariRelease(dev, sampler);

      // put the mesh into a model
      surface = anariNewSurface(dev);
      anariSetParameter(dev, surface, "name", ANARI_STRING, "tutorialSurface_1");
      anariSetParameter(dev, surface, "geometry", ANARI_GEOMETRY, &mesh);
      anariSetParameter(dev, surface, "material", ANARI_MATERIAL, &mat);
      anariCommit(dev, surface);
      anariRelease(dev, mesh);
      anariRelease(dev, mat);

      // put the surface into a group
      group = anariNewGroup(dev);
      anariSetParameter(dev, group, "name", ANARI_STRING, "tutorialGroup_1");
      array = anariNewArray1D(dev, &surface, 0, 0, ANARI_SURFACE, 1, 0);
      anariCommit(dev, array);
      anariSetParameter(dev, group, "surface", ANARI_ARRAY, &array);
      anariCommit(dev, group);
      anariRelease(dev, surface);
      anariRelease(dev, array);

      // put the group into an instance (give the group a world transform)
      instance[1] = anariNewInstance(dev);
      anariSetParameter(dev, instance[1], "name", ANARI_STRING, "tutorialInstance_1");
      anariSetParameter(dev, instance[1], "transform", ANARI_FLOAT32_MAT3x4, transform);
      anariSetParameter(dev, instance[1], "group", ANARI_GROUP, &group);
      anariRelease(dev, group);

      anariCommit(dev, instance[1]);
    }

    // put the instance in the world
    anariSetParameter(dev, world, "name", ANARI_STRING, "tutorialWorld");
    array = anariNewArray1D(dev, instance, 0, 0, ANARI_INSTANCE, (doubleNodes ? 2 : 1), 0);
    anariCommit(dev, array);
    anariSetParameter(dev, world, "instance", ANARI_ARRAY, &array);
    anariRelease(dev, instance[0]);
    if (doubleNodes)
      anariRelease(dev, instance[1]);
    anariRelease(dev, array);
    anariCommit(dev, world);

    // create renderer
    ANARIRenderer renderer =
      anariNewRenderer(dev, "pathtracer"); // choose path tracing renderer

    // complete setup of renderer
    float bgColor[4] = { 1.f, 1.f, 1.f, 1.f }; // white
    anariSetParameter(dev, renderer, "backgroundColor", ANARI_FLOAT32_VEC4, bgColor);
    anariCommit(dev, renderer);

    // create and setup frame
    ANARIFrame frame = anariNewFrame(dev);
    ANARIDataType colFormat = ANARI_UFIXED8_RGBA_SRGB;
    ANARIDataType depthFormat = ANARI_FLOAT32;
    anariSetParameter(dev, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(dev, frame, "color", ANARI_DATA_TYPE, &colFormat);
    anariSetParameter(dev, frame, "depth", ANARI_DATA_TYPE, &depthFormat);

    anariSetParameter(dev, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(dev, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(dev, frame, "world", ANARI_WORLD, &world);

    anariCommit(dev, frame);

    printf("rendering initial frame to firstFrame.png...");

    // render one frame
    anariRenderFrame(dev, frame);
    anariFrameReady(dev, frame, ANARI_WAIT);

    // final cleanups
    anariRelease(dev, renderer);
    anariRelease(dev, camera);
    anariRelease(dev, frame);
    anariRelease(dev, world);

    // USD-SPECIFIC RUNTIME:

    // Remove unused prims in usd
    // Only useful when objects are possibly removed over the whole timeline
    anariSetParameter(dev, dev, "usd::garbagecollect", ANARI_VOID_POINTER, 0);

    // Reset generation of unique names for next frame
    // Only necessary when relying upon auto-generation of names instead of manual creation, AND not retaining ANARI objects over timesteps
    anariSetParameter(dev, dev, "usd::removeunusednames", ANARI_VOID_POINTER, 0);

    // ~
  }


  // CHANGE THE SCENE: Attach only the instance_1 to the world, so instance_0 gets removed.

  for (int timeIdx = 0; timeIdx < numTimeSteps/2; ++timeIdx)
  {
    anariSetParameter(dev, dev, "usd::timestep", ANARI_FLOAT64, timeValues + timeIdx);

    ANARIWorld world = anariNewWorld(dev);

    ANARIArray1D array;
    ANARIInstance instance;
    ANARIGeometry mesh;
    ANARISampler sampler;
    ANARIMaterial mat;
    ANARISurface surface;
    ANARIGroup group;

    {
      mesh = anariNewGeometry(dev, "sphere");
      anariSetParameter(dev, mesh, "name", ANARI_STRING, "tutorialPoints");

      ANARIArray1D array = anariNewArray1D(dev, vertex, 0, 0, ANARI_FLOAT32_VEC3, 4, 0);
      anariCommit(dev, array);
      anariSetParameter(dev, mesh, "vertex.position", ANARI_ARRAY, &array);
      anariRelease(dev, array); // we are done using this handle

      if (useVertexColors)
      {
        array = anariNewArray1D(dev, color, 0, 0, ANARI_FLOAT32_VEC4, 4, 0);
        anariCommit(dev, array);
        anariSetParameter(dev, mesh, "vertex.color", ANARI_ARRAY, &array);
        anariRelease(dev, array);
      }

      array = anariNewArray1D(dev, texcoord, 0, 0, ANARI_FLOAT32_VEC2, 4, 0);
      anariCommit(dev, array);
      anariSetParameter(dev, mesh, "vertex.texcoord", ANARI_ARRAY, &array);
      anariRelease(dev, array);

      array = anariNewArray1D(dev, sphereSizes, 0, 0, ANARI_FLOAT32, 4, 0);
      anariCommit(dev, array);
      anariSetParameter(dev, mesh, "vertex.radius", ANARI_ARRAY, &array);
      anariRelease(dev, array);

      anariSetParameter(dev, mesh, "usd::timestep", ANARI_FLOAT64, timeValues + timeIdx*2); // Switch the child timestep for something else

      anariCommit(dev, mesh);

      sampler = anariNewSampler(dev, "texture2d");
      anariSetParameter(dev, sampler, "name", ANARI_STRING, "tutorialSampler_1");
      anariSetParameter(dev, sampler, "filename", ANARI_STRING, texFile);
      anariSetParameter(dev, sampler, "wrapMode1", ANARI_STRING, &wrapS);
      anariSetParameter(dev, sampler, "wrapMode2", ANARI_STRING, &wrapT);
      anariCommit(dev, sampler);

      mat = anariNewMaterial(dev, "matte");
      anariSetParameter(dev, mat, "name", ANARI_STRING, "tutorialMaterial_1");
      anariSetParameter(dev, mat, "usevertexcolors", ANARI_BOOL, &useVertexColors);
      //anariSetParameter(dev, mat, "map_kd", ANARI_SAMPLER, &sampler);
      anariSetParameter(dev, mat, "color", ANARI_FLOAT32_VEC3, kd);
      anariSetParameter(dev, mat, "usd::timestep", ANARI_FLOAT64, timeValues + timeIdx*2);
      anariCommit(dev, mat);
      anariRelease(dev, sampler);

      // put the mesh into a model
      surface = anariNewSurface(dev);
      anariSetParameter(dev, surface, "name", ANARI_STRING, "tutorialSurface_1");
      anariSetParameter(dev, surface, "geometry", ANARI_GEOMETRY, &mesh);
      anariSetParameter(dev, surface, "material", ANARI_MATERIAL, &mat);
      anariCommit(dev, surface);
      anariRelease(dev, mesh);
      anariRelease(dev, mat);

      // put the surface into a group
      group = anariNewGroup(dev);
      anariSetParameter(dev, group, "name", ANARI_STRING, "tutorialGroup_1");
      array = anariNewArray1D(dev, &surface, 0, 0, ANARI_SURFACE, 1, 0);
      anariCommit(dev, array);
      anariSetParameter(dev, group, "surface", ANARI_ARRAY, &array);
      anariCommit(dev, group);
      anariRelease(dev, surface);
      anariRelease(dev, array);

      // put the group into an instance (give the group a world transform)
      instance = anariNewInstance(dev);
      anariSetParameter(dev, instance, "name", ANARI_STRING, "tutorialInstance_1");
      anariSetParameter(dev, instance, "transform", ANARI_FLOAT32_MAT3x4, transform);
      anariSetParameter(dev, instance, "group", ANARI_GROUP, &group);
      anariRelease(dev, group);

      anariCommit(dev, instance);
    }

    // put the instance in the world
    anariSetParameter(dev, world, "name", ANARI_STRING, "tutorialWorld");
    array = anariNewArray1D(dev, &instance, 0, 0, ANARI_INSTANCE, 1, 0);
    anariCommit(dev, array);
    anariSetParameter(dev, world, "instance", ANARI_ARRAY, &array);
    anariRelease(dev, instance);
    anariRelease(dev, array);

    anariCommit(dev, world);

    // create renderer
    ANARIRenderer renderer =
      anariNewRenderer(dev, "pathtracer"); // choose path tracing renderer

    // complete setup of renderer
    float bgColor[4] = { 1.f, 1.f, 1.f, 1.f }; // white
    anariSetParameter(dev, renderer, "backgroundColor", ANARI_FLOAT32_VEC4, bgColor);
    anariCommit(dev, renderer);

    // create and setup frame
    ANARIFrame frame = anariNewFrame(dev);
    ANARIDataType colFormat = ANARI_UFIXED8_RGBA_SRGB;
    ANARIDataType depthFormat = ANARI_FLOAT32;
    anariSetParameter(dev, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(dev, frame, "color", ANARI_DATA_TYPE, &colFormat);
    anariSetParameter(dev, frame, "depth", ANARI_DATA_TYPE, &depthFormat);

    anariSetParameter(dev, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(dev, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(dev, frame, "world", ANARI_WORLD, &world);

    anariCommit(dev, frame);

    printf("rendering initial frame to firstFrame.png...");

    // render one frame
    anariRenderFrame(dev, frame);
    anariFrameReady(dev, frame, ANARI_WAIT);

    // final cleanups
    anariRelease(dev, renderer);
    anariRelease(dev, camera);
    anariRelease(dev, frame);
    anariRelease(dev, world);


    // USD-SPECIFIC RUNTIME:

    // Remove unused prims in usd
    // Only useful when objects are possibly removed over the whole timeline
    anariSetParameter(dev, dev, "usd::garbagecollect", ANARI_VOID_POINTER, 0);

    // Reset generation of unique names for next frame
    // Only necessary when relying upon auto-generation of names instead of manual creation, AND not retaining ANARI objects over timesteps
    anariSetParameter(dev, dev, "usd::removeunusednames", ANARI_VOID_POINTER, 0);

    // ~
  }

  anariRelease(dev, dev);

  printf("done!\n");

  return 0;
}
