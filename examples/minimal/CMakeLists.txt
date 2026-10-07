cmake_minimum_required(VERSION 3.16)

if(NOT TARGET neondoll)
    add_library(neondoll STATIC
        ${PROJECT_SOURCE_DIR}/../../src/neondoll/body.cpp
    )
    target_include_directories(neondoll
        ${PROJECT_SOURCE_DIR}/../../include
        ${PROJECT_SOURCE_DIR}/../../src/neondoll
    )
endif()

add_executable(neondoll_minimal_example main/main.cpp)
target_include_directories(neondoll_minimal_example PRIVATE
    ${PROJECT_SOURCE_DIR}/../..
    ${PROJECT_SOURCE_DIR}/../../include
)
target_link_libraries(neondoll_minimal_example neondoll)
