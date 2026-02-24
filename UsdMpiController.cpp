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

std::unique_ptr<UsdMpiController> UsdMpiController::CreateDefault()
{
  int initialized = 0;
  MPI_Initialized(&initialized);
  if(!initialized)
    return nullptr;

  int size = 0;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if(size <= 1)
    return nullptr;

  MPI_Comm world = MPI_COMM_WORLD;
  return std::make_unique<UsdMpiController>(&world);
}

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
