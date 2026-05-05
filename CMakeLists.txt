# CMakeList.txt : CMake project for ProjectVeil, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 4.0)

set(CMAKE_CXX_STANDARD 23)#Soon to be 26

set(PRODUCTION_BUILD OFF CACHE BOOL "Production Build" FORCE)

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


if (PRODUCTION_BUILD)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)# LINK time optimization
else()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
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

file (GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

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

target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/gameLayer/")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/")

target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE raylib_static imgui rlimgui FastNoise2 nlohmann_json)
# TODO: Add tests and install targets if needed.