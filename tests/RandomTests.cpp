/**
 * Standalone tests for the random helpers in randomStuff.
 *
 * MiniGames, the Stalker's search sweep and the player's rolls all draw from
 * these, so a range that silently drops its top value (the bug getRandomInt
 * used to have) shows up as a game that can never produce one of its outcomes.
 *
 * randomStuff.cpp calls no raylib function: raylib's header is included for its
 * Vector3 and Color types, and nothing is linked. Every draw except
 * GetRandomRarity's comes from a seeded generator, so these cases are
 * deterministic.
 *
 * Build and run: tests/run_tests.sh
 */

#include <randomStuff.h>

#include <algorithm>
#include <array>
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

static constexpr int kDraws = 5000;

// ─── Integers ──────────────────────────────────────────────────────────────

static void TestIntReachesBothEnds()
{
	std::printf("an int range includes both of its ends\n");
	std::ranlux24_base rng(1234);

	std::array<int, 3> seen = {};
	bool inRange = true;
	for (int i = 0; i < kDraws; ++i)
	{
		const int value = getRandomInt(rng, 1, 3);
		if (value < 1 || value > 3) { inRange = false; continue; }
		++seen[value - 1];
	}

	Check(inRange, "every draw is within 1-3");
	Check(seen[0] > 0, "1 is drawn");
	Check(seen[1] > 0, "2 is drawn");
	Check(seen[2] > 0, "3, the top of the range, is drawn");
}

static void TestIntWithSwappedBoundsStaysInRange()
{
	std::printf("an int range given high-to-low is read as low-to-high\n");
	std::ranlux24_base rng(99);

	bool inRange = true;
	bool sawLow = false;
	bool sawHigh = false;
	for (int i = 0; i < kDraws; ++i)
	{
		const int value = getRandomInt(rng, 5, -5);
		if (value < -5 || value > 5) { inRange = false; }
		if (value == -5) { sawLow = true; }
		if (value == 5) { sawHigh = true; }
	}

	Check(inRange, "every draw is within -5 to 5");
	Check(sawLow && sawHigh, "and both ends are reachable");
}

static void TestIntWithOneValueAlwaysReturnsIt()
{
	std::printf("an int range of one value always returns that value\n");
	std::ranlux24_base rng(7);

	bool always = true;
	for (int i = 0; i < 100; ++i)
	{
		if (getRandomInt(rng, 42, 42) != 42) { always = false; }
	}
	Check(always, "getRandomInt(42, 42) is always 42");
}

static void TestSameSeedSameSequence()
{
	std::printf("two generators with the same seed draw the same sequence\n");
	std::ranlux24_base a(2026);
	std::ranlux24_base b(2026);

	bool same = true;
	for (int i = 0; i < 200; ++i)
	{
		if (getRandomInt(a, 0, 1000) != getRandomInt(b, 0, 1000)) { same = false; }
		if (getRandomFloat(a, -1.0f, 1.0f) != getRandomFloat(b, -1.0f, 1.0f)) { same = false; }
	}
	Check(same, "a seeded run can be replayed");
}

// ─── Floats, chances and vectors ───────────────────────────────────────────

static void TestFloatStaysInRange()
{
	std::printf("a float draw stays within its range\n");
	std::ranlux24_base rng(5);

	bool inRange = true;
	float lowest = 100.0f;
	float highest = -100.0f;
	for (int i = 0; i < kDraws; ++i)
	{
		const float value = getRandomFloat(rng, -2.0f, 3.0f);
		if (value < -2.0f || value > 3.0f) { inRange = false; }
		lowest = std::min(lowest, value);
		highest = std::max(highest, value);
	}

	Check(inRange, "every draw is within -2 to 3");
	Check(lowest < -1.5f && highest > 2.5f, "and the draws spread across the range");
}

static void TestChanceAtItsExtremes()
{
	std::printf("a chance of 0 never comes up and a chance of 1 always does\n");
	std::ranlux24_base rng(11);

	int never = 0;
	int always = 0;
	for (int i = 0; i < kDraws; ++i)
	{
		if (getRandomChance(rng, 0.0f)) { ++never; }
		if (getRandomChance(rng, 1.0f)) { ++always; }
	}

	Check(never == 0, "a 0% chance never succeeds");
	Check(always == kDraws, "a 100% chance always succeeds");
}

static void TestChanceFollowsItsOdds()
{
	std::printf("a 25%% chance comes up about a quarter of the time\n");
	std::ranlux24_base rng(31);

	int hits = 0;
	for (int i = 0; i < kDraws; ++i)
	{
		if (getRandomChance(rng, 0.25f)) { ++hits; }
	}

	const float rate = static_cast<float>(hits) / kDraws;
	Check(rate > 0.21f && rate < 0.29f, "the rate is within a few percent of 25% (got " + std::to_string(rate) + ")");
}

static void TestVectorKeepsEachAxisInItsOwnRange()
{
	std::printf("a random vector keeps each axis within that axis's range\n");
	std::ranlux24_base rng(17);

	const Vector3 min = { -1.0f, 10.0f, 100.0f };
	const Vector3 max = { 1.0f, 20.0f, 200.0f };

	bool inRange = true;
	for (int i = 0; i < 1000; ++i)
	{
		const Vector3 v = getRandomVector3(rng, min, max);
		if (v.x < min.x || v.x > max.x) { inRange = false; }
		if (v.y < min.y || v.y > max.y) { inRange = false; }
		if (v.z < min.z || v.z > max.z) { inRange = false; }
	}
	Check(inRange, "x, y and z never borrow each other's bounds");
}

static void TestColorKeepsItsAlpha()
{
	std::printf("a random colour keeps the alpha it was asked for\n");
	std::ranlux24_base rng(23);

	bool keptAlpha = true;
	bool varied = false;
	Color first = getRandomColor(rng, 128);
	for (int i = 0; i < 100; ++i)
	{
		const Color color = getRandomColor(rng, 128);
		if (color.a != 128) { keptAlpha = false; }
		if (color.r != first.r || color.g != first.g || color.b != first.b) { varied = true; }
	}

	Check(first.a == 128 && keptAlpha, "every colour has alpha 128");
	Check(varied, "the colours themselves vary");
	Check(getRandomColor(rng).a == 255, "the default alpha is opaque");
}

// ─── Rarity ────────────────────────────────────────────────────────────────

static void TestRarityStaysWithinItsBounds()
{
	std::printf("a rarity draw stays within the rarities it was given\n");

	bool inRange = true;
	for (int i = 0; i < 2000; ++i)
	{
		const Rarity rarity = GetRandomRarity(RARE, LEGENDARY);
		if (rarity < RARE || rarity > LEGENDARY) { inRange = false; }
	}
	Check(inRange, "Rare to Legendary never draws Common or Mythic");

	bool exact = true;
	for (int i = 0; i < 200; ++i)
	{
		if (GetRandomRarity(EPIC, EPIC) != EPIC) { exact = false; }
	}
	Check(exact, "a range of one rarity always draws it");
}

static void TestRarityFavoursTheCommon()
{
	std::printf("common rarities are drawn more often than rare ones\n");

	// GetRandomRarity seeds itself, so this is statistical: at 40% against 5%
	// over 20000 draws, Legendary outnumbering Common is not a realistic outcome
	std::array<int, ETERNAL + 1> counts = {};
	for (int i = 0; i < 20000; ++i) { ++counts[GetRandomRarity()]; }

	Check(counts[COMMON] > counts[UNCOMMON], "Common outnumbers Uncommon");
	Check(counts[UNCOMMON] > counts[RARE], "Uncommon outnumbers Rare");
	Check(counts[RARE] > counts[LEGENDARY], "Rare outnumbers Legendary");
	Check(counts[COMMON] > 6500 && counts[COMMON] < 9500, "about 40% are Common (got " + std::to_string(counts[COMMON]) + ")");
}

int main()
{
	std::printf("Random tests\n\n");

	TestIntReachesBothEnds();
	TestIntWithSwappedBoundsStaysInRange();
	TestIntWithOneValueAlwaysReturnsIt();
	TestSameSeedSameSequence();

	TestFloatStaysInRange();
	TestChanceAtItsExtremes();
	TestChanceFollowsItsOdds();
	TestVectorKeepsEachAxisInItsOwnRange();
	TestColorKeepsItsAlpha();

	TestRarityStaysWithinItsBounds();
	TestRarityFavoursTheCommon();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
