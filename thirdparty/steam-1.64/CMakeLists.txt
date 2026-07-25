cmake_minimum_required(VERSION 3.28)
project(STEAM)

# Steamworks SDK 1.64
#
# Layout of the SDK as Valve ships it:
#   public/steam                - headers, included as <steam/steam_api.h>
#   public/steam/lib/<plat>     - encrypted app ticket library (optional API)
#   redistributable_bin/<plat>  - steam_api runtime + Windows import libs
#                                 (the 32 bit Windows binaries sit in the root)
#
# Steam only ships shared libraries, so the runtime library must end up next to
# the executable. steam_copy_runtime_libraries() at the bottom of this file wires
# that up for a given target; the top level CMakeLists calls it for the game.

set(INCLUDE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/public")
set(REDIST_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/redistributable_bin")
set(APPTICKET_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/public/steam/lib")

# Pick the library files for the platform we are building for
if(WIN32)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(STEAM_API_RUNTIME "${REDIST_ROOT_DIR}/win64/steam_api64.dll")
		set(STEAM_API_IMPLIB "${REDIST_ROOT_DIR}/win64/steam_api64.lib")
		set(STEAM_APPTICKET_RUNTIME "${APPTICKET_ROOT_DIR}/win64/sdkencryptedappticket64.dll")
		set(STEAM_APPTICKET_IMPLIB "${APPTICKET_ROOT_DIR}/win64/sdkencryptedappticket64.lib")
	else()
		# 32 bit Windows binaries live in the root of redistributable_bin
		set(STEAM_API_RUNTIME "${REDIST_ROOT_DIR}/steam_api.dll")
		set(STEAM_API_IMPLIB "${REDIST_ROOT_DIR}/steam_api.lib")
		set(STEAM_APPTICKET_RUNTIME "${APPTICKET_ROOT_DIR}/win32/sdkencryptedappticket.dll")
		set(STEAM_APPTICKET_IMPLIB "${APPTICKET_ROOT_DIR}/win32/sdkencryptedappticket.lib")
	endif()
elseif(APPLE)
	# A single universal binary, no per architecture split
	set(STEAM_API_RUNTIME "${REDIST_ROOT_DIR}/osx/libsteam_api.dylib")
	set(STEAM_APPTICKET_RUNTIME "${APPTICKET_ROOT_DIR}/osx/libsdkencryptedappticket.dylib")
elseif(ANDROID)
	# No encrypted app ticket library is shipped for Android
	set(STEAM_API_RUNTIME "${REDIST_ROOT_DIR}/androidarm64/libsteam_api.so")
elseif(UNIX)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)")
		set(STEAM_PLATFORM_DIR "linuxarm64")
	elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(STEAM_PLATFORM_DIR "linux64")
	else()
		set(STEAM_PLATFORM_DIR "linux32")
	endif()

	set(STEAM_API_RUNTIME "${REDIST_ROOT_DIR}/${STEAM_PLATFORM_DIR}/libsteam_api.so")
	set(STEAM_APPTICKET_RUNTIME "${APPTICKET_ROOT_DIR}/${STEAM_PLATFORM_DIR}/libsdkencryptedappticket.so")
else()
	message(FATAL_ERROR "Steamworks SDK: unsupported platform '${CMAKE_SYSTEM_NAME}'")
endif()

if(NOT EXISTS "${STEAM_API_RUNTIME}")
	message(FATAL_ERROR "Steamworks SDK: missing '${STEAM_API_RUNTIME}'")
endif()

# steam_api: the library everything in the Steamworks API goes through
add_library(Steam::API SHARED IMPORTED GLOBAL)
set_target_properties(Steam::API PROPERTIES
	IMPORTED_LOCATION "${STEAM_API_RUNTIME}"
)

if(WIN32)
	set_target_properties(Steam::API PROPERTIES IMPORTED_IMPLIB "${STEAM_API_IMPLIB}")
elseif(UNIX AND NOT APPLE)
	# Valve ships these without a DT_SONAME. Without this CMake would bake the
	# absolute build tree path into the executable instead of looking the
	# library up next to it at runtime.
	set_target_properties(Steam::API PROPERTIES IMPORTED_NO_SONAME TRUE)
endif()

set(STEAM_RUNTIME_LIBRARIES "${STEAM_API_RUNTIME}")

# Encrypted app ticket API (unused today but kept available)
if(STEAM_APPTICKET_RUNTIME AND EXISTS "${STEAM_APPTICKET_RUNTIME}")
	add_library(Steam::EncryptedAppTicket SHARED IMPORTED GLOBAL)
	set_target_properties(Steam::EncryptedAppTicket PROPERTIES
		IMPORTED_LOCATION "${STEAM_APPTICKET_RUNTIME}"
	)

	if(WIN32)
		set_target_properties(Steam::EncryptedAppTicket PROPERTIES
			IMPORTED_IMPLIB "${STEAM_APPTICKET_IMPLIB}"
		)
	elseif(UNIX AND NOT APPLE)
		set_target_properties(Steam::EncryptedAppTicket PROPERTIES IMPORTED_NO_SONAME TRUE)
	endif()

	list(APPEND STEAM_RUNTIME_LIBRARIES "${STEAM_APPTICKET_RUNTIME}")
endif()

# The copy helper below runs in the caller's scope, where the variables of this
# directory are not visible, so hand the file list over as a global property.
set_property(GLOBAL PROPERTY STEAM_RUNTIME_LIBRARIES "${STEAM_RUNTIME_LIBRARIES}")

# Umbrella for collecting different APIs
add_library(STEAM INTERFACE)

target_include_directories(STEAM INTERFACE
	"${INCLUDE_ROOT_DIR}"
	"${INCLUDE_ROOT_DIR}/steam"
)

target_link_libraries(STEAM INTERFACE
	Steam::API
)

if(TARGET Steam::EncryptedAppTicket)
	target_link_libraries(STEAM INTERFACE Steam::EncryptedAppTicket)
endif()

# steam_copy_runtime_libraries(<target>)
#
# Copies the Steam shared libraries for the current platform next to <target>'s
# executable after every build, and on Linux/macOS makes the executable look for
# them there. Steam has no static library, so this is required to run the game
# from the build tree, not a convenience.
function(steam_copy_runtime_libraries TARGET_NAME)
	if(NOT TARGET ${TARGET_NAME})
		message(FATAL_ERROR "steam_copy_runtime_libraries: '${TARGET_NAME}' is not a target")
	endif()

	get_property(STEAM_LIBS GLOBAL PROPERTY STEAM_RUNTIME_LIBRARIES)

	add_custom_command(
		TARGET ${TARGET_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${STEAM_LIBS}
			"$<TARGET_FILE_DIR:${TARGET_NAME}>"
		COMMENT "Copying Steam runtime libraries next to ${TARGET_NAME}"
		VERBATIM
	)

	# Resolve the copied libraries from the executable's own directory
	if(APPLE)
		set_property(TARGET ${TARGET_NAME} APPEND PROPERTY BUILD_RPATH "@loader_path")
		set_property(TARGET ${TARGET_NAME} APPEND PROPERTY INSTALL_RPATH "@loader_path")
	elseif(UNIX)
		set_property(TARGET ${TARGET_NAME} APPEND PROPERTY BUILD_RPATH "$ORIGIN")
		set_property(TARGET ${TARGET_NAME} APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
	endif()
endfunction()
