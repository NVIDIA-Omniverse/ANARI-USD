// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeVolumeWriter_h
#define UsdBridgeVolumeWriter_h

#include "UsdBridgeData.h"

#ifdef _WIN32
#ifdef UsdBridge_Volume_EXPORTS
#define USDDevice_INTERFACE __declspec(dllexport)
#else
#define USDDevice_INTERFACE __declspec(dllimport)
#endif
#endif

class UsdBridgeVolumeWriterI
{
  public:

    virtual bool Initialize(UsdBridgeLogCallback logCallback, void* logUserData) = 0;

    virtual void ToVDB(const UsdBridgeVolumeData& volumeData) = 0;

    virtual void GetSerializedVolumeData(const char*& data, size_t& size) = 0;

    virtual void Release() = 0; // Accommodate change of CRT
};

extern "C" USDDevice_INTERFACE UsdBridgeVolumeWriterI* __cdecl Create_VolumeWriter();

#endif