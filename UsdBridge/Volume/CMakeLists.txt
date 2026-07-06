message( "-------------------- Begin UsdBridgeVolume CmakeLists.txt ----------------------------------------")

# Boost's iostreams cmake config file tacks on generic z and lzma dep names
macro(fix_boost_dependency)
  if(NOT WIN32 AND BOOST_USE_STATIC_LIBS)
    find_package(Boost REQUIRED COMPONENTS iostreams)
    get_property(Boost_linkiface
      TARGET Boost::iostreams
      PROPERTY INTERFACE_LINK_LIBRARIES)

    foreach(libname ${Boost_linkiface})
      if(CMAKE_CONFIGURATION_TYPES)
        if(NOT (${libname} MATCHES "$<$<CONFIG:release>:lzma;z>"))
          list(APPEND Boost_linkiface_new ${libname})
        else()
          message(STATUS "Removed boost dependency: ${libname}")
        endif()
      else()
        if((NOT (${libname} MATCHES "lzma")) AND
          (NOT (${libname} MATCHES "z")))
          list(APPEND Boost_linkiface_new ${libname})
        else()
          message(STATUS "Removed boost dependency: ${libname}")
        endif()
      endif()
    endforeach(libname)

    message(STATUS "CHANGED Boost iostreams link interface to: ${Boost_linkiface_new}")
    set_property(TARGET Boost::iostreams
      PROPERTY INTERFACE_LINK_LIBRARIES ${Boost_linkiface_new})
  endif()
endmacro()

project(UsdBridge_Volume)
add_library(${PROJECT_NAME} SHARED)

if(NOT CMAKE_BUILD_TYPE) # Otherwise IlmBase::Half does not find debug/release libraries
  message(FATAL_ERROR "A CMAKE_BUILD_TYPE has to be chosen for correct dependency resolution of UsdBridge_Volume")
endif()

option(USD_DEVICE_USE_OPENVDB "Enable OpenVDB support for USD device" OFF)

target_sources(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeVolumeWriter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeVolumeWriter.h
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE  
    UsdBridge_Common
)

if (USD_DEVICE_USE_OPENVDB)

  file(GLOB USD_OPENVDB_BUILD "${USD_ROOT_DIR_RELEASE}/include/openvdb")
  option(USE_USD_OPENVDB_BUILD "If USD source installation exists, use OpenVDB from there" ON)

  # Find path with Find*.cmake files that find_package() will use for deps

  unset(OPENVDB_MODULE_DIR CACHE)
  find_path(OPENVDB_MODULE_DIR
    PATHS
      ${OpenVDB_ROOT}
    NAMES
      FindOpenVDB.cmake
    PATH_SUFFIXES
      OpenVDB
      cmake/OpenVDB
      lib/cmake/OpenVDB
  )
  # Also expose the parent UsdBridge/cmake dir so FindOpenVDB's internal
  # find_package(TBB) can locate the local FindTBB.cmake. A dynamically-built
  # OpenVDB package may ship no CMake modules of its own, so without this the
  # lookup falls back to CONFIG mode and fails when no TBBConfig.cmake is provided.
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake" "${CMAKE_CURRENT_LIST_DIR}/../cmake" ${OPENVDB_MODULE_DIR})

  # Set dep package roots (or manual lib dirs) and find variables

  # No consensus on upper/lower case

  if(Boost_USE_STATIC_LIBS OR BOOST_USE_STATIC_LIBS)
    set(BOOST_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_LIBS ON)
    set(BUILD_SHARED_LIBS OFF)
  else()
    set(BOOST_USE_STATIC_LIBS OFF)
    set(Boost_USE_STATIC_LIBS OFF)
    set(BUILD_SHARED_LIBS ON)
  endif()
  if(USDBRIDGE_VOLUME_STATIC_CRT)
    set(Boost_USE_STATIC_RUNTIME ON)
  else()
    set(Boost_USE_STATIC_RUNTIME OFF)
  endif()
  if (WIN32)
    set(Boost_COMPILER "vc141")
  endif()

  if(USE_USD_OPENVDB_BUILD) # Allow for using the usd install as dependency root
    if(NOT ZLIB_ROOT)
      set(ZLIB_ROOT "${OpenVDB_ROOT}")
    endif()
    if(NOT TBB_ROOT)
      set(TBB_ROOT "${OpenVDB_ROOT}")
    endif()
    if(NOT BOOST_ROOT)
      set(BOOST_ROOT "${OpenVDB_ROOT}")
    endif()
    if(NOT Blosc_ROOT)
      set(Blosc_ROOT "${OpenVDB_ROOT}")
    endif()
  endif()

  message(STATUS "OpenVDB OpenVDB_ROOT: ${OpenVDB_ROOT}")													 
  message(STATUS "OpenVDB IlmBase_ROOT: ${IlmBase_ROOT}")
  message(STATUS "OpenVDB ZLIB_ROOT: ${ZLIB_ROOT}")
  message(STATUS "OpenVDB Blosc_ROOT: ${Blosc_ROOT}")
  message(STATUS "OpenVDB TBB_ROOT: ${TBB_ROOT}")
  message(STATUS "OpenVDB BOOST_ROOT: ${BOOST_ROOT}")

  fix_boost_dependency()
  
  find_package(OpenVDB REQUIRED)

  # Pop three paths to remove current dir, parent cmake dir and OPENVDB_MODULE_DIR
  list(POP_FRONT CMAKE_MODULE_PATH)
  list(POP_FRONT CMAKE_MODULE_PATH)
  list(POP_FRONT CMAKE_MODULE_PATH)

  # Add include dirs and link libs

  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      OpenVDB::openvdb
  )
  target_compile_definitions(${PROJECT_NAME}
    PRIVATE
      USE_OPENVDB
      ${USD_DEVICE_TIME_DEFS}
  )

  if(WIN32)
    # Due to explicit pragma lib inclusion in tbb and python
    target_link_directories(${PROJECT_NAME}
      PRIVATE ${Tbb_LIBRARY_DIRS})

    # Windows-specific OpenVDB settings
    option(USDBRIDGE_VOLUME_STATIC_CRT "Enable static CRT on Volume part of UsdBridge" OFF)
    if(USDBRIDGE_VOLUME_STATIC_CRT)
      set_property(TARGET ${PROJECT_NAME}
        PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/UsdBridgeVolumeWriter.cpp PROPERTIES COMPILE_FLAGS /bigobj)
  else()
    if(NOT ${USD_DEVICE_USE_CXX11_ABI})
      target_compile_definitions(${PROJECT_NAME} PRIVATE
        _GLIBCXX_USE_CXX11_ABI=0 )
    endif()
  endif()

endif()

install(TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

## Install optional pdbs
## install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)

# Most dynamic dependencies are shared with USD and therefore installed in the parent-level cmake.
# OpenVDB is the exception: when linked dynamically its runtime library must be redistributed next
# to the device (any compression deps such as blosc/zlib may be embedded in it, while tbb is shared
# with USD). When linked statically it is compiled into this component, so nothing extra is installed.
if(USD_DEVICE_USE_OPENVDB AND NOT OPENVDB_USE_STATIC_LIBS AND INSTALL_USD_DEPS)
  if(WIN32)
    install(DIRECTORY "${OpenVDB_ROOT}/bin/"
      DESTINATION ${CMAKE_INSTALL_BINDIR}
      FILES_MATCHING PATTERN "openvdb*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  else()
    install(DIRECTORY "${OpenVDB_ROOT}/lib/"
      DESTINATION ${CMAKE_INSTALL_LIBDIR}
      FILES_MATCHING PATTERN "libopenvdb*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
  endif()
endif()

message( "-------------------- End UsdBridgeVolume CmakeLists.txt ----------------------------------------")
