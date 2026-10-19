enable_language(C)
if(NOT EXISTS "${WIREGUARD_SOURCE_DIR}/src/neondoll_ip6.h")
    message(FATAL_ERROR "Prepare the pinned NeonDoll patch set before enabling packet tests")
endif()
find_library(SODIUM_LIBRARY NAMES sodium libsodium.so.26 libsodium.so.23)
if(NOT SODIUM_LIBRARY)
    message(FATAL_ERROR "Host libsodium library is required (e.g. libsodium-dev)")
endif()
add_library(wg_packet_host STATIC
    harness.c
    ${WIREGUARD_SOURCE_DIR}/src/wireguard.c
    ${WIREGUARD_SOURCE_DIR}/src/crypto.c
    ${WIREGUARD_SOURCE_DIR}/src/crypto/refc/blake2s.c
    ${WIREGUARD_SOURCE_DIR}/src/crypto/refc/chacha20.c
    ${WIREGUARD_SOURCE_DIR}/src/crypto/refc/chacha20poly1305.c
    ${WIREGUARD_SOURCE_DIR}/src/crypto/refc/poly1305-donna.c
    ${LWIP_SOURCE_DIR}/src/core/ip.c
    ${LWIP_SOURCE_DIR}/src/core/pbuf.c
    ${LWIP_SOURCE_DIR}/src/core/mem.c
    ${LWIP_SOURCE_DIR}/src/core/memp.c
    ${LWIP_SOURCE_DIR}/src/core/def.c
    ${LWIP_SOURCE_DIR}/src/core/ipv4/ip4_addr.c
    ${LWIP_SOURCE_DIR}/src/core/ipv6/ip6_addr.c)
target_include_directories(wg_packet_host PUBLIC stubs ${WIREGUARD_SOURCE_DIR}/src
    ${WIREGUARD_SOURCE_DIR}/include ${LWIP_SOURCE_DIR}/src/include)
target_compile_definitions(wg_packet_host PRIVATE CONFIG_LWIP_IPV6=1 CONFIG_WIREGUARD_MAX_SRC_IPS=4)
target_compile_options(wg_packet_host PRIVATE -ffunction-sections -fdata-sections -Wno-unused-result)
target_link_libraries(wg_packet_host PUBLIC ${SODIUM_LIBRARY})
add_executable(wg_ipv6_tests test_packets.cpp)
target_link_libraries(wg_ipv6_tests PRIVATE wg_packet_host GTest::gtest_main)
target_link_options(wg_ipv6_tests PRIVATE -Wl,--gc-sections)
add_test(NAME patched_wireguard_ipv6 COMMAND wg_ipv6_tests)
