cmake_minimum_required(VERSION 3.16)
project(minimal_body)

# Set extra component directories to include our custom component
set(EXTRA_COMPONENT_DIRS ${PROJECT_PATH}/../components)

# Include the ESP-IDF project manager
include($ENV{IDF_PATH}/tools/cmake/project.cmake)