cmake_minimum_required(VERSION 3.16)

# We assume that if we are building the example, we are in an ESP-IDF environment.
# In ESP-IDF, the project CMakeLists.txt is usually in the main directory and the example is a component.
# However, for simplicity, we'll make this example a standalone executable that uses the neondoll library.
# We'll check for ESP-IDF by looking for the IDF_PATH environment variable.

# If IDF_PATH is set, we are building for ESP32.
if(DEFINED ENV{IDF_PATH})
    # We are in an ESP-IDF build system. We'll create an executable.
    # Note: In ESP-IDF, you typically don't create executables in components, but for simplicity we do.
    # We'll create a simple example that blinks an LED and uses the neondoll Body.
    add_executable(neondoll_minimal_example main.cpp)
    target_include_directories(neondoll_minimal_example PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    )
    target_link_libraries(neondoll_minimal_example PRIVATE neondoll)
    # We need to link against the ESP-IDF components for GPIO, etc.
    # This is a simplified example; in reality, you would use the ESP-IDF build system properly.
    # For M0, we'll just note that the example is set up and leave the ESP-IDF specifics to the user.
else()
    # If not building for ESP32, we still want to build the example for host testing? 
    # But the example is ESP32-specific, so we skip it on host.
    message(STATUS "IDF_PATH not set, skipping ESP32 example build.")
endif()