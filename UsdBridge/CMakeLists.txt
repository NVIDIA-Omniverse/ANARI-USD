message( "-------------------- Begin UsdBridge CmakeLists.txt ----------------------------------------")

if (WIN32)
cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT USD_INSTALL_DIR)
  find_path(USD_INSTALL_DIR NAMES include/pxr/pxr.h)
  message(STATUS "Using USD_INSTALL_DIR: ${USD_INSTALL_DIR}")
endif()

# USD #

message(STATUS "Using cmake value of USD_INSTALL_DIR: ${USD_INSTALL_DIR}")
if(EXISTS "${USD_INSTALL_DIR}/release")
  set(USD_INSTALL_DIR_RELEASE "${USD_INSTALL_DIR}/release")
else()
  set(USD_INSTALL_DIR_RELEASE "${USD_INSTALL_DIR}")
endif()
if(EXISTS "${USD_INSTALL_DIR}/debug")
  set(USD_INSTALL_DIR_DEBUG "${USD_INSTALL_DIR}/debug")
else()
  set(USD_INSTALL_DIR_DEBUG "${USD_INSTALL_DIR}")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  if(NOT EXISTS "${USD_INSTALL_DIR_DEBUG}/include")
    message(FATAL_ERROR "USD_INSTALL_DIR or its /debug subfolder does not have an /include subfolder, so it's not configured correctly at ${USD_INSTALL_DIR_DEBUG}")
  endif()
elseif(NOT EXISTS "${USD_INSTALL_DIR_RELEASE}/include")
  message(FATAL_ERROR "USD_INSTALL_DIR or its /release subfolder does not have an /include subfolder, so it's not configured correctly at ${USD_INSTALL_DIR_RELEASE}")
endif()

set(USD_DIR "$<$<NOT:$<CONFIG:Debug>>:${USD_INSTALL_DIR_RELEASE}>$<$<CONFIG:Debug>:${USD_INSTALL_DIR_DEBUG}>")
set(USD_INCLUDE_DIR "${USD_DIR}/include")

if (WIN32)
  add_definitions("-DBOOST_ALL_DYN_LINK")

  set(USD_LIB_DIR "$<$<NOT:$<CONFIG:Debug>>:${USD_INSTALL_DIR_RELEASE}/lib>$<$<CONFIG:Debug>:${USD_INSTALL_DIR_DEBUG}/lib>")
else()
  if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(USD_LIB_DIR "${USD_INSTALL_DIR_DEBUG}/lib")
  else()
    set(USD_LIB_DIR "${USD_INSTALL_DIR_RELEASE}/lib")
  endif()
endif()

list(APPEND USD_SHARED_LIBS ar arch gf js kind ndr pcp plug sdf tf trace usd usdGeom usdVol usdShade vt work)
message(STATUS "USD_SHARED_LIBS: ${USD_SHARED_LIBS}")

# Python #

find_package(Python REQUIRED COMPONENTS Interpreter Development)

# Bridge #

add_subdirectory(Mdl)
add_subdirectory(Connection)
add_subdirectory(Volume)

set(USDBRIDGE_SOURCES
  UsdBridge.cpp
  UsdBridgeUtils.cpp
  UsdBridgeCaches.cpp
  UsdBridgeUsdWriter.cpp
  UsdBridge.h
  UsdBridgeCaches.h
  UsdBridgeUsdWriter.h
  UsdBridgeData.h
  UsdBridgeUtils.h
  UsdBridgeMacros.h
  usd.h
  ${USDBRIDGE_MDL_SOURCES}
  ${USDBRIDGE_CONNECT_SOURCES}
  ${USDBRIDGE_VOLUME_SOURCES}
)

add_library(UsdBridge STATIC ${USDBRIDGE_SOURCES})

# Includes/libs #

target_include_directories(UsdBridge PRIVATE
  ${USD_INCLUDE_DIR}
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/Mdl
  ${CMAKE_CURRENT_LIST_DIR}/Connection
  ${CMAKE_CURRENT_LIST_DIR}/Volume
  ${USDBRIDGE_MDL_INCLUDE_DIRS}
  ${USDBRIDGE_CONNECT_INCLUDE_DIRS}
  ${USDBRIDGE_VOLUME_INCLUDE_DIRS}
)

target_compile_definitions(UsdBridge PRIVATE
  ${USDBRIDGE_MDL_COMPILE_DEFS}
  ${USDBRIDGE_CONNECT_COMPILE_DEFS}
  ${USDBRIDGE_VOLUME_COMPILE_DEFS}
)

if (WIN32)
  set_source_files_properties(Volume/UsdBridgeVolumeWriter.cpp PROPERTIES COMPILE_FLAGS /bigobj)
  target_link_directories(UsdBridge PUBLIC
    ${USD_LIB_DIR}
    ${USD_PYTHON_LIB_DIR}
    ${USDBRIDGE_MDL_LINK_DIRS}
    ${USDBRIDGE_CONNECT_LINK_DIRS}
    ${USDBRIDGE_VOLUME_LINK_DIRS}
  )
else()
  set(PLATFORM_LIBS pthread dl stdc++fs)
  target_link_libraries(UsdBridge PRIVATE general ${PLATFORM_LIBS} )

  # Assume compilation against the Nvidia Omniverse USD libraries,
  # which are not built with the CXX11 ABI
  if(${USD_DEVICE_USE_OMNIVERSE})
    target_compile_definitions(UsdBridge PRIVATE
      _GLIBCXX_USE_CXX11_ABI=0 )
  endif()

  #target_link_options(UsdBridge PUBLIC
  #  "-Wl,--rpath-link,${USD_LIB_DIR}"
  #  ${USDBRIDGE_MDL_LINK_OPTIONS}
  #  ${USDBRIDGE_CONNECT_LINK_OPTIONS}
  #  ${USDBRIDGE_VOLUME_LINK_OPTIONS})  # Not required for ANARI

  foreach(lib ${USD_SHARED_LIBS})
    find_library(path ${lib} PATHS ${USD_LIB_DIR})
    list(APPEND USD_LIBS ${path})
    unset(path CACHE)
  endforeach()
endif()

target_link_libraries(UsdBridge PUBLIC
  ${USD_LIBS}
  Python::Module
  ${USDBRIDGE_MDL_LINK_LIBS}
  ${USDBRIDGE_CONNECT_LINK_LIBS}
  ${USDBRIDGE_VOLUME_LINK_LIBS}
)

install(TARGETS UsdBridge
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

message( "-------------------- End UsdBridge CmakeLists.txt ----------------------------------------")


