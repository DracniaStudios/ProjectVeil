cmake_minimum_required(VERSION 3.28)
project(FMOD)

set (CORE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core")
set (STUDIO_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/studio")

# ---------------------------------------------------------------------------
# Platform and architecture
#
# FMOD lays its libraries out per platform, and the directory names do not
# follow one scheme: Windows uses bare arch names (x64), Linux prefixes them
# (linux_x86_64), and macOS ships a single universal binary under mac with no
# arch split at all. Selecting on CMAKE_SIZEOF_VOID_P alone — as this file did
# while it was Windows-only — lands in the Windows directories on every
# platform.
# ---------------------------------------------------------------------------
if(WIN32)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)")
		set(FMOD_ARCH_DIR "arm64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM|arm)")
		set(FMOD_ARCH_DIR "arm")
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(FMOD_ARCH_DIR "x64")
	else()
		set(FMOD_ARCH_DIR "x86")
	endif()
elseif(APPLE)
	set(FMOD_ARCH_DIR "mac")
elseif(UNIX)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
		set(FMOD_ARCH_DIR "linux_arm64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm)")
		set(FMOD_ARCH_DIR "linux_arm")
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(FMOD_ARCH_DIR "linux_x86_64")
	else()
		set(FMOD_ARCH_DIR "linux_x86")
	endif()
else()
	message(FATAL_ERROR "FMOD: no vendored libraries for this platform.")
endif()

set (FMOD_CORE_LIB_DIR "${CORE_ROOT_DIR}/lib/${FMOD_LIB_SUBDIR}")
set (FMOD_STUDIO_LIB_DIR "${STUDIO_ROOT_DIR}/lib/${FMOD_LIB_SUBDIR}")

if(NOT EXISTS "${FMOD_CORE_LIB_DIR}")
	message(FATAL_ERROR
		"FMOD: expected libraries in '${FMOD_CORE_LIB_DIR}' but that directory does not exist.")
endif()

# ---------------------------------------------------------------------------
# Resolving a Linux shared object
#
# FMOD's Linux packages ship libfmod.so and libfmod.so.14 as symlinks to the
# real libfmod.so.14.14. They are committed to this repository as ZERO-BYTE
# regular files — git recorded mode 100644 of the empty blob rather than mode
# 120000 symlinks — so linking against the unversioned name silently picks up
# an empty file and fails. This resolves to the first candidate that actually
# has bytes in it, and is version-agnostic so an FMOD bump does not need this
# file edited.
# ---------------------------------------------------------------------------
function(fmod_resolve_so OUT_VAR LIB_DIR BASE_NAME)
	file(GLOB candidates "${LIB_DIR}/${BASE_NAME}.so.*")
	# Longest name last: prefer the fully versioned real file over any
	# intermediate link name that happens to be populated.
	list(SORT candidates)
	list(REVERSE candidates)

	foreach(candidate IN LISTS candidates)
		file(SIZE "${candidate}" candidate_size)
		if(candidate_size GREATER 0)
			set(${OUT_VAR} "${candidate}" PARENT_SCOPE)
			return()
		endif()
	endforeach()

	# Nothing versioned had content; fall back to the plain name so the error,
	# if any, comes from the linker with a path attached.
	set(${OUT_VAR} "${LIB_DIR}/${BASE_NAME}.so" PARENT_SCOPE)
endfunction()

# Core API
add_library(FMOD::Core SHARED IMPORTED GLOBAL)
# Studio API (event-based audio, unused today but kept available)
add_library(FMOD::Studio SHARED IMPORTED GLOBAL)

if(WIN32)
	set_target_properties(FMOD::Core PROPERTIES
		IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
		IMPORTED_LOCATION "${FMOD_CORE_LIB_DIR}/fmod.dll"
		IMPORTED_IMPLIB "${FMOD_CORE_LIB_DIR}/fmod.lib"
		IMPORTED_LOCATION_DEBUG "${FMOD_CORE_LIB_DIR}/fmodL.dll"
		IMPORTED_IMPLIB_DEBUG "${FMOD_CORE_LIB_DIR}/fmodL.lib"
		IMPORTED_LOCATION_RELEASE "${FMOD_CORE_LIB_DIR}/fmod.dll"
		IMPORTED_IMPLIB_RELEASE "${FMOD_CORE_LIB_DIR}/fmod.lib"
	)
	set_target_properties(FMOD::Studio PROPERTIES
		IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
		IMPORTED_LOCATION "${FMOD_STUDIO_LIB_DIR}/fmodstudio.dll"
		IMPORTED_IMPLIB "${FMOD_STUDIO_LIB_DIR}/fmodstudio.lib"
		IMPORTED_LOCATION_DEBUG "${FMOD_STUDIO_LIB_DIR}/fmodstudioL.dll"
		IMPORTED_IMPLIB_DEBUG "${FMOD_STUDIO_LIB_DIR}/fmodstudioL.lib"
		IMPORTED_LOCATION_RELEASE "${FMOD_STUDIO_LIB_DIR}/fmodstudio.dll"
		IMPORTED_IMPLIB_RELEASE "${FMOD_STUDIO_LIB_DIR}/fmodstudio.lib"
	)
elseif(APPLE)
	set_target_properties(FMOD::Core PROPERTIES
		IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
		IMPORTED_LOCATION "${FMOD_CORE_LIB_DIR}/libfmod.dylib"
		IMPORTED_LOCATION_DEBUG "${FMOD_CORE_LIB_DIR}/libfmodL.dylib"
		IMPORTED_LOCATION_RELEASE "${FMOD_CORE_LIB_DIR}/libfmod.dylib"
	)
	set_target_properties(FMOD::Studio PROPERTIES
		IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
		IMPORTED_LOCATION "${FMOD_STUDIO_LIB_DIR}/libfmodstudio.dylib"
		IMPORTED_LOCATION_DEBUG "${FMOD_STUDIO_LIB_DIR}/libfmodstudioL.dylib"
		IMPORTED_LOCATION_RELEASE "${FMOD_STUDIO_LIB_DIR}/libfmodstudio.dylib"
	)
else()
	fmod_resolve_so(FMOD_CORE_RELEASE   "${FMOD_CORE_LIB_DIR}"   "libfmod")
	fmod_resolve_so(FMOD_CORE_DEBUG     "${FMOD_CORE_LIB_DIR}"   "libfmodL")
	fmod_resolve_so(FMOD_STUDIO_RELEASE "${FMOD_STUDIO_LIB_DIR}" "libfmodstudio")
	fmod_resolve_so(FMOD_STUDIO_DEBUG   "${FMOD_STUDIO_LIB_DIR}" "libfmodstudioL")

	set_target_properties(FMOD::Core PROPERTIES
		IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
		IMPORTED_LOCATION "${FMOD_CORE_RELEASE}"
		IMPORTED_LOCATION_DEBUG "${FMOD_CORE_DEBUG}"
		IMPORTED_LOCATION_RELEASE "${FMOD_CORE_RELEASE}"
	)
	set_target_properties(FMOD::Studio PROPERTIES
		IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
		IMPORTED_LOCATION "${FMOD_STUDIO_RELEASE}"
		IMPORTED_LOCATION_DEBUG "${FMOD_STUDIO_DEBUG}"
		IMPORTED_LOCATION_RELEASE "${FMOD_STUDIO_RELEASE}"
	)
endif()

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

# ---------------------------------------------------------------------------
# Runtime libraries
#
# The main CMakeLists copies dependencies with $<TARGET_RUNTIME_DLLS:...>,
# which resolves to nothing except on Windows — the same reason Steam ships its
# own copy helper. This mirrors that convention for FMOD.
#
# On Linux the copy has to rename: the linker records the library's SONAME
# (libfmod.so.14) in DT_NEEDED, not the file name it was given, so the loader
# looks for libfmod.so.14 at startup. Copying libfmod.so.14.14 under its own
# name would leave the executable unable to start. RPATH is set to $ORIGIN so
# the copies beside the binary are the ones found.
# ---------------------------------------------------------------------------
function(fmod_copy_runtime_libraries TARGET_NAME)
	if(WIN32)
		# Handled by $<TARGET_RUNTIME_DLLS:...> in the top-level CMakeLists.
		return()
	endif()

	set_target_properties("${TARGET_NAME}" PROPERTIES
		BUILD_RPATH "$ORIGIN"
		INSTALL_RPATH "$ORIGIN"
	)

	foreach(fmod_target IN ITEMS FMOD::Core FMOD::Studio)
		get_target_property(debug_lib "${fmod_target}" IMPORTED_LOCATION_DEBUG)
		get_target_property(release_lib "${fmod_target}" IMPORTED_LOCATION_RELEASE)

		# Both variants are copied. Which one the executable needs depends on the
		# configuration it was linked in, and copying both keeps a Debug and a
		# Release build in the same output directory from breaking each other.
		foreach(lib IN LISTS debug_lib release_lib)
			if(NOT lib OR NOT EXISTS "${lib}")
				continue()
			endif()

			if(APPLE)
				# dylibs carry no version suffix here; copy under their own name.
				get_filename_component(dest_name "${lib}" NAME)
			else()
				# Destination is the SONAME, which is what the loader will ask for.
				execute_process(
					COMMAND ${CMAKE_OBJDUMP} -p "${lib}"
					OUTPUT_VARIABLE objdump_out
					ERROR_QUIET
				)
				string(REGEX MATCH "SONAME[ \t]+([^ \t\r\n]+)" _match "${objdump_out}")
				if(CMAKE_MATCH_1)
					set(dest_name "${CMAKE_MATCH_1}")
				else()
					# No SONAME readable — fall back to the file's own name.
					get_filename_component(dest_name "${lib}" NAME)
				endif()
			endif()

			add_custom_command(
				TARGET "${TARGET_NAME}" POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
					"${lib}"
					"$<TARGET_FILE_DIR:${TARGET_NAME}>/${dest_name}"
				COMMENT "Copying ${dest_name} next to ${TARGET_NAME}"
			)
		endforeach()
	endforeach()
endfunction()
