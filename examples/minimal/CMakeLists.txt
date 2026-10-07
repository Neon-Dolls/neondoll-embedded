cmake_minimum_required(VERSION 3.16)

# Check if we're building with ESP-IDF by checking if IDF_PATH is defined AND the project.cmake file exists
if(DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/tools/cmake/project.cmake")
    # ESP-IDF build
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(neondoll_minimal_example)
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
    
    # Link against neondoll library
    target_link_libraries(neondoll_minimal_example neondoll)
endif()
