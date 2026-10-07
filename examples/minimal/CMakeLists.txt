cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/tools/cmake/project.cmake")
    # ESP-IDF build
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(neondoll_minimal_example)

    # Add neondoll source files directly to the executable target
    set(neondoll_src
        ${PROJECT_SOURCE_DIR}/../../src/neondoll/body.cpp
        ${PROJECT_SOURCE_DIR}/../../src/neondoll/identity.cpp
    )
    set(neondoll_inc
        ${PROJECT_SOURCE_DIR}/../../include
        ${PROJECT_SOURCE_DIR}/../../src/neondoll
    )
    target_sources(${PROJECT_NAME} PRIVATE ${neondoll_src})
    target_include_directories(${PROJECT_NAME} PRIVATE ${neondoll_inc})
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