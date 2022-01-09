## Copyright 2021 NVIDIA Corporation
## SPDX-License-Identifier: Apache-2.0

macro(append_cmake_prefix_path)
  list(APPEND CMAKE_PREFIX_PATH ${ARGN})
  string(REPLACE ";" "|" CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}")
  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)
endmacro()

macro(setup_subproject_path_vars _NAME)
  set(SUBPROJECT_NAME ${_NAME})

  set(SUBPROJECT_INSTALL_PATH ${INSTALL_DIR_ABSOLUTE})

  set(SUBPROJECT_SOURCE_PATH ${SUBPROJECT_NAME}/source)
  set(SUBPROJECT_STAMP_PATH ${SUBPROJECT_NAME}/stamp)
  set(SUBPROJECT_BUILD_PATH ${SUBPROJECT_NAME}/build)
endmacro()

function(build_subproject)
  # See cmake_parse_arguments docs to see how args get parsed here:
  #    https://cmake.org/cmake/help/latest/command/cmake_parse_arguments.html
  set(oneValueArgs NAME URL SOURCE_DIR)
  set(multiValueArgs BUILD_ARGS DEPENDS_ON)
  cmake_parse_arguments(BUILD_SUBPROJECT "" "${oneValueArgs}"
                        "${multiValueArgs}" ${ARGN})

  if (BUILD_SUBPROJECT_URL AND BUILD_SUBPROJECT_SOURCE_DIR)
    message(FATAL_ERROR "URL and SOURCE_DIR are mutually exclusive for build_subproject()")
  endif()

  # Setup SUBPROJECT_* variables (containing paths) for this function
  setup_subproject_path_vars(${BUILD_SUBPROJECT_NAME})

  if (BUILD_SUBPROJECT_URL)
    set(SOURCE ${SUBPROJECT_SOURCE_PATH})
    set(URL URL ${BUILD_SUBPROJECT_URL})
  else()
    set(SOURCE ${BUILD_SUBPROJECT_SOURCE_DIR})
    unset(URL)
  endif()

  # Build the actual subproject
  ExternalProject_Add(${SUBPROJECT_NAME}
    PREFIX ${SUBPROJECT_NAME}
    DOWNLOAD_DIR ${SUBPROJECT_NAME}
    STAMP_DIR ${SUBPROJECT_STAMP_PATH}
    SOURCE_DIR ${SOURCE}
    BINARY_DIR ${SUBPROJECT_BUILD_PATH}
    ${URL}
    LIST_SEPARATOR | # Use the alternate list separator
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=${CMAKE_CONFIGURATION_TYPES}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      -DCMAKE_INSTALL_PREFIX:PATH=${SUBPROJECT_INSTALL_PATH}
      -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
      ${BUILD_SUBPROJECT_BUILD_ARGS}
    BUILD_COMMAND ${DEFAULT_BUILD_COMMAND}
    BUILD_ALWAYS OFF
  )

  if(BUILD_SUBPROJECT_DEPENDS_ON)
    ExternalProject_Add_StepDependencies(${SUBPROJECT_NAME}
      configure ${BUILD_SUBPROJECT_DEPENDS_ON}
    )
  endif()

  # Place installed component on CMAKE_PREFIX_PATH for downstream consumption
  append_cmake_prefix_path(${SUBPROJECT_INSTALL_PATH})
endfunction()