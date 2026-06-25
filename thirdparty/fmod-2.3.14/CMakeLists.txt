cmake_minimum_required(VERSION 4.2)
project(FMOD)

set (CORE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core")
set (STUDIO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/studio")

add_library(FMOD::Core UNKNOWN IMPORTED)
add_library(FMOD::Studio UNKNOWN IMPORTED)
# Check For ARM architexture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|arm64)")
    message(STATUS "Configuring FMOD for ARM architecture")
    
    # Add Core Library into Interface
    set_target_properties(FMOD::Core PROPERTIES    
        IMPORTED_INCLUDE_DIRECTORY "${CORE_ROOT_DIR}/inc" 
        IMPORTED_IMPLIB "${CORE_ROOT_DIR}/lib/arm" 

        #Fall Back
        IMPORTED_LOCATION "${CORE_ROOT_DIR}/lib/arm"
        # Configuration Override
        IMPORTED_LOCATION_RELEASE "${CORE_ROOT_DIR}/lib/arm/fmod.dll"
        IMPORTED_LOCATION_DEBUG "${CORE_ROOT_DIR}/lib/arm/fmodL.dll"
    )
    # Add Studio Library into Interface
    set_target_properties(FMOD::Studio PROPERTIES
        IMPORTED_INCLUDE_DIRECTORY "${STUDIO_ROOT_DIR}/inc"
        IMPORTED_IMPLIB "${STUDIO_ROOT_DIR}/lib/arm"
    
        #Fall Back
        IMPORTED_LOCATION "${STUDIO_ROOT_DIR}/lib/arm"
        # Configuration Override
        IMPORTED_LOCATION_RELEASE "${STUDIO_ROOT_DIR}/lib/arm/fmod.dll"
        IMPORTED_LOCATION_DEBUG "${STUDIO_ROOT_DIR}/lib/arm/fmodL.dll"
    )

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    message(STATUS "Configuring FMOD for x86 architecture")

    # Add Core Library into Interface
    set_target_properties(FMOD::Core PROPERTIES    
        IMPORTED_INCLUDE_DIRECTORY "${CORE_ROOT_DIR}/inc" 
        IMPORTED_IMPLIB "${CORE_ROOT_DIR}/lib/x64" 

        #Fall Back
        IMPORTED_LOCATION "${CORE_ROOT_DIR}/lib/x64"
        # Configuration Override
        IMPORTED_LOCATION_RELEASE "${CORE_ROOT_DIR}/lib/x64/fmod.dll"
        IMPORTED_LOCATION_DEBUG "${CORE_ROOT_DIR}/lib/x64/fmodL.dll"
    )
    # Add Studio Library into Interface
    set_target_properties(FMOD::Studio PROPERTIES
        IMPORTED_INCLUDE_DIRECTORY "${STUDIO_ROOT_DIR}/inc"
        IMPORTED_IMPLIB "${STUDIO_ROOT_DIR}/lib/x64"
    
        #Fall Back
        IMPORTED_LOCATION "${STUDIO_ROOT_DIR}/lib/x64"
        # Configuration Override
        IMPORTED_LOCATION_RELEASE "${STUDIO_ROOT_DIR}/lib/x64/fmod.dll"
        IMPORTED_LOCATION_DEBUG "${STUDIO_ROOT_DIR}/lib/x64/fmodL.dll"
    )
   
else()
    message(WARNING "Unknown or unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Umbrella for collectiong different APIs
add_library(FMOD INTERFACE IMPORTED)

target_include_directories(FMOD INTERFACE 
    "${CMAKE_CURRENT_SOURCE_DIR}/core/inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/studio/inc"
)

target_link_libraries(FMOD INTERFACE
    FMOD::Core
    FMOD::Studio
)