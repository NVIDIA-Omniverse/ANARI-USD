// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "usd.h"
PXR_NAMESPACE_USING_DIRECTIVE

#include "UsdBridgeCaches.h"

#ifdef VALUE_CLIP_RETIMING
constexpr double UsdBridgePrimCache::PrimStageTimeCode;
#endif

UsdBridgePrimCache::UsdBridgePrimCache(const SdfPath& pp, const SdfPath& nm, ResourceCollectFunc cf)
    : PrimPath(pp), Name(nm), ResourceCollect(cf)
#ifndef NDEBUG
    , Debug_Name(nm.GetString())
#endif
{
  if(cf)
  {
    ResourceKeys = std::make_unique<ResourceContainer>();
  }
}

void UsdBridgePrimCache::AddChild(UsdBridgePrimCache* child)
{
  this->Children.push_back(child);
  child->IncRef();
}

void UsdBridgePrimCache::RemoveChild(UsdBridgePrimCache* child)
{
  auto it = std::find(this->Children.begin(), this->Children.end(), child);
  // Allow for find to fail; in the case where the bridge is recreated and destroyed,
  // a child prim exists which doesn't have a ref in the cache.
  if(it != this->Children.end())
  {
    child->DecRef();
    *it = this->Children.back();
    this->Children.pop_back();
  }
}

void UsdBridgePrimCache::RemoveUnreferencedChildTree(AtRemoveFunc atRemove)
{
  assert(this->RefCount == 0);
  atRemove(this);

  for (UsdBridgePrimCache* child : this->Children)
  {
    child->DecRef();
    if(child->RefCount == 0)
      child->RemoveUnreferencedChildTree(atRemove);
  }
  this->Children.clear();
}

bool UsdBridgePrimCache::AddResourceKey(UsdBridgeResourceKey key) // copy by value
{
  assert(ResourceKeys);
  bool newEntry = std::find(ResourceKeys->begin(), ResourceKeys->end(), key) == ResourceKeys->end();
  if(newEntry)
    ResourceKeys->push_back(key);
  return newEntry;
}

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

void UsdBridgePrimCacheManager::InitializeTopLevelPrim(UsdBridgePrimCache* primCache)
{
  primCache->IncRef(); // No DecRef() to go with this IncRef(). Prims that aren't rooted aren't automatically removed.
}

void UsdBridgePrimCacheManager::AddChild(UsdBridgePrimCache* parent, UsdBridgePrimCache* child)
{
  parent->AddChild(child);
}

void UsdBridgePrimCacheManager::RemoveChild(UsdBridgePrimCache* parent, UsdBridgePrimCache* child)
{
  parent->RemoveChild(child);
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
      it->second->RemoveUnreferencedChildTree(atRemove);
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