// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"
#include "UsdParameterizedObject.h"
#include "anari/frontend/anari_enums.h"

class UsdDevice;

struct UsdDataLayout
{
  bool isDense() const { return byteStride1 == typeSize && byteStride2 == numItems1*byteStride1 && byteStride3 == numItems2*byteStride2; }
  bool isOneDimensional() const { return numItems2 == 1 && numItems3 == 1; }
  void copyDims(uint64_t dims[3]) const { std::memcpy(dims, &numItems1, sizeof(uint64_t)*3); }
  void copyStride(int64_t stride[3]) const { std::memcpy(stride, &byteStride1, sizeof(int64_t)*3); }

  uint64_t typeSize = 0;
  uint64_t numItems1 = 0;
  uint64_t numItems2 = 0;
  uint64_t numItems3 = 0;
  int64_t byteStride1 = 0;
  int64_t byteStride2 = 0;
  int64_t byteStride3 = 0;
};

struct UsdDataArrayParams
{
  // Even though we are not dealing with a usd-backed object, the data array can still have an identifying name
  UsdSharedString* name = nullptr;
  UsdSharedString* usdName = nullptr;
};

class UsdDataArray : public UsdBaseObject, public UsdParameterizedObject<UsdDataArray, UsdDataArrayParams>
{
  public:
    UsdDataArray(const void *appMemory,
      ANARIMemoryDeleter deleter,
      const void *userData,
      ANARIDataType dataType,
      uint64_t numItems1,
      int64_t byteStride1,
      uint64_t numItems2,
      int64_t byteStride2,
      uint64_t numItems3,
      int64_t byteStride3,
      UsdDevice* device
    );
    UsdDataArray(ANARIDataType dataType,
      uint64_t numItems1,
      uint64_t numItems2,
      uint64_t numItems3,
      UsdDevice* device
    );
    ~UsdDataArray();

    void filterSetParam(const char *name,
      ANARIDataType type,
      const void *mem,
      UsdDevice* device) override;

    void filterResetParam(
      const char *name) override;

    int getProperty(const char *name,
      ANARIDataType type,
      void *mem,
      uint64_t size,
      UsdDevice* device) override;

    void commit(UsdDevice* device) override;

    void* map(UsdDevice* device);
    void unmap(UsdDevice* device);

    void privatize();

    const UsdSharedString* getName() const { return getReadParams().usdName; }

    const void* getData() const { return data; }
    ANARIDataType getType() const { return type; }
    const UsdDataLayout& getLayout() const { return layout; }

    size_t getDataSizeInBytes() const { return dataSizeInBytes; }

  protected:
    bool deferCommit(UsdDevice* device) override { return false; }
    bool doCommitData(UsdDevice* device) override { return false; }
    void doCommitRefs(UsdDevice* device) override {}

    void setLayoutAndSize(uint64_t numItems1,
      int64_t byteStride1,
      uint64_t numItems2,
      int64_t byteStride2,
      uint64_t numItems3,
      int64_t byteStride3);
    bool CheckFormatting(UsdDevice* device);

    // Ref manipulation on arrays of anariobjects
    void incRef(const ANARIObject* anariObjects, uint64_t numAnariObjects);
    void decRef(const ANARIObject* anariObjects, uint64_t numAnariObjects);

    // Private memory management
    void allocPrivateData();
    void freePrivateData(bool mappedCopy = false);
    void freePublicData(const void* appMemory);
    void publicToPrivateData();

    // Mapped memory management
    void CreateMappedObjectCopy();
    void TransferAndRemoveMappedObjectCopy();

    const void* data = nullptr;
    ANARIMemoryDeleter dataDeleter = nullptr;
    const void* deleterUserData = nullptr;
    ANARIDataType type;
    UsdDataLayout layout;
    size_t dataSizeInBytes;
    bool isPrivate;

    const void* mappedObjectCopy;

#ifdef CHECK_MEMLEAKS
    UsdDevice* allocDevice;
#endif
};