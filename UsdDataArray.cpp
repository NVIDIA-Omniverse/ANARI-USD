// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdDataArray.h"
#include "UsdDevice.h"
#include "UsdAnari.h"
#include "anari/frontend/type_utility.h"

DEFINE_PARAMETER_MAP(UsdDataArray,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
)

#define TO_OBJ_PTR reinterpret_cast<ANARIObject*>

UsdDataArray::UsdDataArray(const void *appMemory,
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
)
  : UsdParameterizedBaseObject<UsdDataArray, UsdDataArrayParams>(ANARI_ARRAY)
  , data(const_cast<void*>(appMemory))
  , dataDeleter(deleter)
  , deleterUserData(userData)
  , type(dataType)
  , isPrivate(false)
#ifdef CHECK_MEMLEAKS
  , allocDevice(device)
#endif
{
  setLayoutAndSize(numItems1, byteStride1, numItems2, byteStride2, numItems3, byteStride3);

  if (CheckFormatting(device))
  {
    // Make sure to incref all anari objects in case of object array
    if (anari::isObject(type))
    {
      incRef(TO_OBJ_PTR(data), layout.numItems1);
    }
  }
}

UsdDataArray::UsdDataArray(ANARIDataType dataType, uint64_t numItems1, uint64_t numItems2, uint64_t numItems3, UsdDevice* device)
  : UsdParameterizedBaseObject<UsdDataArray, UsdDataArrayParams>(ANARI_ARRAY)
  , type(dataType)
  , isPrivate(true)
#ifdef CHECK_MEMLEAKS
  , allocDevice(device)
#endif
{
  setLayoutAndSize(numItems1, 0, numItems2, 0, numItems3, 0);

  if (CheckFormatting(device))
  {
    allocPrivateData();
  }
}

UsdDataArray::~UsdDataArray()
{
  // Decref anari objects in case of object array
  if (anari::isObject(type))
  {
    decRef(TO_OBJ_PTR(data), layout.numItems1);
  }

  if (isPrivate)
  {
    freePrivateData();
  }
  else
  {
    freePublicData(data);
  }
}

void UsdDataArray::filterSetParam(const char *name,
  ANARIDataType type,
  const void *mem,
  UsdDevice* device)
{
  if(setNameParam(name, type, mem, device))
    device->addToResourceStringList(getWriteParams().usdName); //Name is kept for the lifetime of the device (to allow using pointer for caching resource's names)
}

int UsdDataArray::getProperty(const char * name, ANARIDataType type, void * mem, uint64_t size, UsdDevice* device)
{
  int nameResult = getNameProperty(name, type, mem, size, device);
  return nameResult;
}

void UsdDataArray::commit(UsdDevice* device)
{
  if (anari::isObject(type) && (layout.numItems2 != 1 || layout.numItems3 != 1))
    device->reportStatus(this, ANARI_ARRAY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdDataArray only supports one-dimensional ANARI_OBJECT arrays");

  UsdParameterizedBaseObject<UsdDataArray, UsdDataArrayParams>::commit(device);
}

void * UsdDataArray::map(UsdDevice * device)
{
  if (anari::isObject(type))
  {
    CreateMappedObjectCopy();
  }

  return data;
}

void UsdDataArray::unmap(UsdDevice * device)
{
  if (anari::isObject(type))
  {
    TransferAndRemoveMappedObjectCopy();
  }

  notify(this, device); // Any objects referencing this array need to recommit
}

void UsdDataArray::privatize()
{
  if(!isPrivate)
  {
    publicToPrivateData();
    isPrivate = true;
  }
}

void UsdDataArray::setLayoutAndSize(uint64_t numItems1,
  int64_t byteStride1,
  uint64_t numItems2,
  int64_t byteStride2,
  uint64_t numItems3,
  int64_t byteStride3)
{
  size_t typeSize = anari::sizeOf(type);
  if (byteStride1 == 0)
    byteStride1 = typeSize;
  if (byteStride2 == 0)
    byteStride2 = byteStride1 * numItems1;
  if (byteStride3 == 0)
    byteStride3 = byteStride2 * numItems2;

  dataSizeInBytes = byteStride3 * numItems3 - (byteStride1 - typeSize);
  layout = { typeSize, numItems1, numItems2, numItems3, byteStride1, byteStride2, byteStride3 };
}

bool UsdDataArray::CheckFormatting(UsdDevice* device)
{
  if (anari::isObject(type))
  {
    if (!layout.isDense() || !layout.isOneDimensional())
    {
      device->reportStatus(this, ANARI_ARRAY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT, "UsdDataArray construction failed: arrays with object type have to be one dimensional and without stride.");

      layout.numItems1 = layout.numItems2 = layout.numItems3 = 0;
      data = nullptr;
      type = ANARI_UNKNOWN;

      return false;
    }
  }

  return true;
}

void UsdDataArray::incRef(ANARIObject* anariObjects, uint64_t numAnariObjects)
{
  for (uint64_t i = 0; i < numAnariObjects; ++i)
  {
    UsdBaseObject* baseObj = reinterpret_cast<UsdBaseObject*>(anariObjects[i]);
    if (baseObj)
      baseObj->refInc(helium::RefType::INTERNAL);
  }
}

void UsdDataArray::decRef(ANARIObject* anariObjects, uint64_t numAnariObjects)
{
  for (uint64_t i = 0; i < numAnariObjects; ++i)
  {
    UsdBaseObject* baseObj = reinterpret_cast<UsdBaseObject*>(anariObjects[i]);
#ifdef CHECK_MEMLEAKS
    allocDevice->logObjDeallocation(baseObj);
#endif
    if (baseObj)
    {
      assert(baseObj->useCount(helium::RefType::INTERNAL) > 0);
      baseObj->refDec(helium::RefType::INTERNAL);
    }
  }
}

void UsdDataArray::allocPrivateData()
{
  // Alloc the owned memory
  char* newData = new char[dataSizeInBytes];
  memset(newData, 0, dataSizeInBytes);
  data = newData;

#ifdef CHECK_MEMLEAKS
  allocDevice->logRawAllocation(newData);
#endif
}

void UsdDataArray::freePrivateData(bool mappedCopy)
{
  void*& memToFree = mappedCopy ? mappedObjectCopy : data;

#ifdef CHECK_MEMLEAKS
  allocDevice->logRawDeallocation(memToFree);
#endif

  // Deallocate owned memory
  delete[](char*)memToFree;
  memToFree = nullptr;
}

void UsdDataArray::freePublicData(const void* appMemory)
{
  if (dataDeleter)
  {
    dataDeleter(deleterUserData, appMemory);
    dataDeleter = nullptr;
  }
}

void UsdDataArray::publicToPrivateData()
{
  // Alloc private dest, copy appMemory src to it
  const void* appMemory = data;
  allocPrivateData();

  std::memcpy(data, appMemory, dataSizeInBytes); // In case of object array, Refcount 'transfers' to the copy (splits off user-managed public refcount)

  // Delete appMemory if appropriate
  freePublicData(appMemory);
  // No refcount modification necessary, public refcount managed by user
}

void UsdDataArray::CreateMappedObjectCopy()
{
  // Move the original array to a different spot and allocate new memory for the mapped object array.
  mappedObjectCopy = data;
  allocPrivateData();

  // Transfer contents over to new memory, keep old one for managing references later on.
  std::memcpy(data, mappedObjectCopy, dataSizeInBytes);
}

void UsdDataArray::TransferAndRemoveMappedObjectCopy()
{
  ANARIObject* newAnariObjects = TO_OBJ_PTR(data);
  ANARIObject* oldAnariObjects = TO_OBJ_PTR(mappedObjectCopy);
  uint64_t numAnariObjects = layout.numItems1;

  // First, increase reference counts of all objects that different in the new object array
  for (uint64_t i = 0; i < numAnariObjects; ++i)
  {
    UsdBaseObject* newObj = reinterpret_cast<UsdBaseObject*>(newAnariObjects[i]);
    UsdBaseObject* oldObj = reinterpret_cast<UsdBaseObject*>(oldAnariObjects[i]);

    if (newObj != oldObj && newObj)
      newObj->refInc(helium::RefType::INTERNAL);
  }

  // Then, decrease reference counts of all objects that are different in the original array (which will delete those that not referenced anymore)
  for (uint64_t i = 0; i < numAnariObjects; ++i)
  {
    UsdBaseObject* newObj = reinterpret_cast<UsdBaseObject*>(newAnariObjects[i]);
    UsdBaseObject* oldObj = reinterpret_cast<UsdBaseObject*>(oldAnariObjects[i]);

    if (newObj != oldObj && oldObj)
    {
#ifdef CHECK_MEMLEAKS
      allocDevice->logObjDeallocation(oldObj);
#endif
      oldObj->refDec(helium::RefType::INTERNAL);
    }
  }

  // Release the mapped object copy's allocated memory
  freePrivateData(true);
}
