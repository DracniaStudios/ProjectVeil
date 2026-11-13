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
set(VEIL_BACKEND "raylib" CACHE STRING "Window, input and renderer backend: raylib (fallback) or sdl3gl (SDL3 + OpenGL 3.3)")
set_property(CACHE VEIL_BACKEND PROPERTY STRINGS raylib sdl3gl)
if (NOT VEIL_BACKEND MATCHES "^(raylib|sdl3gl)$")
	message(FATAL_ERROR "VEIL_BACKEND must be raylib or sdl3gl, not '${VEIL_BACKEND}'")
endif()

add_subdirectory(thirdparty/imgui-docking)
if (VEIL_BACKEND STREQUAL "raylib")
	add_subdirectory(thirdparty/raylib-6.0)
	add_subdirectory(thirdparty/rlImgui)
else()
	# SDL3: static only, video and input only. Every name was checked against
	# thirdparty/SDL/CMakeLists.txt (SDL main, 3.5.0 development).
	foreach(_off SDL_SHARED SDL_TEST_LIBRARY SDL_TESTS SDL_EXAMPLES SDL_INSTALL SDL_AUDIO SDL_CAMERA
	             SDL_GPU SDL_RENDER SDL_VULKAN SDL_DIALOG SDL_SENSOR SDL_HAPTIC SDL_POWER)
		set(${_off} OFF CACHE BOOL "" FORCE)
	endforeach()
	set(SDL_STATIC ON CACHE BOOL "" FORCE)
	add_subdirectory(thirdparty/SDL EXCLUDE_FROM_ALL)

	# raylib's headers as vocabulary types only; raylib_static is never linked here
	add_library(raylib_headers INTERFACE)
	target_include_directories(raylib_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/raylib-6.0/src")
	# glad 2.0.8 (<glad/gl.h>), stb_image.h, stb_image_write.h and cgltf.h from the upstream
	# copies in thirdparty/ (ruling R-1), not raylib's bundled ones
	add_library(veil_gl_externals INTERFACE)
	target_include_directories(veil_gl_externals INTERFACE
		"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glad-2.0.8"
		"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb"
		"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/cgltf-1.15")

	add_library(imgui_backend_sdl3gl STATIC
		thirdparty/imgui-docking/imgui/backends/imgui_impl_sdl3.cpp
		thirdparty/imgui-docking/imgui/backends/imgui_impl_opengl3.cpp)
	target_include_directories(imgui_backend_sdl3gl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui-docking/imgui/backends")
	target_link_libraries(imgui_backend_sdl3gl PUBLIC imgui SDL3::SDL3-static)

	add_executable(veil_gl_probe tests/GlProbe.cpp)
	target_link_libraries(veil_gl_probe PRIVATE SDL3::SDL3-static veil_gl_externals)
endif()
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

# The game is only defined for the raylib backend until the SDL3 port reaches it (B-15
# removes this wrapper)
if (VEIL_BACKEND STREQUAL "raylib")

file (GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# One backend's files per build (B-04). Valid for every later backend work order.
if (VEIL_BACKEND STREQUAL "raylib")
	list(FILTER MY_SOURCES EXCLUDE REGEX "/backend_(sdl3|gl)/")
else()
	list(FILTER MY_SOURCES EXCLUDE REGEX "/backend_raylib/")
endif()

# The development multiplayer menu and switches: Start Server, Join, --host,
# --join (MultiplayerSplitPlan.md N7). Never in a production build (Q5, Q12).
option(VEIL_DEV_NET "Development multiplayer menu (Start Server / Join Server)" ON)

# Without GameNetworkingSockets there is no GnsTransport: single-player still
# works through Loopback, and Start Server / Join say why they can't run
if (NOT TARGET GameNetworkingSockets::static)
	list(FILTER MY_SOURCES EXCLUDE REGEX "/src/net/gns/")
endif()

# Simulation library (Review A, A-11; A-decision.md D2). Explicit list: adding a file
# to the simulation is a reviewed edit. Links no raylib, ImGui, FMOD, GNS or httplib.
# Keep this block self-contained: a later backend option (B-02) must not add a
# backend library, include folder or definition to veil_sim.
set(VEIL_SIM_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/sim/ServerWorld.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/sim/StationTable.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/sim/Presentation.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/sim/Worlds.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/net/Protocol.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/net/LoopbackTransport.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/SceneManagement/Scene.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/Save/SaveSystem.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/Save/SaveStore.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/World/WorldKey.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/World/FileWorldStore.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/ResourcePaths.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/randomStuff.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/gameMap.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/GameObject.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/Entity.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/Player.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/ContactRules.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/OverlayStack.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/Objects/InteractableObject.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/Components/Collider3D.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/Components/RigidBody3D.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/Components/PhysicsWorld.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/Components/PlayerActions.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/Components/PlayerCamera.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/AI/Stalker.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/AI/Director.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/Perception/SoundField.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/platform/asserts.cpp")
list(REMOVE_ITEM MY_SOURCES ${VEIL_SIM_SOURCES})
add_library(veil_sim STATIC ${VEIL_SIM_SOURCES})
set_property(TARGET veil_sim PROPERTY CXX_STANDARD 23)
target_include_directories(veil_sim PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/Physics3D"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/Save"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/engineLayer/SceneManagement"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/platform"
	"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/raylib-6.0/src")   # raylib.h, raymath.h: types and inline math only
target_link_libraries(veil_sim PUBLIC nlohmann_json EnTT::EnTT serialize::serialize)
target_compile_definitions(veil_sim PUBLIC PRODUCTION_BUILD=$<IF:$<BOOL:${PRODUCTION_BUILD}>,1,0>)
if (MSVC)
	target_compile_definitions(veil_sim PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()

# Add source to this project's executable.
add_executable("${PROJECT_NAME}")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET "${PROJECT_NAME}" PROPERTY CXX_STANDARD 23)
endif()

# No resource path is compiled in: the game finds resources/ and saves/ beside its
# executable at run time (src/engineLayer/ResourcePaths.h), so a build copied to
# another machine reads its own files.
if (PRODUCTION_BUILD)
	target_compile_definitions("${CMAKE_PROJECT_NAME}" PUBLIC PRODUCTION_BUILD=1)
else()
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
target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE veil_sim)

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

# Headless simulation test: links veil_sim and nothing else (A-decision.md D3)
option(VEIL_BUILD_SIM_TESTS "Build the headless simulation test (links veil_sim only)" ON)
if (VEIL_BUILD_SIM_TESTS)
	enable_testing()
	add_executable(veil_sim_headless_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/HeadlessSimTests.cpp")
	set_property(TARGET veil_sim_headless_tests PROPERTY CXX_STANDARD 23)
	target_link_libraries(veil_sim_headless_tests PRIVATE veil_sim)
	add_test(NAME veil_sim_headless COMMAND veil_sim_headless_tests)
	set_tests_properties(veil_sim_headless PROPERTIES ENVIRONMENT "VEIL_RESOURCES_DIR=${CMAKE_CURRENT_SOURCE_DIR}/resources")
endif()

endif() # VEIL_BACKEND STREQUAL "raylib"
