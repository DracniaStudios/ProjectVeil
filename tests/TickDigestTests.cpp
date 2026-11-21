/**
 * Tick digest (work order A-01). Runs 3,000 scripted ticks of chunk_1 twice in
 * one process, prints the digest, and checks it is non-zero and repeatable.
 * Every later structural work order compares this line before and after.
 */

#include <raylib.h>

#include "TickDigest.h"

#include <cstdio>
#include <string>
#include <Client/GamePresentation.h>
#include "TestWindow.h"

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

int main()
{
	OpenTestWindow("veil-tick-digest", true, true);
	Client::InstallGamePresentation();

	constexpr int kTicks = 3000;
	const std::uint64_t first = TickDigest::RunChunk1(kTicks);
	const std::uint64_t second = TickDigest::RunChunk1(kTicks);

	std::printf("[TickDigest] chunk_1, %d ticks: 0x%016llx\n", kTicks, static_cast<unsigned long long>(first));
	if (second != first)
		std::printf("[TickDigest] second run: 0x%016llx\n", static_cast<unsigned long long>(second));

	Check(first != 0, "the digest is non-zero (chunk_1 loaded)");
	Check(first == second, "two runs in one process give the same digest");

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	CloseTestWindow();
	return g_failures == 0 ? 0 : 1;
}
