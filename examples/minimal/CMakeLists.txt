cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/tools/cmake/project.cmake")
    # ESP-IDF build: we are building the example as an ESP-IDF project.
    # We want to include our Body implementation in the main component.
    set(EXTRA_SRCS ${PROJECT_SOURCE_DIR}/../../src/neondoll/body.cpp)
    set(EXTRA_INCLUDES ${PROJECT_SOURCE_DIR}/../../include)
    idf_component_register(
        SRCS "main/main.cpp" ${EXTRA_SRCS}
        INCLUDE_DIRS "." ${EXTRA_INCLUDES}
        REQUIRES esp_system
    )
else()
    # Host build: we are being built by the top-level CMakeLists.txt.
    # The top-level CMakeLists.txt has already built the neondoll library and set up the example to link to it.
    # We just need to create the executable and link to neondoll.
    add_executable(neondoll_minimal_example main/main.cpp)
    target_include_directories(neondoll_minimal_example PRIVATE
        ${PROJECT_SOURCE_DIR}/../..
        ${PROJECT_SOURCE_DIR}/../../include
    )
    target_link_libraries(neondoll_minimal_example PRIVATE neondoll)
endif()