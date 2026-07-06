## Copyright 2020 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

if (NOT WIN32)
  set (PLATFORM_LIBS m)
endif()

# Point the Visual Studio debugger at the installed executable and run it from the
# install bin dir, so F5 debugging works against the deployed binary.
function(set_example_debugger target)
  set_target_properties(${target} PROPERTIES
    VS_DEBUGGER_COMMAND "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:${target}>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
endfunction()

project(anariTutorialUsd)
add_executable(${PROJECT_NAME} anariTutorial_usd.c)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image ${PLATFORM_LIBS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
set_example_debugger(${PROJECT_NAME})

project(anariTutorialUsdCpp)
add_executable(${PROJECT_NAME} anariTutorial_usd.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image ${PLATFORM_LIBS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
set_example_debugger(${PROJECT_NAME})

project(anariTutorialUsdTime)
add_executable(${PROJECT_NAME} anariTutorial_usd_time.c)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image ${PLATFORM_LIBS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
set_example_debugger(${PROJECT_NAME})

project(anariTutorialUsdRecreate)
add_executable(${PROJECT_NAME} anariTutorial_usd_recreate.c)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image ${PLATFORM_LIBS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
set_example_debugger(${PROJECT_NAME})

project(anariTutorialUsdVolume)
add_executable(${PROJECT_NAME} anariTutorial_usd_volume.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image ${PLATFORM_LIBS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
set_example_debugger(${PROJECT_NAME})

if(USD_DEVICE_MPI_ENABLED)
  find_package(MPI REQUIRED COMPONENTS CXX)
  project(anariTutorialUsdMpi)
  add_executable(${PROJECT_NAME} anariTutorial_usd_mpi.cpp)
  target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari MPI::MPI_CXX ${PLATFORM_LIBS})
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  set_example_debugger(${PROJECT_NAME})
endif()