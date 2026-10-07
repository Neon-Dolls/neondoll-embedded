cmake_minimum_required(VERSION 3.16)

# Check if we're building with ESP-IDF
if(DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/tools/cmake/project.cmake")
    # ESP-IDF build
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(neondoll_minimal_example)

    # Build the neondoll library from our sources
    add_library(neondoll STATIC
        ${PROJECT_SOURCE_DIR}/../../src/neondoll/body.cpp
    )
    target_include_directories(neondoll
        ${PROJECT_SOURCE_DIR}/../../include
        ${PROJECT_SOURCE_DIR}/../../src/neondoll
    )

    # Link the neondoll library to the main executable (created by project())
    target_link_libraries(${PROJECT_NAME} PRIVATE neondoll)
else()
    # Regular CMake build for host tests
    project(neondoll_minimal_example LANGUAGES C CXX)
    
    # Set C++ standard
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    
    # Include directories
    include_directories(${PROJECT_SOURCE_DIR}/../..)
    include_directories(${PROJECT_SOURCE_DIR}/../../include)
    
    # Source files
    set(SOURCES main/main.cpp)
    
    # Create executable
    add_executable(neondoll_minimal_example ${SOURCES})
    
    # Link against neondoll library (built in the top-level CMakeLists.txt)
    target_link_libraries(neondoll_minimal_example neondoll)
endif()
