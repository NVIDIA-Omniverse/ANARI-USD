// Copyright 2021 NVIDIA Corporation
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

#include "anariTutorial_usd_common.h"

int main(int argc, const char **argv)
{
  stbi_flip_vertically_on_write(1);

  // image size
  int frameSize[2] = { 1024, 768 };
  int textureSize[2] = { 256, 256 };

  uint8_t* textureData = 0;
  int numTexComponents = 3;
  textureData = generateTexture(textureSize, numTexComponents);

  // camera
  float cam_pos[] = {0.f, 0.f, 0.f};
  float cam_up[] = {0.f, 1.f, 0.f};
  float cam_view[] = {0.1f, 0.f, 1.f};

  float transform[16] = {
    3.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 3.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 3.0f, 0.0f,
    2.0f, 3.0f, 4.0f, 1.0f };

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
  float texcoord[] = {
      0.0f,
      0.0f,
      1.0f,
      0.0f,
      1.0f,
      1.0f,
      0.0f,
      1.0f};
  float sphereSizes[] = { 0.1f,
      2.0f,
      0.3f,
      0.05f };
  int32_t index[] = {0, 1, 2, 1, 2, 3};

  float protoVertex[] = {-3.0f,
      -1.0f,
      -1.0f,
      3.0f,
      -1.0f,
      -1.0f,
      -3.0f,
      1.0f,
      -1.0f,
      3.0f,
      1.0f,
      -1.0f,
      -3.0f,
      -1.0f,
      1.0f,
      3.0f,
      -1.0f,
      1.0f,
      -3.0f,
      1.0f,
      1.0f,
      3.0f,
      1.0f,
      1.0f};
  float protoColor[] = {0.0f,
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
      1.0f,
      0.0f,
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
  float protoTexcoord[] = {
      0.0f,
      0.0f,
      0.0f,
      1.0f,
      0.25f,
      0.0f,
      0.25f,
      1.0f,
      0.5f,
      0.0f,
      0.5f,
      1.0f,
      0.75f,
      0.0f,
      0.75f,
      1.0f};
  int32_t protoIndex[] = {0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5, 4, 5, 6, 6, 5, 7, 6, 7, 0, 0, 7, 1};
  float protoTransform[16] = {
    0.2f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.2f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.2f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f };

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

  anariSetParameter(dev, dev, "usd::connection.logVerbosity", ANARI_INT32, &connLogVerbosity);

  if (outputOmniverse)
  {
    anariSetParameter(dev, dev, "usd::serialize.hostName", ANARI_STRING, "ov-test");
    anariSetParameter(dev, dev, "usd::serialize.location", ANARI_STRING, "/Users/test/anari");
  }
  anariSetParameter(dev, dev, "usd::serialize.outputBinary", ANARI_BOOL, &outputBinary);

  // commit device
  anariCommitParameters(dev, dev);

  printf("done!\n");
  printf("setting up camera...");

  // create and setup camera
  ANARICamera camera = anariNewCamera(dev, "perspective");
  float aspect = frameSize[0] / (float)frameSize[1];
  anariSetParameter(dev, camera, "aspect", ANARI_FLOAT32, &aspect);
  anariSetParameter(dev, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
  anariSetParameter(dev, camera, "direction", ANARI_FLOAT32_VEC3, cam_view);
  anariSetParameter(dev, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
  anariCommitParameters(dev, camera); // commit each object to indicate mods are done

  // Setup texture array
  ANARIArray2D texArray = anariNewArray2D(dev, textureData, 0, 0, ANARI_UINT8_VEC3, textureSize[0], textureSize[1]); // Make sure this matches numTexComponents

  printf("done!\n");
  printf("setting up scene...");

  double timeValues[] = { 1, 0, 2, 3, 5, 8, 4, 6, 7, 9, 10 };
  double geomTimeValues[] = { 3, 0, 4, 9, 7, 8, 6, 5, 1, 2, 10 };
  double matTimeValues[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
  int numTimeSteps = sizeof(timeValues) / sizeof(double);

  int useVertexColors = 0;
  int useTexture = 1;
  int useUsdGeomPoints = 0;

  // CREATE ALL TIMESTEPS:

  for (int timeIdx = 0; timeIdx < numTimeSteps; ++timeIdx)
  {
    int doubleNodes = ((timeIdx % 3) == 1);

    anariSetParameter(dev, dev, "usd::time", ANARI_FLOAT64, timeValues + timeIdx);
    anariCommitParameters(dev, dev);

    ANARIWorld world = anariNewWorld(dev);

    // create and setup model and mesh
    ANARIGeometry mesh = anariNewGeometry(dev, "triangle");
    anariSetParameter(dev, mesh, "name", ANARI_STRING, "tutorialMesh");

    float scaledVertex[12];
    for (int v = 0; v < 12; ++v)
    {
      scaledVertex[v] = vertex[v] * (1.0f + timeIdx);
    }

    ANARIArray1D array = anariNewArray1D(dev, scaledVertex, 0, 0, ANARI_FLOAT32_VEC3, 4);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, mesh, "vertex.position", ANARI_ARRAY, &array);
    anariRelease(dev, array); // we are done using this handle

    if (useVertexColors)
    {
      array = anariNewArray1D(dev, color, 0, 0, ANARI_FLOAT32_VEC4, 4);
      anariCommitParameters(dev, array);
      anariSetParameter(dev, mesh, "vertex.color", ANARI_ARRAY, &array);
      anariRelease(dev, array);
    }

    array = anariNewArray1D(dev, texcoord, 0, 0, ANARI_FLOAT32_VEC2, 4);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, mesh, "vertex.attribute0", ANARI_ARRAY, &array);
    anariRelease(dev, array);

    array = anariNewArray1D(dev, index, 0, 0, ANARI_INT32_VEC3, 2);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, mesh, "primitive.index", ANARI_ARRAY, &array);
    anariRelease(dev, array);

    int timeVarying = 0xFFFFFFBF;// Texcoords are not timeVarying
    anariSetParameter(dev, mesh, "usd::timeVarying", ANARI_INT32, &timeVarying);
    anariSetParameter(dev, mesh, "usd::time", ANARI_FLOAT64, geomTimeValues + timeIdx);

    anariCommitParameters(dev, mesh);

    ANARISampler sampler = anariNewSampler(dev, "image2D");
    anariSetParameter(dev, sampler, "name", ANARI_STRING, "tutorialSampler_0");
    //anariSetParameter(dev, sampler, "usd::imageUrl", ANARI_STRING, texFile);
    anariSetParameter(dev, sampler, "image", ANARI_ARRAY, &texArray);
    anariSetParameter(dev, sampler, "inAttribute", ANARI_STRING, "attribute0");
    anariSetParameter(dev, sampler, "wrapMode1", ANARI_STRING, wrapS);
    anariSetParameter(dev, sampler, "wrapMode2", ANARI_STRING, wrapT);
    anariCommitParameters(dev, sampler);

    ANARIMaterial mat = anariNewMaterial(dev, "matte");
    anariSetParameter(dev, mat, "name", ANARI_STRING, "tutorialMaterial_0");

    float opacity = 1.0;// -timeIdx * 0.1f;
    if (useVertexColors)
      anariSetParameter(dev, mat, "color", ANARI_STRING, "color");
    else if(useTexture)
      anariSetParameter(dev, mat, "color", ANARI_SAMPLER, &sampler);
    else
      anariSetParameter(dev, mat, "color", ANARI_FLOAT32_VEC3, kd);
    anariSetParameter(dev, mat, "opacity", ANARI_FLOAT32, &opacity);
    anariSetParameter(dev, mat, "usd::time", ANARI_FLOAT64, matTimeValues + timeIdx);
    anariCommitParameters(dev, mat);
    anariRelease(dev, sampler);

    // put the mesh into a model
    ANARISurface surface;
    surface = anariNewSurface(dev);
    anariSetParameter(dev, surface, "name", ANARI_STRING, "tutorialSurface_0");
    anariSetParameter(dev, surface, "geometry", ANARI_GEOMETRY, &mesh);
    anariSetParameter(dev, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(dev, surface);
    anariRelease(dev, mesh);
    anariRelease(dev, mat);

    // put the surface into a group
    ANARIGroup group;
    group = anariNewGroup(dev);
    anariSetParameter(dev, group, "name", ANARI_STRING, "tutorialGroup_0");
    array = anariNewArray1D(dev, &surface, 0, 0, ANARI_SURFACE, 1);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, group, "surface", ANARI_ARRAY, &array);
    anariCommitParameters(dev, group);
    anariRelease(dev, surface);
    anariRelease(dev, array);

    // put the group into an instance (give the group a world transform)
    ANARIInstance instance[2];
    instance[0] = anariNewInstance(dev, "transform");
    anariSetParameter(dev, instance[0], "name", ANARI_STRING, "tutorialInstance_0");
    anariSetParameter(dev, instance[0], "transform", ANARI_FLOAT32_MAT4, transform);
    anariSetParameter(dev, instance[0], "group", ANARI_GROUP, &group);
    anariRelease(dev, group);

    // create and setup light for Ambient Occlusion
    ANARILight light = anariNewLight(dev, "ambient");
    anariSetParameter(dev, light, "name", ANARI_STRING, "tutorialLight");
    anariCommitParameters(dev, light);
    array = anariNewArray1D(dev, &light, 0, 0, ANARI_LIGHT, 1);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, world, "light", ANARI_ARRAY, &array);
    anariRelease(dev, light);
    anariRelease(dev, array);

    anariCommitParameters(dev, instance[0]);

    if (doubleNodes)
    {
      //ANARIGeometry protoMesh = anariNewGeometry(dev, "triangle");
      //anariSetParameter(dev, protoMesh, "name", ANARI_STRING, "tutorialProtoMesh");
//
      //ANARIArray1D array = anariNewArray1D(dev, protoVertex, 0, 0, ANARI_FLOAT32_VEC3, 8);
      //anariCommitParameters(dev, array);
      //anariSetParameter(dev, protoMesh, "vertex.position", ANARI_ARRAY, &array);
      //anariRelease(dev, array); // we are done using this handle
//
      //if (useVertexColors)
      //{
      //  array = anariNewArray1D(dev, protoColor, 0, 0, ANARI_FLOAT32_VEC4, 8);
      //  anariCommitParameters(dev, array);
      //  anariSetParameter(dev, protoMesh, "vertex.color", ANARI_ARRAY, &array);
      //  anariRelease(dev, array);
      //}
//
      //array = anariNewArray1D(dev, protoTexcoord, 0, 0, ANARI_FLOAT32_VEC2, 8);
      //anariCommitParameters(dev, array);
      //anariSetParameter(dev, protoMesh, "vertex.attribute0", ANARI_ARRAY, &array);
      //anariRelease(dev, array);
//
      //array = anariNewArray1D(dev, protoIndex, 0, 0, ANARI_INT32_VEC3, 8);
      //anariCommitParameters(dev, array);
      //anariSetParameter(dev, protoMesh, "primitive.index", ANARI_ARRAY, &array);
      //anariRelease(dev, array);
//
      //anariCommitParameters(dev, protoMesh);

      mesh = anariNewGeometry(dev, "sphere");
      anariSetParameter(dev, mesh, "name", ANARI_STRING, "tutorialPoints");
      //anariSetParameter(dev, mesh, "usd::useUsdGeomPoints", ANARI_BOOL, &useUsdGeomPoints);
      //anariSetParameter(dev, mesh, "shapeType", ANARI_STRING, "cylinder");
      //anariSetParameter(dev, mesh, "shapeGeometry", ANARI_GEOMETRY, &protoMesh);
      //anariSetParameter(dev, mesh, "shapeTransform", ANARI_FLOAT32_MAT4, protoTransform);
      //anariRelease(dev, protoMesh);

      array = anariNewArray1D(dev, vertex, 0, 0, ANARI_FLOAT32_VEC3, 4);
      anariCommitParameters(dev, array);
      anariSetParameter(dev, mesh, "vertex.position", ANARI_ARRAY, &array);
      anariRelease(dev, array); // we are done using this handle

      if (useVertexColors)
      {
        array = anariNewArray1D(dev, color, 0, 0, ANARI_FLOAT32_VEC4, 4);
        anariCommitParameters(dev, array);
        anariSetParameter(dev, mesh, "vertex.color", ANARI_ARRAY, &array);
        anariRelease(dev, array);
      }

      array = anariNewArray1D(dev, texcoord, 0, 0, ANARI_FLOAT32_VEC2, 4);
      anariCommitParameters(dev, array);
      anariSetParameter(dev, mesh, "vertex.attribute0", ANARI_ARRAY, &array);
      anariRelease(dev, array);

      array = anariNewArray1D(dev, sphereSizes, 0, 0, ANARI_FLOAT32, 4);
      anariCommitParameters(dev, array);
      anariSetParameter(dev, mesh, "vertex.radius", ANARI_ARRAY, &array);
      //anariSetParameter(dev, mesh, "vertex.scale", ANARI_ARRAY, &array);
      anariRelease(dev, array);

      anariSetParameter(dev, mesh, "usd::time", ANARI_FLOAT64, timeValues + timeIdx);

      anariCommitParameters(dev, mesh);

      sampler = anariNewSampler(dev, "image2D");
      anariSetParameter(dev, sampler, "name", ANARI_STRING, "tutorialSampler_1");
      //anariSetParameter(dev, sampler, "usd::imageUrl", ANARI_STRING, texFile);
      anariSetParameter(dev, sampler, "image", ANARI_ARRAY, &texArray);
      anariSetParameter(dev, sampler, "inAttribute", ANARI_STRING, "attribute0");
      anariSetParameter(dev, sampler, "wrapMode1", ANARI_STRING, wrapS);
      anariSetParameter(dev, sampler, "wrapMode2", ANARI_STRING, wrapT);
      anariCommitParameters(dev, sampler);

      mat = anariNewMaterial(dev, "matte");
      anariSetParameter(dev, mat, "name", ANARI_STRING, "tutorialMaterial_1");
      if (useVertexColors)
        anariSetParameter(dev, mat, "color", ANARI_STRING, "color");
      else if(useTexture)
        anariSetParameter(dev, mat, "color", ANARI_SAMPLER, &sampler);
      else
        anariSetParameter(dev, mat, "color", ANARI_FLOAT32_VEC3, kd);
      anariSetParameter(dev, mat, "usd::time", ANARI_FLOAT64, timeValues + timeIdx);
      timeVarying = 3;// Only colors and opacities are timevarying
      anariSetParameter(dev, mat, "usd::timeVarying", ANARI_INT32, &timeVarying);
      anariCommitParameters(dev, mat);
      anariRelease(dev, sampler);

      // put the mesh into a model
      surface = anariNewSurface(dev);
      anariSetParameter(dev, surface, "name", ANARI_STRING, "tutorialSurface_1");
      anariSetParameter(dev, surface, "geometry", ANARI_GEOMETRY, &mesh);
      anariSetParameter(dev, surface, "material", ANARI_MATERIAL, &mat);
      anariCommitParameters(dev, surface);
      anariRelease(dev, mesh);
      anariRelease(dev, mat);

      // put the surface into a group
      group = anariNewGroup(dev);
      anariSetParameter(dev, group, "name", ANARI_STRING, "tutorialGroup_1");
      array = anariNewArray1D(dev, &surface, 0, 0, ANARI_SURFACE, 1);
      anariCommitParameters(dev, array);
      anariSetParameter(dev, group, "surface", ANARI_ARRAY, &array);
      anariCommitParameters(dev, group);
      anariRelease(dev, surface);
      anariRelease(dev, array);

      // put the group into an instance (give the group a world transform)
      instance[1] = anariNewInstance(dev, "transform");
      anariSetParameter(dev, instance[1], "name", ANARI_STRING, "tutorialInstance_1");
      anariSetParameter(dev, instance[1], "transform", ANARI_FLOAT32_MAT4, transform);
      anariSetParameter(dev, instance[1], "group", ANARI_GROUP, &group);
      anariRelease(dev, group);

      anariCommitParameters(dev, instance[1]);
    }

    // put the instance in the world
    anariSetParameter(dev, world, "name", ANARI_STRING, "tutorialWorld");
    array = anariNewArray1D(dev, instance, 0, 0, ANARI_INSTANCE, (doubleNodes ? 2 : 1));
    anariCommitParameters(dev, array);
    anariSetParameter(dev, world, "instance", ANARI_ARRAY, &array);
    anariRelease(dev, instance[0]);
    if (doubleNodes)
      anariRelease(dev, instance[1]);
    anariRelease(dev, array);
    anariCommitParameters(dev, world);

    // create renderer
    ANARIRenderer renderer =
      anariNewRenderer(dev, "pathtracer"); // choose path tracing renderer

    // complete setup of renderer
    float bgColor[4] = { 1.f, 1.f, 1.f, 1.f }; // white
    anariSetParameter(dev, renderer, "backgroundColor", ANARI_FLOAT32_VEC4, bgColor);
    anariCommitParameters(dev, renderer);

    // create and setup frame
    ANARIFrame frame = anariNewFrame(dev);
    ANARIDataType colFormat = ANARI_UFIXED8_RGBA_SRGB;
    ANARIDataType depthFormat = ANARI_FLOAT32;
    anariSetParameter(dev, frame, "size", ANARI_UINT32_VEC2, frameSize);
    anariSetParameter(dev, frame, "channel.color", ANARI_DATA_TYPE, &colFormat);
    anariSetParameter(dev, frame, "channel.depth", ANARI_DATA_TYPE, &depthFormat);

    anariSetParameter(dev, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(dev, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(dev, frame, "world", ANARI_WORLD, &world);

    anariCommitParameters(dev, frame);

    printf("rendering frame...");

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
    anariSetParameter(dev, dev, "usd::garbageCollect", ANARI_VOID_POINTER, 0);

    // Reset generation of unique names for next frame
    // Only necessary when relying upon auto-generation of names instead of manual creation, AND not retaining ANARI objects over timesteps
    anariSetParameter(dev, dev, "usd::removeUnusedNames", ANARI_VOID_POINTER, 0);

    // ~

    // Constant color get a bit more red every step
    kd[0] = timeIdx / (float)numTimeSteps;
  }


  // CHANGE THE SCENE: Attach only the instance_1 to the world, so instance_0 gets removed.

  for (int timeIdx = 0; timeIdx < numTimeSteps/2; ++timeIdx)
  {
    anariSetParameter(dev, dev, "usd::time", ANARI_FLOAT64, timeValues + timeIdx);
    anariCommitParameters(dev, dev);

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
      //anariSetParameter(dev, mesh, "usd::useUsdGeomPoints", ANARI_BOOL, &useUsdGeomPoints);

      ANARIArray1D array = anariNewArray1D(dev, vertex, 0, 0, ANARI_FLOAT32_VEC3, 4);
      anariCommitParameters(dev, array);
      anariSetParameter(dev, mesh, "vertex.position", ANARI_ARRAY, &array);
      anariRelease(dev, array); // we are done using this handle

      if (useVertexColors)
      {
        array = anariNewArray1D(dev, color, 0, 0, ANARI_FLOAT32_VEC4, 4);
        anariCommitParameters(dev, array);
        anariSetParameter(dev, mesh, "vertex.color", ANARI_ARRAY, &array);
        anariRelease(dev, array);
      }

      array = anariNewArray1D(dev, texcoord, 0, 0, ANARI_FLOAT32_VEC2, 4);
      anariCommitParameters(dev, array);
      anariSetParameter(dev, mesh, "vertex.attribute0", ANARI_ARRAY, &array);
      anariRelease(dev, array);

      array = anariNewArray1D(dev, sphereSizes, 0, 0, ANARI_FLOAT32, 4);
      anariCommitParameters(dev, array);
      anariSetParameter(dev, mesh, "vertex.radius", ANARI_ARRAY, &array);
      //anariSetParameter(dev, mesh, "vertex.scale", ANARI_ARRAY, &array);
      anariRelease(dev, array);

      anariSetParameter(dev, mesh, "usd::time", ANARI_FLOAT64, timeValues + timeIdx*2); // Switch the child timestep for something else

      anariCommitParameters(dev, mesh);

      sampler = anariNewSampler(dev, "image2D");
      anariSetParameter(dev, sampler, "name", ANARI_STRING, "tutorialSampler_1");
      //anariSetParameter(dev, sampler, "usd::imageUrl", ANARI_STRING, texFile);
      anariSetParameter(dev, sampler, "image", ANARI_ARRAY, &texArray);
      anariSetParameter(dev, sampler, "inAttribute", ANARI_STRING, "attribute0");
      anariSetParameter(dev, sampler, "wrapMode1", ANARI_STRING, wrapS);
      anariSetParameter(dev, sampler, "wrapMode2", ANARI_STRING, wrapT);
      anariCommitParameters(dev, sampler);

      mat = anariNewMaterial(dev, "matte");
      anariSetParameter(dev, mat, "name", ANARI_STRING, "tutorialMaterial_1");
      if (useVertexColors)
        anariSetParameter(dev, mat, "color", ANARI_STRING, "color");
      else
        anariSetParameter(dev, mat, "color", ANARI_FLOAT32_VEC3, kd);
      anariSetParameter(dev, mat, "usd::time", ANARI_FLOAT64, timeValues + timeIdx*2);
      anariCommitParameters(dev, mat);
      anariRelease(dev, sampler);

      // put the mesh into a model
      surface = anariNewSurface(dev);
      anariSetParameter(dev, surface, "name", ANARI_STRING, "tutorialSurface_1");
      anariSetParameter(dev, surface, "geometry", ANARI_GEOMETRY, &mesh);
      anariSetParameter(dev, surface, "material", ANARI_MATERIAL, &mat);
      anariCommitParameters(dev, surface);
      anariRelease(dev, mesh);
      anariRelease(dev, mat);

      // put the surface into a group
      group = anariNewGroup(dev);
      anariSetParameter(dev, group, "name", ANARI_STRING, "tutorialGroup_1");
      array = anariNewArray1D(dev, &surface, 0, 0, ANARI_SURFACE, 1);
      anariCommitParameters(dev, array);
      anariSetParameter(dev, group, "surface", ANARI_ARRAY, &array);
      anariCommitParameters(dev, group);
      anariRelease(dev, surface);
      anariRelease(dev, array);

      // put the group into an instance (give the group a world transform)
      instance = anariNewInstance(dev, "transform");
      anariSetParameter(dev, instance, "name", ANARI_STRING, "tutorialInstance_1");
      anariSetParameter(dev, instance, "transform", ANARI_FLOAT32_MAT4, transform);
      anariSetParameter(dev, instance, "group", ANARI_GROUP, &group);
      anariRelease(dev, group);

      anariCommitParameters(dev, instance);
    }

    // put the instance in the world
    anariSetParameter(dev, world, "name", ANARI_STRING, "tutorialWorld");
    array = anariNewArray1D(dev, &instance, 0, 0, ANARI_INSTANCE, 1);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, world, "instance", ANARI_ARRAY, &array);
    anariRelease(dev, instance);
    anariRelease(dev, array);

    anariCommitParameters(dev, world);

    // create renderer
    ANARIRenderer renderer =
      anariNewRenderer(dev, "pathtracer"); // choose path tracing renderer

    // complete setup of renderer
    float bgColor[4] = { 1.f, 1.f, 1.f, 1.f }; // white
    anariSetParameter(dev, renderer, "backgroundColor", ANARI_FLOAT32_VEC4, bgColor);
    anariCommitParameters(dev, renderer);

    // create and setup frame
    ANARIFrame frame = anariNewFrame(dev);
    ANARIDataType colFormat = ANARI_UFIXED8_RGBA_SRGB;
    ANARIDataType depthFormat = ANARI_FLOAT32;
    anariSetParameter(dev, frame, "size", ANARI_UINT32_VEC2, frameSize);
    anariSetParameter(dev, frame, "channel.color", ANARI_DATA_TYPE, &colFormat);
    anariSetParameter(dev, frame, "channel.depth", ANARI_DATA_TYPE, &depthFormat);

    anariSetParameter(dev, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(dev, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(dev, frame, "world", ANARI_WORLD, &world);

    anariCommitParameters(dev, frame);

    printf("rendering frame...");

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
    anariSetParameter(dev, dev, "usd::garbageCollect", ANARI_VOID_POINTER, 0);

    // Reset generation of unique names for next frame
    // Only necessary when relying upon auto-generation of names instead of manual creation, AND not retaining ANARI objects over timesteps
    anariSetParameter(dev, dev, "usd::removeUnusedNames", ANARI_VOID_POINTER, 0);

    // ~
  }

  anariRelease(dev, texArray);

  anariRelease(dev, dev);

  freeTexture(textureData);

  printf("done!\n");

  return 0;
}
