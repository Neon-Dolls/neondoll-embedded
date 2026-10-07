cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/tools/cmake/project.cmake")
    # We are building with ESP-IDF
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    if(NOT PROJECT_NAME)
        # This is the top-level project call for the ESP-IDF build
        project(neondoll_minimal_example)
    endif()

    # Build the neondoll library from our sources
    add_library(neondoll STATIC
        ${PROJECT_SOURCE_DIR}/../../src/neondoll/body.cpp
    )
    target_include_directories(neondoll
        ${PROJECT_SOURCE_DIR}/../../include
        ${PROJECT_SOURCE_DIR}/../../src/neondoll
    )

    # Link the neondoll library to the main executable
    target_link_libraries(${PROJECT_NAME} PRIVATE neondoll)
else()
    # Not building with ESP-IDF (host build via top-level CMakeLists.txt)
    # We assume we are already in a project (the top-level neondoll-embedded project)
    # So we just create the executable and link to the neondoll library
    add_executable(neondoll_minimal_example main/main.cpp)
    target_include_directories(neondoll_minimal_example PRIVATE
        ${PROJECT_SOURCE_DIR}/../..
        ${PROJECT_SOURCE_DIR}/../../include
    )
    target_link_libraries(neondoll_minimal_example PRIVATE neondoll)
endif()
