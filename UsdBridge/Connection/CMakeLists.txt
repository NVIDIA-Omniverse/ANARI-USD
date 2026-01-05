message( "-------------------- Begin UsdBridgeConnect CmakeLists.txt ----------------------------------------")

message(STATUS "----- UsdCon level: ${USD_DEVICE_TIME_DEFS}")

option(USD_DEVICE_USE_OMNIVERSE "Enable Omniverse support for USD device" OFF)

project(UsdBridge_Connect)
add_library(${PROJECT_NAME} INTERFACE)

####################
# Omniverse Client #
####################

function(get_omni_dependency
  DEP_ROOT_DIR
  DEP_INCLUDE_DIR
  DEP_LIB_DIR
  INPUT_ROOT_DIR
  INPUT_ROOT_DIR_NAME
  INPUT_NAME
  INPUT_HEADER)

  if (NOT EXISTS ${INPUT_ROOT_DIR})
    find_path(INPUT_ROOT_DIR NAMES ${INPUT_HEADER} DOC "Path to ${INPUT_NAME}")
    message(STATUS "Using ${INPUT_ROOT_DIR_NAME}: ${INPUT_ROOT_DIR}")
  endif()

  if (NOT EXISTS ${INPUT_ROOT_DIR})
    message(FATAL_ERROR "No valid ${INPUT_ROOT_DIR_NAME} set, or found using CMAKE_PREFIX_PATH: ${INPUT_ROOT_DIR}")
  endif()

  set(${DEP_ROOT_DIR} ${INPUT_ROOT_DIR} PARENT_SCOPE)

  if(EXISTS ${INPUT_ROOT_DIR})
    MESSAGE(STATUS "UsdBridge is using ${INPUT_NAME} at: ${INPUT_ROOT_DIR}")

    set(DEP_DIR_RELEASE "${INPUT_ROOT_DIR}/release")
    set(DEP_DIR_DEBUG "${INPUT_ROOT_DIR}/debug")

    # Check for include directory location: root level vs config-specific
    if(EXISTS "${INPUT_ROOT_DIR}/include")
      # Structure like omni_client_library: include/ at root
      set(${DEP_INCLUDE_DIR} "${INPUT_ROOT_DIR}/include" PARENT_SCOPE)
    else()
      # Structure like omni_usd_resolver: include/ under each config
      if (CMAKE_CONFIGURATION_TYPES)
        set(${DEP_INCLUDE_DIR} "$<$<NOT:$<CONFIG:Debug>>:${DEP_DIR_RELEASE}/include>$<$<CONFIG:Debug>:${DEP_DIR_DEBUG}/include>" PARENT_SCOPE)
      else()
        if(CMAKE_BUILD_TYPE MATCHES "Debug")
          set(${DEP_INCLUDE_DIR} "${DEP_DIR_DEBUG}/include" PARENT_SCOPE)
        else()
          set(${DEP_INCLUDE_DIR} "${DEP_DIR_RELEASE}/include" PARENT_SCOPE)
        endif()
      endif()
    endif()

    # Check for library directory location: config root vs config/lib
    if(EXISTS "${DEP_DIR_RELEASE}/lib")
      # Structure like omni_usd_resolver: libraries under config/lib/
      set(DEP_LIB_DIR_RELEASE "${DEP_DIR_RELEASE}/lib")
      set(DEP_LIB_DIR_DEBUG "${DEP_DIR_DEBUG}/lib")
    else()
      # Structure like omni_client_library: libraries directly under config/
      set(DEP_LIB_DIR_RELEASE "${DEP_DIR_RELEASE}")
      set(DEP_LIB_DIR_DEBUG "${DEP_DIR_DEBUG}")
    endif()

    if (CMAKE_CONFIGURATION_TYPES)
      set(_DEP_LIB_DIR "$<$<NOT:$<CONFIG:Debug>>:${DEP_LIB_DIR_RELEASE}>$<$<CONFIG:Debug>:${DEP_LIB_DIR_DEBUG}>")
    else()
      if(CMAKE_BUILD_TYPE MATCHES "Debug")
        set(_DEP_LIB_DIR ${DEP_LIB_DIR_DEBUG})
      else()
        set(_DEP_LIB_DIR ${DEP_LIB_DIR_RELEASE})
      endif()
    endif()

    set(${DEP_LIB_DIR} ${_DEP_LIB_DIR} PARENT_SCOPE)

    MESSAGE(STATUS "${INPUT_NAME} LIB_DIR: ${_DEP_LIB_DIR}")
  else()
    MESSAGE(FATAL_ERROR "Cannot find ${INPUT_ROOT_DIR_NAME}: ${INPUT_ROOT_DIR}")
  endif()

endfunction()

function (setup_client_library_target omniclient_root_dir)

  # check if a client-library target exists already and if not define it
  if (NOT TARGET omni::client-library)
    add_library(omni::client-library SHARED IMPORTED)
    set_property(TARGET omni::client-library APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG RELEASE RELWITHDEBINFO)
    if(WIN32)
      set_target_properties(omni::client-library PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${omniclient_root_dir}/include"
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${omniclient_root_dir}/include"
        IMPORTED_IMPLIB_DEBUG "${omniclient_root_dir}/debug/omniclient.lib"
        IMPORTED_LOCATION_DEBUG "${omniclient_root_dir}/debug/omniclient.dll"
        IMPORTED_IMPLIB_RELEASE "${omniclient_root_dir}/release/omniclient.lib"
        IMPORTED_LOCATION_RELEASE "${omniclient_root_dir}/release/omniclient.dll"
        IMPORTED_IMPLIB_RELWITHDEBINFO "${omniclient_root_dir}/release/omniclient.lib"
        IMPORTED_LOCATION_RELWITHDEBINFO "${omniclient_root_dir}/release/omniclient.dll")
    else()
      set_target_properties(omni::client-library PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${omniclient_root_dir}/include"
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${omniclient_root_dir}/include"
        IMPORTED_LOCATION_DEBUG "${omniclient_root_dir}/debug/libomniclient.so"
        IMPORTED_LOCATION_RELEASE "${omniclient_root_dir}/release/libomniclient.so"
        IMPORTED_LOCATION_RELWITHDEBINFO "${omniclient_root_dir}/release/libomniclient.so")
    endif()
  endif()

endfunction()

function(remove_usd_resolver_misc_deps)
  # Remove carbonite::carb and omni::trace from omniusdresolver::omni_usd_resolver dependencies
  if(TARGET omniusdresolver::omni_usd_resolver)
    get_target_property(OMNI_USD_RESOLVER_INTERFACE_LINK_LIBS omniusdresolver::omni_usd_resolver INTERFACE_LINK_LIBRARIES)
    if(OMNI_USD_RESOLVER_INTERFACE_LINK_LIBS)
      list(REMOVE_ITEM OMNI_USD_RESOLVER_INTERFACE_LINK_LIBS "carbonite::carb" "omni::trace")
      set_target_properties(omniusdresolver::omni_usd_resolver PROPERTIES
        INTERFACE_LINK_LIBRARIES "${OMNI_USD_RESOLVER_INTERFACE_LINK_LIBS}")
    endif()
  endif()
endfunction()


if(${USD_DEVICE_USE_OMNIVERSE})
  set(OMNICLIENT_ROOT_DIR "" CACHE PATH "Path to the OmniClient Library")
  set(OMNIUSDRESOLVER_ROOT_DIR "" CACHE PATH "Path to the Omni Usd Resolver Library")

  # Handle OmniClient (no cmake config files - use manual approach)
  get_omni_dependency(
    OMNICLIENT_ROOT_DIR
    OMNICLIENT_INCLUDE_DIR
    OMNICLIENT_LIB_DIR
    "${OMNICLIENT_ROOT_DIR}"
    "OMNICLIENT_ROOT_DIR"
    "Omniverse Client"
    "include/OmniClient.h")

  list(APPEND OMNICLIENT_SHARED_LIBS omniclient)

  # Check structure and use appropriate approach for USD resolver
  if(EXISTS "${OMNIUSDRESOLVER_ROOT_DIR}/include/OmniUsdResolver.h")
    # Old structure: use get_omni_dependency for manual setup
    get_omni_dependency(
      OMNIUSDRESOLVER_ROOT_DIR
      OMNIUSDRESOLVER_INCLUDE_DIR
      OMNIUSDRESOLVER_LIB_DIR
      "${OMNIUSDRESOLVER_ROOT_DIR}"
      "OMNIUSDRESOLVER_ROOT_DIR"
      "Omniverse USD Resolver"
      "include/OmniUsdResolver.h")
    list(APPEND OMNIUSDRESOLVER_SHARED_LIBS omni_usd_resolver)
    set(OMNIUSDRESOLVER_USE_MANUAL_APPROACH TRUE)
  else()
    # Handle Omni USD Resolver - detect structure and choose approach
    if (NOT EXISTS ${OMNIUSDRESOLVER_ROOT_DIR})
      find_path(OMNIUSDRESOLVER_ROOT_DIR
        NAMES
          "include/omni_usd_resolver/OmniUsdResolver.h"        # Direct package directory
          "release/include/omni_usd_resolver/OmniUsdResolver.h" # Parent with release subdir
          "debug/include/omni_usd_resolver/OmniUsdResolver.h"   # Parent with debug subdir
        DOC "Path to Omniverse USD Resolver")
      message(STATUS "Using OMNIUSDRESOLVER_ROOT_DIR: ${OMNIUSDRESOLVER_ROOT_DIR}")
    endif()

    if (NOT EXISTS ${OMNIUSDRESOLVER_ROOT_DIR})
      message(FATAL_ERROR "No valid OMNIUSDRESOLVER_ROOT_DIR set, or found using CMAKE_PREFIX_PATH: ${OMNIUSDRESOLVER_ROOT_DIR}")
    endif()

    # New structure: use cmake config approach
    # Add cmake directories to module path based on build type
    set(OMNIUSDRESOLVER_CMAKE_PATHS ${OMNIUSDRESOLVER_ROOT_DIR}/cmake)

    # Add config-specific path
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
      list(APPEND OMNIUSDRESOLVER_CMAKE_PATHS ${OMNIUSDRESOLVER_ROOT_DIR}/debug/cmake)
    else()
      list(APPEND OMNIUSDRESOLVER_CMAKE_PATHS ${OMNIUSDRESOLVER_ROOT_DIR}/release/cmake)
    endif()

    find_package(omniUsdResolver REQUIRED
      HINTS ${OMNIUSDRESOLVER_CMAKE_PATHS})

    set(OMNIUSDRESOLVER_USE_MANUAL_APPROACH FALSE)
  endif()

  # Setup targets for omniverse client
  setup_client_library_target(${OMNICLIENT_ROOT_DIR})

  # Remove unwanted dependencies from USD resolver
  remove_usd_resolver_misc_deps()

else()
  MESSAGE(STATUS "UsdBridge is NOT using OmniClient for remote connections.")
endif()

####################
# UsdBridgeConnect #
####################

target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_sources(${PROJECT_NAME}
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeConnection.cpp
    ${CMAKE_CURRENT_LIST_DIR}/UsdBridgeConnection.h)

target_link_libraries(${PROJECT_NAME}
  INTERFACE  
    UsdBridge_Common
  )

if(${USD_DEVICE_USE_OMNIVERSE})
  # Include directories and compile definitions
  target_include_directories(${PROJECT_NAME} INTERFACE ${OMNICLIENT_INCLUDE_DIR} ${OMNIUSDRESOLVER_INCLUDE_DIR})
  target_compile_definitions(${PROJECT_NAME} INTERFACE OMNIVERSE_CONNECTION_ENABLE)

  # Link libraries
  if (WIN32)
    target_link_directories(${PROJECT_NAME} INTERFACE ${OMNICLIENT_LIB_DIR} ${OMNIUSDRESOLVER_LIB_DIR})
    set(USDBRIDGE_CONNECT_LINK_LIBS ${OMNICLIENT_SHARED_LIBS} ${OMNIUSDRESOLVER_SHARED_LIBS})
  else()
    if(NOT ${USD_DEVICE_USE_CXX11_ABI})
      target_compile_definitions(${PROJECT_NAME} INTERFACE
        _GLIBCXX_USE_CXX11_ABI=0 )
    endif()

    foreach(lib ${OMNICLIENT_SHARED_LIBS})
      find_library(path ${lib} PATHS ${OMNICLIENT_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
      list(APPEND OMNI_LIBS ${path})
      unset(path CACHE)
    endforeach()

    foreach(lib ${OMNIUSDRESOLVER_SHARED_LIBS})
      find_library(path ${lib} PATHS ${OMNIUSDRESOLVER_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
      list(APPEND OMNI_LIBS ${path})
      unset(path CACHE)
    endforeach()

    set(USDBRIDGE_CONNECT_LINK_LIBS ${OMNI_LIBS})
  endif()

  if(NOT OMNIUSDRESOLVER_USE_MANUAL_APPROACH)
    list(APPEND USDBRIDGE_CONNECT_LINK_LIBS omniusdresolver::omni_usd_resolver)
  endif()

  # Link both manual libraries and cmake target
  target_link_libraries(${PROJECT_NAME} INTERFACE ${USDBRIDGE_CONNECT_LINK_LIBS})

  option(INSTALL_OMNIVERSE_DEPS "Enable install of Omniverse dependencies with USD device" OFF)
  if(${INSTALL_OMNIVERSE_DEPS})
    set(PLATFORM_INSTALL_LIBDIR "$<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_INSTALL_LIBDIR}>$<$<PLATFORM_ID:Windows>:${CMAKE_INSTALL_BINDIR}>/")

    # Install OmniClient library
    install(
      DIRECTORY "${OMNICLIENT_LIB_DIR}/"
      DESTINATION ${PLATFORM_INSTALL_LIBDIR}
      PATTERN "*${CMAKE_STATIC_LIBRARY_SUFFIX}" EXCLUDE
      PATTERN "bindings-python" EXCLUDE)

    if(OMNIUSDRESOLVER_USE_MANUAL_APPROACH)
      install(
        DIRECTORY "${OMNIUSDRESOLVER_LIB_DIR}/"
        DESTINATION ${PLATFORM_INSTALL_LIBDIR}
        PATTERN "*${CMAKE_STATIC_LIBRARY_SUFFIX}" EXCLUDE
        PATTERN "bindings-python" EXCLUDE)
    else()
      # Detect directory structure and set prefixes
      if(EXISTS "${OMNIUSDRESOLVER_ROOT_DIR}/release/lib" OR EXISTS "${OMNIUSDRESOLVER_ROOT_DIR}/debug/lib")
        # Parent directory structure: use release/debug prefixes
        set(OMNIUSDRESOLVER_PREFIX_RELEASE "release/")
        set(OMNIUSDRESOLVER_PREFIX_DEBUG "debug/")
      endif()

      # Install Omni USD Resolver library
      install(
        DIRECTORY "$<$<NOT:$<CONFIG:Debug>>:${OMNIUSDRESOLVER_ROOT_DIR}/${OMNIUSDRESOLVER_PREFIX_RELEASE}lib/>$<$<CONFIG:Debug>:${OMNIUSDRESOLVER_ROOT_DIR}/${OMNIUSDRESOLVER_PREFIX_DEBUG}lib/>"
        DESTINATION ${PLATFORM_INSTALL_LIBDIR}
        PATTERN "*${CMAKE_STATIC_LIBRARY_SUFFIX}" EXCLUDE)
      # Install Omni USD Resolver DLLs/shared libraries from bin
      install(
        DIRECTORY "$<$<NOT:$<CONFIG:Debug>>:${OMNIUSDRESOLVER_ROOT_DIR}/${OMNIUSDRESOLVER_PREFIX_RELEASE}bin/>$<$<CONFIG:Debug>:${OMNIUSDRESOLVER_ROOT_DIR}/${OMNIUSDRESOLVER_PREFIX_DEBUG}bin/>"
        DESTINATION ${PLATFORM_INSTALL_LIBDIR}
        FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
    endif()

    # Install Omni USD Resolver plugin files (plugInfo.json)
    # Determine the correct plugin source directory based on config
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
      set(OMNIUSDRESOLVER_PLUGIN_SOURCE_DIR "${OMNIUSDRESOLVER_ROOT_DIR}/${OMNIUSDRESOLVER_PREFIX_DEBUG}plugins")
    else()
      set(OMNIUSDRESOLVER_PLUGIN_SOURCE_DIR "${OMNIUSDRESOLVER_ROOT_DIR}/${OMNIUSDRESOLVER_PREFIX_RELEASE}plugins")
    endif()

    # Install plugInfo.json and related plugin files to plugin/omni_usd_resolver/
    if(EXISTS "${OMNIUSDRESOLVER_PLUGIN_SOURCE_DIR}")
      install(
        DIRECTORY "${OMNIUSDRESOLVER_PLUGIN_SOURCE_DIR}/"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/plugin/")

      message(STATUS "Installing Omni USD Resolver plugins from: ${OMNIUSDRESOLVER_PLUGIN_SOURCE_DIR}")
      message(STATUS "Installing to: ${CMAKE_INSTALL_PREFIX}/plugin/")
    else()
      message(WARNING "Omni USD Resolver plugin directory not found: ${OMNIUSDRESOLVER_PLUGIN_SOURCE_DIR}")
    endif()
  endif()
endif()

message( "-------------------- End UsdBridgeConnect CmakeLists.txt ----------------------------------------")

