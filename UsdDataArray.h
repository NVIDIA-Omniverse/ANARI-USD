// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "UsdBaseObject.h"
#include "anari/anari_enums.h"

class UsdDevice;

struct UsdDataLayout
{
  bool isDense() const { return byteStride1 == typeSize && byteStride2 == numItems1*byteStride1 && byteStride3 == numItems2*byteStride2; }
  bool isOneDimensional() const { return numItems2 == 1 && numItems3 == 1; }

  uint64_t typeSize;
  uint64_t numItems1;
  int64_t byteStride1;
  uint64_t numItems2;
  int64_t byteStride2;
  uint64_t numItems3;
  int64_t byteStride3;
};

class UsdDataArray : public UsdBaseObject
{
  public:
    UsdDataArray(void *appMemory,
      ANARIMemoryDeleter deleter,
      void *userData,
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

    const void* getData() const { return data; }
    ANARIDataType getType() const { return type; }
    const UsdDataLayout& getLayout() const { return layout; }

    size_t getDataSizeInBytes() const { return dataSizeInBytes; }

  protected:
    bool deferCommit(UsdDevice* device) override { return false; }
    void doCommitWork(UsdDevice* device) override {}

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
    void freePublicData(void* appMemory);
    void publicToPrivateData();

    // Mapped memory management
    void CreateMappedObjectCopy();
    void TransferAndRemoveMappedObjectCopy();

    void* data = nullptr;
    ANARIMemoryDeleter dataDeleter = nullptr;
    void* deleterUserData = nullptr;
    ANARIDataType type;
    UsdDataLayout layout;
    size_t dataSizeInBytes;
    bool isPrivate;

    void* mappedObjectCopy;

#ifdef CHECK_MEMLEAKS
    UsdDevice* allocDevice;
#endif
};