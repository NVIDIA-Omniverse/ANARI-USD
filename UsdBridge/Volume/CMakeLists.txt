message( "-------------------- Begin UsdBridgeVolume CmakeLists.txt ----------------------------------------")

option(USD_DEVICE_USE_OPENVDB "Enable OpenVDB support for USD device" OFF)

project(UsdBridge_Volume)

add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME}
INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeVolumeWriter.cpp
  ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeVolumeWriter.h
)

if (USD_DEVICE_USE_OPENVDB)
  # NOTE(jda) - OpenVDB should export a config, not module
  find_path(OPENVDB_MODULE_DIR
  NAMES
    FindOpenVDB.cmake
  PATH_SUFFIXES
    OpenVDB
    cmake/OpenVDB
    lib/cmake/OpenVDB
  )
  list(APPEND CMAKE_MODULE_PATH ${OPENVDB_MODULE_DIR})
  mark_as_advanced(OPENVDB_MODULE_DIR)

  find_package(OpenVDB REQUIRED)
  target_link_libraries(${PROJECT_NAME} INTERFACE OpenVDB::openvdb)
  target_compile_definitions(${PROJECT_NAME}
  INTERFACE
    USE_OPENVDB
    OPENVDB_STATICLIB
  )
endif()

message( "-------------------- End UsdBridgeVolume CmakeLists.txt ----------------------------------------")