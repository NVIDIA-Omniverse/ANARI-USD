// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#ifndef UsdMpiController_h
#define UsdMpiController_h

#include "UsdBridge/Common/UsdBridgeParallelController.h"

#include <memory>

// Concrete parallel controller using MPI.
// MPI types are hidden behind PIMPL so this header does not include <mpi.h>.
class UsdMpiController : public UsdBridgeParallelController
{
public:
  // mpiCommPtr: pointer to an MPI_Comm (ie. the value passed through ANARI_VOID_POINTER)
  explicit UsdMpiController(const void* mpiCommPtr);
  ~UsdMpiController() override;

  // Returns a controller using MPI_COMM_WORLD if MPI is initialized, nullptr otherwise.
  static std::unique_ptr<UsdMpiController> CreateDefault();

  int GetRank() const override;
  int GetSize() const override;
  void BroadcastInt(int& value) override;
  void Barrier() override;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

#endif
