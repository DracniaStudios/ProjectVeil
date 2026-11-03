# CMakeList.txt : CMake project for ProjectVeil, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.28)

set(CMAKE_CXX_STANDARD 23)

set(PRODUCTION_BUILD OFF CACHE BOOL "Production Build" FORCE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/game")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/game")

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

if (MSVC)
	set(CMAKE_MSVC_RUNTINE_LIBRARY "<MultiThreaded$<$<CONFIG:Debug>:Debug>")
	set(CMAKE_MSVC_RUNTINE_LIBRARY "<MultiThreaded$<$<CONFIG:Release>:Release>")
	add_compile_options(arch/AVX2)
else()
	#add_compile_options(0mavx2 -CMakeFindDependencyMacro)
endif()

include (CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if (ipo_supported)
	message (STATUS "IPO / LTO enabled")
	if (PRODUCTION_BUILD)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)# LINK time optimization
	else()
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
	endif()
else()
	message(WARNING "IPO / LTO not supported: ${ipo_error}")
endif()

# Static Link Libraries
set (BUILD_SHARED_LIBS OFF CACHE BOOL "Build Libraries As Static" FORCE)

project ("ProjectVeil")

#Add sub directories
add_subdirectory(thirdparty/raylib-6.0)
add_subdirectory(thirdparty/imgui-docking)
add_subdirectory(thirdparty/rlImgui)
add_subdirectory(thirdparty/FastNoise2-1.1.1)
add_subdirectory(thirdparty/json-3.12.0) #Soon to me Customizable
add_subdirectory(thirdparty/fmod-2.3.14)
add_subdirectory(thirdparty/steam-1.64)
add_subdirectory(thirdparty/entt-4.0.0) # Header only ECS, exposes EnTT::EnTT
# thirdparty/flecs-4.1.6 stays vendored but is not built or linked: EnTT is the
# ECS (MultiplayerSplitPlan.md Q45)
add_subdirectory(thirdparty/serialize-2.1.3) # Header only bitpacked wire serialization, exposes serialize::serialize

# GameNetworkingSockets needs protobuf. If CMake cannot find one it is fetched at
# configure time (see thirdparty/GameNetworkingSockets-1.6.1/CMakeLists.txt), so
# turn this off to configure without network access.
option(VEIL_BUILD_GNS "Build GameNetworkingSockets" ON)
if (VEIL_BUILD_GNS)
	add_subdirectory(thirdparty/GameNetworkingSockets-1.6.1)
endif()

file (GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# The development multiplayer menu and switches: Start Server, Join, --host,
# --join (MultiplayerSplitPlan.md N7). Never in a production build (Q5, Q12).
option(VEIL_DEV_NET "Development multiplayer menu (Start Server / Join Server)" ON)

# Without GameNetworkingSockets there is no GnsTransport: single-player still
# works through Loopback, and Start Server / Join say why they can't run
if (NOT TARGET GameNetworkingSockets::static)
	list(FILTER MY_SOURCES EXCLUDE REGEX "/src/net/gns/")
endif()

# Add source to this project's executable.
add_executable("${PROJECT_NAME}")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET "${PROJECT_NAME}" PROPERTY CXX_STANDARD 23)
endif()

# Define Absolute Project Path
if (PRODUCTION_BUILD)
	# Setup the ASSETS_PATH macro to the final assets directory when you share the project
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="./resources/")
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=1)
else()
	# This is useful to get an ASSETS_PATH in your IDE during development but you should comment this if you compile a release version
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/")
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=0)
endif()

# Add Sources To Executable
target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES})

if (VEIL_DEV_NET AND NOT PRODUCTION_BUILD)
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PRIVATE VEIL_DEV_NET=1)
endif()

# Removes Terminal/Console from Production Build
if(MSVC) # If using the VS compiler...

	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC _CRT_SECURE_NO_WARNINGS)

	if(PRODUCTION_BUILD)
		#YOU CAN REMOVE THE CONSOLE WITH THIS LINE!
		set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup") #no console
	endif()

elseif(MINGW) # Windows / MinGW GCC/Clang
	if(PRODUCTION_BUILD)
		# Remove console on Windows under MinGW
		target_link_options("${CMAKE_PROJECT_NAME}" PRIVATE
			-mwindows
		)
	endif()

elseif(APPLE OR UNIX) # macOS / Linux
	if(PRODUCTION_BUILD)
		target_link_options("${CMAKE_PROJECT_NAME}" PRIVATE -s)
	endif()
endif()

# Add Mass Unit System
# src/ itself, so the networking split's folders are named in every include:
# <sim/...>, <net/...>, <Client/...> (MultiplayerSplitPlan.md)
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/Assets")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/Audio")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/Editor")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/InputSystem")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/Lighting")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/Memory")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/ParticleSystem")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/Physics3D")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/Save")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/SceneManagement")

target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/")

target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE 
raylib_static 
imgui 
rlimgui 
FastNoise2 
nlohmann_json 
FMOD
STEAM
EnTT::EnTT
serialize::serialize)

# cpp-httplib (header only, MIT): the Host's HTTP line to SpacetimeDB. Only
# src/net/spacetime/ may include it (rule R8, MultiplayerSplitPlan.md Q54).
target_include_directories("${CMAKE_PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/cpp-httplib-0.64.0")
if (WIN32)
	target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE ws2_32) # cpp-httplib sockets, with or without GNS
endif()

# Only defined when GameNetworkingSockets could be built on this machine
if (TARGET GameNetworkingSockets::static)
	target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE GameNetworkingSockets::static)
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PRIVATE VEIL_HAS_GNS=1)
endif()


# TODO: Add tests and install targets if needed.

# Automate the copying of resources to the build directory
add_custom_command(
	TARGET "${CMAKE_PROJECT_NAME}" POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/resources"
		"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources"
)

# Copy shared library dependencies (e.g. fmod.dll/fmodL.dll) next to the executable
add_custom_command(
	TARGET "${CMAKE_PROJECT_NAME}" POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy -t
		"$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>"
		"$<TARGET_RUNTIME_DLLS:${CMAKE_PROJECT_NAME}>"
	COMMAND_EXPAND_LISTS
)

# Copy the Steam libraries for the platform being built (defined by
# thirdparty/steam-1.64). TARGET_RUNTIME_DLLS above only resolves DLLs on
# Windows, so Steam handles its own copying to stay cross platform.
steam_copy_runtime_libraries("${CMAKE_PROJECT_NAME}")

# Same story for FMOD: the .so/.dylib libraries are invisible to
# TARGET_RUNTIME_DLLS, and on Linux they must be copied under their SONAME
# rather than their file name for the loader to find them at startup.
fmod_copy_runtime_libraries("${CMAKE_PROJECT_NAME}")