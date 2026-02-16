// Copyright 2020 The Khronos Group
// SPDX-License-Identifier: Apache-2.0

#include "UsdMpiController.h"

#include <mpi.h>

struct UsdMpiController::Impl
{
  MPI_Comm comm;
  int rank = -1;
  int size = -1;
};

UsdMpiController::UsdMpiController(const void* mpiCommPtr)
  : m_impl(std::make_unique<Impl>())
{
  m_impl->comm = *(const MPI_Comm*)mpiCommPtr;
  MPI_Comm_rank(m_impl->comm, &m_impl->rank);
  MPI_Comm_size(m_impl->comm, &m_impl->size);
}

UsdMpiController::~UsdMpiController() = default;

int UsdMpiController::GetRank() const
{
  return m_impl->rank;
}

int UsdMpiController::GetSize() const
{
  return m_impl->size;
}

void UsdMpiController::BroadcastInt(int& value)
{
  MPI_Bcast(&value, 1, MPI_INT, 0, m_impl->comm);
}

void UsdMpiController::Barrier()
{
  MPI_Barrier(m_impl->comm);
}
