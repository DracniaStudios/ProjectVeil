/**
 * Standalone tests for the overlay stack.
 *
 * The stack decides which Overlay (see CONTEXT.md) has the player's input and
 * what the Back action does in each state. It draws nothing and includes
 * nothing from raylib or the engine, so this suite is the cheapest tier of all:
 * no library linked, nothing initialised.
 *
 * If OverlayStack.cpp ever needs a game header to compile, the rules have leaked
 * out of it and this file stops linking.
 *
 * Build and run: tests/run_tests.sh
 */

#include <OverlayStack.h>

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

static void TestNewStackLeavesGameplayWithInput()
{
	std::printf("a new stack leaves gameplay with input\n");

	OverlayStack overlays;

	Check(overlays.GameplayHasInput(), "gameplay has input with nothing open");
	Check(!overlays.Top().has_value(), "nothing is on top");
	Check(!overlays.DimsWorld(), "the world is not dimmed");
}

static void TestBackActionDuringGameplayDoesNothing()
{
	std::printf("the Back action during gameplay opens nothing\n");

	OverlayStack overlays;
	const std::optional<Overlay> closed = overlays.BackPressed();

	Check(!closed.has_value(), "nothing is reported closed");
	Check(!overlays.Top().has_value(), "nothing is on top");
	Check(overlays.GameplayHasInput(), "gameplay keeps input");
}

static void TestTitleTakesInputWithoutDimming()
{
	std::printf("the Title takes input without dimming, and ignores the Back action\n");

	OverlayStack overlays;
	overlays.Open(Overlay::Title);

	Check(!overlays.GameplayHasInput(), "gameplay has no input under the Title");
	Check(!overlays.DimsWorld(), "the Main Menu world is shown undimmed behind the Title");

	const std::optional<Overlay> closed = overlays.BackPressed();
	Check(!closed.has_value(), "the Back action closes nothing");
	Check(overlays.Top() == Overlay::Title, "the Title is still on top");
}

static void TestClosingReturnsToTheOverlayBeneath()
{
	std::printf("closing Settings returns to the Title beneath it\n");

	OverlayStack overlays;
	overlays.Open(Overlay::Title);
	overlays.Open(Overlay::Settings);

	Check(overlays.Top() == Overlay::Settings, "Settings is on top of the Title");
	Check(overlays.DimsWorld(), "the world is dimmed behind Settings");

	overlays.Close(Overlay::Settings);

	Check(overlays.Top() == Overlay::Title, "the Title is back on top");
	Check(!overlays.DimsWorld(), "the dimming goes with Settings");
}

static void TestTitleCannotBeClosed()
{
	std::printf("the Title cannot be closed\n");

	OverlayStack overlays;
	overlays.Open(Overlay::Title);
	overlays.Close(Overlay::Title);

	Check(overlays.Top() == Overlay::Title, "the Title stays open until its scene goes");
	Check(!overlays.GameplayHasInput(), "the Main Menu world never gets player input");
}

static void TestOpeningAnOpenOverlayDoesNothing()
{
	std::printf("opening an overlay that is already open does nothing\n");

	OverlayStack overlays;
	overlays.Open(Overlay::Title);
	overlays.Open(Overlay::Settings);
	overlays.Open(Overlay::Multiplayer);
	overlays.Open(Overlay::Settings);

	Check(overlays.Top() == Overlay::Multiplayer, "Settings is not opened a second time over Multiplayer");

	overlays.Close(Overlay::Multiplayer);
	Check(overlays.Top() == Overlay::Settings, "the one Settings is still beneath it");

	overlays.Close(Overlay::Settings);
	Check(overlays.Top() == Overlay::Title, "one Close removes it: there was never a second copy");
}

static void TestBackActionClosesWhateverIsOnTop()
{
	std::printf("the Back action backs out of a menu, and quits a MiniGame\n");

	OverlayStack menus;
	menus.Open(Overlay::Title);
	menus.Open(Overlay::Settings);
	Check(menus.BackPressed() == Overlay::Settings, "Settings is reported closed");
	Check(menus.Top() == Overlay::Title, "backing out of Settings lands on the Title");

	OverlayStack station;
	station.Open(Overlay::MiniGame);
	Check(!station.GameplayHasInput(), "a running MiniGame takes input from gameplay");
	Check(station.DimsWorld(), "the world is dimmed behind a MiniGame");

	// The scene frees the minigame on hearing this; its own Close afterwards must be harmless
	Check(station.BackPressed() == Overlay::MiniGame, "the MiniGame is reported closed so the scene can release it");
	station.Close(Overlay::MiniGame);
	Check(station.GameplayHasInput(), "closing an overlay that is no longer open does nothing");
}

static void TestCloseAllKeepsTheTitle()
{
	std::printf("closing everything keeps the Title\n");

	OverlayStack mainMenu;
	mainMenu.Open(Overlay::Title);
	mainMenu.Open(Overlay::Multiplayer);
	mainMenu.Open(Overlay::Settings);
	mainMenu.CloseAll();
	Check(mainMenu.Top() == Overlay::Title, "the Main Menu is left on its Title");

	OverlayStack tutorial;
	tutorial.Open(Overlay::MiniGame);
	tutorial.Open(Overlay::Settings);
	tutorial.CloseAll();
	Check(tutorial.GameplayHasInput(), "a scene without a Title is handed back to gameplay");
}

static void TestOpenOverlaysReadBottomFirst()
{
	std::printf("the open overlays read bottom first, by name, for the editor\n");

	OverlayStack overlays;
	overlays.Open(Overlay::Title);
	overlays.Open(Overlay::Settings);

	const std::vector<Overlay>& open = overlays.OpenOverlays();
	Check(open.size() == 2, "two overlays are listed");
	Check(open.size() == 2 && open[0] == Overlay::Title && open[1] == Overlay::Settings, "the Title is listed first, Settings above it");

	Check(std::string(OverlayName(Overlay::Title)) == "Title", "the Title is named Title");
	Check(std::string(OverlayName(Overlay::Settings)) == "Settings", "Settings is named Settings");
	Check(std::string(OverlayName(Overlay::Multiplayer)) == "Multiplayer", "Multiplayer is named Multiplayer");
	Check(std::string(OverlayName(Overlay::MiniGame)) == "MiniGame", "a MiniGame is named MiniGame");
}

int main()
{
	std::printf("OverlayStack tests\n\n");

	TestNewStackLeavesGameplayWithInput();
	TestBackActionDuringGameplayDoesNothing();
	TestTitleTakesInputWithoutDimming();
	TestClosingReturnsToTheOverlayBeneath();
	TestTitleCannotBeClosed();
	TestOpeningAnOpenOverlayDoesNothing();
	TestBackActionClosesWhateverIsOnTop();
	TestCloseAllKeepsTheTitle();
	TestOpenOverlaysReadBottomFirst();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
