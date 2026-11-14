/**
 * Presentation independence (work order A-12; decisions D7 and D9, finding H2).
 *
 * The same World save must give the same content hash, the same tick digest and the
 * same mesh-collider values whether the client's presentation is installed or not.
 * The only presentation member the simulation reads is BindVisuals, which fits every
 * COLLIDER_MESH collider to the bound model. Headless, a mesh collider keeps the size
 * and offset its World save recorded. This test loads the real `models` folder, so
 * Run A fits the crane terminals to the real Terminal.glb, and asks whether that fit
 * equals what chunk_1.json saved (ruling R-9).
 *
 *   Run A  presentation installed, real models loaded.
 *   Run B  nothing installed: the saved values as loaded.
 *
 * Every value is printed. A disagreement is a result, not something to tune away: the
 * comparisons are exact (bit patterns) and have no tolerance.
 *
 * Test code only. Tier 3 (windowed): binding real models uploads them to the GPU.
 */

#include <raylib.h>

#include "TickDigest.h"

#include <AssetManager.h>
#include <Render/Render.h>
#include <Client/GamePresentation.h>
#include <sim/Presentation.h>
#include <sim/Worlds.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

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

namespace {

	struct MeshCollider
	{
		std::uint64_t id = 0;
		std::string name;
		std::string model;
		Vector3 size = {};
		Vector3 offset = {};
		float radius = 0.0f;
		int meshCount = 0;       // meshes the object's model holds: 0 means nothing was bound
		bool sharedAsset = false; // the model is the AssetManager's, not a generated cube
	};

	struct RunResult
	{
		bool loaded = false;
		std::uint64_t contentHash = 0;
		std::uint64_t digest = 0;
		std::vector<MeshCollider> meshColliders;   // ascending id
	};

	unsigned long long U(std::uint64_t v) { return static_cast<unsigned long long>(v); }

	// Builds `world` the way its Scene does, records what the World save made, ticks
	// `ticks` scripted ticks (0 for none), records the digest and deletes the scene.
	// `tutorial` selects Sim::LoadTutorialWorld (chunk_1) or the main menu's spawn point
	// and "mainMenu". The scene is deleted before this returns, so the models it
	// generated are released through whichever presentation created them.
	RunResult Run(bool tutorial, int ticks)
	{
		RunResult r;
		SceneManagement::Scene* scene = SceneManagement::Scene_new();
		if (tutorial)
		{
			r.loaded = Sim::LoadTutorialWorld(*scene);
		}
		else
		{
			scene->SetSpawnPoint(Vector3{0, 2, 0});
			r.loaded = SaveSystem::LoadWorld("mainMenu", *scene);
		}
		r.contentHash = scene->contentHash;

		const auto note = [&](GameObject& o)
		{
			if (o.rigidBody3D.collider.shape != COLLIDER_MESH) { return; }
			MeshCollider m;
			m.id = static_cast<std::uint64_t>(o.id);
			m.name = o.name;
			m.model = o.modelName;
			m.size = o.rigidBody3D.collider.size;
			m.offset = o.rigidBody3D.collider.offset;
			m.radius = o.rigidBody3D.collider.radius;
			m.meshCount = o.model ? 1 : 0; // the handle is one model: 1 means something was bound
			m.sharedAsset = o.model && !o.usesUnitCube;
			r.meshColliders.push_back(m);
		};
		scene->gameMap.ForEachGameObject(note);
		scene->gameMap.ForEachEntity(note);
		scene->gameMap.ForEachInteractable(note);
		std::sort(r.meshColliders.begin(), r.meshColliders.end(),
			[](const MeshCollider& a, const MeshCollider& b) { return a.id < b.id; });

		if (ticks > 0 && r.loaded)
		{
			for (int tick = 0; tick < ticks; ++tick)
			{
				const Sim::PlayerCommand cmd = TickDigest::CommandFor(tick);
				SceneManagement::Scene_tick(*scene, Sim::kTickDelta, std::span<const Sim::PlayerCommand>(&cmd, 1));
			}
			r.digest = TickDigest::Of(*scene);
		}
		SceneManagement::Scene_delete(scene);
		return r;
	}

	bool SameBits(float a, float b) { return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b); }

	// Compares one float component; prints both values and bit patterns on a mismatch
	bool CompareFloat(const MeshCollider& a, const MeshCollider& b, const char* what, float fitted, float saved)
	{
		if (SameBits(fitted, saved)) { return true; }
		std::printf("  MISMATCH id %llu \"%s\" %s: fitted %.9g (0x%08x)  saved %.9g (0x%08x)\n",
			U(a.id), a.name.c_str(), what,
			fitted, std::bit_cast<std::uint32_t>(fitted),
			saved, std::bit_cast<std::uint32_t>(saved));
		(void)b;
		return false;
	}

	bool CompareCollider(const MeshCollider& a, const MeshCollider& b)
	{
		bool same = true;
		same &= CompareFloat(a, b, "size.x", a.size.x, b.size.x);
		same &= CompareFloat(a, b, "size.y", a.size.y, b.size.y);
		same &= CompareFloat(a, b, "size.z", a.size.z, b.size.z);
		same &= CompareFloat(a, b, "offset.x", a.offset.x, b.offset.x);
		same &= CompareFloat(a, b, "offset.y", a.offset.y, b.offset.y);
		same &= CompareFloat(a, b, "offset.z", a.offset.z, b.offset.z);
		same &= CompareFloat(a, b, "radius", a.radius, b.radius);
		return same;
	}

	void PrintCollider(const char* label, const MeshCollider& m)
	{
		std::printf("    %s size (%.9g, %.9g, %.9g) offset (%.9g, %.9g, %.9g) radius %.9g\n", label,
			m.size.x, m.size.y, m.size.z, m.offset.x, m.offset.y, m.offset.z, m.radius);
	}

	void TestChunk1()
	{
		std::printf("chunk_1: presentation installed (real models) vs nothing installed\n");
		constexpr int kTicks = 3000;

		// Run A: the client's presentation, then delete its scene, all while installed
		Client::InstallGamePresentation();
		const RunResult a = Run(true, kTicks);
		// Run B: only after Run A's scene is gone
		Sim::InstallPresentation(nullptr);
		const RunResult b = Run(true, kTicks);

		Check(a.loaded, "Run A: chunk_1 loads");
		Check(b.loaded, "Run B: chunk_1 loads");
		std::printf("  Run A (presentation): content hash 0x%016llx  digest 0x%016llx\n", U(a.contentHash), U(a.digest));
		std::printf("  Run B (headless)    : content hash 0x%016llx  digest 0x%016llx\n", U(b.contentHash), U(b.digest));

		Check(a.contentHash != 0 && b.contentHash != 0, "both content hashes are set");
		Check(a.contentHash == b.contentHash, "the content hash is the same with and without the presentation");
		Check(a.digest != 0 && b.digest != 0, "both digests are non-zero");
		Check(a.digest == b.digest, "the tick digest is the same with and without the presentation");

		Check(!a.meshColliders.empty(), "chunk_1 has mesh colliders");
		Check(a.meshColliders.size() == b.meshColliders.size(), "both runs see the same number of mesh colliders ("
			+ std::to_string(a.meshColliders.size()) + " vs " + std::to_string(b.meshColliders.size()) + ")");

		std::printf("  mesh colliders (A = fitted, B = saved):\n");
		const std::size_t n = std::min(a.meshColliders.size(), b.meshColliders.size());
		for (std::size_t i = 0; i < n; ++i)
		{
			const MeshCollider& ma = a.meshColliders[i];
			const MeshCollider& mb = b.meshColliders[i];
			std::printf("  id %llu \"%s\" model \"%s\" (A: %d meshes, %s; B: %d meshes)\n", U(ma.id), ma.name.c_str(), ma.model.c_str(),
				ma.meshCount, ma.sharedAsset ? "shared asset" : "generated cube", mb.meshCount);
			PrintCollider("fitted", ma);
			PrintCollider("saved ", mb);
			// Guards against a vacuous pass: Run A really bound a model, Run B really did not
			Check(ma.meshCount > 0, "mesh collider id " + std::to_string(ma.id) + ": Run A bound a model");
			Check(mb.meshCount == 0, "mesh collider id " + std::to_string(mb.id) + ": Run B bound no model");
			if (!ma.model.empty()) { Check(ma.sharedAsset, "mesh collider id " + std::to_string(ma.id) + ": Run A bound the real " + ma.model + " asset, not a cube fallback"); }
			Check(ma.id == mb.id, "mesh collider " + std::to_string(i) + ": same object id in both runs");
			Check(CompareCollider(ma, mb), "mesh collider id " + std::to_string(ma.id) + " \"" + ma.name
				+ "\": fitted equals saved, bit for bit");
		}
	}

	void TestMainMenu()
	{
		std::printf("mainMenu: content hash with and without the presentation\n");
		Client::InstallGamePresentation();
		const RunResult a = Run(false, 0);
		Sim::InstallPresentation(nullptr);
		const RunResult b = Run(false, 0);

		Check(a.loaded && b.loaded, "mainMenu loads in both runs");
		std::printf("  Run A (presentation): content hash 0x%016llx, %zu mesh colliders\n", U(a.contentHash), a.meshColliders.size());
		std::printf("  Run B (headless)    : content hash 0x%016llx, %zu mesh colliders\n", U(b.contentHash), b.meshColliders.size());
		Check(a.contentHash != 0, "the content hash is set");
		Check(a.contentHash == b.contentHash, "the mainMenu content hash is the same with and without the presentation");
		Check(a.meshColliders.empty() && b.meshColliders.empty(), "mainMenu has no mesh colliders");
	}
}

int main()
{
	SetTraceLogLevel(LOG_ERROR);
	SetConfigFlags(FLAG_WINDOW_HIDDEN);
	InitWindow(64, 64, "veil-presentation-independence");
	Render::Init(); // the unit cube an object with no model asset binds

	// Models only (not the 4K textures). The lighting system is deliberately not
	// initialised: the renderer binds the shader at each draw, and the simulation never
	// reads it.
	const auto started = std::chrono::steady_clock::now();
	AssetManager::AssetManager::getInstance().loadFolder("models");
	int models = 0;
	for (const auto& asset : AssetManager::AssetManager::getInstance().assets)
	{
		if (asset.type == AssetManager::ASSET_MODEL) { ++models; }
	}
	std::printf("loaded %d models in %.1f s\n", models,
		std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
	Check(models > 0, "the models folder loaded");

	TestChunk1();
	TestMainMenu();

	Sim::InstallPresentation(nullptr);
	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	Render::Shutdown();
	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
