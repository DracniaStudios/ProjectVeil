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
	-I src/engineLayer \
	-I thirdparty/raylib-6.0/src \
	-I "$JSON_INC" \
	-I "$FMOD_STUDIO_INC" \
	-I "$FMOD_CORE_INC" \
	tests/SoundFieldTests.cpp \
	src/gameLayer/Perception/SoundField.cpp \
	src/gameLayer/Components/Collider3D.cpp \
	-o "$BIN" \
	"$RAYLIB_LIB" -lX11 -lGL -lpthread -ldl -lrt -lm

echo
"$BIN"
SOUNDFIELD_STATUS=$?

# ---------------------------------------------------------------------------
# Collider tests
#
# Same cheap tier as the SoundField suite: Collider3D.cpp deliberately has no
# engine dependency, so it links against raylib alone and needs no window. If
# this block ever starts wanting game objects to link, something has included an
# engine header in the collider.
# ---------------------------------------------------------------------------
COLLIDER_BIN="$OUT_DIR/collider_tests"

echo
echo "building $COLLIDER_BIN"
g++ -std=c++23 -Wall \
	-I src/gameLayer \
	-I src/engineLayer \
	-I thirdparty/raylib-6.0/src \
	-I "$JSON_INC" \
	tests/ColliderTests.cpp \
	src/gameLayer/Components/Collider3D.cpp \
	-o "$COLLIDER_BIN" \
	"$RAYLIB_LIB" -lX11 -lGL -lpthread -ldl -lrt -lm

echo
"$COLLIDER_BIN"
COLLIDER_STATUS=$?

# ---------------------------------------------------------------------------
# Stalker FSM tests
#
# These need the engine. Stalker::update runs Entity::update ->
# GameObject::update -> RigidBody3D::UpdateForce, and the emitter tests drive
# Scene and RigidBody3D directly; both reach SceneManager::currentScene. Rather
# than restructure the build into a library (which would risk the MSVC build
# that cannot be verified from here), they link against the objects CMake
# already produced, minus the translation unit holding main.
#
# Requires the game to have been built: cmake --build <build-dir>
# ---------------------------------------------------------------------------
OBJ_ROOT="$BUILD_DIR/CMakeFiles/ProjectVeil.dir"
if [[ ! -d "$OBJ_ROOT" ]]; then
	echo
	echo "skipping engine-linked suites: no compiled objects under '$OBJ_ROOT'." >&2
	echo "                               build the game first: cmake --build $BUILD_DIR" >&2
	exit $SOUNDFIELD_STATUS
fi

# Every object except the one defining main. Detected rather than hardcoded, so
# moving the entry point does not silently produce a duplicate-main link error.
GAME_OBJECTS=()
while IFS= read -r obj; do
	if nm -C "$obj" 2>/dev/null | grep -qE '^[0-9a-f]+ T main$'; then continue; fi
	GAME_OBJECTS+=("$obj")
done < <(find "$OBJ_ROOT" -name '*.o')

if [[ ${#GAME_OBJECTS[@]} -eq 0 ]]; then
	echo "error: no linkable game objects found under '$OBJ_ROOT'." >&2
	exit 1
fi

# The game links these too; the FSM test pulls in enough of the engine to need
# the same set.
IMGUI_LIBS=()
while IFS= read -r lib; do IMGUI_LIBS+=("$lib"); done < <(find "$BUILD_DIR" -name '*.a' ! -name 'libraylib.a')

# Pick FMOD by host architecture. A bare find walks every arch directory and
# will happily hand the linker an arm64 .so on an x86_64 box.
case "$(uname -m)" in
	x86_64)          FMOD_ARCH_DIR=linux_x86_64 ;;
	aarch64|arm64)   FMOD_ARCH_DIR=linux_arm64 ;;
	arm*)            FMOD_ARCH_DIR=linux_arm ;;
	i?86)            FMOD_ARCH_DIR=linux_x86 ;;
	*) echo "error: unsupported architecture $(uname -m)" >&2; exit 1 ;;
esac

# The shipped libfmod.so / libfmod.so.14 symlinks are committed as zero-byte
# files, so -size +0 is what finds the real versioned library.
FMOD_CORE_LIB="$(find "thirdparty/fmod-2.3.14/core/lib/$FMOD_ARCH_DIR" -name 'libfmodL.so.*' -size +0 -print -quit)"
FMOD_STUDIO_LIB="$(find "thirdparty/fmod-2.3.14/studio/lib/$FMOD_ARCH_DIR" -name 'libfmodstudioL.so.*' -size +0 -print -quit)"

# Both suites link identically, so they are built and run from one loop: a new
# engine-linked suite is a line here rather than another copy of the g++ call.
ENGINE_STATUS=0
for suite in "StalkerFsmTests:stalker_fsm_tests" "EmitterTests:emitter_tests" \
             "TaskStationTests:task_station_tests"; do
	SRC="tests/${suite%%:*}.cpp"
	TEST_BIN="$OUT_DIR/${suite##*:}"

	echo
	echo "building $TEST_BIN"
	g++ -std=c++23 \
		-I src/gameLayer \
		-I src/engineLayer \
		-I src/platform \
		-I thirdparty/raylib-6.0/src \
		-I "$JSON_INC" \
		-I "$FMOD_STUDIO_INC" \
		-I "$FMOD_CORE_INC" \
		-I thirdparty/imgui-docking/imgui \
		-I thirdparty/rlImgui \
		-I thirdparty/FastNoise2-1.1.1/include \
		-I thirdparty/steam-1.64/public \
		-DRESOURCES_PATH=\"$ROOT/resources/\" \
		"$SRC" \
		"${GAME_OBJECTS[@]}" \
		"${IMGUI_LIBS[@]}" \
		"$RAYLIB_LIB" \
		"$FMOD_CORE_LIB" "$FMOD_STUDIO_LIB" \
		-o "$TEST_BIN" \
		-Wl,-rpath,"$ROOT/$BUILD_DIR/game" \
		-lX11 -lGL -lpthread -ldl -lrt -lm

# ---------------------------------------------------------------------------
# Collider mode tests
#
# The shape maths is covered by collider_tests above, which needs nothing but
# raylib. What lives in RigidBody3D::resolveConstrains -- collision pushes,
# trigger does not -- needs real game objects, so this one links the engine on
# the same terms as the FSM tests and reuses everything they discovered.
# ---------------------------------------------------------------------------
TRIGGER_BIN="$OUT_DIR/trigger_tests"
echo
echo "building $TRIGGER_BIN"
g++ -std=c++23 \
	-I src/gameLayer \
	-I src/engineLayer \
	-I src/platform \
	-I thirdparty/raylib-6.0/src \
	-I "$JSON_INC" \
	-I "$FMOD_STUDIO_INC" \
	-I "$FMOD_CORE_INC" \
	-I thirdparty/imgui-docking/imgui \
	-I thirdparty/rlImgui \
	-I thirdparty/FastNoise2-1.1.1/include \
	-I thirdparty/steam-1.64/public \
	-DRESOURCES_PATH=\"$ROOT/resources/\" \
	tests/TriggerTests.cpp \
	"${GAME_OBJECTS[@]}" \
	"${IMGUI_LIBS[@]}" \
	"$RAYLIB_LIB" \
	"$FMOD_CORE_LIB" "$FMOD_STUDIO_LIB" \
	-o "$TRIGGER_BIN" \
	-Wl,-rpath,"$ROOT/$BUILD_DIR/game" \
	-lX11 -lGL -lpthread -ldl -lrt -lm

echo
if [[ -z "${DISPLAY:-}" ]] && command -v xvfb-run >/dev/null 2>&1; then
	xvfb-run -a --server-args="-screen 0 640x480x24" "$TRIGGER_BIN"
else
	"$TRIGGER_BIN"
fi
TRIGGER_STATUS=$?

if [[ $SOUNDFIELD_STATUS -ne 0 || $COLLIDER_STATUS -ne 0 || $FSM_STATUS -ne 0 || $TRIGGER_STATUS -ne 0 ]]; then exit 1; fi
	echo
	# GameObject's constructor needs a GL context, so a display is required.
	# xvfb-run supplies one on a headless machine; a real session already has one.
	# Failures are collected rather than fatal, so one red suite does not hide
	# the state of the next.
	if [[ -z "${DISPLAY:-}" ]] && command -v xvfb-run >/dev/null 2>&1; then
		xvfb-run -a --server-args="-screen 0 640x480x24" "$TEST_BIN" || ENGINE_STATUS=1
	else
		"$TEST_BIN" || ENGINE_STATUS=1
	fi
done

if [[ $SOUNDFIELD_STATUS -ne 0 || $ENGINE_STATUS -ne 0 ]]; then exit 1; fi
