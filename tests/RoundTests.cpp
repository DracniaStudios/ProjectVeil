/**
 * The round: catches, the Full wipe and the escape (MultiplayerSplitPlan.md G3,
 * G6; rules C-1, C-2, C-3; VerticalSliceDecisions decision 3).
 *
 *   - A catch ends the round with a Full wipe: every item of decision 3 that
 *     exists in code is back as authored afterwards (tamper state, the tool and
 *     the tamper count arrive with the tamper loop in 2027).
 *   - Two catches in one tick are one RoundOver and one epoch (T2.2-lite, G6).
 *   - The Exit is locked until its lock is open; walking in then wins the round,
 *     and a won round counts nothing more.
 *
 * The wipe is tested on the real World save (chunk_1), so "as authored" means
 * what the level designer saved, not a fixture.
 */

#include <raylib.h>

#include <Scene.h>
#include <SaveSystem.h>
#include <AI/Stalker.h>
#include <sim/FixedTick.h>

#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <Client/GamePresentation.h>

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

static Stalker* FindStalker(SceneManagement::Scene& scene)
{
	Stalker* found = nullptr;
	scene.gameMap.ForEachEntity([&](Entity& entity) {
		if (entity.kind == ENTITYKIND_STALKER) { found = static_cast<Stalker*>(&entity); return false; }
		return true;
	});
	return found;
}

static int CountEvents(const std::vector<Sim::GameEvent>& events, Sim::GameEventType type)
{
	int n = 0;
	for (const auto& e : events) { if (e.type == type) { ++n; } }
	return n;
}

static std::set<std::uint64_t> AllIds(SceneManagement::Scene& scene)
{
	std::set<std::uint64_t> ids;
	scene.gameMap.ForEachObject([&](GameObject& object) { ids.insert(object.id); });
	return ids;
}

static void TestCatchFullWipesTheRealWorld()
{
	std::printf("a catch Full-wipes chunk_1 back to its authored state\n");

	SceneManagement::Scene* scene = SceneManagement::Scene_new();
	scene->SetSpawnPoint(Vector3(20, 2, 0));
	if (!SaveSystem::LoadWorld("chunk_1", *scene)) { Check(false, "chunk_1 loads"); SceneManagement::Scene_delete(scene); return; }

	const std::set<std::uint64_t> authoredIds = AllIds(*scene);
	const std::size_t authoredCount = authoredIds.size();
	Player* player = scene->PlayerInSlot(0);

	// --- Dirty everything decision 3 names --------------------------------
	// Task progress: a station completed, and its Target toggled
	InteractableObject* station = scene->gameMap.FindInteractable(312); // Stalker Door Terminal -> 314
	GameObject* door = scene->gameMap.FindWorldObject(314);
	Check(station != nullptr && door != nullptr, "chunk_1 has the Stalker Door Terminal and its door");
	if (station && door)
	{
		Check(!station->isCompleted && door->isEnabled, "authored: the station is open and the door shut");
		station->isCompleted = true;
		station->isInteractable = false;
		door->onDisable();
	}

	// The artifact pickup taken (the Unlock destroys itself, the player gains the artifact)
	InteractableObject* pickup = scene->gameMap.FindInteractable(311);
	Check(pickup != nullptr && pickup->interactType == INTERACT_UNLOCK, "chunk_1 has the artifact pickup");
	if (pickup) { pickup->onInteract(*scene, *player); }
	scene->gameMap.SweepDestroyed(scene);
	Check(scene->gameMap.FindInteractable(311) == nullptr && player->artifact != nullptr, "the pickup is gone and the player holds the artifact");

	// Something spawned at runtime
	GameObject spawnedPrototype = {};
	spawnedPrototype.name = "Runtime Crate";
	const std::uint64_t spawnedId = scene->gameMap.SpawnGameObject(spawnedPrototype)->id;

	// The player hurt, buffed and walked off
	player->rigidBody3D.Teleport(Vector3(40, 2, 10));
	player->health = 1.0f;
	if (CooldownTimer* buff = player->getBuff(BUFF_MOVEMENT)) { buff->use(); }
	player->SetLookAngles(1.0f, 0.5f);
	player->flashlightOn = true;

	// The stalker hunting, the Director with memory, noise in the air
	Stalker* stalker = FindStalker(*scene);
	Check(stalker != nullptr, "chunk_1 has a stalker");
	if (stalker)
	{
		stalker->state = STALKER_HUNT;
		stalker->hasLastKnown = true;
		stalker->lastKnownPosition = Vector3(1, 0, 1);
	}
	for (int i = 0; i < 120; ++i) { scene->director.Update(scene, Sim::kTickDelta); }
	scene->soundField.Emit(Vector3(0, 0, 0), 1.0f, SOUND_FOOTSTEP, player->id);

	// --- The catch --------------------------------------------------------
	scene->RaiseCatch(player->id);
	scene->ResolveRound();
	const std::vector<Sim::GameEvent> events = scene->TakeEvents();

	Check(CountEvents(events, Sim::GameEventType::PlayerCaught) == 1, "one PlayerCaught");
	Check(!events.empty() && events.front().slot == 0, "for Slot 0");
	Check(CountEvents(events, Sim::GameEventType::RoundOver) == 1, "one RoundOver");
	Check(!events.empty() && events.back().outcome == Sim::RoundOutcome::Caught && events.back().roundEpoch == 0, "RoundOver(Caught) for round 0");
	Check(scene->roundEpoch == 1, "the epoch moves on");

	// --- Everything back --------------------------------------------------
	Check(AllIds(*scene) == authoredIds, "exactly the authored objects exist, every one under its own id");
	Check(scene->gameMap.FindWorldObject(spawnedId) == nullptr || authoredIds.count(spawnedId) == 1, "the runtime object is gone");
	Check(AllIds(*scene).size() == authoredCount, "nothing extra was left behind");

	station = scene->gameMap.FindInteractable(312);
	door = scene->gameMap.FindWorldObject(314);
	Check(station && !station->isCompleted && station->isInteractable, "task progress is cleared: the station is open again");
	Check(door && door->isEnabled, "and its door is shut again");
	Check(scene->gameMap.FindInteractable(311) != nullptr, "the artifact pickup is back");

	player = scene->PlayerInSlot(0);
	Check(Vector3Distance(player->getPosition(), scene->SpawnPointFor(0)) < 0.01f, "the player is back at spawn");
	Check(std::fabs(player->health - player->getMaxHealth()) < 0.01f, "at full health");
	const CooldownTimer* buff = player->getBuff(BUFF_MOVEMENT);
	Check(buff == nullptr || buff->remaining_time() <= 0, "with no buff running");
	Check(player->artifact == nullptr && player->artifactUnlocked == -1, "without the artifact");
	Check(player->lookYaw == 0.0f && player->lookPitch == 0.0f && !player->flashlightOn, "looking down +Z, flashlight off");

	stalker = FindStalker(*scene);
	Check(stalker && stalker->state == STALKER_PATROL && !stalker->hasLastKnown, "the stalker is back to Patrol with no belief");
	Check(scene->director.Log().empty() && scene->director.Now() == 0.0f, "the Director's memory is cleared");
	Check(scene->soundField.Events().empty(), "the sound field is quiet");
	Check(scene->CaughtThisTick().empty(), "the catch is used up");

	SceneManagement::Scene_delete(scene);
}

static void TestTwoCatchesInOneTickAreOneRound()
{
	std::printf("T2.2-lite: two Slots caught in one tick -> one RoundOver, one epoch\n");

	SceneManagement::Scene* scene = SceneManagement::Scene_new();
	scene->AddPlayer(1);
	scene->RaiseCatch(Sim::PlayerIdForSlot(0));
	scene->RaiseCatch(Sim::PlayerIdForSlot(1));
	scene->RaiseCatch(Sim::PlayerIdForSlot(1)); // twice in one tick is one catch
	scene->ResolveRound();
	const auto events = scene->TakeEvents();

	Check(CountEvents(events, Sim::GameEventType::PlayerCaught) == 2, "each Slot's catch is reported once");
	Check(CountEvents(events, Sim::GameEventType::RoundOver) == 1, "but there is one RoundOver");
	Check(scene->roundEpoch == 1, "and the epoch moves on by one");

	scene->ResolveRound();
	Check(scene->TakeEvents().empty() && scene->roundEpoch == 1, "a tick with nothing to resolve reports nothing");

	SceneManagement::Scene_delete(scene);
}

// A shut door, an Exit behind it locked by the door, and a station that opens it
struct ExitRoom
{
	SceneManagement::Scene* scene = nullptr;
	GameObject* door = nullptr;
	InteractableObject* exit = nullptr;
};

static ExitRoom MakeExitRoom(bool withLock)
{
	ExitRoom room;
	room.scene = SceneManagement::Scene_new();

	GameObject door = {};
	door.name = "Exit Door";
	door.rigidBody3D.Teleport(Vector3(0, 1, 10));
	room.door = room.scene->gameMap.SpawnGameObject(door);

	InteractableObject station(INTERACT_MINIGAME, 0, 0);
	station.name = "Exit Terminal";
	station.rigidBody3D.Teleport(Vector3(5, 1, 0));
	room.scene->gameMap.SpawnInteractable(station);

	InteractableObject exit(INTERACT_EXIT, 0, 0);
	exit.name = "Exit";
	exit.targetId = withLock ? room.door->id : 0;
	exit.rigidBody3D.Teleport(Vector3(0, 1, 20));
	exit.rigidBody3D.scale = Vector3(4, 4, 4);
	exit.rigidBody3D.collider.mode = COLLIDER_TRIGGER;
	exit.rigidBody3D.SyncBroadPhaseBox();
	room.exit = room.scene->gameMap.SpawnInteractable(exit);
	room.exit->rigidBody3D.SyncBroadPhaseBox();
	return room;
}

static void PutPlayerInExit(ExitRoom& room)
{
	Player* player = room.scene->PlayerInSlot(0);
	player->rigidBody3D.Teleport(Vector3(0, 1, 20));
	player->rigidBody3D.SyncBroadPhaseBox();
}

static void TestExitLockAndEscape()
{
	std::printf("the Exit is locked until its door opens; escaping wins the round\n");

	ExitRoom room = MakeExitRoom(true);
	SceneManagement::Scene& scene = *room.scene;
	PutPlayerInExit(room);

	Check(!room.exit->IsExitUnlocked(scene.gameMap), "the Exit is locked while its door is shut");
	scene.CheckEscapes();
	scene.ResolveRound();
	Check(scene.TakeEvents().empty() && scene.roundEpoch == 0, "standing in a locked Exit escapes nothing");

	room.door->onDisable(); // the station's Target toggled: the door opens
	Check(room.exit->IsExitUnlocked(scene.gameMap), "the open door unlocks the Exit");
	scene.CheckEscapes();
	scene.ResolveRound();
	const auto events = scene.TakeEvents();
	Check(events.size() == 1 && events[0].type == Sim::GameEventType::RoundOver && events[0].outcome == Sim::RoundOutcome::Escaped, "walking in sends RoundOver(Escaped)");
	Check(scene.roundWon && scene.roundEpoch == 1, "the round is won and the epoch moves on");

	// A won round counts nothing more
	scene.RaiseCatch(scene.PlayerInSlot(0)->id);
	scene.ResolveRound();
	Check(scene.TakeEvents().empty() && scene.roundEpoch == 1, "a catch after the escape is ignored");

	SceneManagement::Scene_delete(room.scene);
}

static void TestCatchBeatsEscapeInOneTick()
{
	std::printf("a catch and an escape in one tick count as the catch\n");

	ExitRoom room = MakeExitRoom(true);
	room.door->onDisable();
	PutPlayerInExit(room);
	room.scene->AddPlayer(1);

	room.scene->RaiseCatch(Sim::PlayerIdForSlot(1));
	room.scene->CheckEscapes();
	room.scene->ResolveRound();
	const auto events = room.scene->TakeEvents();
	Check(!events.empty() && events.back().outcome == Sim::RoundOutcome::Caught, "the round ends Caught");

	SceneManagement::Scene_delete(room.scene);
}

static void TestUnlockedExitWithNoLockNeedsEveryStation()
{
	std::printf("an Exit with no lock opens once every Task station is completed\n");

	ExitRoom room = MakeExitRoom(false);
	Check(!room.exit->IsExitUnlocked(room.scene->gameMap), "locked while a station is outstanding");
	room.scene->gameMap.ForEachInteractable([](InteractableObject& s) { if (s.interactType == INTERACT_MINIGAME) { s.isCompleted = true; } });
	Check(room.exit->IsExitUnlocked(room.scene->gameMap), "unlocked once every station is completed");

	room.exit->isEnabled = false;
	Check(!room.exit->IsExitUnlocked(room.scene->gameMap), "a disabled Exit is never unlocked");

	SceneManagement::Scene_delete(room.scene);
}

int main()
{
	SetTraceLogLevel(LOG_ERROR);
	SetConfigFlags(FLAG_WINDOW_HIDDEN);
	InitWindow(64, 64, "veil-round-tests");
	Client::InstallGamePresentation();

	std::printf("Round tests\n\n");

	TestCatchFullWipesTheRealWorld();
	TestTwoCatchesInOneTickAreOneRound();
	TestExitLockAndEscape();
	TestCatchBeatsEscapeInOneTick();
	TestUnlockedExitWithNoLockNeedsEveryStation();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
