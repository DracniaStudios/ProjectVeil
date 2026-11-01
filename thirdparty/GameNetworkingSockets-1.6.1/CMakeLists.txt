cmake_minimum_required(VERSION 3.28)
project(GameNetworkingSockets C CXX)

# GameNetworkingSockets 1.6.1
#
# Valve's own top level CMakeLists.txt and cmake/ folder are not vendored, only
# include/, src/ and tests/. src/CMakeLists.txt holds the real target and source
# lists and is used as is; this file stands in for the parts of the upstream top
# level that it depends on:
#   - the option variables it reads (static library only, no tests/tools/ICE)
#   - set_target_common_gns_properties() and add_sanitizers()
#   - the crypto backend choice and the global definitions
#   - provisioning protobuf, which GNS cannot be built without
#
# Link against GameNetworkingSockets::static. If a prerequisite cannot be met this
# file warns and returns without defining that target, so the top level CMakeLists
# only links it when it exists.
#
# Including GNS: use <gns/steamnetworkingsockets.h>, not <steam/...>.
# GNS and the Steamworks SDK (thirdparty/steam-1.64) both ship headers under steam/
# that share file names (steamnetworkingtypes.h, isteamnetworkingsockets.h,
# steamtypes.h, ...) but differ in content, and they use the same include guards.
# With both on the include path, <steam/isteamnetworkingutils.h> silently resolves
# to whichever directory comes first and the other copy is then skipped. So the
# raw steam/ directory is not exposed to consumers; the headers are reachable as
# <gns/...> instead (see the end of this file). Even so, keep code that includes
# the GNS headers and code that includes the Steamworks headers in separate
# translation units, since the two declare the same types.

include(FetchContent)

if(NOT (WIN32 OR CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$"))
	message(WARNING "GameNetworkingSockets: unsupported platform '${CMAKE_SYSTEM_NAME}' - building without it")
	return()
endif()

# ---------------------------------------------------------------------------
# Crypto
#
# Windows has BCrypt built in, so no OpenSSL is needed there. BCrypt has no
# ed25519/curve25519, so those come from the reference implementation in
# src/external. Everywhere else both go through OpenSSL (1.1.1 or newer).
# ---------------------------------------------------------------------------
if(WIN32)
	set(USE_CRYPTO "BCrypt")
	set(USE_CRYPTO25519 "Reference")
else()
	find_package(OpenSSL)
	if(NOT OPENSSL_FOUND)
		message(WARNING "GameNetworkingSockets: OpenSSL not found - building without it")
		return()
	endif()
	set(USE_CRYPTO "OpenSSL")
	set(USE_CRYPTO25519 "OpenSSL")
endif()

# ---------------------------------------------------------------------------
# protobuf
#
# GNS compiles its wire messages with protoc and links libprotobuf. Use an
# installed copy if CMake can find one (vcpkg, a distro package). Otherwise build
# protobuf 3.21.12 from source, the last release without an abseil dependency,
# fetched once into the build tree and checked against a pinned hash.
#
# src/CMakeLists.txt then calls find_package(Protobuf CONFIG) and
# protobuf_generate_cpp() itself. An in-tree build exports neither (they only
# exist in the install-time templates), so a small config package is assembled
# from protobuf's own templates and pointed at with Protobuf_DIR.
# ---------------------------------------------------------------------------
set(GNS_PROTOBUF_SHIM_DIR "${CMAKE_CURRENT_BINARY_DIR}/protobuf-shim")

# A previous configure leaves Protobuf_DIR cached pointing at the shim below. That
# is not an installed protobuf; forget it so the probe only sees real ones.
if(Protobuf_DIR STREQUAL GNS_PROTOBUF_SHIM_DIR)
	unset(Protobuf_DIR CACHE)
endif()

find_package(Protobuf QUIET CONFIG)

if(NOT Protobuf_FOUND)
	FetchContent_Declare(protobuf
		URL https://github.com/protocolbuffers/protobuf/releases/download/v21.12/protobuf-cpp-3.21.12.tar.gz
		URL_HASH SHA256=4eab9b524aa5913c6fffb20b2a8abf5ef7f95a80bc0701f3a6dbb4c607f73460
		DOWNLOAD_EXTRACT_TIMESTAMP TRUE
		EXCLUDE_FROM_ALL
	)

	# protobuf's own option() calls ignore normal variables, so these have to be cache entries
	set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
	set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
	set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)
	set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
	# Match the game's dynamic CRT; protobuf defaults to /MT, which fails to link against /MD
	set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)

	FetchContent_MakeAvailable(protobuf)

	file(READ "${protobuf_SOURCE_DIR}/cmake/protobuf-config.cmake.in" GNS_PROTOBUF_CONFIG)
	# Drop what only makes sense for an installed package: the zlib probe and the
	# includes of the options and exported-targets files. The targets already exist.
	string(REPLACE "@_protobuf_FIND_ZLIB@" "" GNS_PROTOBUF_CONFIG "${GNS_PROTOBUF_CONFIG}")
	string(REGEX REPLACE
		"include\\(\"\\$\\{CMAKE_CURRENT_LIST_DIR\\}/protobuf-(options|targets)\\.cmake\"\\)" ""
		GNS_PROTOBUF_CONFIG "${GNS_PROTOBUF_CONFIG}")
	file(WRITE "${GNS_PROTOBUF_SHIM_DIR}/protobuf-config.cmake"
		"set(protobuf_MODULE_COMPATIBLE ON)\n${GNS_PROTOBUF_CONFIG}")
	# Keep only the protobuf_generate_cpp() definitions from the FindProtobuf
	# compatibility module. Everything after the "# Environment" marker looks the
	# libraries up by LOCATION, which CMake rejects for in-tree targets.
	file(READ "${protobuf_SOURCE_DIR}/cmake/protobuf-module.cmake.in" GNS_PROTOBUF_MODULE)
	string(FIND "${GNS_PROTOBUF_MODULE}" "# Environment" GNS_PROTOBUF_MODULE_CUT)
	if(GNS_PROTOBUF_MODULE_CUT EQUAL -1)
		message(FATAL_ERROR "GameNetworkingSockets: protobuf-module.cmake.in no longer has the expected layout")
	endif()
	string(SUBSTRING "${GNS_PROTOBUF_MODULE}" 0 ${GNS_PROTOBUF_MODULE_CUT} GNS_PROTOBUF_MODULE)
	# GNS's .proto files import each other by bare file name, so protoc needs the
	# directory of every proto on its include path
	file(WRITE "${GNS_PROTOBUF_SHIM_DIR}/protobuf-module.cmake"
		"${GNS_PROTOBUF_MODULE}\nif(NOT DEFINED PROTOBUF_GENERATE_CPP_APPEND_PATH)\n  set(PROTOBUF_GENERATE_CPP_APPEND_PATH TRUE)\nendif()\n")
	file(WRITE "${GNS_PROTOBUF_SHIM_DIR}/protobuf-config-version.cmake"
		"set(PACKAGE_VERSION_COMPATIBLE TRUE)\n")
	set(Protobuf_DIR "${GNS_PROTOBUF_SHIM_DIR}" CACHE PATH "" FORCE)

	# Tells set_target_common_gns_properties() not to define PROTOBUF_USE_DLLS on MSVC
	set(Protobuf_USE_STATIC_LIBS ON)
endif()

# ---------------------------------------------------------------------------
# What upstream's top level provides to src/CMakeLists.txt
#
# Plain variables rather than option()/cache entries, so nothing leaks into the
# cache under generic names like BUILD_TESTS.
# ---------------------------------------------------------------------------
set(GNS_VERSION "1.6.1") # only stamps Apple framework bundles
set(BUILD_STATIC_LIB ON)
set(BUILD_SHARED_LIB OFF)
set(BUILD_FRAMEWORK OFF)
set(BUILD_TESTS OFF)
set(BUILD_TOOLS OFF)
set(USE_STEAMWEBRTC OFF)
set(WERROR OFF)
set(SANITIZE OFF)
set(SANITIZE_UNDEFINED OFF)
# Native ICE client for NAT-punched peer to peer. The game uses a client/server
# topology, so this is left out. Flip it on if P2P connections are ever needed.
set(ENABLE_ICE OFF)

# After the protobuf block on purpose: add_definitions() and CMAKE_CXX_FLAGS are
# inherited by every directory created below this point, and none of the following
# is meant for protobuf's own targets.
add_definitions(-DVALVE_CRYPTO_ENABLE_25519)
if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS)
	# Replaced per target below; leaving them in gives a command line warning on every file
	string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	string(REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# We always need at least SSE2 on x86
if(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64.*|x86_64.*|AMD64.*|i686.*|i386.*|x86.*")
	set(GNS_TARGET_ARCH_FLAGS "-msse2")
endif()

# Sanitizer support is not wired up; src/CMakeLists.txt calls this for every target
function(add_sanitizers)
endfunction()

function(set_target_common_gns_properties TGT)
	target_compile_definitions(${TGT} PRIVATE GOOGLE_PROTOBUF_NO_RTTI)

	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		# Lets the linker drop unused functions, the analog of /Gy on MSVC
		target_compile_options(${TGT} PRIVATE -ffunction-sections -fdata-sections ${GNS_TARGET_ARCH_FLAGS})
	endif()

	if(CMAKE_SYSTEM_NAME MATCHES "Linux")
		target_compile_definitions(${TGT} PUBLIC LINUX)
	elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
		target_compile_definitions(${TGT} PUBLIC OSX)
	elseif(WIN32)
		target_compile_definitions(${TGT} PUBLIC _WINDOWS)
		if(MSVC)
			if(NOT Protobuf_USE_STATIC_LIBS)
				target_compile_definitions(${TGT} PRIVATE PROTOBUF_USE_DLLS)
			endif()
			target_compile_options(${TGT} PRIVATE
				/EHs-c-   # No C++ exceptions
				# Warnings from protobuf's headers and the MSVC standard library that cannot be fixed here
				/wd4146 /wd4530 /wd4244 /wd4251 /wd4267
			)
			# No RTTI except in Debug, where assert_cast uses dynamic_cast
			target_compile_options(${TGT} PRIVATE $<IF:$<CONFIG:Debug>,/GR,/GR->)
		else()
			target_compile_definitions(${TGT} PRIVATE __STDC_FORMAT_MACROS=1 __USE_MINGW_ANSI_STDIO=0)
			target_compile_options(${TGT} PRIVATE -fno-stack-protector)
		endif()
	endif()

	if(NOT WIN32)
		target_compile_options(${TGT} PRIVATE -fstack-protector-strong)
	endif()

	set_target_properties(${TGT} PROPERTIES CXX_STANDARD 11)
endfunction()

# src/CMakeLists.txt defines an install(EXPORT) for the library, and CMake refuses
# it because libprotobuf (built in-tree, protobuf_INSTALL OFF) is not in an export
# set. Nothing here is ever installed, so skip the install rules for this tree.
set(CMAKE_SKIP_INSTALL_RULES ON)

add_subdirectory(src)

# Publish the public headers as <gns/...> instead of the upstream <steam/...>.
# Each forwarding header includes the real file by absolute path, and the public
# headers only include each other with "relative" quotes, so those still resolve
# next to the real file. GNS's own sources keep the raw include directory, which
# is on the target's private INCLUDE_DIRECTORIES; only what consumers see changes.
set(GNS_FORWARD_ROOT "${CMAKE_CURRENT_BINARY_DIR}/gns_include")
file(GLOB GNS_PUBLIC_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/steam/*.h")
foreach(GNS_HEADER IN LISTS GNS_PUBLIC_HEADERS)
	cmake_path(GET GNS_HEADER FILENAME GNS_HEADER_NAME)
	file(CONFIGURE OUTPUT "${GNS_FORWARD_ROOT}/gns/${GNS_HEADER_NAME}"
		CONTENT "#include \"${GNS_HEADER}\"\n" @ONLY NEWLINE_STYLE LF)
endforeach()
set_property(TARGET GameNetworkingSockets_s PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${GNS_FORWARD_ROOT}")
