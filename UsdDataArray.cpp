// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdDataArray.h"
#include "UsdDevice.h"
#include "UsdAnari.h"
#include "anari/type_utility.h"

DEFINE_PARAMETER_MAP(UsdDataArray,
  REGISTER_PARAMETER_MACRO("name", ANARI_STRING, name)
  REGISTER_PARAMETER_MACRO("usd::name", ANARI_STRING, usdName)
)

#define TO_OBJ_PTR reinterpret_cast<const ANARIObject*>

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
  : UsdBaseObject(ANARI_ARRAY)
  , data(appMemory)
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
  : UsdBaseObject(ANARI_ARRAY)
  , type(dataType)
  , isPrivate(true)
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
  if(std::strcmp(name, "name") == 0)
  {
    const char* srcCstr = reinterpret_cast<const char*>(mem);
    
    if(srcCstr != 0 && strlen(srcCstr) > 0)
    {
      setParam(name, type, mem, device);
      setParam("usd::name", type, mem, device);
      formatUsdName(getWriteParams().usdName);

      //Name is kept for the lifetime of the device (to allow using pointer for shared resource caching)
      device->addToSharedStringList(getWriteParams().usdName); 
    }
    else
      device->reportStatus(this, ANARI_ARRAY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
        "UsdDataArray name parameter set failed: name of zero length.");
  }
}

void UsdDataArray::filterResetParam(
  const char *name)
{

}

int UsdDataArray::getProperty(const char * name, ANARIDataType type, void * mem, uint64_t size, UsdDevice* device)
{
  if (strcmp(name, "usd::name") == 0)
  {
    device->reportStatus(this, ANARI_ARRAY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdDataArray does not have a usd::name property, as it is not represented in the USD output.");
  }
  return 0;
}

void UsdDataArray::commit(UsdDevice* device)
{
  transferWriteToReadParams();

  if (anari::isObject(type) && (layout.numItems2 != 1 || layout.numItems3 != 1))
    device->reportStatus(this, ANARI_ARRAY, ANARI_SEVERITY_ERROR, ANARI_STATUS_INVALID_ARGUMENT,
      "UsdDataArray only supports one-dimensional ANARI_OBJECT arrays");
}

void * UsdDataArray::map(UsdDevice * device)
{
  if (anari::isObject(type))
  {
    CreateMappedObjectCopy();
  }

  return const_cast<void *>(data);
}

void UsdDataArray::unmap(UsdDevice * device)
{
  if (anari::isObject(type))
  {
    TransferAndRemoveMappedObjectCopy();
  }
}

void UsdDataArray::privatize()
{
  publicToPrivateData();
  isPrivate = true;
}

void UsdDataArray::setLayoutAndSize(uint64_t numItems1,
  int64_t byteStride1,
  uint64_t numItems2,
  int64_t byteStride2,
  uint64_t numItems3,
  int64_t byteStride3)
{
  size_t typeSize = AnariTypeSize(type);
  if (byteStride1 == 0)
    byteStride1 = typeSize;
  if (byteStride2 == 0)
    byteStride2 = byteStride1 * numItems1;
  if (byteStride3 == 0)
    byteStride3 = byteStride2 * numItems2;

  dataSizeInBytes = byteStride3 * numItems3 - (byteStride1 - typeSize);
  layout = { typeSize, numItems1, byteStride1, numItems2, byteStride2, numItems3, byteStride3 };
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

void UsdDataArray::incRef(const ANARIObject* anariObjects, uint64_t numAnariObjects)
{
  for (int i = 0; i < numAnariObjects; ++i)
  {
    const UsdBaseObject* baseObj = (reinterpret_cast<const UsdBaseObject*>(anariObjects[i]));
    if (baseObj)
      baseObj->refInc(anari::RefType::INTERNAL);
  }
}

void UsdDataArray::decRef(const ANARIObject* anariObjects, uint64_t numAnariObjects)
{
  for (int i = 0; i < numAnariObjects; ++i)
  {
    const UsdBaseObject* baseObj = (reinterpret_cast<const UsdBaseObject*>(anariObjects[i]));
#ifdef CHECK_MEMLEAKS
    allocDevice->LogDeallocation(baseObj);
#endif
    if (baseObj)
      baseObj->refDec(anari::RefType::INTERNAL);
  }
}

void UsdDataArray::allocPrivateData()
{
  // Alloc the owned memory
  char* newData = new char[dataSizeInBytes];
  memset(newData, 0, dataSizeInBytes);
  data = newData;
}

void UsdDataArray::freePrivateData(bool mappedCopy)
{
  const void*& memToFree = mappedCopy ? mappedObjectCopy : data;

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

  std::memcpy(const_cast<void *>(data), appMemory, dataSizeInBytes); // In case of object array, Refcount 'transfers' to the copy (splits off user-managed public refcount)

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
  std::memcpy(const_cast<void *>(data), mappedObjectCopy, dataSizeInBytes);
}

void UsdDataArray::TransferAndRemoveMappedObjectCopy()
{
  const ANARIObject* newAnariObjects = TO_OBJ_PTR(data);
  const ANARIObject* oldAnariObjects = TO_OBJ_PTR(mappedObjectCopy);
  uint64_t numAnariObjects = layout.numItems1;

  // First, increase reference counts of all objects that different in the new object array
  for (int i = 0; i < numAnariObjects; ++i)
  {
    const UsdBaseObject* newObj = (reinterpret_cast<const UsdBaseObject*>(newAnariObjects[i]));
    const UsdBaseObject* oldObj = (reinterpret_cast<const UsdBaseObject*>(oldAnariObjects[i]));

    if (newObj != oldObj && newObj)
      newObj->refInc(anari::RefType::INTERNAL);
  }

  // Then, decrease reference counts of all objects that are different in the original array (which will delete those that not referenced anymore)
  for (int i = 0; i < numAnariObjects; ++i)
  {
    const UsdBaseObject* newObj = (reinterpret_cast<const UsdBaseObject*>(newAnariObjects[i]));
    const UsdBaseObject* oldObj = (reinterpret_cast<const UsdBaseObject*>(oldAnariObjects[i]));

    if (newObj != oldObj && oldObj)
    {
#ifdef CHECK_MEMLEAKS
      allocDevice->LogDeallocation(oldObj);
#endif
      oldObj->refDec(anari::RefType::INTERNAL);
    }
  }

  // Release the mapped object copy's allocated memory
  freePrivateData(true);
}
