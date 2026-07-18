cmake_minimum_required(VERSION 3.28)
project(FMOD)

set (CORE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core")
set (STUDIO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/studio")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|arm64)")
    set(FMOD_ARCH_DIR "arm")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(FMOD_ARCH_DIR "x64")
else()
    set(FMOD_ARCH_DIR "x86")
endif()

set (FMOD_CORE_LIB_DIR "${CORE_ROOT_DIR}/lib/${FMOD_ARCH_DIR}")
set (FMOD_STUDIO_LIB_DIR "${STUDIO_ROOT_DIR}/lib/${FMOD_ARCH_DIR}")

# Core API
add_library(FMOD::Core SHARED IMPORTED)
set_target_properties(FMOD::Core PROPERTIES
    IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
    IMPORTED_LOCATION "${FMOD_CORE_LIB_DIR}/fmod.dll"
    IMPORTED_IMPLIB "${FMOD_CORE_LIB_DIR}/fmod.lib"
    IMPORTED_LOCATION_DEBUG "${FMOD_CORE_LIB_DIR}/fmodL.dll"
    IMPORTED_IMPLIB_DEBUG "${FMOD_CORE_LIB_DIR}/fmodL.lib"
    IMPORTED_LOCATION_RELEASE "${FMOD_CORE_LIB_DIR}/fmod.dll"
    IMPORTED_IMPLIB_RELEASE "${FMOD_CORE_LIB_DIR}/fmod.lib"
)

# Studio API (event-based audio, unused today but kept available)
add_library(FMOD::Studio SHARED IMPORTED)
set_target_properties(FMOD::Studio PROPERTIES
    IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
    IMPORTED_LOCATION "${FMOD_STUDIO_LIB_DIR}/fmodstudio.dll"
    IMPORTED_IMPLIB "${FMOD_STUDIO_LIB_DIR}/fmodstudio.lib"
    IMPORTED_LOCATION_DEBUG "${FMOD_STUDIO_LIB_DIR}/fmodstudioL.dll"
    IMPORTED_IMPLIB_DEBUG "${FMOD_STUDIO_LIB_DIR}/fmodstudioL.lib"
    IMPORTED_LOCATION_RELEASE "${FMOD_STUDIO_LIB_DIR}/fmodstudio.dll"
    IMPORTED_IMPLIB_RELEASE "${FMOD_STUDIO_LIB_DIR}/fmodstudio.lib"
)

# Umbrella for collecting different APIs
add_library(FMOD INTERFACE)

target_include_directories(FMOD INTERFACE
    "${CORE_ROOT_DIR}/inc"
    "${STUDIO_ROOT_DIR}/inc"
)

target_link_libraries(FMOD INTERFACE
    FMOD::Core
    FMOD::Studio
)
