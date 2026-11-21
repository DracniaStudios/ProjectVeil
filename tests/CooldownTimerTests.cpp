/**
 * Standalone tests for CooldownTimer (helpers.h).
 *
 * The timer gates every Entity's Attack() and holds each Buff's duration, so a
 * timer that never becomes ready again leaves an entity unable to fire, and one
 * that is always ready lets it fire every frame -- the bug the attack cooldown
 * was added to fix.
 *
 * helpers.h is header-only and includes nothing from raylib or the engine, so
 * this links no library at all. The timer reads the real steady clock, so the
 * short waits below are real ones, kept well apart from the durations they
 * measure.
 *
 * Build and run: tests/run_tests.sh
 */

#include <helpers.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

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

static void Wait(double seconds)
{
	std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

static void TestANewTimerIsReady()
{
	std::printf("a new timer is ready at once\n");
	CooldownTimer timer(60.0);

	Check(timer.is_ready(), "nothing has used it yet");
	Check(timer.remaining_time() == 0.0, "so no time remains");
	Check(timer.cooldownID == -1, "and it names no buff until given an id");
}

static void TestUseStartsTheCooldown()
{
	std::printf("using a timer starts its cooldown\n");
	CooldownTimer timer(60.0);

	Check(timer.use(), "the first use succeeds");
	Check(!timer.is_ready(), "the timer is cooling down afterwards");

	const double remaining = timer.remaining_time();
	Check(remaining > 59.0 && remaining <= 60.0, "almost the whole minute remains (got " + std::to_string(remaining) + ")");
}

static void TestUseIsRefusedWhileCoolingDown()
{
	std::printf("a cooling-down timer refuses to be used, and is not restarted by trying\n");
	CooldownTimer timer(0.5);
	timer.use();

	Wait(0.1);
	const double before = timer.remaining_time();
	Check(!timer.use(), "a second use inside the cooldown is refused");

	const double after = timer.remaining_time();
	Check(after <= before, "the refused use did not push the end of the cooldown back");
}

static void TestTheTimerIsReadyAgainOnceTheCooldownPasses()
{
	std::printf("a timer is ready again once its cooldown has passed\n");
	CooldownTimer timer(0.05);
	timer.use();

	Check(!timer.is_ready(), "not ready straight after use");
	Wait(0.15);

	Check(timer.is_ready(), "ready once the 50 ms have passed");
	Check(timer.remaining_time() == 0.0, "with nothing remaining");
	Check(timer.use(), "and it can be used again");
	Check(!timer.is_ready(), "which starts a fresh cooldown");
}

static void TestRemainingTimeCountsDown()
{
	std::printf("the remaining time counts down while the timer cools\n");
	CooldownTimer timer(5.0);
	timer.use();

	const double first = timer.remaining_time();
	Wait(0.1);
	const double second = timer.remaining_time();

	Check(second < first, "less time remains 100 ms later");
	Check(first - second > 0.05, "by about the time that passed (got " + std::to_string(first - second) + ")");
}

static void TestAZeroCooldownIsAlwaysUsable()
{
	std::printf("a zero-length cooldown can be used every time\n");
	CooldownTimer timer(0.0);

	bool always = true;
	for (int i = 0; i < 10; ++i)
	{
		if (!timer.use()) { always = false; }
	}
	Check(always, "ten uses in a row all succeed");
}

static void TestCopiesCoolDownApart()
{
	std::printf("a copied timer cools down apart from the one it was copied from\n");

	// Entity keeps its buff timers by value in a vector, so each copy has to
	// carry its own state
	CooldownTimer original(60.0);
	CooldownTimer copy = original;
	copy.use();

	Check(original.is_ready(), "using the copy leaves the original ready");
	Check(!copy.is_ready(), "while the copy cools down");
}

int main()
{
	std::printf("Cooldown timer tests\n\n");

	TestANewTimerIsReady();
	TestUseStartsTheCooldown();
	TestUseIsRefusedWhileCoolingDown();
	TestTheTimerIsReadyAgainOnceTheCooldownPasses();
	TestRemainingTimeCountsDown();
	TestAZeroCooldownIsAlwaysUsable();
	TestCopiesCoolDownApart();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
