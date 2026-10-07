cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/tools/cmake/project.cmake")
    # ESP-IDF build
    cmake_minimum_required(VERSION 3.16)
    project(neondoll_minimal_example)

    # Set C++ standard
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # Build the neondoll library from our sources
    add_library(neondoll STATIC
        ${PROJECT_SOURCE_DIR}/../../src/neondoll/body.cpp
    )
    target_include_directories(neondoll
        ${PROJECT_SOURCE_DIR}/../../include
        ${PROJECT_SOURCE_DIR}/../../src/neondoll
    )

    # Create the example executable and link to the neondoll library
    add_executable(neondoll_minimal_example main/main.cpp)
    target_link_libraries(neondoll_minimal_example neondoll)
else()
    # Host build: we are being built by the top-level CMakeLists.txt
    # We assume the top-level CMakeLists.txt has already set up the project and built the neondoll library.
    # But just in case, we will build the neondoll library here as well (it's harmless if already built by parent).
    add_library(neondoll STATIC
        ${PROJECT_SOURCE_DIR}/../../src/neondoll/body.cpp
    )
    target_include_directories(neondoll
        ${PROJECT_SOURCE_DIR}/../../include
        ${PROJECT_SOURCE_DIR}/../../src/neondoll
    )

    add_executable(neondoll_minimal_example main/main.cpp)
    target_include_directories(neondoll_minimal_example PRIVATE
        ${PROJECT_SOURCE_DIR}/../..
        ${PROJECT_SOURCE_DIR}/../../include
    )
    target_link_libraries(neondoll_minimal_example PRIVATE neondoll)
endif()
