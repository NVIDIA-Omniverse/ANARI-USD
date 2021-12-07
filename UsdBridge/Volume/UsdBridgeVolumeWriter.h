// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeVolumeWriter_h
#define UsdBridgeVolumeWriter_h

#include "UsdBridgeData.h"

#include <ostream>

class UsdBridgeVolumeWriter
{
  public:
    UsdBridgeVolumeWriter();
    ~UsdBridgeVolumeWriter();

    bool Initialize(UsdBridgeLogCallback logCallback, void* logUserData);

    void ToVDB(const UsdBridgeVolumeData& volumeData, std::ostream& vdbOutput);
    
    static UsdBridgeLogCallback LogCallback;
    static void* LogUserData;

  protected:
};



#endif