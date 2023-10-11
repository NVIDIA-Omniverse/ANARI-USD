// Copyright 2021 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <assert.h>
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

typedef enum TestType
{
  TEST_MESH,
  TEST_CONES,
  TEST_GLYPHS
} TestType_t;

typedef struct TestParameters
{
  int writeAtCommit;
  int useVertexColors;
  int useTexture;
  int useIndices;
  int isTransparent;
  TestType_t testType;
  int useGlyphMesh;
} TestParameters_t;

typedef struct TexData
{
  uint8_t* textureData;
  uint8_t* opacityTextureData;
  int textureSize[2];
} TexData_t;

float transform[16] = {
  3.0f, 0.0f, 0.0f, 0.0f,
  0.0f, 3.0f, 0.0f, 0.0f,
  0.0f, 0.0f, 3.0f, 0.0f,
  2.0f, 3.0f, 4.0f, 1.0f };

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
int32_t index[] = { 0, 1, 2, 2, 1, 3 };

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
int32_t protoIndex[] = {0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5, 4, 5, 6, 6, 5, 7, 6, 7, 0, 0, 7, 1};
float protoTransform[16] = {
  0.75f, 0.0f, 0.0f, 0.0f,
  0.0f, 1.5f, 0.0f, 0.0f,
  0.0f, 0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 0.0f, 1.0f };

float kd[] = { 0.0f, 0.0f, 1.0f };

ANARIInstance createMeshInstance(ANARIDevice dev, TestParameters_t testParams, TexData_t testData)
{
  int useVertexColors = testParams.useVertexColors;
  int useTexture = testParams.useTexture;
  int isTransparent = testParams.isTransparent;

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

  array = anariNewArray1D(dev, index, 0, 0, ANARI_INT32_VEC3, 2);
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

    array = anariNewArray2D(dev, testData.textureData, 0, 0, ANARI_UINT8_VEC3, testData.textureSize[0], testData.textureSize[1]); // Make sure this matches numTexComponents
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

    array = anariNewArray2D(dev, testData.opacityTextureData, 0, 0, ANARI_UINT8, testData.textureSize[0], testData.textureSize[1]);
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
  anariSetParameter(dev, instance, "transform", ANARI_FLOAT32_MAT4, transform);
  anariSetParameter(dev, instance, "group", ANARI_GROUP, &group);
  anariRelease(dev, group);

  return instance;
}

ANARIInstance createConesInstance(ANARIDevice dev, TestParameters_t testParams, GridData_t testData)
{
  // create and setup geom
  ANARIGeometry cones = anariNewGeometry(dev, "cone");
  anariSetParameter(dev, cones, "name", ANARI_STRING, "tutorialMesh");

  int sizeX = testData.gridSize[0], sizeY = testData.gridSize[1];
  int numVertices = sizeX*sizeY;

  // Create a snake pattern through the vertices
  int numIndexTuples = sizeY*sizeX-1;
  int* indices = (int*)malloc(2*(numIndexTuples+1)*sizeof(int)); // Extra space for a dummy tuple
  int vertIdxTo = 0;
  for(int i = 0; i < sizeX; ++i)
  {
    for(int j = 0; j < sizeY; ++j)
    {
      int vertIdxFrom = vertIdxTo;
      vertIdxTo = vertIdxFrom + ((j == sizeY-1) ? sizeY : ((i%2) ? -1 : 1));

      int idxAddr = 2*(sizeY*i+j);
      indices[idxAddr] = vertIdxFrom;
      indices[idxAddr+1] = vertIdxTo;
    }
  }

  ANARIArray1D array = anariNewArray1D(dev, testData.gridVertex, 0, 0, ANARI_FLOAT32_VEC3, numVertices);
  anariCommitParameters(dev, array);
  anariSetParameter(dev, cones, "vertex.position", ANARI_ARRAY, &array);
  anariRelease(dev, array); // we are done using this handle

  if(testParams.useIndices)
  {
    array = anariNewArray1D(dev, indices, 0, 0, ANARI_INT32_VEC2, numIndexTuples);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, cones, "primitive.index", ANARI_ARRAY, &array);
    anariRelease(dev, array);
  }

  // Set the vertex colors
  if (testParams.useVertexColors)
  {
    array = anariNewArray1D(dev, testData.gridColor, 0, 0, ANARI_FLOAT32_VEC3, numVertices);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, cones, "vertex.color", ANARI_ARRAY, &array);
    anariRelease(dev, array);
  }

  if(testParams.isTransparent)
  {
    array = anariNewArray1D(dev, testData.gridOpacity, 0, 0, ANARI_FLOAT32, numVertices);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, cones, "vertex.attribute0", ANARI_ARRAY, &array);
    anariRelease(dev, array);
  }

  array = anariNewArray1D(dev, testData.gridRadius, 0, 0, ANARI_FLOAT32, numVertices);
  anariCommitParameters(dev, array);
  anariSetParameter(dev, cones, "vertex.radius", ANARI_ARRAY, &array);
  anariRelease(dev, array);

  anariCommitParameters(dev, cones);

  // Create a material
  ANARIMaterial mat = anariNewMaterial(dev, "matte");
  anariSetParameter(dev, mat, "name", ANARI_STRING, "tutorialMaterial");

  if (testParams.useVertexColors)
    anariSetParameter(dev, mat, "color", ANARI_STRING, "color");
  else
    anariSetParameter(dev, mat, "color", ANARI_FLOAT32_VEC3, kd);

  float opacityConstant = testParams.isTransparent ? 0.65f : 1.0f;
  if(testParams.isTransparent)
  {
    anariSetParameter(dev, mat, "alphaMode", ANARI_STRING, "blend");
    if(testParams.useVertexColors)
      anariSetParameter(dev, mat, "opacity", ANARI_STRING, "attribute1");
    else
      anariSetParameter(dev, mat, "opacity", ANARI_FLOAT32, &opacityConstant);
  }
  else
  {
    anariSetParameter(dev, mat, "opacity", ANARI_FLOAT32, &opacityConstant);
  }

  anariCommitParameters(dev, mat);

  // put the mesh into a surface
  ANARISurface surface = anariNewSurface(dev);
  anariSetParameter(dev, surface, "name", ANARI_STRING, "tutorialSurface");
  anariSetParameter(dev, surface, "geometry", ANARI_GEOMETRY, &cones);
  anariSetParameter(dev, surface, "material", ANARI_MATERIAL, &mat);
  anariCommitParameters(dev, surface);
  anariRelease(dev, cones);
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
  anariSetParameter(dev, instance, "group", ANARI_GROUP, &group);
  anariRelease(dev, group);

  free(indices);

  return instance;
}

ANARIInstance createGlyphsInstance(ANARIDevice dev, TestParameters_t testParams, GridData_t testData)
{
  int sizeX = testData.gridSize[0], sizeY = testData.gridSize[1];
  int numVertices = sizeX*sizeY;

  ANARIArray1D array;
  ANARIGeometry protoMesh;

  // The prototype mesh (instanced geometry)
  if(testParams.useGlyphMesh)
  {
    protoMesh = anariNewGeometry(dev, "triangle");
    anariSetParameter(dev, protoMesh, "name", ANARI_STRING, "tutorialProtoMesh");

    array = anariNewArray1D(dev, protoVertex, 0, 0, ANARI_FLOAT32_VEC3, 8);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, protoMesh, "vertex.position", ANARI_ARRAY, &array);
    anariRelease(dev, array); // we are done using this handle

    if (testParams.useVertexColors)
    {
      array = anariNewArray1D(dev, protoColor, 0, 0, ANARI_FLOAT32_VEC4, 8);
      anariCommitParameters(dev, array);
      anariSetParameter(dev, protoMesh, "vertex.color", ANARI_ARRAY, &array);
      anariRelease(dev, array);
    }

    array = anariNewArray1D(dev, protoIndex, 0, 0, ANARI_INT32_VEC3, 8);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, protoMesh, "primitive.index", ANARI_ARRAY, &array);
    anariRelease(dev, array);

    anariCommitParameters(dev, protoMesh);
  }

  // The glyph geometry
  ANARIGeometry glyphs = anariNewGeometry(dev, "glyph");
  if(testParams.useGlyphMesh)
    anariSetParameter(dev, glyphs, "shapeGeometry", ANARI_GEOMETRY, &protoMesh);
  else
    anariSetParameter(dev, glyphs, "shapeType", ANARI_STRING, "cylinder");

  anariSetParameter(dev, glyphs, "shapeTransform", ANARI_FLOAT32_MAT4, protoTransform);

  if(testParams.useGlyphMesh)
    anariRelease(dev, protoMesh);

  array = anariNewArray1D(dev, testData.gridVertex, 0, 0, ANARI_FLOAT32_VEC3, numVertices);
  anariCommitParameters(dev, array);
  anariSetParameter(dev, glyphs, "vertex.position", ANARI_ARRAY, &array);
  anariRelease(dev, array); // we are done using this handle

  if (testParams.useVertexColors)
  {
    array = anariNewArray1D(dev, testData.gridColor, 0, 0, ANARI_FLOAT32_VEC3, numVertices);
    anariCommitParameters(dev, array);
    anariSetParameter(dev, glyphs, "vertex.color", ANARI_ARRAY, &array);
    anariRelease(dev, array);
  }

  array = anariNewArray1D(dev, testData.gridRadius, 0, 0, ANARI_FLOAT32, numVertices);
  anariCommitParameters(dev, array);
  anariSetParameter(dev, glyphs, "vertex.scale", ANARI_ARRAY, &array);
  anariRelease(dev, array);

  array = anariNewArray1D(dev, testData.gridOrientation, 0, 0, ANARI_FLOAT32_QUAT_IJKW, numVertices);
  anariCommitParameters(dev, array);
  anariSetParameter(dev, glyphs, "vertex.orientation", ANARI_ARRAY, &array);
  anariRelease(dev, array);

  anariCommitParameters(dev, glyphs);

  // Create a material
  ANARIMaterial mat = anariNewMaterial(dev, "matte");
  anariSetParameter(dev, mat, "name", ANARI_STRING, "tutorialMaterial");

  if (testParams.useVertexColors)
    anariSetParameter(dev, mat, "color", ANARI_STRING, "color");
  else
    anariSetParameter(dev, mat, "color", ANARI_FLOAT32_VEC3, kd);

  float opacityConstant = testParams.isTransparent ? 0.65f : 1.0f;
  if(testParams.isTransparent)
  {
    anariSetParameter(dev, mat, "alphaMode", ANARI_STRING, "blend");
    if(testParams.useVertexColors)
      anariSetParameter(dev, mat, "opacity", ANARI_STRING, "attribute1");
    else
      anariSetParameter(dev, mat, "opacity", ANARI_FLOAT32, &opacityConstant);
  }
  else
  {
    anariSetParameter(dev, mat, "opacity", ANARI_FLOAT32, &opacityConstant);
  }

  anariCommitParameters(dev, mat);

  // put the mesh into a surface
  ANARISurface surface = anariNewSurface(dev);
  anariSetParameter(dev, surface, "name", ANARI_STRING, "tutorialSurface");
  anariSetParameter(dev, surface, "geometry", ANARI_GEOMETRY, &glyphs);
  anariSetParameter(dev, surface, "material", ANARI_MATERIAL, &mat);
  anariCommitParameters(dev, surface);
  anariRelease(dev, glyphs);
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
  anariSetParameter(dev, instance, "group", ANARI_GROUP, &group);
  anariRelease(dev, group);

  return instance;
}

void doTest(TestParameters_t testParams)
{
  printf("\n\n--------------  Starting new test iteration \n\n");
  printf("Parameters: writeatcommit %d, useVertexColors %d, useTexture %d,  transparent %d, geom type %s \n",
    testParams.writeAtCommit, testParams.useVertexColors, testParams.useTexture, testParams.isTransparent,
    ((testParams.testType == TEST_MESH) ? "mesh" : ((testParams.testType == TEST_CONES) ? "cones" : "glyphs")));

  stbi_flip_vertically_on_write(1);

  // image sizes
  int frameSize[2] = { 1024, 768 };
  int textureSize[2] = { 256, 256 };
  int gridSize[2] = { 100, 100 };

  uint8_t* textureData = 0;
  int numTexComponents = 3;
  textureData = generateTexture(textureSize, numTexComponents);

  uint8_t* opacityTextureData = 0;
  opacityTextureData = generateTexture(textureSize, 1);

  GridData_t gridData;
  createSineWaveGrid(gridSize, &gridData);

  TexData_t texData;
  texData.textureData = textureData;
  texData.opacityTextureData = opacityTextureData;
  texData.textureSize[0] = textureSize[0];
  texData.textureSize[1] = textureSize[1];

  // camera
  float cam_pos[] = { 0.f, 0.f, 0.f };
  float cam_up[] = { 0.f, 1.f, 0.f };
  float cam_view[] = { 0.1f, 0.f, 1.f };

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

  ANARIInstance instance;
  switch(testParams.testType)
  {
    case TEST_MESH:
      instance = createMeshInstance(dev, testParams, texData);
      break;
    case TEST_CONES:
      instance = createConesInstance(dev, testParams, gridData);
      break;
    case TEST_GLYPHS:
      instance = createGlyphsInstance(dev, testParams, gridData);
      break;
    default:
      break;
  };

  if(!instance)
    return;

  anariCommitParameters(dev, instance);

  // create and setup light for Ambient Occlusion
  ANARILight light = anariNewLight(dev, "ambient");
  anariSetParameter(dev, light, "name", ANARI_STRING, "tutorialLight");
  anariCommitParameters(dev, light);
  ANARIArray1D array = anariNewArray1D(dev, &light, 0, 0, ANARI_LIGHT, 1);
  anariCommitParameters(dev, array);
  anariSetParameter(dev, world, "light", ANARI_ARRAY, &array);
  anariRelease(dev, light);
  anariRelease(dev, array);

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
  freeSineWaveGrid(&gridData);

  printf("done!\n");
}

int main(int argc, const char **argv)
{
  TestParameters_t testParams;

  testParams.testType = TEST_MESH;
  testParams.writeAtCommit = 0;
  testParams.isTransparent = 0;

  // Test standard flow with textures
  testParams.useVertexColors = 0;
  testParams.useTexture = 1;
  doTest(testParams);

  // With vertex colors
  testParams.useVertexColors = 1;
  testParams.useTexture = 0;
  doTest(testParams);

  // With color constant
  testParams.useVertexColors = 0;
  testParams.useTexture = 0;
  doTest(testParams);

  // Transparency
  testParams.useVertexColors = 0;
  testParams.useTexture = 0;
  testParams.isTransparent = 1;
  doTest(testParams);

  // Transparency (vertex)
  testParams.useVertexColors = 1;
  testParams.useTexture = 0;
  doTest(testParams);

  // Transparency (sampler)
  testParams.useVertexColors = 0;
  testParams.useTexture = 1;
  doTest(testParams);

  // Test immediate mode writing
  testParams.writeAtCommit = 1;
  testParams.useVertexColors = 0;
  testParams.useTexture = 1;
  testParams.isTransparent = 0;
  doTest(testParams);

  // Cones tests
  testParams.testType = TEST_CONES;
  testParams.writeAtCommit = 0;
  testParams.useTexture = 0;
  testParams.useIndices = 0;

  // With vertex colors
  testParams.useVertexColors = 1;
  doTest(testParams);

  // With color constant
  testParams.useVertexColors = 0;
  doTest(testParams);

  // Using indices (and vertex colors)
  testParams.useIndices = 1;
  doTest(testParams);

  // Glyphs tests
  testParams.testType = TEST_GLYPHS;
  testParams.writeAtCommit = 0;
  testParams.useTexture = 0;
  testParams.useIndices = 0;
  testParams.useGlyphMesh = 0;

  // With vertex colors
  testParams.useVertexColors = 1;
  doTest(testParams);

  // With color constant
  testParams.useVertexColors = 0;
  doTest(testParams);

  // Using indices (and vertex colors)
  testParams.useGlyphMesh = 1;
  doTest(testParams);

  return 0;
}
