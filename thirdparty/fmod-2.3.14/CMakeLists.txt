cmake_minimum_required(VERSION 3.28)
project(FMOD)

set (CORE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core")
set (STUDIO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/studio")

# Pick the prebuilt binary set for the host platform/architecture. Each
# platform ships its libraries under a differently named directory with a
# different shared-library naming convention, so both have to be selected
# together.
if(APPLE)
    set(FMOD_LIB_SUBDIR "mac")
    set(FMOD_LIB_PREFIX "lib")
    set(FMOD_LIB_SUFFIX ".dylib")
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(FMOD_LIB_SUBDIR "linux_arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
        set(FMOD_LIB_SUBDIR "linux_arm")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(FMOD_LIB_SUBDIR "linux_x86_64")
    else()
        set(FMOD_LIB_SUBDIR "linux_x86")
    endif()
    set(FMOD_LIB_PREFIX "lib")
    # The bare "libfmod.so" / "libfmod.so.14" names in this SDK drop are meant
    # to be symlinks to the fully versioned file below, but ship as broken
    # zero-byte placeholders — linking against them fails with "undefined
    # reference" for every FMOD symbol. Link against the versioned file
    # directly to sidestep the broken symlinks.
    set(FMOD_LIB_SUFFIX ".so.14.14")
else() # Windows
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(FMOD_LIB_SUBDIR "arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|ARM)")
        set(FMOD_LIB_SUBDIR "arm")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(FMOD_LIB_SUBDIR "x64")
    else()
        set(FMOD_LIB_SUBDIR "x86")
    endif()
    set(FMOD_LIB_PREFIX "")
    set(FMOD_LIB_SUFFIX ".dll")
endif()

set (FMOD_CORE_LIB_DIR "${CORE_ROOT_DIR}/lib/${FMOD_LIB_SUBDIR}")
set (FMOD_STUDIO_LIB_DIR "${STUDIO_ROOT_DIR}/lib/${FMOD_LIB_SUBDIR}")

# Core API
add_library(FMOD::Core SHARED IMPORTED)
set_target_properties(FMOD::Core PROPERTIES
    IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
    IMPORTED_LOCATION "${FMOD_CORE_LIB_DIR}/${FMOD_LIB_PREFIX}fmod${FMOD_LIB_SUFFIX}"
    IMPORTED_LOCATION_DEBUG "${FMOD_CORE_LIB_DIR}/${FMOD_LIB_PREFIX}fmodL${FMOD_LIB_SUFFIX}"
    IMPORTED_LOCATION_RELEASE "${FMOD_CORE_LIB_DIR}/${FMOD_LIB_PREFIX}fmod${FMOD_LIB_SUFFIX}"
)

# Studio API (event-based audio, unused today but kept available)
add_library(FMOD::Studio SHARED IMPORTED)
set_target_properties(FMOD::Studio PROPERTIES
    IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
    IMPORTED_LOCATION "${FMOD_STUDIO_LIB_DIR}/${FMOD_LIB_PREFIX}fmodstudio${FMOD_LIB_SUFFIX}"
    IMPORTED_LOCATION_DEBUG "${FMOD_STUDIO_LIB_DIR}/${FMOD_LIB_PREFIX}fmodstudioL${FMOD_LIB_SUFFIX}"
    IMPORTED_LOCATION_RELEASE "${FMOD_STUDIO_LIB_DIR}/${FMOD_LIB_PREFIX}fmodstudio${FMOD_LIB_SUFFIX}"
)

if(WIN32)
    # DLLs need a separate .lib import library; ELF/Mach-O shared objects link directly.
    set_target_properties(FMOD::Core PROPERTIES
        IMPORTED_IMPLIB "${FMOD_CORE_LIB_DIR}/fmod.lib"
        IMPORTED_IMPLIB_DEBUG "${FMOD_CORE_LIB_DIR}/fmodL.lib"
        IMPORTED_IMPLIB_RELEASE "${FMOD_CORE_LIB_DIR}/fmod.lib"
    )
    set_target_properties(FMOD::Studio PROPERTIES
        IMPORTED_IMPLIB "${FMOD_STUDIO_LIB_DIR}/fmodstudio.lib"
        IMPORTED_IMPLIB_DEBUG "${FMOD_STUDIO_LIB_DIR}/fmodstudioL.lib"
        IMPORTED_IMPLIB_RELEASE "${FMOD_STUDIO_LIB_DIR}/fmodstudio.lib"
    )
endif()

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
