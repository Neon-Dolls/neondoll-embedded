cmake_minimum_required(VERSION 3.16)

if(DEFINED ENV{IDF_PATH} AND EXISTS "$ENV{IDF_PATH}/tools/cmake/project.cmake")
    # We are building with ESP-IDF
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(neondoll_minimal_example)

    # Build the executable from our sources: main.cpp and body.cpp
    add_executable(${PROJECT_NAME}
        main/main.cpp
        ../../src/neondoll/body.cpp
    )
    target_include_directories(${PROJECT_NAME}
        PRIVATE
            ../../include
            ../../src/neondoll
    )
else()
    # Not building with ESP-IDF (host build via top-level CMakeLists.txt)
    # We are being built by the top-level CMakeLists.txt, which has already built the neondoll library.
    add_executable(neondoll_minimal_example main/main.cpp)
    target_include_directories(neondoll_minimal_example PRIVATE
        ${PROJECT_SOURCE_DIR}/../..
        ${PROJECT_SOURCE_DIR}/../../include
    )
    target_link_libraries(neondoll_minimal_example neondoll)
endif()
