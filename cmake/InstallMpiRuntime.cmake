## Copyright 2020 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

# Installs the MPI runtime next to the USD device so it can be loaded on machines
# without a system MPI. Windows: the Microsoft MPI runtime DLL(s) + mpiexec.
# Linux: the OpenMPI shared libs plus their transitive deps (hwloc/libevent/pmix),
# which are not present on the GPU-less test runners.
function(usd_device_install_mpi_runtime)
  if(WIN32)
    # Packman MS-MPI keeps msmpi.dll under Lib/, not Bin/ (Bin has mpiexec.exe).
    set(_MSMPI_LIB_DIR "")
    if(DEFINED ENV{MSMPI_LIB_DIR} AND NOT "$ENV{MSMPI_LIB_DIR}" STREQUAL "")
      set(_MSMPI_LIB_DIR "$ENV{MSMPI_LIB_DIR}")
    elseif(DEFINED ENV{MSMPI_LIB64} AND NOT "$ENV{MSMPI_LIB64}" STREQUAL "")
      get_filename_component(_MSMPI_LIB_DIR "$ENV{MSMPI_LIB64}" DIRECTORY)
    endif()
    set(_MSMPI_BIN "")
    if(DEFINED ENV{MSMPI_BIN} AND NOT "$ENV{MSMPI_BIN}" STREQUAL "")
      set(_MSMPI_BIN "$ENV{MSMPI_BIN}")
    elseif(_MSMPI_LIB_DIR)
      get_filename_component(_MSMPI_ROOT "${_MSMPI_LIB_DIR}" DIRECTORY)
      set(_MSMPI_BIN "${_MSMPI_ROOT}/Bin")
    endif()
    if(_MSMPI_LIB_DIR)
      message(STATUS "MPI runtime install lib: ${_MSMPI_LIB_DIR}")
      file(GLOB _MSMPI_DLLS "${_MSMPI_LIB_DIR}/msmpi*.dll")
      if(_MSMPI_DLLS)
        install(FILES ${_MSMPI_DLLS} DESTINATION ${CMAKE_INSTALL_BINDIR})
      else()
        message(WARNING "No MS-MPI runtime DLLs found under ${_MSMPI_LIB_DIR}")
      endif()
    else()
      message(WARNING "Microsoft MPI Lib directory not found; MPI runtime will not be installed")
    endif()
    if(_MSMPI_BIN AND EXISTS "${_MSMPI_BIN}/mpiexec.exe")
      install(FILES "${_MSMPI_BIN}/mpiexec.exe" DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
  else()
    # Prefer OPENMPI_LIB_DIR from the environment (set by build.py for system MPI);
    # deriving the path from MPI::MPI_CXX IMPORTED_LOCATION alone is unreliable.
    set(_OPENMPI_LIB_DIR "")
    if(DEFINED ENV{OPENMPI_LIB_DIR} AND NOT "$ENV{OPENMPI_LIB_DIR}" STREQUAL "")
      set(_OPENMPI_LIB_DIR "$ENV{OPENMPI_LIB_DIR}")
    else()
      foreach(_MPI_IMPORTED_PROP
          IMPORTED_LOCATION IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_DEBUG
          IMPORTED_IMPLIB IMPORTED_IMPLIB_RELEASE IMPORTED_IMPLIB_DEBUG)
        if(NOT _OPENMPI_LIB_DIR)
          get_target_property(_MPI_IMPORTED_LOCATION MPI::MPI_CXX ${_MPI_IMPORTED_PROP})
          if(_MPI_IMPORTED_LOCATION AND NOT _MPI_IMPORTED_LOCATION STREQUAL "${_MPI_IMPORTED_LOCATION}-NOTFOUND")
            get_filename_component(_OPENMPI_LIB_DIR "${_MPI_IMPORTED_LOCATION}" DIRECTORY)
          endif()
        endif()
      endforeach()
    endif()
    if(_OPENMPI_LIB_DIR)
      message(STATUS "MPI runtime install lib: ${_OPENMPI_LIB_DIR}")
      # pmix comes from the multiarch dir below, not here, to avoid globbing its
      # symlink chain twice.
      file(GLOB MPI_SHARED_LIBS
        "${_OPENMPI_LIB_DIR}/libmpi*.so*"
        "${_OPENMPI_LIB_DIR}/libopen-rte*.so*"
        "${_OPENMPI_LIB_DIR}/libopen-pal*.so*")
      # hwloc/libevent/pmix are transitive deps in the multiarch dir, not the
      # openmpi/ subdir, and aren't shipped on the GPU-less test runners.
      get_filename_component(_MPI_MULTIARCH_DIR "${_OPENMPI_LIB_DIR}/../.." ABSOLUTE)
      if(IS_DIRECTORY "${_MPI_MULTIARCH_DIR}")
        file(GLOB MPI_TRANSITIVE_LIBS
          "${_MPI_MULTIARCH_DIR}/libhwloc.so*"
          "${_MPI_MULTIARCH_DIR}/libevent*.so*"
          "${_MPI_MULTIARCH_DIR}/libpmix.so*")
        list(APPEND MPI_SHARED_LIBS ${MPI_TRANSITIVE_LIBS})
      endif()
      if(MPI_SHARED_LIBS)
        list(REMOVE_DUPLICATES MPI_SHARED_LIBS)
      endif()
      if(MPI_SHARED_LIBS)
        # Install by walking each symlink chain across directories, copying the real
        # lib and recreating the .so/.so.N symlinks. Mirrors FOLLOW_SYMLINK_CHAIN
        # but avoids its pmix ELOOP bug.
        install(CODE "
          set(_mpi_dest \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}\")
          file(MAKE_DIRECTORY \"\${_mpi_dest}\")
          foreach(_mpi_lib ${MPI_SHARED_LIBS})
            set(_mpi_cur \"\${_mpi_lib}\")
            set(_mpi_hops 0)
            while(_mpi_hops LESS 16)
              math(EXPR _mpi_hops \"\${_mpi_hops} + 1\")
              get_filename_component(_mpi_name \"\${_mpi_cur}\" NAME)
              get_filename_component(_mpi_dir \"\${_mpi_cur}\" DIRECTORY)
              if(IS_SYMLINK \"\${_mpi_cur}\")
                file(READ_SYMLINK \"\${_mpi_cur}\" _mpi_tgt)
                get_filename_component(_mpi_tgtname \"\${_mpi_tgt}\" NAME)
                if(NOT _mpi_tgtname STREQUAL _mpi_name)
                  file(REMOVE \"\${_mpi_dest}/\${_mpi_name}\")
                  execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink
                    \"\${_mpi_tgtname}\" \"\${_mpi_dest}/\${_mpi_name}\")
                endif()
                if(IS_ABSOLUTE \"\${_mpi_tgt}\")
                  set(_mpi_cur \"\${_mpi_tgt}\")
                else()
                  get_filename_component(_mpi_cur \"\${_mpi_dir}/\${_mpi_tgt}\" ABSOLUTE)
                endif()
              else()
                file(INSTALL DESTINATION \"\${_mpi_dest}\" TYPE SHARED_LIBRARY FILES \"\${_mpi_cur}\")
                break()
              endif()
            endwhile()
          endforeach()
        ")
      else()
        message(WARNING "No OpenMPI shared libraries found under ${_OPENMPI_LIB_DIR}")
      endif()
      get_filename_component(MPI_ROOT_DIR "${_OPENMPI_LIB_DIR}" DIRECTORY)
      if(EXISTS "${MPI_ROOT_DIR}/bin/mpirun")
        install(PROGRAMS "${MPI_ROOT_DIR}/bin/mpirun" DESTINATION ${CMAKE_INSTALL_BINDIR})
      endif()
    else()
      message(WARNING "OpenMPI lib directory not found; MPI runtime will not be installed")
    endif()
  endif()
endfunction()
