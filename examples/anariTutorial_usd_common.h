// Copyright 2021 NVIDIA Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <math.h>
#define PI 3.14159265

const char *g_libraryType = "usd";

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
  uint32_t *pixel = (uint32_t *)anariMapFrame(d, frame, "color", &size[0], &size[1], &type);

  if (type == ANARI_UFIXED8_RGBA_SRGB)
    stbi_write_png(fileName, size[0], size[1], 4, pixel, 4 * size[0]);
  else
    printf("Incorrectly returned color buffer pixel type, image not saved.\n");

  anariUnmapFrame(d, frame, "color");
}

inline uint8_t normalize(float val)
{
  return (uint8_t)floorf((val+1.0f)*128.0f); // 0.5*256
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

      texData[offset] = normalize(rVal);
      texData[offset+1] = normalize(gVal);
      texData[offset+2] = normalize(bVal);
    }
  }
  
  return texData;
}

void freeTexture(uint8_t* data)
{
  free(data);
}