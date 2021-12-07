// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "usd.h"
PXR_NAMESPACE_USING_DIRECTIVE

#include "UsdBridgeCaches.h"

UsdBridgeTemporalCache::ConstPrimCacheIterator UsdBridgeTemporalCache::FindPrimCache(const UsdBridgeHandle& handle) const
{
  ConstPrimCacheIterator it = std::find_if(
    UsdPrimCaches.begin(),
    UsdPrimCaches.end(),
    [handle](const PrimCacheContainer::value_type& cmp) -> bool { return cmp.second.get() == handle.value;  }
  );

  return it;
}

UsdBridgeTemporalCache::ConstPrimCacheIterator UsdBridgeTemporalCache::CreatePrimCache(const std::string& name, const std::string& fullPath, ResourceCollectFunc collectFunc)
{
  SdfPath nameSuffix(name);
  SdfPath primPath(fullPath);

  // Create new cache entry
  std::unique_ptr<UsdBridgePrimCache> cacheEntry = std::make_unique<UsdBridgePrimCache>(primPath, nameSuffix, collectFunc);
  return UsdPrimCaches.emplace(name, std::move(cacheEntry)).first;
}

void UsdBridgeTemporalCache::InitializeWorldPrim(UsdBridgePrimCache* worldCache)
{
#ifdef TIME_BASED_CACHING
  worldCache->IncRef(); // No DecRef() to go with this IncRef(). Worlds remain within the usd.
#endif
}

#ifdef TIME_BASED_CACHING
void UsdBridgeTemporalCache::AddChild(UsdBridgePrimCache* parent, UsdBridgePrimCache* child)
{
  parent->Children.push_back(child);
  child->IncRef();
}

void UsdBridgeTemporalCache::RemoveChild(UsdBridgePrimCache* parent, UsdBridgePrimCache* child)
{
  auto it = std::find(parent->Children.begin(), parent->Children.end(), child);
  child->DecRef();
  *it = parent->Children.back();
  parent->Children.pop_back();
}

void UsdBridgeTemporalCache::RemoveUnreferencedChildTree(UsdBridgePrimCache* parent)
{
  for (UsdBridgePrimCache* child : parent->Children)
  {
    child->DecRef();
    if(child->RefCount == 0)
      RemoveUnreferencedChildTree(child);
  }
  parent->Children.clear();
}

void UsdBridgeTemporalCache::RemoveUnreferencedPrimCaches(std::function<void(ConstPrimCacheIterator)> atRemove)
{
  // First recursively remove all the child references for unreferenced prims
  // Can only be performed at garbage collect. 
  // If this is done during RemoveChild, an unreferenced parent cannot subsequently be revived with an AddChild.
  PrimCacheContainer::iterator it = UsdPrimCaches.begin();
  while (it != UsdPrimCaches.end())
  {
    if (it->second->RefCount == 0)
    {
      RemoveUnreferencedChildTree(it->second.get());
    }
    ++it;
  }

  // Now delete all prims without references from the cache
  it = UsdPrimCaches.begin();
  while (it != UsdPrimCaches.end())
  {
    if (it->second->RefCount == 0)
    {
      atRemove(it);

      it = UsdPrimCaches.erase(it);
    }
    else
      ++it;
  }
}
#endif