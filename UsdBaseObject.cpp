// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBaseObject.h"
#include "UsdDevice.h"

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
      device->addToCommitList(this, false); // Commit refs, but no more data
  }
  else
    device->addToCommitList(this, true); // Commit data and refs
}
