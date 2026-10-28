/**
 * Tests for SceneManager: when scenes are built during a transition, how long
 * the fade takes, and that the scene being played is handed to what runs it.
 *
 * A push used to build the requested scene on the spot, mid-frame, so the
 * incoming scene was in use before the outgoing one had faded out. Building the
 * main menu also fell through into rebuilding the tutorial. These pin the fixed
 * order: a push only records the request, the scene is built exactly once at the
 * faded-out midpoint, and the scene it replaces is released.
 *
 * Driven through SceneManager_frame and SceneManager_tick, the way the game
 * drives them, with a scene factory of the tests' own: it builds bare scenes,
 * so nothing loads a world file or plays music.
 *
 * Like EmitterTests this links against the game's object files and needs a GL
 * context, since a scene's player binds a model when it is enabled. See
 * tests/run_tests.sh.
 */

#include <raylib.h>

#include <Scene.h>
#include <SceneManager.h>

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

using SceneManagement::SceneFade;
using SceneManagement::FADE_SECONDS;

// How many times the factory built each scene, and the last scene ticked
static int g_built[SCENE_COUNT] = {};
static int g_ticks = 0;
static SceneManagement::Scene* g_tickedScene = nullptr;

static void RecordTick(SceneManagement::Scene& scene, float /*delta*/)
{
	++g_ticks;
	g_tickedScene = &scene;
}

// A bare scene named like the game's own, with no world file and no music
static SceneManagement::Scene* BuildTestScene(int sceneID)
{
	if (sceneID < 0 || sceneID >= SCENE_COUNT) { return nullptr; }

	SceneManagement::Scene* scene = SceneManagement::Scene_new();
	scene->name = sceneID == SCENE_MAIN_MENU ? "Main Menu" : "Tutorial";
	scene->update = RecordTick;
	++g_built[sceneID];
	return scene;
}

static SceneManagement::Scene* BuildNothing(int /*sceneID*/)
{
	return nullptr;
}

static bool IsScene(const SceneManagement::Scene* scene, const char* name)
{
	return scene != nullptr && std::string(scene->name) == name;
}

// One frame long enough to finish whichever half of the fade is running
static void FinishHalf(SceneManagement::SceneManager& manager)
{
	SceneManagement::SceneManager_frame(&manager, FADE_SECONDS + 0.01f);
}

// Runs frames until the fade ends, from either half
static void FinishFade(SceneManagement::SceneManager& manager)
{
	while (manager.fade != SceneFade::None) { FinishHalf(manager); }
}

static void TestStartUpBuildsOnlyTheMainMenu(SceneManagement::SceneManager& manager)
{
	std::printf("start-up builds only the main menu\n");

	SceneManagement::SceneManager_init(&manager, BuildTestScene);

	Check(IsScene(manager.currentScene, "Main Menu"), "the main menu is current after start-up");
	Check(manager.scenes[SCENE_MAIN_MENU] == manager.currentScene, "the main menu fills its slot");
	Check(manager.scenes[SCENE_TUTORIAL] == nullptr, "the tutorial is not built at start-up");
	Check(g_built[SCENE_MAIN_MENU] == 1 && g_built[SCENE_TUTORIAL] == 0, "the factory built the main menu once and nothing else");
	Check(manager.nextSceneID == -1, "no scene is left pending");
	Check(manager.fade == SceneFade::In, "start-up fades the main menu in");

	FinishFade(manager);
	Check(manager.fade == SceneFade::None, "and the fade ends");
}

static void TestPushKeepsTheOutgoingSceneUntilTheMidpoint(SceneManagement::SceneManager& manager)
{
	std::printf("a push leaves the outgoing scene running through its fade-out\n");

	SceneManagement::Scene* menu = manager.currentScene;
	SceneManagement::SceneManager_push(&manager, SCENE_TUTORIAL);

	Check(manager.currentScene == menu, "the main menu stays current while it fades out");
	Check(manager.scenes[SCENE_TUTORIAL] == nullptr, "the tutorial is not built before the midpoint");
	Check(manager.nextSceneID == SCENE_TUTORIAL, "the request is recorded");
	Check(manager.fade == SceneFade::Out, "the push starts a fade-out");

	SceneManagement::SceneManager_frame(&manager, FADE_SECONDS * 0.5f);
	Check(manager.currentScene == menu && g_built[SCENE_TUTORIAL] == 0, "halfway through the fade-out, the main menu is still current");
}

static void TestMidpointSwapsInTheRequestedScene(SceneManagement::SceneManager& manager)
{
	std::printf("the midpoint builds the requested scene once and releases the old one\n");

	SceneManagement::SceneManager_frame(&manager, FADE_SECONDS * 0.5f + 0.01f);

	Check(IsScene(manager.currentScene, "Tutorial"), "the tutorial is current after the midpoint");
	Check(g_built[SCENE_TUTORIAL] == 1, "and was built exactly once");
	Check(manager.scenes[SCENE_TUTORIAL] == manager.currentScene, "the tutorial fills its slot");
	Check(manager.scenes[SCENE_MAIN_MENU] == nullptr, "the replaced main menu no longer has a slot");
	Check(manager.nextSceneID == -1, "the request is consumed");
	Check(manager.fade == SceneFade::In, "the tutorial fades in");

	FinishFade(manager);
}

static void TestTheFadeTakesTheSameTimeAtAnyFrameRate(SceneManagement::SceneManager& manager)
{
	std::printf("each half of the fade takes FADE_SECONDS at any frame rate "
		"(regression: it stepped 5 of 255 per frame, so it ran twice as long at 30 FPS)\n");

	for (const float framesPerSecond : { 30.0f, 60.0f, 144.0f })
	{
		const float frameTime = 1.0f / framesPerSecond;
		const std::string rate = std::to_string(static_cast<int>(framesPerSecond)) + " FPS";

		SceneManagement::Scene* before = manager.currentScene;
		SceneManagement::SceneManager_push(&manager, SCENE_TUTORIAL);

		float elapsed = 0.0f;
		while (manager.currentScene == before && elapsed < 10.0f)
		{
			SceneManagement::SceneManager_frame(&manager, frameTime);
			elapsed += frameTime;
		}
		Check(elapsed >= FADE_SECONDS - 0.001f && elapsed <= FADE_SECONDS + frameTime + 0.001f,
			"the fade-out takes FADE_SECONDS at " + rate);

		elapsed = 0.0f;
		while (manager.fade != SceneFade::None && elapsed < 10.0f)
		{
			SceneManagement::SceneManager_frame(&manager, frameTime);
			elapsed += frameTime;
		}
		Check(elapsed >= FADE_SECONDS - 0.001f && elapsed <= FADE_SECONDS + frameTime + 0.001f,
			"and so does the fade-in at " + rate);
	}
}

static void TestRepushingASceneBuildsAFreshInstance(SceneManagement::SceneManager& manager)
{
	std::printf("pushing the current scene again reloads it\n");

	SceneManagement::Scene* first = manager.currentScene;
	SceneManagement::SceneManager_push(&manager, SCENE_TUTORIAL);
	Check(manager.currentScene == first, "the old instance runs until the midpoint");

	FinishHalf(manager);
	Check(IsScene(manager.currentScene, "Tutorial"), "the tutorial is current again");
	Check(manager.currentScene != first, "it is a freshly built instance");
	Check(manager.scenes[SCENE_TUTORIAL] == manager.currentScene, "the slot holds the new instance");

	FinishFade(manager);
}

static void TestReturningToTheMainMenu(SceneManagement::SceneManager& manager)
{
	std::printf("returning to the main menu releases the tutorial\n");

	SceneManagement::SceneManager_push(&manager, SCENE_MAIN_MENU);
	FinishHalf(manager);

	Check(IsScene(manager.currentScene, "Main Menu"), "the main menu is current");
	Check(manager.scenes[SCENE_TUTORIAL] == nullptr, "the tutorial no longer has a slot");

	FinishFade(manager);
}

static void TestOutOfRangePushIsIgnored(SceneManagement::SceneManager& manager)
{
	std::printf("an unknown scene id is ignored\n");

	SceneManagement::Scene* before = manager.currentScene;
	SceneManagement::SceneManager_push(&manager, SCENE_COUNT);
	SceneManagement::SceneManager_push(&manager, -1);

	Check(manager.currentScene == before, "the current scene is unchanged");
	Check(manager.nextSceneID == -1, "nothing is left pending");
	Check(manager.fade == SceneFade::None, "no transition starts");
}

static void TestAFactoryThatBuildsNothingKeepsTheCurrentScene(SceneManagement::SceneManager& manager)
{
	std::printf("a push the factory builds nothing for keeps the current scene\n");

	SceneManagement::Scene* before = manager.currentScene;
	manager.factory = BuildNothing;
	SceneManagement::SceneManager_push(&manager, SCENE_TUTORIAL);
	FinishFade(manager);

	Check(manager.currentScene == before, "the current scene is still current");
	Check(manager.scenes[SCENE_MAIN_MENU] == before, "and keeps its slot");

	manager.factory = BuildTestScene;
}

static void TestTheTickHandsOverTheSceneBeingPlayed(SceneManagement::SceneManager& manager)
{
	std::printf("a tick runs the current scene's simulation and hands it the scene\n");

	g_ticks = 0;
	g_tickedScene = nullptr;
	SceneManagement::SceneManager_tick(&manager, 1.0f / 60.0f);

	Check(g_ticks == 1, "the scene's own update ran once");
	Check(g_tickedScene == manager.currentScene, "and was handed the current scene");

	SceneManagement::SceneManager_frame(&manager, 1.0f / 60.0f);
	Check(g_ticks == 1, "a frame alone runs no simulation");
}

int main()
{
	// A scene's player binds a model when it is enabled, so a GL context has to
	// exist before any scene is built.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-scene-transition-tests");

	std::printf("Scene transition tests\n\n");

	auto& manager = SceneManagement::SceneManager::getInstance();
	TestStartUpBuildsOnlyTheMainMenu(manager);
	TestPushKeepsTheOutgoingSceneUntilTheMidpoint(manager);
	TestMidpointSwapsInTheRequestedScene(manager);
	TestTheFadeTakesTheSameTimeAtAnyFrameRate(manager);
	TestRepushingASceneBuildsAFreshInstance(manager);
	TestReturningToTheMainMenu(manager);
	TestOutOfRangePushIsIgnored(manager);
	TestAFactoryThatBuildsNothingKeepsTheCurrentScene(manager);
	TestTheTickHandsOverTheSceneBeingPlayed(manager);

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
