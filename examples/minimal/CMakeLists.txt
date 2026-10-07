cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/tools/cmake/project.cmake")
    # ESP-IDF build
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(neondoll_minimal_example)

    # Build the neondoll library if it doesn't exist
    if(NOT TARGET neondoll)
        add_library(neondoll STATIC
            ${PROJECT_SOURCE_DIR}/../../src/neondoll/body.cpp
        )
        target_include_directories(neondoll
            ${PROJECT_SOURCE_DIR}/../../include
            ${PROJECT_SOURCE_DIR}/../../src/neondoll
        )
    endif()

    # Link the neondoll library to the main executable
    target_link_libraries(${PROJECT_NAME} PRIVATE neondoll)
else()
    # Host build (via top-level CMakeLists.txt)
    # We assume the top-level CMakeLists.txt has already built the neondoll library.
    # We just create the executable and link to it.
    project(neondoll_minimal_example LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    add_executable(neondoll_minimal_example main/main.cpp)
    target_include_directories(neondoll_minimal_example PRIVATE
        ${PROJECT_SOURCE_DIR}/../..
        ${PROJECT_SOURCE_DIR}/../../include
    )
    target_link_libraries(neondoll_minimal_example PRIVATE neondoll)
endif()
