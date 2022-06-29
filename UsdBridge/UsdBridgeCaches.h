// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef OmniBridgeCaches_h
#define OmniBridgeCaches_h

#include <string>
#include <map>

#include "UsdBridgeData.h"

struct UsdBridgePrimCache;
class UsdBridgeUsdWriter;
class UsdBridgePrimCacheManager;

typedef std::pair<std::string, UsdStageRefPtr> UsdStagePair; //Stage ptr and filename
typedef std::pair<std::pair<bool,bool>, UsdBridgePrimCache*> BoolEntryPair; // Prim exists in stage, prim exists in cache, result cache entry ptr
typedef void (*ResourceCollectFunc)(const UsdBridgePrimCache*, const UsdBridgeUsdWriter&);
typedef std::function<void(UsdBridgePrimCache*)> AtRemoveFunc;
typedef std::vector<UsdBridgePrimCache*> UsdBridgePrimCacheList;


struct UsdBridgeRefCache
{
public:
  friend class UsdBridgePrimCacheManager; 

protected:
  void IncRef() { ++RefCount; }
  void DecRef() { --RefCount; }

  unsigned int RefCount = 0;
  std::vector<UsdBridgePrimCache*> Children;
#ifdef TIME_BASED_CACHING
  //Could also contain a mapping from child to an array of (parentTime,childTime) 
  //This would allow single timesteps to be removed in case of unused/replaced references at a parentTime (instead of removal of child if visible), without breaking garbage collection.
  //Additionally, a refcount per timestep would help to perform garbage collection of individual child clip stages/files, 
  //when no parent references that particular childTime anymore (due to removal/replacing with refs to other children, or replacement with different childTimes for the same child).
#endif
};

struct UsdBridgePrimCache : public UsdBridgeRefCache
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
  UsdStagePair ManifestStage; // Holds the manifest
  std::unordered_map<double, UsdStagePair> ClipStages; // Holds the stage(s) to the timevarying data

  uint32_t LastTimeVaryingBits = 0; // Used to detect changes in timevarying status of parameters

  static constexpr double PrimStageTimeCode = 0.0; // Prim stages are stored in ClipStages under specified time code
  const UsdStagePair& GetPrimStagePair() const
  { 
    auto it = ClipStages.find(PrimStageTimeCode);
    assert(it != ClipStages.end()); 
    return it->second; 
  }

  template<typename DataMemberType>
  bool TimeVarBitsUpdate(DataMemberType newTimeVarBits)
  {
    uint32_t newBits = static_cast<uint32_t>(newTimeVarBits);
    bool hasChanged = (LastTimeVaryingBits != newBits);
    LastTimeVaryingBits = newBits;
    return hasChanged;
  }
#endif

#ifndef NDEBUG
  std::string Debug_Name;
#endif
};

class UsdBridgePrimCacheManager
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

  void AddChild(UsdBridgePrimCache* parent, UsdBridgePrimCache* child);
  void RemoveChild(UsdBridgePrimCache* parent, UsdBridgePrimCache* child);
  void RemoveUnreferencedChildTree(UsdBridgePrimCache* parent, AtRemoveFunc atRemove);
  void RemoveUnreferencedPrimCaches(AtRemoveFunc atRemove);

protected:

  PrimCacheContainer UsdPrimCaches;
};

#endif