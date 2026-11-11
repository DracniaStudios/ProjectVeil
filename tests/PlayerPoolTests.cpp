/**
 * Players by Slot (MultiplayerSplitPlan.md G5; DedicatedServerPlan.md Steps
 * 2.1 and 2.9): the Scene holds a player per Slot instead of one player, each
 * with an id from its Slot, and only the client's accessor knows which one is
 * "mine" (rule R2).
 */

#include <raylib.h>

#include <Scene.h>
#include <Client/LocalPlayer.h>
#include <sim/Slots.h>
#include <sim/FixedTick.h>

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

static void TestSlotsAndIds()
{
	std::printf("players live in Slots with ids from their Slot\n");

	SceneManagement::Scene* scene = SceneManagement::Scene_new();
	Check(scene->PlayerCount() == 1 && scene->PlayerInSlot(0) != nullptr, "a new scene has the Slot 0 player only");
	Check(scene->PlayerInSlot(0)->id == Sim::PlayerIdForSlot(0), "whose id is Slot 0's");
	Check(Sim::PlayerIdForSlot(0) >= FIRST_OBJECT_ID + OBJECT_LIMIT, "player ids sit far above the GameMap's");

	Player* second = scene->AddPlayer(2);
	Check(second != nullptr && second->id == Sim::PlayerIdForSlot(2), "a joining player takes its Slot's id");
	Check(scene->AddPlayer(2) == second, "adding to a taken Slot returns the player already there");
	Check(scene->AddPlayer(Sim::kMaxPlayers) == nullptr && scene->AddPlayer(-1) == nullptr, "a Slot outside the ceiling is refused");
	Check(scene->PlayerCount() == 2, "two players now");

	Check(scene->FindPlayer(second->id) == second, "a player is found by id");
	Check(scene->FindPlayer(12345) == nullptr, "a world id finds no player");
	Check(Sim::SlotForPlayerId(second->id) == 2 && Sim::SlotForPlayerId(7) == -1, "ids map back to Slots");

	int visited = 0;
	int lastSlot = -1;
	bool inOrder = true;
	scene->ForEachPlayer([&](int slot, Player&) { ++visited; inOrder = inOrder && slot > lastSlot; lastSlot = slot; });
	Check(visited == 2 && inOrder, "ForEachPlayer visits every player in Slot order");

	scene->SetSpawnPoint(Vector3{ 10, 2, 0 });
	Check(Vector3Distance(scene->PlayerInSlot(0)->getPosition(), Vector3{ 10, 2, 0 }) < 0.01f, "Slot 0 spawns on the spawn point");
	Check(Vector3Distance(second->getPosition(), scene->PlayerInSlot(0)->getPosition()) > 1.0f, "another Slot spawns clear of Slot 0");

	// The client's accessor, and nothing else, says which player is local
	Check(Client::LocalPlayer(*scene) == scene->PlayerInSlot(0), "the local player defaults to Slot 0");
	Client::SetLocalSlot(2);
	Check(Client::LocalPlayer(*scene) == second, "a joined client's local player is its Welcome slot");
	Client::SetLocalSlot(0);

	scene->RemovePlayer(2);
	Check(scene->PlayerInSlot(2) == nullptr && scene->PlayerCount() == 1, "removing a player frees its Slot");

	SceneManagement::Scene_delete(scene);
}

static void TestEveryPlayerTicksAndCollides()
{
	std::printf("every player ticks on its own command and is in the collision solve\n");

	SceneManagement::Scene* scene = SceneManagement::Scene_new();
	Player* a = scene->PlayerInSlot(0);
	Player* b = scene->AddPlayer(1);
	for (Player* p : { a, b }) { p->rigidBody3D.SetGravity(0.0f, 0.0f, 0.0f); }

	const Vector3 aStart = a->getPosition();
	const Vector3 bStart = b->getPosition();

	Sim::PlayerCommand commands[2] = {};
	commands[1].moveY = 1; // only Slot 1 walks
	for (int i = 0; i < 30; ++i)
	{
		SceneManagement::Scene_tick(*scene, Sim::kTickDelta, std::span<const Sim::PlayerCommand>(commands, 2));
	}
	// Walking is planar; height is gravity's and the floor's business
	const auto planar = [](Vector3 p, Vector3 q) { return Vector2Distance(Vector2{ p.x, p.z }, Vector2{ q.x, q.z }); };
	Check(planar(a->getPosition(), aStart) < 0.01f, "Slot 0 stands still on an empty command");
	Check(planar(b->getPosition(), bStart) > 0.5f, "Slot 1 walks on its own command");

	// Two players pushed into each other are separated by the solver
	b->rigidBody3D.Teleport(Vector3Add(a->getPosition(), Vector3{ 0.3f, 0.0f, 0.0f }));
	scene->SolveCollisions(8);
	Check(Vector3Distance(a->getPosition(), b->getPosition()) > 0.1f, "two overlapping players are pushed apart");

	SceneManagement::Scene_delete(scene);
}

int main()
{
	SetTraceLogLevel(LOG_ERROR);
	SetConfigFlags(FLAG_WINDOW_HIDDEN);
	InitWindow(64, 64, "veil-player-pool-tests");

	std::printf("Player pool tests\n\n");

	TestSlotsAndIds();
	TestEveryPlayerTicksAndCollides();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
