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

typedef struct TestParameters
{
  int writeAtCommit;
  int useVertexColors;
  int useTexture;
  int transparent;
} TestParameters;

void doTest(TestParameters testParams)
{
  printf("\n\n--------------  Starting new test iteration \n\n");
  printf("Parameters: writeatcommit %d, useVertexColors %d, useTexture %d,  transparent %d \n", testParams.writeAtCommit, testParams.useVertexColors, testParams.useTexture, testParams.transparent);

  stbi_flip_vertically_on_write(1);

  // image sizes
  int frameSize[2] = { 1024, 768 };
  int textureSize[2] = { 256, 256 };

  uint8_t* textureData = 0;
  int numTexComponents = 3;
  textureData = generateTexture(textureSize, numTexComponents);

  uint8_t* opacityTextureData = 0;
  opacityTextureData = generateTexture(textureSize, 1);

  // camera
  float cam_pos[] = { 0.f, 0.f, 0.f };
  float cam_up[] = { 0.f, 1.f, 0.f };
  float cam_view[] = { 0.1f, 0.f, 1.f };

  float transform[12] = { 3.0f, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 3.0f, 2.0f, 3.0f, 4.0f };

  // triangle mesh data
  float vertex[] = { -1.0f,
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
      0.3f };
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
  float color[] = { 0.0f,
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
      1.0f };
  float opacities[] = { 0.2f,
      0.5f,
      0.7f,
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
  int32_t index[] = { 0, 1, 2, 2, 1, 3 };

  float kd[] = { 0.0f, 0.0f, 1.0f };

  printf("initialize ANARI...");

  ANARILibrary lib = anariLoadLibrary(g_libraryType, statusFunc, NULL);

  ANARIDevice dev = anariNewDevice(lib, "usd");

  if (!dev) {
    printf("\n\nERROR: could not load device '%s'\n", "usd");
    return;
  }

  int outputBinary = 0;
  int outputOmniverse = 0;
  int connLogVerbosity = 0;
  int outputMaterial = 1;
  int outputPreviewSurface = 1;
  int outputMdl = 1;

  int useVertexColors = testParams.useVertexColors;
  int useTexture = testParams.useTexture;
  int isTransparent = testParams.transparent;

  anariSetParameter(dev, dev, "usd::connection.logVerbosity", ANARI_INT32, &connLogVerbosity);

  if (outputOmniverse)
  {
    anariSetParameter(dev, dev, "usd::serialize.hostName", ANARI_STRING, "localhost");
    anariSetParameter(dev, dev, "usd::serialize.location", ANARI_STRING, "/Users/test/anari");
  }
  anariSetParameter(dev, dev, "usd::serialize.outputBinary", ANARI_BOOL, &outputBinary);
  anariSetParameter(dev, dev, "usd::output.material", ANARI_BOOL, &outputMaterial);
  anariSetParameter(dev, dev, "usd::output.previewSurfaceShader", ANARI_BOOL, &outputPreviewSurface);
  anariSetParameter(dev, dev, "usd::output.mdlShader", ANARI_BOOL, &outputMdl);
  anariSetParameter(dev, dev, "usd::writeAtCommit", ANARI_BOOL, &testParams.writeAtCommit);

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

  ANARIArray1D array = anariNewArray1D(dev, vertex, 0, 0, ANARI_FLOAT32_VEC3, 4);
  anariCommitParameters(dev, array);
  anariSetParameter(dev, mesh, "vertex.position", ANARI_ARRAY, &array);
  anariRelease(dev, array); // we are done using this handle

  array = anariNewArray1D(dev, normal, 0, 0, ANARI_FLOAT32_VEC3, 4);
  anariCommitParameters(dev, array);
  anariSetParameter(dev, mesh, "vertex.normal", ANARI_ARRAY, &array);
  anariRelease(dev, array);

  // Set the vertex colors
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

  if(isTransparent)
  {
    array = anariNewArray1D(dev, opacities, 0, 0, ANARI_FLOAT32, 4);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, mesh, "vertex.attribute1", ANARI_ARRAY, &array);
    anariRelease(dev, array);
  }

  //array = anariNewArray1D(dev, sphereSizes, 0, 0, ANARI_FLOAT32, 4);
  //anariCommitParameters(dev, array);
  //anariSetParameter(dev, mesh, "vertex.radius", ANARI_ARRAY, &array);
  //anariRelease(dev, array);

  array = anariNewArray1D(dev, index, 0, 0, ANARI_UINT32_VEC3, 2);
  anariCommitParameters(dev, array);
  anariSetParameter(dev, mesh, "primitive.index", ANARI_ARRAY, &array);
  anariRelease(dev, array);

  anariCommitParameters(dev, mesh);


  // Create a sampler
  ANARISampler sampler = 0;
  if(useTexture)
  {
    sampler = anariNewSampler(dev, "image2D");
    anariSetParameter(dev, sampler, "name", ANARI_STRING, "tutorialSampler");

    anariSetParameter(dev, sampler, "inAttribute", ANARI_STRING, "attribute0");
    anariSetParameter(dev, sampler, "wrapMode1", ANARI_STRING, wrapS);
    anariSetParameter(dev, sampler, "wrapMode2", ANARI_STRING, wrapT);

    array = anariNewArray2D(dev, textureData, 0, 0, ANARI_UINT8_VEC3, textureSize[0], textureSize[1]); // Make sure this matches numTexComponents
    //anariSetParameter(dev, sampler, "usd::imageUrl", ANARI_STRING, texFile);
    anariSetParameter(dev, sampler, "image", ANARI_ARRAY, &array);

    anariCommitParameters(dev, sampler);
    anariRelease(dev, array);
  }

  ANARISampler opacitySampler = 0;
  if(isTransparent && useTexture)
  {
    opacitySampler = anariNewSampler(dev, "image2D");
    anariSetParameter(dev, opacitySampler, "name", ANARI_STRING, "tutorialSamplerOpacity");

    anariSetParameter(dev, opacitySampler, "inAttribute", ANARI_STRING, "attribute0");
    anariSetParameter(dev, opacitySampler, "wrapMode1", ANARI_STRING, wrapS);
    anariSetParameter(dev, opacitySampler, "wrapMode2", ANARI_STRING, wrapT);

    array = anariNewArray2D(dev, opacityTextureData, 0, 0, ANARI_UINT8, textureSize[0], textureSize[1]);
    anariSetParameter(dev, opacitySampler, "image", ANARI_ARRAY, &array);

    anariCommitParameters(dev, opacitySampler);
    anariRelease(dev, array);
  }

  // Create a material
  ANARIMaterial mat = anariNewMaterial(dev, "matte");
  anariSetParameter(dev, mat, "name", ANARI_STRING, "tutorialMaterial");

  if (useVertexColors)
    anariSetParameter(dev, mat, "color", ANARI_STRING, "color");
  else if(useTexture)
    anariSetParameter(dev, mat, "color", ANARI_SAMPLER, &sampler);
  else
    anariSetParameter(dev, mat, "color", ANARI_FLOAT32_VEC3, kd);

  float opacityConstant = isTransparent ? 0.65f : 1.0f;
  if(isTransparent)
  {
    anariSetParameter(dev, mat, "alphaMode", ANARI_STRING, "blend");
    if(useVertexColors)
      anariSetParameter(dev, mat, "opacity", ANARI_STRING, "attribute1");
    else if(useTexture)
      anariSetParameter(dev, mat, "opacity", ANARI_SAMPLER, &opacitySampler);
    else
      anariSetParameter(dev, mat, "opacity", ANARI_FLOAT32, &opacityConstant);
  }
  else
  {
    anariSetParameter(dev, mat, "opacity", ANARI_FLOAT32, &opacityConstant);
  }

  int materialTimeVarying = 1<<3; // Set emissive to timevarying (should appear in material stage under primstages/)
  anariSetParameter(dev, mat, "usd::timeVarying", ANARI_INT32, &materialTimeVarying);
  anariCommitParameters(dev, mat);
  anariRelease(dev, sampler);
  anariRelease(dev, opacitySampler);

  // put the mesh into a surface
  ANARISurface surface = anariNewSurface(dev);
  anariSetParameter(dev, surface, "name", ANARI_STRING, "tutorialSurface");
  anariSetParameter(dev, surface, "geometry", ANARI_GEOMETRY, &mesh);
  anariSetParameter(dev, surface, "material", ANARI_MATERIAL, &mat);
  anariCommitParameters(dev, surface);
  anariRelease(dev, mesh);
  anariRelease(dev, mat);

  // put the surface into a group
  ANARIGroup group = anariNewGroup(dev);
  anariSetParameter(dev, group, "name", ANARI_STRING, "tutorialGroup");
  array = anariNewArray1D(dev, &surface, 0, 0, ANARI_SURFACE, 1);
  anariCommitParameters(dev, array);
  anariSetParameter(dev, group, "surface", ANARI_ARRAY, &array);
  anariCommitParameters(dev, group);
  anariRelease(dev, surface);
  anariRelease(dev, array);

  // put the group into an instance (give the group a world transform)
  ANARIInstance instance = anariNewInstance(dev, "transform");
  anariSetParameter(dev, instance, "name", ANARI_STRING, "tutorialInstance");
  anariSetParameter(dev, instance, "transform", ANARI_FLOAT32_MAT3x4, transform);
  anariSetParameter(dev, instance, "group", ANARI_GROUP, &group);
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

  anariCommitParameters(dev, instance);

  // put the instance in the world
  anariSetParameter(dev, world, "name", ANARI_STRING, "tutorialWorld");
  array = anariNewArray1D(dev, &instance, 0, 0, ANARI_INSTANCE, 1);
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

  freeTexture(textureData);

  printf("done!\n");
}


int main(int argc, const char **argv)
{
  TestParameters testParams;

  // Test standard flow with textures
  testParams.writeAtCommit = 0;
  testParams.useVertexColors = 0;
  testParams.useTexture = 1;
  testParams.transparent = 0;
  doTest(testParams);

  // With vertex colors
  testParams.writeAtCommit = 0;
  testParams.useVertexColors = 1;
  testParams.useTexture = 0;
  testParams.transparent = 0;
  doTest(testParams);

  // With color constant
  testParams.writeAtCommit = 0;
  testParams.useVertexColors = 0;
  testParams.useTexture = 0;
  testParams.transparent = 0;
  doTest(testParams);

  // Transparency
  testParams.writeAtCommit = 0;
  testParams.useVertexColors = 0;
  testParams.useTexture = 0;
  testParams.transparent = 1;
  doTest(testParams);

  // Transparency (vertex)
  testParams.writeAtCommit = 0;
  testParams.useVertexColors = 1;
  testParams.useTexture = 0;
  testParams.transparent = 1;
  doTest(testParams);

  // Transparency (sampler)
  testParams.writeAtCommit = 0;
  testParams.useVertexColors = 0;
  testParams.useTexture = 1;
  testParams.transparent = 1;
  doTest(testParams);

  // Test immediate mode writing
  testParams.writeAtCommit = 1;
  testParams.useVertexColors = 0;
  testParams.useTexture = 1;
  testParams.transparent = 0;
  doTest(testParams);

  return 0;
}
