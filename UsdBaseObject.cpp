// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBaseObject.h"
#include "UsdDevice.h"

void UsdBaseObject::commit(UsdDevice* device)
{ 
  bool deferDataCommit = !device->isInitialized() || !device->getReadParams().writeAtCommit || deferCommit(device);
  if(!deferDataCommit)
  {                 
    bool commitRefs = doCommitData(device);
    if(commitRefs)
      device->addToCommitList(this, false); // Commit refs, but no more data
  }

  device->addToCommitList(this, true); // Commit data and refs
}
