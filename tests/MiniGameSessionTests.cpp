/**
 * Standalone tests for MiniGames: the session that runs them, each game played
 * with scripted input, and the 2D body every game's cursor is.
 *
 * A game ends by returning an outcome rather than by freeing itself, and reads
 * its input, screen size and randomness from what it is handed. That is what
 * lets this suite sit in the cheap tier: raylib linked for its rectangle and
 * collision helpers, but no window, no engine, no Player and no GameMap.
 *
 * If a game or the session ever needs a game header to compile, the session's
 * rules have leaked back out of it and this file stops linking.
 *
 * Build and run: tests/run_tests.sh
 */

#include <MiniGameSession.h>
#include <Physics.h>

#include <cmath>
#include <cstdio>
#include <memory>
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

static void CheckNear(float actual, float expected, const std::string& what)
{
	++g_checks;
	if (std::fabs(actual - expected) > 0.001f)
	{
		++g_failures;
		std::printf("  FAIL  %s (expected ~%.3f, got %.3f)\n", what.c_str(), expected, actual);
	}
}

/** RigidBody2D **/

static void TestBodyStaysInsideItsBounds()
{
	std::printf("a body is kept inside its bounds, by its scale\n");

	Physics2D::RigidBody2D body;
	body.scale = Vector3(5, 5, 5);
	body.useGravity = false;
	body.bounds = Rectangle{ 100, 100, 200, 100 };

	body.teleport(Vector2{ 1000, -50 });
	body.update(1.0f / 60.0f);

	CheckNear(body.translation.x, 295.0f, "pushed back from the right edge by its scale");
	CheckNear(body.translation.y, 105.0f, "pushed back from the top edge by its scale");
}

static void TestBodyWithoutBoundsIsNotClamped()
{
	std::printf("a body without bounds goes where it is put\n");

	Physics2D::RigidBody2D body;
	body.useGravity = false;
	body.teleport(Vector2{ -40, 5000 });
	body.update(1.0f / 60.0f);

	CheckNear(body.translation.x, -40.0f, "x is left alone");
	CheckNear(body.translation.y, 5000.0f, "y is left alone");
}

static void TestBodyRectanglesUseTheScaleAsSize()
{
	std::printf("a body's rectangles are its scale in size\n");

	Physics2D::RigidBody2D body;
	body.scale = Vector3(10, 4, 1);
	body.teleport(Vector2{ 300, 200 });

	const Rectangle centered = body.getRectCentered();
	CheckNear(centered.x, 295.0f, "centred: left edge is half the width left of the body");
	CheckNear(centered.y, 198.0f, "centred: top edge is half the height above the body");
	CheckNear(centered.width, 10.0f, "centred: as wide as the body's scale");
	CheckNear(centered.height, 4.0f, "centred: as tall as the body's scale");

	const Rectangle cornered = body.getRectCornered();
	CheckNear(cornered.x, 300.0f, "cornered: top-left corner at the body");
	CheckNear(cornered.y, 200.0f, "cornered: top-left corner at the body");
	CheckNear(cornered.width, 10.0f, "cornered: as wide as the body's scale");
	CheckNear(cornered.height, 4.0f, "cornered: as tall as the body's scale");
}

/** MiniGameSession, against a scripted stand-in game **/

// What the stand-in game does, and what the test can see of it
struct FakeScript
{
	MiniGameOutcome next = MiniGameOutcome::Running; // What every Tick returns
	int built = 0;                                   // Games the factory has made
	MiniGameInput lastInput = {};                    // What the running game was last handed
	std::uint32_t lastSeed = 0;                      // What the last game was built with
	Vector2 lastScreen = {};
};

class FakeGame : public MiniGame
{
public:
	FakeGame(FakeScript& script, const MiniGameSetup& setup) : script(script)
	{
		name = "Fake";
		data.scoreGoal = 3;
		script.lastSeed = setup.seed;
		script.lastScreen = setup.screenSize;
	}

protected:
	MiniGameOutcome Update(const MiniGameInput& input, float) override
	{
		script.lastInput = input;
		data.score++; // Progress, so a snapshot can be told from a fresh game
		return script.next;
	}
	void DrawGame() const override {}

private:
	FakeScript& script;
};

// Ids 0 to 5 build a stand-in; anything else names no game
static MiniGameSession::Factory FakeFactory(FakeScript& script)
{
	return [&script](int id, const MiniGameSetup& setup) -> std::unique_ptr<MiniGame>
	{
		if (id < 0 || id > 5) { return nullptr; }
		++script.built;
		return std::make_unique<FakeGame>(script, setup);
	};
}

static void TestNewSessionRunsNothing()
{
	std::printf("a new session runs nothing\n");

	FakeScript script;
	MiniGameSession session(FakeFactory(script));

	Check(!session.IsRunning(), "no game is running");
	Check(session.GameId() == -1, "no game has been played");
	Check(!session.History().hasCurrent && !session.History().hasPrevious, "there is no history yet");
}

static void TestStartRunsTheGameForItsStation()
{
	std::printf("starting runs the game for the station that started it\n");

	FakeScript script;
	MiniGameSession session(FakeFactory(script));

	Check(session.Start(MINI_GAME_CRANE_ID, 42, Vector2{ 800, 600 }, 7), "a known game starts");
	Check(session.IsRunning(), "the game is running");
	Check(session.GameId() == MINI_GAME_CRANE_ID, "the session knows which game");
	Check(session.StationId() == 42, "and which station started it");
	Check(script.built == 1, "one game was built");
	Check(session.History().hasCurrent && session.History().current.scoreGoal == 3, "the opening layout is recorded");
	Check(!session.History().hasPrevious, "nothing was displaced");
}

static void TestUnknownGameIsRejected()
{
	std::printf("an id that names no game is rejected, changing nothing\n");

	FakeScript script;
	MiniGameSession session(FakeFactory(script));
	session.Start(MINI_GAME_CRANE_ID, 42, Vector2{ 800, 600 }, 7);

	Check(!session.Start(999, 7, Vector2{ 800, 600 }, 7), "the unknown id is refused");
	Check(session.IsRunning() && session.GameId() == MINI_GAME_CRANE_ID, "the running game carries on");
	Check(session.StationId() == 42, "still for its own station");
	Check(!session.History().hasPrevious, "the history was not rolled");
}

static void TestTickHandsTheGameItsInput()
{
	std::printf("a tick hands the game its input and reports the round still running\n");

	FakeScript script;
	MiniGameSession session(FakeFactory(script));
	session.Start(MINI_GAME_CRANE_ID, 42, Vector2{ 800, 600 }, 7);

	MiniGameInput input;
	input.pressed.jump = true;
	const MiniGameResult result = session.Tick(input, 1.0f / 60.0f);

	Check(result.outcome == MiniGameOutcome::Running, "the round is still running");
	Check(script.lastInput.pressed.jump, "the game saw the jump");
	Check(session.IsRunning(), "the game is still running");
}

static void TestLostRoundStartsFresh()
{
	std::printf("a lost round is replaced by a fresh game at the same station\n");

	FakeScript script;
	MiniGameSession session(FakeFactory(script));
	session.Start(MINI_GAME_CRANE_ID, 42, Vector2{ 800, 600 }, 7);
	const std::uint32_t firstSeed = script.lastSeed;

	MiniGameInput input;
	input.screenSize = Vector2{ 1024, 768 }; // The window was resized during the round
	session.Tick(input, 1.0f / 60.0f);       // Some progress: score 1
	script.next = MiniGameOutcome::Lost;
	const MiniGameResult result = session.Tick(input, 1.0f / 60.0f);

	Check(result.outcome == MiniGameOutcome::Lost, "the loss is reported");
	Check(script.built == 2, "a fresh game was built");
	Check(session.IsRunning() && session.GameId() == MINI_GAME_CRANE_ID, "the same game is running again");
	Check(session.StationId() == 42, "still for the same station");
	Check(script.lastSeed != firstSeed, "the fresh round is randomised afresh");
	Check(script.lastScreen.x == 1024 && script.lastScreen.y == 768, "and laid out for the window as it is now");
	Check(session.History().hasPrevious && session.History().previous.score == 2, "the lost round's progress is kept");
	Check(session.History().current.score == 0, "the fresh round starts from nothing");
}

static void TestWonRoundEndsTheGameOnce()
{
	std::printf("a won round ends the game and is reported once, with its game and station\n");

	FakeScript script;
	MiniGameSession session(FakeFactory(script));
	session.Start(MINI_GAME_DOCTOR_ID, 42, Vector2{ 800, 600 }, 7);

	script.next = MiniGameOutcome::Won;
	const MiniGameResult won = session.Tick(MiniGameInput{}, 1.0f / 60.0f);

	Check(won.outcome == MiniGameOutcome::Won, "the win is reported");
	Check(won.gameId == MINI_GAME_DOCTOR_ID, "for the game that was won");
	Check(won.stationId == 42, "and the station it was won at");
	Check(!session.IsRunning(), "the game has ended");
	Check(session.StationId() == 0, "no station is occupied any more");
	Check(session.GameId() == MINI_GAME_DOCTOR_ID, "the last game played is remembered");
	Check(session.History().current.score == 1, "the winning round's final state is kept");

	const MiniGameResult after = session.Tick(MiniGameInput{}, 1.0f / 60.0f);
	Check(after.outcome == MiniGameOutcome::Running, "the win is not reported a second time");
	Check(script.built == 1, "no new game was built");
}

static void TestQuitKeepsTheProgress()
{
	std::printf("quitting ends the game and keeps how far the player got\n");

	FakeScript script;
	MiniGameSession session(FakeFactory(script));
	session.Start(MINI_GAME_CRANE_ID, 42, Vector2{ 800, 600 }, 7);
	session.Tick(MiniGameInput{}, 1.0f / 60.0f);
	session.Tick(MiniGameInput{}, 1.0f / 60.0f);

	session.Quit();

	Check(!session.IsRunning(), "the game has ended");
	Check(session.StationId() == 0, "the station is free");
	Check(session.History().hasCurrent && session.History().current.score == 2, "the score reached is kept, not the opening one");

	session.Quit();
	Check(session.History().current.score == 2, "quitting again with nothing running changes nothing");
}

static void TestHistoryIsTwoDeep()
{
	std::printf("starting a game rolls the history back one slot, two deep\n");

	FakeScript script;
	MiniGameSession session(FakeFactory(script));
	session.Start(MINI_GAME_CRANE_ID, 42, Vector2{ 800, 600 }, 7);
	session.Tick(MiniGameInput{}, 1.0f / 60.0f); // Crane reaches score 1

	session.Start(MINI_GAME_MAZE_ID, 0, Vector2{ 800, 600 }, 8);
	Check(session.History().hasPrevious && session.History().previous.score == 1, "the displaced game's final score is previous");
	Check(session.History().current.score == 0, "the new game's opening state is current");

	session.Tick(MiniGameInput{}, 1.0f / 60.0f);
	session.Tick(MiniGameInput{}, 1.0f / 60.0f); // Maze reaches score 2
	session.Start(MINI_GAME_DOCTOR_ID, 0, Vector2{ 800, 600 }, 9);
	Check(session.History().previous.score == 2, "the oldest game has dropped out");
}

static void TestEditorWinAndRetryActLikeTheRealThing()
{
	std::printf("the editor's Win and Retry act like a real win and a real loss\n");

	FakeScript script;
	MiniGameSession session(FakeFactory(script));
	session.Start(MINI_GAME_SIMON_SAYS_ID, 42, Vector2{ 800, 600 }, 7);
	session.Tick(MiniGameInput{}, 1.0f / 60.0f);

	session.Retry(Vector2{ 640, 480 });
	Check(script.built == 2 && session.IsRunning(), "Retry builds a fresh game");
	Check(session.Game() != nullptr && session.Game()->data.score == 0, "which starts from nothing");
	Check(session.StationId() == 42, "for the same station");

	const MiniGameResult won = session.Win();
	Check(won.outcome == MiniGameOutcome::Won && won.gameId == MINI_GAME_SIMON_SAYS_ID && won.stationId == 42, "Win reports what a real win reports");
	Check(!session.IsRunning() && session.Game() == nullptr, "and ends the game");

	const MiniGameResult nothing = session.Win();
	Check(nothing.outcome == MiniGameOutcome::Running, "Win with nothing running wins nothing");
}

/** The games, played with scripted input **/

static constexpr float kFrame = 1.0f / 60.0f;
static const MiniGameSetup kSetup{ Vector2{ 1280, 720 }, 1234 };

// One frame with a single key pressed (and held)
static MiniGameInput Press(bool MiniGameKeys::* key)
{
	MiniGameInput input;
	input.held.*key = true;
	input.pressed.*key = true;
	input.screenSize = kSetup.screenSize;
	return input;
}

static MiniGameInput Nothing()
{
	MiniGameInput input;
	input.screenSize = kSetup.screenSize;
	return input;
}

static void TestRoShamBooIsWonByTheRightCard()
{
	std::printf("Ro Sham Boo: the wrong card plays on, the right one wins\n");

	std::unique_ptr<MiniGame> game = CreateMiniGame(MINI_GAME_RO_SHAM_BOO_ID, kSetup);
	Check(game != nullptr, "the game builds");
	if (!game) { return; }

	// Cards are left (1), middle (2) and right (3); the goal is which one it picked
	const int goal = game->data.scoreGoal;
	Check(goal >= 1 && goal <= 3, "it picks one of the three cards");

	bool MiniGameKeys::* cards[] = { &MiniGameKeys::left, &MiniGameKeys::backward, &MiniGameKeys::right };
	const int wrong = goal == 1 ? 2 : 1;

	Check(game->Tick(Press(cards[wrong - 1]), kFrame) == MiniGameOutcome::Running, "a wrong card plays on");
	Check(game->Tick(Press(cards[goal - 1]), kFrame) == MiniGameOutcome::Won, "the right card wins");
}

static void TestSimonSaysIsWonByTheShownSequence()
{
	std::printf("Simon Says: a wrong key does not advance, the shown sequence wins\n");

	std::unique_ptr<MiniGame> game = CreateMiniGame(MINI_GAME_SIMON_SAYS_ID, kSetup);
	Check(game != nullptr, "the game builds");
	if (!game) { return; }

	// Each step's key is shown as 0 forward, 1 left, 2 backward, 3 right
	bool MiniGameKeys::* keys[] = { &MiniGameKeys::forward, &MiniGameKeys::left, &MiniGameKeys::backward, &MiniGameKeys::right };
	const std::vector<Rectangle> steps = game->data.obstacles;
	Check(static_cast<int>(steps.size()) == game->data.scoreGoal, "one step per point needed");

	const int first = static_cast<int>(steps.front().y);
	game->Tick(Press(keys[(first + 1) % 4]), kFrame);
	Check(game->data.score == 0, "a wrong key does not advance");

	MiniGameOutcome outcome = MiniGameOutcome::Running;
	for (const Rectangle& step : steps) { outcome = game->Tick(Press(keys[static_cast<int>(step.y)]), kFrame); }
	Check(outcome == MiniGameOutcome::Won, "the whole sequence wins");
}

static void TestMazeMovesWithWasdAndNeverEnds()
{
	std::printf("Maze: WASD moves the cursor, and the unfinished game never ends\n");

	std::unique_ptr<MiniGame> game = CreateMiniGame(MINI_GAME_MAZE_ID, kSetup);
	Check(game != nullptr, "the game builds");
	if (!game) { return; }

	const float startX = game->cursor.translation.x;
	MiniGameOutcome outcome = MiniGameOutcome::Running;
	for (int frame = 0; frame < 30; ++frame)
	{
		MiniGameInput input = Nothing();
		input.held.right = true;
		outcome = game->Tick(input, kFrame);
		if (outcome != MiniGameOutcome::Running) { break; }
	}

	CheckNear(game->cursor.translation.x - startX, 30.0f, "holding right for half a second moves it 60 px/s");
	Check(outcome == MiniGameOutcome::Running, "it is neither won nor lost");
}

static void TestDoctorIsWonByHoldingStillOnTheGoal()
{
	std::printf("Doctor: the cursor drifts, and holding still on the goal wins\n");

	std::unique_ptr<MiniGame> game = CreateMiniGame(MINI_GAME_DOCTOR_ID, kSetup);
	Check(game != nullptr, "the game builds");
	if (!game) { return; }

	const float startX = game->cursor.translation.x;
	Check(game->Tick(Nothing(), kFrame) == MiniGameOutcome::Running, "drifting is not a win");
	Check(game->cursor.translation.x > startX, "with nothing held the cursor drifts");

	const Rectangle goal = game->data.goal;
	game->cursor.teleport(Vector2{ goal.x + 1, goal.y + 1 });

	MiniGameInput passing = Nothing();
	passing.held.left = true;
	Check(game->Tick(passing, kFrame) == MiniGameOutcome::Running, "passing over the goal still moving is not a win");

	game->cursor.teleport(Vector2{ goal.x + 1, goal.y + 1 });
	MiniGameInput still = Nothing();
	still.held.left = true;
	still.held.forward = true;
	Check(game->Tick(still, kFrame) == MiniGameOutcome::Won, "holding both directions on the goal wins");
}

static Vector2 CenterOf(Rectangle rect) { return Vector2{ rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f }; }

static void TestCraneLosesOnObstaclesAndWinsAtTheGoal()
{
	std::printf("Crane: an obstacle or leaving the screen loses, the goal wins\n");

	std::unique_ptr<MiniGame> game = CreateMiniGame(MINI_GAME_CRANE_ID, kSetup);
	Check(game != nullptr, "the game builds");
	if (!game) { return; }
	Check(!game->data.obstacles.empty(), "it lays out obstacles");

	game->cursor.teleport(CenterOf(game->data.obstacles.front()));
	Check(game->Tick(Nothing(), kFrame) == MiniGameOutcome::Lost, "touching an obstacle loses");

	game = CreateMiniGame(MINI_GAME_CRANE_ID, kSetup);
	const Rectangle screen = game->data.screen;
	game->cursor.teleport(Vector2{ screen.x - 50, screen.y + screen.height * 0.5f });
	Check(game->Tick(Nothing(), kFrame) == MiniGameOutcome::Lost, "leaving the screen loses");

	game = CreateMiniGame(MINI_GAME_CRANE_ID, kSetup);
	game->cursor.teleport(CenterOf(game->data.goal));
	Check(game->Tick(Nothing(), kFrame) == MiniGameOutcome::Won, "reaching the goal wins");
}

static void TestFlappyBirdLosesOutsideTheGreenAndWinsByScoring()
{
	std::printf("Flappy Bird: leaving the green area loses, scoring both goals wins\n");

	std::unique_ptr<MiniGame> game = CreateMiniGame(MINI_GAME_FLAPPY_BIRD_ID, kSetup);
	Check(game != nullptr, "the game builds");
	if (!game) { return; }

	// The green area is the play screen inset by 5%. Between its edge and the
	// screen's is still on screen, but out of bounds: getRectCentered used to
	// stretch far enough to the right that this never counted.
	const Rectangle screen = game->data.screen;
	const Rectangle green = generateScaleRect(screen, Rectangle{ 1.05f, 1.05f, 0.95f, 0.95f });
	game->cursor.teleport(Vector2{ (screen.x + green.x) * 0.5f, green.y + green.height * 0.5f });
	Check(game->Tick(Nothing(), kFrame) == MiniGameOutcome::Lost, "leaving the green area loses");

	// The goals are the bands at the green area's two ends; the right one is live first
	game = CreateMiniGame(MINI_GAME_FLAPPY_BIRD_ID, kSetup);
	const Rectangle leftGoal = generateScaleRect(screen, Rectangle{ 1.05f, 1.05f, 0.1f, 0.95f });
	const Rectangle rightGoal = generateScaleRect(screen, Rectangle{ 2.75f, 1.05f, 0.1f, 0.95f });
	const Rectangle goals[] = { rightGoal, leftGoal, rightGoal };

	MiniGameOutcome outcome = MiniGameOutcome::Running;
	for (const Rectangle& goal : goals)
	{
		game->cursor.teleport(CenterOf(goal));
		outcome = game->Tick(Nothing(), kFrame);
	}
	Check(game->data.score == game->data.scoreGoal, "each live goal scores once");
	Check(outcome == MiniGameOutcome::Won, "reaching the score goal wins");
}

int main()
{
	std::printf("MiniGame tests\n\n");

	TestNewSessionRunsNothing();
	TestStartRunsTheGameForItsStation();
	TestUnknownGameIsRejected();
	TestTickHandsTheGameItsInput();
	TestLostRoundStartsFresh();
	TestWonRoundEndsTheGameOnce();
	TestQuitKeepsTheProgress();
	TestHistoryIsTwoDeep();
	TestEditorWinAndRetryActLikeTheRealThing();

	TestRoShamBooIsWonByTheRightCard();
	TestSimonSaysIsWonByTheShownSequence();
	TestMazeMovesWithWasdAndNeverEnds();
	TestDoctorIsWonByHoldingStillOnTheGoal();
	TestCraneLosesOnObstaclesAndWinsAtTheGoal();
	TestFlappyBirdLosesOutsideTheGreenAndWinsByScoring();

	TestBodyStaysInsideItsBounds();
	TestBodyWithoutBoundsIsNotClamped();
	TestBodyRectanglesUseTheScaleAsSize();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
