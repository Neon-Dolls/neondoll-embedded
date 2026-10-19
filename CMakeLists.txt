cmake_minimum_required(VERSION 3.16)
project(neondoll_host LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
add_library(neondoll components/neondoll/src/neondoll.cpp
                     components/neondoll/src/identity.cpp
                     components/neondoll/src/identity_storage.cpp
                     components/neondoll/src/network_supervisor.cpp
                     components/neondoll/src/wireguard_transport.cpp)
target_include_directories(neondoll PUBLIC components/neondoll/include)
target_compile_options(neondoll PRIVATE -Wall -Wextra -Werror -pedantic)
if(BUILD_TESTING)
    if(GTEST_SOURCE_DIR)
        add_subdirectory(${GTEST_SOURCE_DIR} ${CMAKE_BINARY_DIR}/googletest EXCLUDE_FROM_ALL)
    else()
        find_package(GTest REQUIRED)
    endif()
    add_executable(network_supervisor_tests host_test/test_network_supervisor.cpp)
    target_link_libraries(network_supervisor_tests PRIVATE neondoll GTest::gtest_main)
    add_test(NAME network_recovery_policy COMMAND network_supervisor_tests)
    add_executable(wireguard_transport_tests host_test/test_wireguard_transport.cpp)
    target_link_libraries(wireguard_transport_tests PRIVATE neondoll GTest::gtest_main)
    add_test(NAME wireguard_transport_lifecycle COMMAND wireguard_transport_tests)
    add_executable(neondoll_tests host_test/test_neondoll.cpp
                                 host_test/test_neondoll_identity.cpp
                                 host_test/test_identity_faults.cpp)
    target_link_libraries(neondoll_tests PRIVATE neondoll GTest::gtest_main)
    add_test(NAME identity_and_lifecycle COMMAND neondoll_tests)
    add_library(esp32_network_host components/neondoll/src/esp32_network.cpp
        host_test/esp_idf_stubs/network_fake.cpp)
    target_include_directories(esp32_network_host PUBLIC host_test/esp_idf_stubs)
    target_link_libraries(esp32_network_host PUBLIC neondoll)
    target_compile_options(esp32_network_host PRIVATE -Wall -Wextra -Werror -pedantic)
    add_executable(esp32_network_tests host_test/test_esp32_network.cpp)
    target_link_libraries(esp32_network_tests PRIVATE esp32_network_host GTest::gtest_main)
    add_test(NAME esp32_network_lifecycle COMMAND esp32_network_tests)
    add_executable(esp32_adapter_tests host_test/test_esp32_platform.cpp
        components/neondoll/src/esp32_platform.cpp)
    target_include_directories(esp32_adapter_tests PRIVATE host_test/esp_idf_stubs)
    target_link_libraries(esp32_adapter_tests PRIVATE esp32_network_host GTest::gtest_main)
    add_test(NAME esp32_adapter_faults COMMAND esp32_adapter_tests)
endif()
