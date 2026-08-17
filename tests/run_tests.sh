#!/usr/bin/env bash
#
# Builds and runs the standalone perception tests.
#
# These are not part of the game binary on purpose. FMOD is vendored
# Windows-only (.dll/.lib, no .so), so the game cannot link on Linux and
# anything hosted inside it would be unrunnable there. SoundField references no
# FMOD symbol, so it links against raylib alone and actually executes.
#
# Requires a configured build directory for the raylib static library:
#   cmake --preset linux-debug
#
# Usage: tests/run_tests.sh [build-dir]        (default out/build/linux-debug)

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${1:-out/build/linux-debug}"

if [[ ! -d "$BUILD_DIR" ]]; then
	echo "error: build directory '$BUILD_DIR' not found." >&2
	echo "       run 'cmake --preset linux-debug' first." >&2
	exit 1
fi

RAYLIB_LIB="$(find "$BUILD_DIR" -name libraylib.a -print -quit)"
if [[ -z "$RAYLIB_LIB" ]]; then
	echo "error: libraylib.a not found under '$BUILD_DIR'." >&2
	echo "       build raylib first: cmake --build $BUILD_DIR" >&2
	exit 1
fi

# Header locations are discovered rather than hardcoded so a dependency bump
# does not silently break the tests.
JSON_INC="$(dirname "$(dirname "$(find thirdparty -path '*nlohmann/json.hpp' -print -quit)")")"
FMOD_STUDIO_INC="$(dirname "$(find thirdparty -name fmod_studio.hpp -print -quit)")"
FMOD_CORE_INC="$(dirname "$(find thirdparty -name fmod.hpp -print -quit)")"

OUT_DIR="$BUILD_DIR/tests"
mkdir -p "$OUT_DIR"
BIN="$OUT_DIR/soundfield_tests"

echo "building $BIN"
g++ -std=c++23 -Wall \
	-I src/gameLayer \
	-I thirdparty/raylib-6.0/src \
	-I "$JSON_INC" \
	-I "$FMOD_STUDIO_INC" \
	-I "$FMOD_CORE_INC" \
	tests/SoundFieldTests.cpp \
	src/gameLayer/Perception/SoundField.cpp \
	-o "$BIN" \
	"$RAYLIB_LIB" -lX11 -lGL -lpthread -ldl -lrt -lm

echo
"$BIN"
