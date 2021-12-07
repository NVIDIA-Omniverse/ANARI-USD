## Copyright 2020 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

add_subdirectory(UsdBridge)

set(CMAKE_CXX_STANDARD 17)

if(EXISTS ${CMAKE_SOURCE_DIR}/.git AND IS_DIRECTORY ${CMAKE_SOURCE_DIR}/.git)
  find_package(Git REQUIRED)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE "USD_DEVICE_VERSION"
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
else()
  set(USD_DEVICE_VERSION -1)
endif()

set(USDModule_SOURCES
  UsdAnari.cpp
  UsdDevice.cpp
  UsdDataArray.cpp
  UsdGeometry.cpp
  UsdSurface.cpp
  UsdGroup.cpp
  UsdSpatialField.cpp
  UsdVolume.cpp
  UsdInstance.cpp
  UsdSampler.cpp
  UsdMaterial.cpp
  UsdWorld.cpp
  UsdRenderer.cpp
  UsdFrame.cpp)

set(USDModule_HEADERS
  UsdCommonMacros.h
  UsdAnari.h
  UsdParameterizedObject.h
  UsdDevice.h
  UsdBaseObject.h
  UsdDataArray.h
  UsdGeometry.h
  UsdSurface.h
  UsdGroup.h
  UsdSpatialField.h
  UsdVolume.h
  UsdInstance.h
  UsdSampler.h
  UsdMaterial.h
  UsdWorld.h
  UsdRenderer.h
  UsdFrame.h)

set(USDModule_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/libs/anari_utilities/include)

add_library(anari_library_usd SHARED ${USDModule_SOURCES} ${USDModule_HEADERS})

target_compile_definitions(anari_library_usd
	PRIVATE -DDEVICE_VERSION=${USD_DEVICE_VERSION})

target_include_directories(anari_library_usd PUBLIC ${USDModule_INCLUDE_DIRECTORIES})
target_link_libraries(anari_library_usd
	PUBLIC anari
	PRIVATE anari_utilities UsdBridge)

add_subdirectory(examples)

install(TARGETS anari_library_usd
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

