cmake_minimum_required(VERSION 3.28)
project(FLECS LANGUAGES C)

# Flecs 4.1.6
#
# Layout of this folder:
#   include/   public headers, both the C API (flecs.h) and the C++ API
#              (flecs/addons/cpp), which is header only on top of the C library
#   src/       the C implementation, taken unmodified from the v4.1.6 tag
#
# Only the static library is built. The upstream CMakeLists.txt also builds a
# shared library, installs, and pulls in a warnings module (cmake/); none of that
# applies to a vendored static dependency, so it is replaced by this file.
#
# Link against flecs::flecs. The flecs_STATIC definition it carries is what stops
# FLECS_API (include/flecs/bake_config.h) from resolving to __declspec(dllimport)
# on Windows, so consumers must not set up the include path by hand.

file(GLOB_RECURSE FLECS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

if(NOT FLECS_SOURCES)
	message(FATAL_ERROR
		"Flecs: no sources found in '${CMAKE_CURRENT_SOURCE_DIR}/src'. "
		"Only the headers are vendored without it, and Flecs is a compiled library.")
endif()

add_library(flecs STATIC ${FLECS_SOURCES})
add_library(flecs::flecs ALIAS flecs)

target_include_directories(flecs PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(flecs PUBLIC flecs_STATIC)

# The implementation includes its own private headers by bare name
target_include_directories(flecs PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# The C++ API's templates are compiled into the game, so the static library has
# to agree with the game on the C runtime. PIC only matters off Windows, but is
# what makes the archive linkable into a shared object later.
set_target_properties(flecs PROPERTIES
	C_STANDARD 99
	C_EXTENSIONS ON
	POSITION_INDEPENDENT_CODE ON
)

if(WIN32)
	# The HTTP/REST addon uses winsock; stack traces use dbghelp
	target_link_libraries(flecs PUBLIC wsock32 ws2_32 dbghelp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	target_link_libraries(flecs PUBLIC pthread m)
endif()
