// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef OmniBridgeCaches_h
#define OmniBridgeCaches_h

#include <string>
#include <map>

#include "UsdBridgeData.h"

struct UsdBridgePrimCache;
class UsdBridgeUsdWriter;
class UsdBridgeTemporalCache;

typedef std::pair<std::string, UsdStageRefPtr> UsdStagePair; //Stage ptr and filename
typedef std::pair<std::pair<bool,bool>, UsdBridgePrimCache*> BoolEntryPair; // Prim exists in stage, prim exists in cache, result cache entry ptr
typedef void (*ResourceCollectFunc)(const UsdBridgePrimCache*, const UsdBridgeUsdWriter&);
typedef std::vector<UsdBridgePrimCache*> UsdBridgePrimCacheList;

#ifdef TIME_BASED_CACHING
struct UsdBridgeRefCache
{
public:
  friend class UsdBridgeTemporalCache; // Only temporal cache can manage references

protected:
  void IncRef() { ++RefCount; }
  void DecRef() { --RefCount; }

  unsigned int RefCount = 0;
  std::vector<UsdBridgePrimCache*> Children;
  //Could also contain a mapping from child to an array of (parentTime,childTime) 
  //This would allow single timesteps to be removed in case of unused/replaced references at a parentTime (instead of removal of child if visible), without breaking garbage collection.
  //Additionally, a refcount per timestep would help to perform garbage collection of individual child clip stages/files, 
  //when no parent references that particular childTime anymore (due to removal/replacing with refs to other children, or replacement with different childTimes for the same child).
};

struct UsdBridgePrimCache : public UsdBridgeRefCache
#else
struct UsdBridgePrimCache
#endif
{
  //Constructors
  UsdBridgePrimCache(const SdfPath& pp, const SdfPath& nm, ResourceCollectFunc cf)
    : PrimPath(pp), Name(nm), ResourceCollect(cf)
#ifndef NDEBUG
    , Debug_Name(nm.GetString()) 
#endif
  {}

  SdfPath PrimPath;
  SdfPath Name;
  ResourceCollectFunc ResourceCollect;

#ifdef VALUE_CLIP_RETIMING
  UsdStagePair PrimStage; // Holds the stage with timevarying data for the prim, or the (init-only) manifest if TIME_CLIP_STAGES and prim supports it (ie. geometry) 
  bool OwnsPrimStage = true;
#endif

#ifdef TIME_CLIP_STAGES
  std::unordered_map<double, UsdStagePair> ClipStages; // Holds the stages to the per-clip data
#endif

#ifndef NDEBUG
  std::string Debug_Name;
#endif
};

class UsdBridgeTemporalCache
{
public:
  typedef std::unordered_map<std::string, std::unique_ptr<UsdBridgePrimCache>> PrimCacheContainer;
  typedef PrimCacheContainer::iterator PrimCacheIterator;
  typedef PrimCacheContainer::const_iterator ConstPrimCacheIterator;

  inline UsdBridgePrimCache* ConvertToPrimCache(const UsdBridgeHandle& handle) const
  {
#ifndef NDEBUG
    auto primCacheIt = FindPrimCache(handle);
    assert(ValidIterator(primCacheIt));
#endif
    return handle.value;
  }

  ConstPrimCacheIterator FindPrimCache(const std::string& name) const { return UsdPrimCaches.find(name); }
  ConstPrimCacheIterator FindPrimCache(const UsdBridgeHandle& handle) const;

  inline bool ValidIterator(ConstPrimCacheIterator it) const { return it != UsdPrimCaches.end(); }

  ConstPrimCacheIterator CreatePrimCache(const std::string& name, const std::string& fullPath, ResourceCollectFunc collectFunc = nullptr);
  void RemovePrimCache(ConstPrimCacheIterator it) { UsdPrimCaches.erase(it); }

  void InitializeWorldPrim(UsdBridgePrimCache* worldCache);

#ifdef TIME_BASED_CACHING
  void AddChild(UsdBridgePrimCache* parent, UsdBridgePrimCache* child);
  void RemoveChild(UsdBridgePrimCache* parent, UsdBridgePrimCache* child);
  void RemoveUnreferencedChildTree(UsdBridgePrimCache* parent);
  void RemoveUnreferencedPrimCaches(std::function<void(ConstPrimCacheIterator)> atRemove);
#endif

protected:

  PrimCacheContainer UsdPrimCaches;
};

#endif