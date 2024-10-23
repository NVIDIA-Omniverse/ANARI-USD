// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBaseObject.h"
#include "UsdDevice.h"

#include <algorithm>

UsdBaseObject::UsdBaseObject(ANARIDataType t, UsdDevice* device)
      : type(t)
{
  // The object will not be committed (as in, user-written write params will not be set to read params),
  // but handles will be initialized and the object with its default data/refs will be written out to USD
  // (but only if the prim wasn't yet written to USD before, see 'isNew' in doCommit implementations).
  if(device)
    device->addToCommitList(this, true);
}

void UsdBaseObject::commit(UsdDevice* device)
{ 
  bool deferDataCommit = !device->isInitialized() || !device->getReadParams().writeAtCommit || deferCommit(device);
  if(!deferDataCommit)
  {                 
    bool commitRefs = doCommitData(device);
    if(commitRefs)
      device->addToCommitList(this, false); // Commit refs, but no more data later on
  }
  else
    device->addToCommitList(this, true); // Commit data and refs later on
}

void UsdBaseObject::addObserver(UsdBaseObject* observer)
{
  // duplicate entries allowed in case the same object is observed from multiple references on the same observer
  // (implicit ref counter of observed object)
  observers.push_back(observer);
}

void UsdBaseObject::removeObserver(UsdBaseObject* observer)
{
  auto it = std::find(observers.begin(), observers.end(), observer);
  assert(it != observers.end());

  *it = observers.back();
  observers.pop_back();
}

void UsdBaseObject::notify(UsdBaseObject* caller, UsdDevice* device)
{
  auto it = observers.begin();
  while(it != observers.end())
  {
    auto observer = *it;

#ifdef CHECK_MEMLEAKS
    assert(device->isObjAllocated(observer));
#endif

    // Don't call observe twice
    if(std::find(observers.begin(), it, observer) == it)
      observer->observe(caller, device);

    ++it;
  }
}

void UsdBridgeAddToCommitList(UsdDevice* device, UsdBaseObject* object, bool commitData)
{
  device->addToCommitList(object, commitData);
}
