cmake_minimum_required(VERSION 3.16)

# Build an executable for the host test
add_executable(neondoll_host_test test_body.cpp)
target_include_directories(neondoll_host_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
target_link_libraries(neondoll_host_test PRIVATE neondoll)

# Add a test that runs the executable
add_test(NAME neondoll_host_test COMMAND neondoll_host_test)