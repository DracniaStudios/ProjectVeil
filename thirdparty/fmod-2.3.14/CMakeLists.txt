cmake_minimum_required(VERSION 4.2)
project(FMOD)

set (CORE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core")
set (STUDIO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/studio")

# Umbrella for collectiong different APIs
add_library(FMOD INTERFACE)

target_include_directories(FMOD INTERFACE 
    "${CMAKE_CURRENT_SOURCE_DIR}/core/inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/studio/inc"
)
