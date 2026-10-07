cmake_minimum_required(VERSION 3.16)
project(neondoll-embedded LANGUAGES C CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories for the library
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Library target for the portable interfaces and core logic
# Note: IdentityImpl is now header-only, so we only need body.cpp
add_library(neondoll STATIC
    src/neondoll/body.cpp
)

# Example target (ESP32)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_HOST_TESTS "Build host tests" ON)

if(BUILD_EXAMPLES)
    # We'll check for ESP-IDF by looking for the IDF_PATH environment variable or a toolchain file.
    # For now, we'll assume the example is only built when ESP-IDF is available.
    # We'll create a minimal example that blinks an LED and initializes the Body skeleton.
    # The example will be in examples/minimal/
    # We'll create a separate CMakeLists.txt in the example directory.
    add_subdirectory(examples/minimal)
endif()

# Host tests
if(BUILD_HOST_TESTS)
    enable_testing()
    add_subdirectory(host_test)
endif()

# Install headers
install(DIRECTORY include/ DESTINATION include)
