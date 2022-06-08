// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdBaseObject.h"
#include "UsdDevice.h"

void UsdBaseObject::commit(UsdDevice* device)
{ 
  if(device->getParams().writeAtCommit && !deferCommit(device))
  {                    
    doCommitWork(device);
    //device->removeFromCommitList(this); // In case a different object added this one to the commit list
  }
  else
    device->addToCommitList(this); // Simply add to the commit list
}
