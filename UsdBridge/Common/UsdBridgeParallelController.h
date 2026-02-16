// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdBridgeParallelController_h
#define UsdBridgeParallelController_h

// Abstract interface for parallel (MPI) synchronization.
// Allows UsdBridge components to perform collective operations
// without depending on MPI directly.
class UsdBridgeParallelController
{
public:
  virtual ~UsdBridgeParallelController() = default;

  // Get the rank of this process (0-based)
  virtual int GetRank() const = 0;

  // Get the total number of processes
  virtual int GetSize() const = 0;

  // Broadcast an integer value from rank 0 to all ranks
  virtual void BroadcastInt(int& value) = 0;

  // Barrier - block until all ranks have reached this point
  virtual void Barrier() = 0;
};

#endif
