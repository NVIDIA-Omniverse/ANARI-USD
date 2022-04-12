# FindBlosc.cmake is broken on windows for static dependencies
macro(fix_blosc_dependency)
  if(WIN32 AND BLOSC_USE_STATIC_LIBS)
    get_property(BLOSC_imploc
      TARGET Blosc::blosc
      PROPERTY IMPORTED_LOCATION)

    get_filename_component(BLOSC_imploc_dir ${BLOSC_imploc} DIRECTORY)
    get_filename_component(BLOSC_imploc_name ${BLOSC_imploc} NAME)
    set(BLOSC_imploc "${BLOSC_imploc_dir}/lib${BLOSC_imploc_name}")
    message(STATUS "CHANGED BLOSC LIBRARY TO: ${BLOSC_imploc}")

    set_property(TARGET Blosc::blosc
      PROPERTY IMPORTED_LOCATION ${BLOSC_imploc})
  endif()
endmacro()

message( "-------------------- Begin UsdBridgeVolume CmakeLists.txt ----------------------------------------")

option(USD_DEVICE_USE_OPENVDB "Enable OpenVDB support for USD device" OFF)

project(UsdBridge_Volume)

add_library(${PROJECT_NAME} SHARED)
target_sources(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../Common/UsdBridgeUtils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeVolumeWriter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeVolumeWriter.h
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}
      "${CMAKE_CURRENT_LIST_DIR}/../Common"
)

if (USD_DEVICE_USE_OPENVDB)

  file(GLOB USD_OPENVDB_BUILD "${USD_ROOT_DIR_RELEASE}/include/openvdb")
  option(USE_USD_OPENVDB_BUILD "If USD source installation exists, use OpenVDB from there" ON)

  # Support custom install dir OPENVDB_ROOT_DIR

  if(USE_USD_OPENVDB_BUILD AND EXISTS ${USD_OPENVDB_BUILD})
    set(OPENVDB_ROOT_DIR ${USD_ROOT_DIR} CACHE PATH "" FORCE)
  endif()

  if (NOT EXISTS ${OPENVDB_ROOT_DIR})
    find_path(OPENVDB_ROOT_DIR NAMES include/openvdb/openvdb.h DOC "Path of OpenVDB install with static library")
    message(STATUS "Using OPENVDB_ROOT_DIR: ${OPENVDB_ROOT_DIR}")
  endif()

  if (NOT EXISTS ${OPENVDB_ROOT_DIR})
    message(FATAL_ERROR "No valid OPENVDB_ROOT_DIR set, or found using CMAKE_PREFIX_PATH: ${OPENVDB_ROOT_DIR}")
  endif()

  MESSAGE(STATUS "OpenVDB enabled. In case superbuild disabled BUILD_USD, it's taken from OPENVDB_ROOT_DIR: ${OPENVDB_ROOT_DIR}")

  if(EXISTS "${OPENVDB_ROOT_DIR}/release")
    set(OPENVDB_ROOT_DIR_RELEASE "${OPENVDB_ROOT_DIR}/release")
  else()
    set(OPENVDB_ROOT_DIR_RELEASE "${OPENVDB_ROOT_DIR}")
  endif()
  if(EXISTS "${OPENVDB_ROOT_DIR}/debug")
    set(OPENVDB_ROOT_DIR_DEBUG "${OPENVDB_ROOT_DIR}/debug")
  else()
    set(OPENVDB_ROOT_DIR_DEBUG "${OPENVDB_ROOT_DIR}")
  endif()

  # Find path with Find*.cmake files that find_package() will use for deps

  unset(OPENVDB_MODULE_DIR CACHE)
  find_path(OPENVDB_MODULE_DIR
    PATHS
      ${OPENVDB_ROOT_DIR_RELEASE}
    NAMES
      FindOpenVDB.cmake
    PATH_SUFFIXES
      OpenVDB
      cmake/OpenVDB
      lib/cmake/OpenVDB
  )
  list(PREPEND CMAKE_MODULE_PATH ${OPENVDB_MODULE_DIR})

  ### Find openVDB package (+ dependencies)

  set(OPENVDB_LIB_DIR_RELEASE "${OPENVDB_ROOT_DIR_RELEASE}/lib")
  set(OPENVDB_LIB_DIR_DEBUG "${OPENVDB_ROOT_DIR_DEBUG}/lib")

  if(Boost_USE_STATIC_LIBS OR BOOST_USE_STATIC_LIBS) # No consensus on upper/lower case
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

  if(USE_USD_OPENVDB_BUILD) # Allow for using the usd install as dependency root
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
      set(OPENVDB_DEP_ROOT "${OPENVDB_ROOT_DIR_DEBUG}")
    else()
      set(OPENVDB_DEP_ROOT "${OPENVDB_ROOT_DIR_RELEASE}") # Gets chosen in multiconfig situations => release deps regardless of config
    endif()

    if(NOT ZLIB_ROOT)
      set(ZLIB_ROOT "${OPENVDB_DEP_ROOT}")
    endif()
    if(NOT TBB_ROOT)
      set(TBB_ROOT "${OPENVDB_DEP_ROOT}")
    endif()
    if(NOT BOOST_ROOT)
      set(BOOST_ROOT "${OPENVDB_DEP_ROOT}")
    endif()
    if(NOT IlmBase_ROOT)
      if(ILMBASE_ROOT)
        set(IlmBase_ROOT ${ILMBASE_ROOT})
      else()
        set(IlmBase_ROOT "${OPENVDB_DEP_ROOT}")
      endif()
    endif()
    if(NOT ILMBASE_ROOT)
      set(ILMBASE_ROOT ${IlmBase_ROOT})
    endif()
    if(NOT Blosc_ROOT)
      if(BLOSC_ROOT)
        set(Blosc_ROOT ${BLOSC_ROOT})
      else()
        set(Blosc_ROOT "${OPENVDB_DEP_ROOT}")
      endif()
    endif()
    if(NOT BLOSC_ROOT)
      set(BLOSC_ROOT ${Blosc_ROOT})
    endif()
    #if(NOT OpenVDB_ROOT)
    #  if(OPENVDB_ROOT)
    #    set(OpenVDB_ROOT OPENVDB_ROOT)
    #  else()
    #    set(OpenVDB_ROOT "${OPENVDB_DEP_ROOT}")
    #  endif()
    #endif()
    #if(NOT OPENVDB_ROOT)
    #  set(OPENVDB_ROOT ${OpenVDB_ROOT})
    #endif()
  endif()

  message(STATUS "OpenVDB IlmBase_ROOT: ${IlmBase_ROOT}")
  message(STATUS "OpenVDB ZLIB_ROOT: ${ZLIB_ROOT}")
  message(STATUS "OpenVDB Blosc_ROOT: ${Blosc_ROOT}")
  message(STATUS "OpenVDB TBB_ROOT: ${TBB_ROOT}")
  message(STATUS "OpenVDB BOOST_ROOT: ${BOOST_ROOT}")

  # Dynamic library fails to link fully on Windows, but FindOpenVDB.cmake can't properly select the static library. Do it manually instead.
  if (WIN32)
    set(OPENVDB_LIB_FILE_NAME "libopenvdb.lib")
  else()
    set(OPENVDB_LIB_FILE_NAME "libopenvdb.a")
  endif()
  find_library(OPENVDB_LIBRARY_RELEASE ${OPENVDB_LIB_FILE_NAME} PATHS ${OPENVDB_LIB_DIR_RELEASE} NO_DEFAULT_PATH REQUIRED)
  find_library(OPENVDB_LIBRARY_DEBUG ${OPENVDB_LIB_FILE_NAME} PATHS ${OPENVDB_LIB_DIR_DEBUG} NO_DEFAULT_PATH REQUIRED)

  # Manually fetch dependency targets that FindOpenVDB.cmake would have fetched
  find_package(IlmBase REQUIRED COMPONENTS Half)
  find_package(TBB REQUIRED COMPONENTS tbb)
  find_package(ZLIB REQUIRED)
  find_package(Boost REQUIRED COMPONENTS iostreams system)
  find_package(Blosc REQUIRED)
  #find_package(OpenVDB REQUIRED)

  fix_blosc_dependency()

  set(OPENVDB_DEPENDENCY_TARGETS
    Boost::iostreams
    Boost::system
    IlmBase::Half
    TBB::tbb
    ZLIB::ZLIB
    Blosc::blosc)

  list(POP_FRONT CMAKE_MODULE_PATH)

  # Set debug and release variables

  set(OPENVDB_INCLUDE_DIRS_RELEASE "${OPENVDB_ROOT_DIR_RELEASE}/include")
  set(OPENVDB_LIB_FILES_RELEASE ${OPENVDB_LIBRARY_RELEASE})

  set(OPENVDB_INCLUDE_DIRS_DEBUG "${OPENVDB_ROOT_DIR_DEBUG}/include")
  set(OPENVDB_LIB_FILES_DEBUG ${OPENVDB_LIBRARY_DEBUG})

  set(OPENVDB_INCLUDE_DIRS "$<$<NOT:$<CONFIG:Debug>>:${OPENVDB_INCLUDE_DIRS_RELEASE}>$<$<CONFIG:Debug>:${OPENVDB_INCLUDE_DIRS_DEBUG}>")
  set(OPENVDB_LIB_FILES "$<$<NOT:$<CONFIG:Debug>>:${OPENVDB_LIB_FILES_RELEASE}>$<$<CONFIG:Debug>:${OPENVDB_LIB_FILES_DEBUG}>")

  message(STATUS "Using OpenVDB include dirs: ${OPENVDB_INCLUDE_DIRS}")
  message(STATUS "Using OpenVDB libs: ${OPENVDB_LIB_FILES}")

  # Add include dirs and link libs

  target_include_directories(${PROJECT_NAME}
    PRIVATE
      ${OPENVDB_INCLUDE_DIRS}
  )
  target_link_libraries(${PROJECT_NAME}
    PRIVATE
      ${OPENVDB_LIB_FILES}
      ${OPENVDB_DEPENDENCY_TARGETS}
  )
  target_compile_definitions(${PROJECT_NAME}
    PRIVATE
      USE_OPENVDB
      OPENVDB_STATICLIB
      ${USD_DEVICE_TIME_DEFS}
  )

  if(ILMBASE_USE_STATIC_LIBS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE OPENVDB_OPENEXR_STATICLIB)
  endif()

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
  endif()

  install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

  # No dependency install required; all dynamic dependencies should be shared with USD and therefore installed in the parent-level cmake,
  # and if not, make sure they are static to avoid problems with versioning.

endif()

message( "-------------------- End UsdBridgeVolume CmakeLists.txt ----------------------------------------")
