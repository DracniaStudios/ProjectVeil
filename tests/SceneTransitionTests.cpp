/**
 * Tests for when scenes are built during a transition.
 *
 * A push used to build the requested scene on the spot, mid-frame, and
 * Scene_new() makes the scene it builds current, so the incoming scene was in
 * use before the outgoing one had faded out. Building the main menu also fell
 * through into rebuilding the tutorial. These pin the fixed order: a push only
 * records the request, the scene is built exactly once at the faded-out
 * midpoint, and the scene it replaces is released.
 *
 * The midpoint is reached by calling SceneManager_transition(IN), which is what
 * SceneManager_update does once the fade-out completes; driving the update loop
 * itself would also tick the scenes, which needs the full game set up.
 *
 * Like EmitterTests this links against the game's object files and needs a GL
 * context for GameObject's fallback cube. See tests/run_tests.sh.
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

static bool IsScene(const SceneManagement::Scene* scene, const char* name)
{
	return scene != nullptr && std::string(scene->name) == name;
}

static void TestStartUpBuildsOnlyTheMainMenu(SceneManagement::SceneManager& manager)
{
	std::printf("start-up builds only the main menu\n");

	SceneManagement::SceneManager_init(&manager);

	Check(IsScene(manager.currentScene, "Main Menu"), "the main menu is current after start-up");
	Check(manager.scenes[SCENE_MAIN_MENU] == manager.currentScene, "the main menu fills its slot");
	Check(manager.scenes[SCENE_TUTORIAL] == nullptr, "the tutorial is not built at start-up");
	Check(manager.nextSceneID == -1, "no scene is left pending");
	Check(manager.transition->direction == IN, "start-up fades the main menu in");

	SceneManagement::SceneManager_transition(&manager, NONE);
}

static void TestPushKeepsTheOutgoingSceneUntilTheMidpoint(SceneManagement::SceneManager& manager)
{
	std::printf("a push leaves the outgoing scene running through its fade-out\n");

	SceneManagement::Scene* menu = manager.currentScene;
	SceneManagement::SceneManager_push(&manager, SCENE_TUTORIAL);

	Check(manager.currentScene == menu, "the main menu stays current while it fades out");
	Check(manager.scenes[SCENE_TUTORIAL] == nullptr, "the tutorial is not built before the midpoint");
	Check(manager.nextSceneID == SCENE_TUTORIAL, "the request is recorded");
	Check(manager.transition->direction == OUT, "the push starts a fade-out");
}

static void TestMidpointSwapsInTheRequestedScene(SceneManagement::SceneManager& manager)
{
	std::printf("the midpoint builds the requested scene once and releases the old one\n");

	SceneManagement::SceneManager_transition(&manager, IN);

	Check(IsScene(manager.currentScene, "Tutorial"), "the tutorial is current after the midpoint");
	Check(manager.scenes[SCENE_TUTORIAL] == manager.currentScene, "the tutorial fills its slot");
	Check(manager.scenes[SCENE_MAIN_MENU] == nullptr, "the replaced main menu no longer has a slot");
	Check(manager.nextSceneID == -1, "the request is consumed");
	Check(manager.transition->direction == IN, "the tutorial fades in");

	SceneManagement::SceneManager_transition(&manager, NONE);
}

static void TestRepushingASceneBuildsAFreshInstance(SceneManagement::SceneManager& manager)
{
	std::printf("pushing the current scene again reloads it\n");

	SceneManagement::Scene* first = manager.currentScene;
	SceneManagement::SceneManager_push(&manager, SCENE_TUTORIAL);
	Check(manager.currentScene == first, "the old instance runs until the midpoint");

	SceneManagement::SceneManager_transition(&manager, IN);
	Check(IsScene(manager.currentScene, "Tutorial"), "the tutorial is current again");
	Check(manager.currentScene != first, "it is a freshly built instance");
	Check(manager.scenes[SCENE_TUTORIAL] == manager.currentScene, "the slot holds the new instance");

	SceneManagement::SceneManager_transition(&manager, NONE);
}

static void TestReturningToTheMainMenu(SceneManagement::SceneManager& manager)
{
	std::printf("returning to the main menu releases the tutorial\n");

	SceneManagement::SceneManager_push(&manager, SCENE_MAIN_MENU);
	SceneManagement::SceneManager_transition(&manager, IN);

	Check(IsScene(manager.currentScene, "Main Menu"), "the main menu is current");
	Check(manager.scenes[SCENE_TUTORIAL] == nullptr, "the tutorial no longer has a slot");

	SceneManagement::SceneManager_transition(&manager, NONE);
}

static void TestOutOfRangePushIsIgnored(SceneManagement::SceneManager& manager)
{
	std::printf("an unknown scene id is ignored\n");

	SceneManagement::Scene* before = manager.currentScene;
	SceneManagement::SceneManager_push(&manager, SCENE_COUNT);
	SceneManagement::SceneManager_push(&manager, -1);

	Check(manager.currentScene == before, "the current scene is unchanged");
	Check(manager.nextSceneID == -1, "nothing is left pending");
	Check(manager.transition->direction == NONE, "no transition starts");
}

int main()
{
	// GameObject's constructor uploads a fallback cube, so a GL context has to
	// exist before any world object is built.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-scene-transition-tests");

	std::printf("Scene transition tests\n\n");

	auto& manager = SceneManagement::SceneManager::getInstance();
	TestStartUpBuildsOnlyTheMainMenu(manager);
	TestPushKeepsTheOutgoingSceneUntilTheMidpoint(manager);
	TestMidpointSwapsInTheRequestedScene(manager);
	TestRepushingASceneBuildsAFreshInstance(manager);
	TestReturningToTheMainMenu(manager);
	TestOutOfRangePushIsIgnored(manager);

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
