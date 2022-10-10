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
  float normal[] = { 0.0f,
    0.0f,
    1.0f,
    0.0f,
    1.0f,
    0.0f,
    0.0f,
    1.0f,
    0.0f,
    1.0f,
    0.0f,
    0.0f
  };
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
      1.0f };
  float sphereSizes[] = { 0.1f,
      2.0f,
      0.3f,
      0.05f };
  int32_t index[] = {0, 1, 2, 2, 1, 3};

  float kd[] = { 0.0f, 0.0f, 1.0f };

  for(int anariPass = 0; anariPass < 2; ++anariPass)
  {
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
    int outputMaterial = 1;
    int createNewSession = (anariPass == 0) ? 1 : 0;

    int useVertexColors = (anariPass == 0);
    int useTexture = 1;

    anariSetParameter(dev, dev, "usd::connection.logVerbosity", ANARI_INT32, &connLogVerbosity);

    if (outputOmniverse)
    {
      anariSetParameter(dev, dev, "usd::serialize.hostName", ANARI_STRING, "ov-test");
      anariSetParameter(dev, dev, "usd::serialize.location", ANARI_STRING, "/Users/test/anari");
    }
    anariSetParameter(dev, dev, "usd::serialize.outputBinary", ANARI_BOOL, &outputBinary);
    anariSetParameter(dev, dev, "usd::serialize.newSession", ANARI_BOOL, &createNewSession);
    anariSetParameter(dev, dev, "usd::output.material", ANARI_BOOL, &outputMaterial);

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

    printf("done!\n");
    printf("setting up scene...");

    ANARIWorld world = anariNewWorld(dev);

    // create and setup mesh
    ANARIGeometry mesh = anariNewGeometry(dev, "triangle");
    anariSetParameter(dev, mesh, "name", ANARI_STRING, "tutorialMesh");

    ANARIArray1D array = anariNewArray1D(dev, vertex, 0, 0, ANARI_FLOAT32_VEC3, 4, 0);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, mesh, "vertex.position", ANARI_ARRAY, &array);
    anariRelease(dev, array); // we are done using this handle

    array = anariNewArray1D(dev, normal, 0, 0, ANARI_FLOAT32_VEC3, 4, 0);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, mesh, "vertex.normal", ANARI_ARRAY, &array);
    anariRelease(dev, array);

    // Set the vertex colors
    if (useVertexColors)
    {
      array = anariNewArray1D(dev, color, 0, 0, ANARI_FLOAT32_VEC4, 4, 0);
      anariCommitParameters(dev, array);
      anariSetParameter(dev, mesh, "vertex.color", ANARI_ARRAY, &array);
      anariRelease(dev, array);
    }

    array = anariNewArray1D(dev, texcoord, 0, 0, ANARI_FLOAT32_VEC2, 4, 0);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, mesh, "vertex.attribute0", ANARI_ARRAY, &array);
    anariRelease(dev, array);

    //array = anariNewArray1D(dev, sphereSizes, 0, 0, ANARI_FLOAT32, 4, 0);
    //anariCommitParameters(dev, array);
    //anariSetParameter(dev, mesh, "vertex.radius", ANARI_ARRAY, &array);
    //anariRelease(dev, array);

    array = anariNewArray1D(dev, index, 0, 0, ANARI_UINT32_VEC3, 2, 0);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, mesh, "primitive.index", ANARI_ARRAY, &array);
    anariRelease(dev, array);

    anariCommitParameters(dev, mesh);

    ANARISampler sampler = anariNewSampler(dev, "image2D");
    ANARIMaterial mat = anariNewMaterial(dev, "matte");
    // The second iteration should not commit samplers/materials and not add it to the surface,
    // so the material prim itself will remain untouched in the pre-existing stage,
    // while its reference will be removed from the newly committed surface prim
    if(anariPass == 0)
    {
      // Create a sampler
      anariSetParameter(dev, sampler, "name", ANARI_STRING, "tutorialSampler");
      //anariSetParameter(dev, sampler, "usd::imageUrl", ANARI_STRING, texFile);
      array = anariNewArray2D(dev, textureData, 0, 0, ANARI_UINT8_VEC3, textureSize[0], textureSize[1], 0, 0); // Make sure this matches numTexComponents
      anariSetParameter(dev, sampler, "image", ANARI_ARRAY, &array);
      anariSetParameter(dev, sampler, "inAttribute", ANARI_STRING, "attribute0");
      anariSetParameter(dev, sampler, "wrapMode1", ANARI_STRING, wrapS);
      anariSetParameter(dev, sampler, "wrapMode2", ANARI_STRING, wrapT);
      anariCommitParameters(dev, sampler);
      anariRelease(dev, array);

      // Create a material
      anariSetParameter(dev, mat, "name", ANARI_STRING, "tutorialMaterial");

      float opacity = 1.0f;
      if (useVertexColors)
        anariSetParameter(dev, mat, "color", ANARI_STRING, "color");
      else if(useTexture)
        anariSetParameter(dev, mat, "color", ANARI_SAMPLER, &sampler);
      else
        anariSetParameter(dev, mat, "color", ANARI_FLOAT32_VEC3, kd);
      anariSetParameter(dev, mat, "opacity", ANARI_FLOAT32, &opacity);
      anariCommitParameters(dev, mat);
    }
    anariRelease(dev, sampler);

    // put the mesh into a surface
    ANARISurface surface = anariNewSurface(dev);
    anariSetParameter(dev, surface, "name", ANARI_STRING, "tutorialSurface");
    anariSetParameter(dev, surface, "geometry", ANARI_GEOMETRY, &mesh);
    if (anariPass == 0)
      anariSetParameter(dev, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(dev, surface);
    anariRelease(dev, mesh);
    anariRelease(dev, mat);

    // put the surface into a group
    ANARIGroup group = anariNewGroup(dev);
    anariSetParameter(dev, group, "name", ANARI_STRING, "tutorialGroup");
    array = anariNewArray1D(dev, &surface, 0, 0, ANARI_SURFACE, 1, 0);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, group, "surface", ANARI_ARRAY, &array);
    anariCommitParameters(dev, group);
    anariRelease(dev, surface);
    anariRelease(dev, array);

    // put the group into an instance (give the group a world transform)
    ANARIInstance instance = anariNewInstance(dev);
    anariSetParameter(dev, instance, "name", ANARI_STRING, "tutorialInstance");
    anariSetParameter(dev, instance, "transform", ANARI_FLOAT32_MAT3x4, transform);
    anariSetParameter(dev, instance, "group", ANARI_GROUP, &group);
    anariRelease(dev, group);

    // create and setup light for Ambient Occlusion
    ANARILight light = anariNewLight(dev, "ambient");
    anariSetParameter(dev, light, "name", ANARI_STRING, "tutorialLight");
    anariCommitParameters(dev, light);
    array = anariNewArray1D(dev, &light, 0, 0, ANARI_LIGHT, 1, 0);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, world, "light", ANARI_ARRAY, &array);
    anariRelease(dev, light);
    anariRelease(dev, array);

    anariCommitParameters(dev, instance);

    // put the instance in the world
    anariSetParameter(dev, world, "name", ANARI_STRING, "tutorialWorld");
    array = anariNewArray1D(dev, &instance, 0, 0, ANARI_INSTANCE, 1, 0);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, world, "instance", ANARI_ARRAY, &array);
    anariRelease(dev, instance);
    anariRelease(dev, array);

    anariCommitParameters(dev, world);

    printf("done!\n");

    // print out world bounds
    float worldBounds[6];
    if (anariGetProperty(dev,
      world,
      "bounds",
      ANARI_FLOAT32_BOX3,
      worldBounds,
      sizeof(worldBounds),
      ANARI_WAIT)) {
      printf("\nworld bounds: ({%f, %f, %f}, {%f, %f, %f}\n\n",
        worldBounds[0],
        worldBounds[1],
        worldBounds[2],
        worldBounds[3],
        worldBounds[4],
        worldBounds[5]);
    }
    else {
      printf("\nworld bounds not returned\n\n");
    }

    printf("setting up renderer...");

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
    anariSetParameter(dev, frame, "color", ANARI_DATA_TYPE, &colFormat);
    anariSetParameter(dev, frame, "depth", ANARI_DATA_TYPE, &depthFormat);

    anariSetParameter(dev, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(dev, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(dev, frame, "world", ANARI_WORLD, &world);

    anariCommitParameters(dev, frame);

    printf("rendering frame...");

    // render one frame
    anariRenderFrame(dev, frame);
    anariFrameReady(dev, frame, ANARI_WAIT);

    printf("done!\n");
    printf("\ncleaning up objects...");

    // final cleanups
    anariRelease(dev, renderer);
    anariRelease(dev, camera);
    anariRelease(dev, frame);
    anariRelease(dev, world);

    anariRelease(dev, dev);

    anariUnloadLibrary(lib);

    printf("done!\n");

    for(int vIdx = 0; vIdx < sizeof(vertex)/sizeof(float); ++vIdx)
    {
      vertex[vIdx] += 1.0f;
    }
  }

  freeTexture(textureData);

  return 0;
}
