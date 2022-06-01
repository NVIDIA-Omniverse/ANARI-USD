// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "usd.h"
PXR_NAMESPACE_USING_DIRECTIVE

#include "UsdBridgeCaches.h"

UsdBridgePrimCacheManager::ConstPrimCacheIterator UsdBridgePrimCacheManager::FindPrimCache(const UsdBridgeHandle& handle) const
{
  ConstPrimCacheIterator it = std::find_if(
    UsdPrimCaches.begin(),
    UsdPrimCaches.end(),
    [handle](const PrimCacheContainer::value_type& cmp) -> bool { return cmp.second.get() == handle.value;  }
  );

  return it;
}

UsdBridgePrimCacheManager::ConstPrimCacheIterator UsdBridgePrimCacheManager::CreatePrimCache(const std::string& name, const std::string& fullPath, ResourceCollectFunc collectFunc)
{
  SdfPath nameSuffix(name);
  SdfPath primPath(fullPath);

  // Create new cache entry
  std::unique_ptr<UsdBridgePrimCache> cacheEntry = std::make_unique<UsdBridgePrimCache>(primPath, nameSuffix, collectFunc);
  return UsdPrimCaches.emplace(name, std::move(cacheEntry)).first;
}

void UsdBridgePrimCacheManager::InitializeWorldPrim(UsdBridgePrimCache* worldCache)
{
  worldCache->IncRef(); // No DecRef() to go with this IncRef(). Worlds remain within the usd.
}

void UsdBridgePrimCacheManager::AddChild(UsdBridgePrimCache* parent, UsdBridgePrimCache* child)
{
  parent->Children.push_back(child);
  child->IncRef();
}

void UsdBridgePrimCacheManager::RemoveChild(UsdBridgePrimCache* parent, UsdBridgePrimCache* child)
{
  auto it = std::find(parent->Children.begin(), parent->Children.end(), child);
  // Allow for find to fail; in the case where the bridge is recreated and destroyed, 
  // a child prim exists which doesn't have a ref in the cache. 
  if(it != parent->Children.end()) 
  {
    child->DecRef();
    *it = parent->Children.back();
    parent->Children.pop_back();
  }
}

void UsdBridgePrimCacheManager::RemoveUnreferencedChildTree(UsdBridgePrimCache* parent, AtRemoveFunc atRemove)
{
  assert(parent->RefCount == 0);
  atRemove(parent);

  for (UsdBridgePrimCache* child : parent->Children)
  {
    child->DecRef();
    if(child->RefCount == 0)
      RemoveUnreferencedChildTree(child, atRemove);
  }
  parent->Children.clear();
}

void UsdBridgePrimCacheManager::RemoveUnreferencedPrimCaches(AtRemoveFunc atRemove)
{
  // First recursively remove all the child references for unreferenced prims
  // Can only be performed at garbage collect. 
  // If this is done during RemoveChild, an unreferenced parent cannot subsequently be revived with an AddChild.
  PrimCacheContainer::iterator it = UsdPrimCaches.begin();
  while (it != UsdPrimCaches.end())
  {
    if (it->second->RefCount == 0)
    {
      RemoveUnreferencedChildTree(it->second.get(), atRemove);
    }
    ++it;
  }

  // Now delete all prims without references from the cache
  it = UsdPrimCaches.begin();
  while (it != UsdPrimCaches.end())
  {
    if (it->second->RefCount == 0)
      it = UsdPrimCaches.erase(it);
    else
      ++it;
  }
}