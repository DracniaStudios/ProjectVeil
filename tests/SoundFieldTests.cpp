/**
 * Standalone tests for the sound-only perception layer.
 *
 * Deliberately a separate binary rather than a mode of the game. The game
 * cannot link on Linux — FMOD is vendored Windows-only (.dll/.lib, no .so) —
 * so anything hosted inside it is unrunnable on this platform and "it
 * compiles" would be the only verification available. SoundField touches no
 * FMOD symbol, so it can be linked against raylib alone and actually run.
 *
 * Occlusion is exercised with a null GameMap, which skips the raycast path.
 * Occlusion needs real world geometry and belongs in the in-game inspector
 * pass, not here.
 *
 * Build and run (from the repo root, after configuring linux-debug):
 *
 *   g++ -std=c++23 -I src/gameLayer -I thirdparty/raylib-6.0/src \
 *       tests/SoundFieldTests.cpp src/gameLayer/Perception/SoundField.cpp \
 *       -o out/soundfield_tests \
 *       -L out/build/linux-debug/thirdparty/raylib-6.0/raylib -lraylib -lm
 *   ./out/soundfield_tests
 */

#include <Perception/SoundField.h>

#include <raymath.h>

#include <cmath>
#include <cstdio>
#include <string>

static int g_failures = 0;
static int g_checks = 0;

static void Check(bool condition, const std::string& what)
{
	++g_checks;
	if (!condition)
	{
		++g_failures;
		std::printf("  FAIL  %s\n", what.c_str());
	}
}

static void CheckNear(float actual, float expected, float tolerance, const std::string& what)
{
	++g_checks;
	if (std::fabs(actual - expected) > tolerance)
	{
		++g_failures;
		std::printf("  FAIL  %s (expected ~%.4f, got %.4f)\n", what.c_str(), expected, actual);
	}
}

// A noise at the listener's own position, so distance falloff is out of play.
static void EmitAt(SoundField& field, Vector3 position, float loudness, std::uint64_t sourceId = 0)
{
	field.Emit(position, loudness, SOUND_FOOTSTEP, sourceId);
}

static void TestEmitAndHear()
{
	std::printf("emit and hear\n");
	SoundField field;
	EmitAt(field, Vector3{ 0, 0, 0 }, 1.0f);

	SoundEvent heard = {};
	float loudness = 0.0f;
	const bool found = field.LoudestAudible(Vector3{ 0, 0, 0 }, 0.0f, 0, nullptr, heard, loudness);

	Check(found, "a fresh emission is audible at its own position");
	CheckNear(loudness, 1.0f, 0.001f, "loudness at zero distance is unattenuated");
}

static void TestSilentEmissionIgnored()
{
	std::printf("zero-loudness emission is dropped\n");
	SoundField field;
	EmitAt(field, Vector3{ 0, 0, 0 }, 0.0f);
	Check(field.Events().empty(), "a zero-loudness emission is not recorded");
}

static void TestDistanceFalloff()
{
	std::printf("distance falloff\n");
	SoundField field;
	EmitAt(field, Vector3{ 0, 0, 0 }, 1.0f);

	const SoundEvent& event = field.Events().front();

	// The contract in the header: exactly half loudness at kHalfDistance.
	const float atHalf = field.AudibleLoudnessAt(
		Vector3{ SoundField::kHalfDistance, 0, 0 }, event, nullptr);
	CheckNear(atHalf, 0.5f, 0.001f, "loudness is halved at kHalfDistance");

	const float atZero = field.AudibleLoudnessAt(Vector3{ 0, 0, 0 }, event, nullptr);
	const float atFar = field.AudibleLoudnessAt(Vector3{ 100, 0, 0 }, event, nullptr);
	Check(atZero > atHalf && atHalf > atFar, "loudness decreases monotonically with distance");
	Check(atFar > 0.0f, "falloff approaches zero without reaching it (no divergence)");
}

static void TestExpiry()
{
	std::printf("expiry after TTL\n");
	SoundField field;
	EmitAt(field, Vector3{ 0, 0, 0 }, 1.0f);
	Check(field.Events().size() == 1, "event recorded");

	// Just short of the TTL it must still be there: going quiet should not pay
	// off instantly.
	field.Update(SoundField::kEventTTL - 0.1f);
	Check(field.Events().size() == 1, "event survives until its TTL");

	field.Update(0.2f);
	Check(field.Events().empty(), "event expires once past its TTL");

	SoundEvent heard = {};
	float loudness = 0.0f;
	Check(!field.LoudestAudible(Vector3{ 0, 0, 0 }, 0.0f, 0, nullptr, heard, loudness),
		"nothing is audible after expiry");
}

static void TestIgnoresOwnSource()
{
	std::printf("listener ignores its own emissions\n");
	SoundField field;
	constexpr std::uint64_t kSelf = 42;
	EmitAt(field, Vector3{ 0, 0, 0 }, 1.0f, kSelf);

	SoundEvent heard = {};
	float loudness = 0.0f;

	Check(!field.LoudestAudible(Vector3{ 0, 0, 0 }, 0.0f, kSelf, nullptr, heard, loudness),
		"an emission from the ignored source is not heard");

	Check(field.LoudestAudible(Vector3{ 0, 0, 0 }, 0.0f, 7, nullptr, heard, loudness),
		"the same emission is heard by a different listener");
}

static void TestThreshold()
{
	std::printf("threshold gating\n");
	SoundField field;
	EmitAt(field, Vector3{ 0, 0, 0 }, 0.2f);

	SoundEvent heard = {};
	float loudness = 0.0f;

	Check(!field.LoudestAudible(Vector3{ 0, 0, 0 }, 0.5f, 0, nullptr, heard, loudness),
		"a noise below the threshold is not heard");
	Check(field.LoudestAudible(Vector3{ 0, 0, 0 }, 0.1f, 0, nullptr, heard, loudness),
		"the same noise above the threshold is heard");
}

static void TestLoudestWins()
{
	std::printf("loudest audible wins\n");
	SoundField field;
	EmitAt(field, Vector3{ 0, 0, 0 }, 0.3f);
	EmitAt(field, Vector3{ 1, 0, 0 }, 0.9f);

	SoundEvent heard = {};
	float loudness = 0.0f;
	const bool found = field.LoudestAudible(Vector3{ 0, 0, 0 }, 0.0f, 0, nullptr, heard, loudness);

	Check(found, "something is audible");
	CheckNear(heard.position.x, 1.0f, 0.001f, "the louder distant noise beats the quieter near one");
}

static void TestDistanceCanBeatRawLoudness()
{
	std::printf("attenuation is applied before comparison\n");
	SoundField field;
	// Loud but very far, versus quiet and close. The near one should win once
	// falloff is applied — otherwise the stalker would always chase the
	// loudest raw emission regardless of where it happened.
	EmitAt(field, Vector3{ 200, 0, 0 }, 1.0f);
	EmitAt(field, Vector3{ 1, 0, 0 }, 0.4f);

	SoundEvent heard = {};
	float loudness = 0.0f;
	field.LoudestAudible(Vector3{ 0, 0, 0 }, 0.0f, 0, nullptr, heard, loudness);

	CheckNear(heard.position.x, 1.0f, 0.001f, "the near quiet noise beats the far loud one");
}

static void TestRingWraparound()
{
	std::printf("ring wraparound at capacity\n");
	SoundField field;

	// Overfill by half a buffer. Nothing should grow past capacity and nothing
	// should crash on the wrap.
	const std::size_t overfill = SoundField::kCapacity + (SoundField::kCapacity / 2);
	for (std::size_t i = 0; i < overfill; ++i)
	{
		EmitAt(field, Vector3{ static_cast<float>(i), 0, 0 }, 1.0f);
	}

	Check(field.Events().size() == SoundField::kCapacity,
		"the buffer never grows beyond kCapacity");

	SoundEvent heard = {};
	float loudness = 0.0f;
	Check(field.LoudestAudible(Vector3{ 0, 0, 0 }, 0.0f, 0, nullptr, heard, loudness),
		"the field is still queryable after wrapping");
}

static void TestClockAdvances()
{
	std::printf("clock and timestamps\n");
	SoundField field;
	field.Update(1.0f);
	EmitAt(field, Vector3{ 0, 0, 0 }, 1.0f);

	CheckNear(field.Now(), 1.0f, 0.001f, "clock advances with Update");
	CheckNear(field.Events().front().timestamp, 1.0f, 0.001f,
		"an emission is stamped with the current clock, not the caller's value");
}

int main()
{
	std::printf("SoundField tests\n\n");

	TestEmitAndHear();
	TestSilentEmissionIgnored();
	TestDistanceFalloff();
	TestExpiry();
	TestIgnoresOwnSource();
	TestThreshold();
	TestLoudestWins();
	TestDistanceCanBeatRawLoudness();
	TestRingWraparound();
	TestClockAdvances();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
