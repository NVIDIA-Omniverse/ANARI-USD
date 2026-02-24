// Copyright 2021 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <math.h>
#include <string.h>
#define PI 3.14159265

const char *g_libraryType = "usd";

// Severity threshold for status messages (default: warning and above).
// Maps to ANARIStatusSeverity values: 0=fatal, 1=error, 2=warning,
// 3=perf, 4=info, 5=debug.
ANARIStatusSeverity g_minSeverity = ANARI_SEVERITY_WARNING;

void parseArgs(int argc, const char **argv)
{
  for (int i = 1; i < argc; ++i)
  {
    if (strcmp(argv[i], "-v") == 0 && i + 1 < argc)
    {
      int v = atoi(argv[++i]);
      if (v >= 0 && v <= 5)
        g_minSeverity = (ANARIStatusSeverity)v;
      else
        fprintf(stderr, "Invalid verbosity level %d, expected 0-5\n", v);
    }
  }
}

#ifdef _WIN32
const char* texFile = "d:/models/texture.png";
#else
const char* texFile = "/home/<username>/models/texture.png"; // Point this to any png
#endif

const char* wrapS = "repeat";
const char* wrapT = "repeat";

/******************************************************************/
void statusFunc(const void *userData,
  ANARIDevice device,
  ANARIObject source,
  ANARIDataType sourceType,
  ANARIStatusSeverity severity,
  ANARIStatusCode code,
  const char *message)
{
  (void)userData;
  if (severity > g_minSeverity)
    return;
  if (severity == ANARI_SEVERITY_FATAL_ERROR)
  {
    fprintf(stderr, "[FATAL] %s\n", message);
  }
  else if (severity == ANARI_SEVERITY_ERROR)
  {
    fprintf(stderr, "[ERROR] %s\n", message);
  }
  else if (severity == ANARI_SEVERITY_WARNING)
  {
    fprintf(stderr, "[WARN ] %s\n", message);
  }
  else if (severity == ANARI_SEVERITY_PERFORMANCE_WARNING)
  {
    fprintf(stderr, "[PERF ] %s\n", message);
  }
  else if (severity == ANARI_SEVERITY_INFO)
  {
    fprintf(stderr, "[INFO ] %s\n", message);
  }
  else if (severity == ANARI_SEVERITY_DEBUG)
  {
    fprintf(stderr, "[DEBUG] %s\n", message);
  }
}

void writePNG(const char *fileName, ANARIDevice d, ANARIFrame frame)
{
  uint32_t size[2] = {0, 0};
  ANARIDataType type = ANARI_UNKNOWN;
  uint32_t *pixel = (uint32_t *)anariMapFrame(d, frame, "channel.color", &size[0], &size[1], &type);

  if (type == ANARI_UFIXED8_RGBA_SRGB)
    stbi_write_png(fileName, size[0], size[1], 4, pixel, 4 * size[0]);
  else
    printf("Incorrectly returned color buffer pixel type, image not saved.\n");

  anariUnmapFrame(d, frame, "channel.color");
}

uint8_t touint8t(float val)
{
  return (uint8_t)floorf((val + 1.0f)*127.5f); // 0.5*255
}

uint8_t* generateTexture(int* size, int numComps)
{
  uint8_t* texData = (uint8_t*)malloc(size[0]*size[1]*numComps);

  for(int i = 0; i < size[0]; ++i)
  {
    for(int j = 0; j < size[1]; ++j)
    {
      int pixIdx = i*size[1]+j;
      int offset = pixIdx*numComps;

      float rVal = sin((float)i/size[0] * 2.0f * PI);
      float gVal = cos((float)i/size[0] * 8.0f * PI);
      float bVal = cos((float)j/size[1] * 20.0f * PI);

      texData[offset] = touint8t(rVal);
      if(numComps >= 2)
        texData[offset+1] = touint8t(gVal);
      if(numComps >= 3)
        texData[offset+2] = touint8t(bVal);
      if(numComps == 4)
        texData[offset+3] = 255;
    }
  }

  return texData;
}

void freeTexture(uint8_t* data)
{
  free(data);
}

typedef struct GridData
{
  float* gridVertex;
  float* gridColor;
  float* gridOpacity;
  float* gridRadius;
  float* gridOrientation;
  int gridSize[2];
} GridData_t;

void createSineWaveGrid(int* size, GridData_t* gridData)
{
  int baseSize = size[0]*size[1]*sizeof(float);
  float* vertData = (float*)malloc(baseSize*3);
  float* colorData = (float*)malloc(baseSize*3);
  float* opacityData = (float*)malloc(baseSize);
  float* radiusData = (float*)malloc(baseSize);
  float* orientData = (float*)malloc(baseSize*4);

  for(int i = 0; i < size[0]; ++i)
  {
    for(int j = 0; j < size[1]; ++j)
    {
      int vertIdx = i*size[1]+j;
      int vertAddr = vertIdx*3;

      float iFrac = (float)i/size[0];
      float jFrac = (float)j/size[1];

      // Vertex
      float amp = sin((iFrac+jFrac) * 8.0f * PI) + cos(jFrac * 5.5f * PI);

      vertData[vertAddr] = ((float)i)*5.0f;
      vertData[vertAddr+1] = ((float)j)*5.0f;
      vertData[vertAddr+2] = amp*10.0f;

      // Color
      float rVal = sin(iFrac * 2.0f * PI);
      float gVal = cos(iFrac * 8.0f * PI);
      float bVal = cos(jFrac * 20.0f * PI);

      colorData[vertAddr] = rVal;
      colorData[vertAddr+1] = gVal;
      colorData[vertAddr+2] = bVal;

      // Opacity
      float opacity = 0.3f + 0.3f*(cos(iFrac * 1.0f * PI) + sin(jFrac * 7.0f * PI));

      opacityData[vertIdx] = opacity;

      // Radius
      //spacing is 5.0, so make sure not to go over 2.5
      float radius = 1.25f + 0.5f*(cos(iFrac * 5.0f * PI) + sin(jFrac * 2.0f * PI));

      radiusData[vertIdx] = radius;

      // Orientation
      int orientAddr = vertIdx*4;
      float roll = iFrac*PI*7.3, pitch = jFrac*PI*4.9, yaw = -(jFrac+0.45f)*PI*3.3;
      float sinRoll = sin(roll), sinPitch = sin(pitch), sinYaw = sin(yaw);
      float cosRoll = cos(roll), cosPitch = cos(pitch), cosYaw = cos(yaw);
      orientData[orientAddr] = cosRoll*cosPitch*cosYaw+sinRoll*sinPitch*sinYaw;
      orientData[orientAddr+1] = sinRoll*cosPitch*cosYaw-cosRoll*sinPitch*sinYaw;
      orientData[orientAddr+2] = cosRoll*sinPitch*cosYaw+sinRoll*cosPitch*sinYaw;
      orientData[orientAddr+3] = cosRoll*cosPitch*sinYaw-sinRoll*sinPitch*cosYaw;
    }
  }

  gridData->gridVertex = vertData;
  gridData->gridColor = colorData;
  gridData->gridOpacity = opacityData;
  gridData->gridRadius = radiusData;
  gridData->gridOrientation = orientData;
  gridData->gridSize[0] = size[0];
  gridData->gridSize[1] = size[1];
}

void freeSineWaveGrid(GridData_t* gridData)
{
  free(gridData->gridVertex);
  free(gridData->gridColor);
  free(gridData->gridOpacity);
  free(gridData->gridRadius);
  free(gridData->gridOrientation);
}