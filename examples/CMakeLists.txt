## Copyright 2020 The Khronos Group
## SPDX-License-Identifier: Apache-2.0

if (NOT WIN32)
  set (PLATFORM_LIBS m)
endif()

project(anariTutorialUsd)
add_executable(${PROJECT_NAME} anariTutorial_usd.c)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image ${PLATFORM_LIBS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

project(anariTutorialUsdCpp)
add_executable(${PROJECT_NAME} anariTutorial_usd.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image ${PLATFORM_LIBS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

project(anariTutorialUsdTime)
add_executable(${PROJECT_NAME} anariTutorial_usd_time.c)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image ${PLATFORM_LIBS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

project(anariTutorialUsdRecreate)
add_executable(${PROJECT_NAME} anariTutorial_usd_recreate.c)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image ${PLATFORM_LIBS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

project(anariTutorialUsdVolume)
add_executable(${PROJECT_NAME} anariTutorial_usd_volume.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari stb_image ${PLATFORM_LIBS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(USD_DEVICE_MPI_ENABLED)
  find_package(MPI REQUIRED COMPONENTS CXX)
  project(anariTutorialUsdMpi)
  add_executable(${PROJECT_NAME} anariTutorial_usd_mpi.cpp)
  target_link_libraries(${PROJECT_NAME} PRIVATE anari::anari MPI::MPI_CXX ${PLATFORM_LIBS})
  install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()