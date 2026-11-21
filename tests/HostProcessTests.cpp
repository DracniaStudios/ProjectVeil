/**
 * Several applications against one live SpacetimeDB: the Host's World store
 * across processes (MultiplayerSplitPlan.md W5, W-11; decisions Q49, Q54).
 *
 * The driver (no arguments) starts copies of this very executable as separate
 * processes, each a "Host" with its own identity file, plus the live
 * SpacetimeDB on 127.0.0.1:3000 with veil_world published. Skipped when nothing
 * answers there.
 *
 *   takeover      process A opens a fresh world and completes a station; process
 *                 B, another identity, opens the same world, becomes the owner,
 *                 sees A's station and completes another; process C, A's
 *                 identity again, takes it back and sees both.
 *   stale owner   A holds the world open while B takes it over; A's next write
 *                 is refused ("not the owner") and A's store Fails; B's
 *                 writes keep going through.
 *   host.lock     two processes sharing one lock file: the second Fails with
 *                 "another Host" while the first runs, and opens once the
 *                 first has exited.
 *
 * A child prints "key=value" lines to its stdout file; the driver reads them.
 *   HostProcessTests host <dir> <name> <identity> <lock> <key> <station> <holdMs> <beforeWriteMs>
 */

#include <net/spacetime/SpacetimeHttp.h>
#include <net/spacetime/SpacetimeWorldStore.h>
#include <World/WorldKey.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace st = Net::Spacetime;
namespace fs = std::filesystem;
using World::WorldStoreStatus;
using Clock = std::chrono::steady_clock;

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

template <class Fn>
static bool WaitFor(Fn done, double seconds)
{
	const auto end = Clock::now() + std::chrono::duration<double>(seconds);
	while (Clock::now() < end)
	{
		if (done()) { return true; }
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return done();
}

// ---- The child: one Host process ------------------------------------------

static const char* StatusName(WorldStoreStatus s)
{
	switch (s)
	{
	case WorldStoreStatus::Opening:  return "Opening";
	case WorldStoreStatus::Ready:    return "Ready";
	case WorldStoreStatus::Degraded: return "Degraded";
	case WorldStoreStatus::Failed:   return "Failed";
	case WorldStoreStatus::Closed:   return "Closed";
	}
	return "?";
}

static int RunChild(int argc, char** argv)
{
	if (argc < 11) { return 2; }
	const fs::path dir = argv[2];
	const std::string name = argv[3];
	const fs::path identity = argv[4];
	const fs::path lock = argv[5];
	const std::string key = argv[6];
	const std::uint64_t station = std::strtoull(argv[7], nullptr, 10);
	const int holdMs = std::atoi(argv[8]);
	const int beforeWriteMs = std::atoi(argv[9]);

	std::ofstream out(dir / (name + ".out"), std::ios::trunc);
	auto say = [&](const std::string& line) { out << line << std::endl; };

	st::StoreConfig config;
	config.identityFile = identity;
	config.lockFile = lock;
	config.firstBackoff = std::chrono::milliseconds(100);
	config.maxBackoff = std::chrono::milliseconds(400);
	st::SpacetimeWorldStore store(config);
	store.BeginOpen(key);
	WaitFor([&] { return store.Status() != WorldStoreStatus::Opening; }, 15.0);
	say(std::string("open=") + StatusName(store.Status()));
	say("error=" + store.LastError());

	World::WorldState state;
	if (store.ReadState(state))
	{
		std::set<std::uint64_t> sorted(state.completedStations.begin(), state.completedStations.end());
		std::string list;
		for (const std::uint64_t id : sorted) { list += (list.empty() ? "" : ",") + std::to_string(id); }
		say("stations=" + list);
	}
	say("READY");

	if (store.Status() == WorldStoreStatus::Ready && station != 0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(beforeWriteMs));
		store.SubmitStationCompleted(station);
		WaitFor([&] { return store.Pending() == 0 || store.Status() == WorldStoreStatus::Failed; }, 10.0);
		say(std::string("write=") + StatusName(store.Status()));
		say("writeError=" + store.LastError());
		say("confirmed=" + std::to_string(store.Confirmed()));
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
	store.Close(std::chrono::seconds(2));
	say("DONE");
	return 0;
}

// ---- The driver -----------------------------------------------------------

struct Child
{
	std::string name;
	std::future<int> exit;
	fs::path outFile;
};

static fs::path g_exe;
static fs::path g_dir;

static Child Launch(const std::string& name, const std::string& identityFile, const std::string& lockFile,
	const std::string& key, std::uint64_t station, int holdMs, int beforeWriteMs)
{
	Child child;
	child.name = name;
	child.outFile = g_dir / (name + ".out");
	fs::remove(child.outFile);

	std::ostringstream cmd;
#if defined(_WIN32)
	cmd << "\"";  // cmd.exe /c strips one outer pair of quotes
#endif
	cmd << "\"" << g_exe.string() << "\" host \"" << g_dir.string() << "\" " << name
		<< " \"" << (g_dir / identityFile).string() << "\" \"" << (g_dir / lockFile).string() << "\" "
		<< key << " " << station << " " << holdMs << " " << beforeWriteMs << " x";
#if defined(_WIN32)
	cmd << "\"";
#endif
	const std::string line = cmd.str();
	child.exit = std::async(std::launch::async, [line] { return std::system(line.c_str()); });
	return child;
}

static std::vector<std::string> Lines(const fs::path& file)
{
	std::vector<std::string> lines;
	std::ifstream in(file);
	std::string line;
	while (std::getline(in, line)) { lines.push_back(line); }
	return lines;
}

static std::string Value(const fs::path& file, const std::string& field)
{
	for (const std::string& line : Lines(file))
	{
		if (line.rfind(field + "=", 0) == 0) { return line.substr(field.size() + 1); }
	}
	return "<missing>";
}

static bool Has(const fs::path& file, const std::string& line)
{
	for (const std::string& l : Lines(file)) { if (l == line) { return true; } }
	return false;
}

// Blocks until the child has finished, then returns its output file
static const fs::path& Finish(Child& child)
{
	const int code = child.exit.get();
	Check(code == 0, child.name + " exited cleanly (code " + std::to_string(code) + ")");
	return child.outFile;
}

static bool WaitReady(const Child& child) { return WaitFor([&] { return Has(child.outFile, "READY"); }, 20.0); }

static std::string FreshKey(const char* id)
{
	return World::MakeWorldKey(id, static_cast<std::uint64_t>(Clock::now().time_since_epoch().count()));
}

static void TestTakeover()
{
	std::printf("takeover: three processes, two identities, one world\n");
	const std::string key = FreshKey("veil_proc_test");

	Child a = Launch("a1", "id_a.json", "a.lock", key, 302, 0, 0);
	const fs::path& aOut = Finish(a);
	Check(Value(aOut, "open") == "Ready", "A opens a fresh world: " + Value(aOut, "error"));
	Check(Value(aOut, "write") == "Ready" && Value(aOut, "confirmed") == "1", "A completes station 302");

	Child b = Launch("b1", "id_b.json", "b.lock", key, 312, 0, 0);
	const fs::path& bOut = Finish(b);
	Check(Value(bOut, "open") == "Ready", "B, another identity, opens the same world: " + Value(bOut, "error"));
	Check(Value(bOut, "stations") == "302", "and sees A's station: " + Value(bOut, "stations"));
	Check(Value(bOut, "write") == "Ready", "B's write goes through as the new owner: " + Value(bOut, "writeError"));

	Child c = Launch("a2", "id_a.json", "a.lock", key, 0, 0, 0);
	const fs::path& cOut = Finish(c);
	Check(Value(cOut, "open") == "Ready", "A's identity takes the world back: " + Value(cOut, "error"));
	Check(Value(cOut, "stations") == "302,312", "and sees both stations: " + Value(cOut, "stations"));
}

static void TestStaleOwner()
{
	std::printf("stale owner: the previous Host is refused once another takes over\n");
	const std::string key = FreshKey("veil_stale_test");

	// A opens, then waits 4 s before its write; B takes over meanwhile
	Child a = Launch("stale_a", "id_a.json", "a.lock", key, 700, 0, 4000);
	Check(WaitReady(a), "A is open");

	Child b = Launch("stale_b", "id_b.json", "b.lock", key, 701, 0, 0);
	const fs::path& bOut = Finish(b);
	Check(Value(bOut, "open") == "Ready" && Value(bOut, "write") == "Ready", "B takes over and writes: " + Value(bOut, "error") + " " + Value(bOut, "writeError"));

	const fs::path& aOut = Finish(a);
	Check(Value(aOut, "write") == "Failed", "A's later write Fails: " + Value(aOut, "write"));
	Check(Value(aOut, "writeError").find("not the owner") != std::string::npos, "with the reducer's reason: " + Value(aOut, "writeError"));

	// And the world holds B's station, not A's refused one
	Child c = Launch("stale_c", "id_c.json", "c.lock", key, 0, 0, 0);
	const fs::path& cOut = Finish(c);
	Check(Value(cOut, "stations") == "701", "the world holds only B's station: " + Value(cOut, "stations"));
}

static void TestHostLockAcrossProcesses()
{
	std::printf("host.lock: a second process on the same lock file is kept out\n");
	const std::string key = FreshKey("veil_lock_test");

	Child first = Launch("lock_first", "id_a.json", "shared.lock", key, 0, 3000, 0);
	Check(WaitReady(first), "the first Host is open");
	Check(Value(first.outFile, "open") == "Ready", "and Ready: " + Value(first.outFile, "error"));

	Child second = Launch("lock_second", "id_b.json", "shared.lock", key, 0, 0, 0);
	const fs::path& secondOut = Finish(second);
	Check(Value(secondOut, "open") == "Failed" && Value(secondOut, "error").find("another Host") != std::string::npos,
		"the second Host Fails with \"another Host\": " + Value(secondOut, "error"));

	Finish(first);

	Child third = Launch("lock_third", "id_b.json", "shared.lock", key, 0, 0, 0);
	const fs::path& thirdOut = Finish(third);
	Check(Value(thirdOut, "open") == "Ready", "once the first has exited the lock is free: " + Value(thirdOut, "error"));
}

int main(int argc, char** argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	if (argc >= 2 && std::string(argv[1]) == "host") { return RunChild(argc, argv); }

	std::printf("Host process tests\n\n");
	st::HttpClient probe("127.0.0.1", 3000);
	if (!probe.Sql("veil-world", "SELECT * FROM world WHERE world_key = 'none:0000000000000000'", "").transportOk)
	{
		std::printf("  SKIPPED: no SpacetimeDB on 127.0.0.1:3000\n");
		return 0;
	}

	g_exe = fs::absolute(argv[0]);
	g_dir = fs::temp_directory_path() / "veil-host-process-tests";
	std::error_code ignored;
	fs::remove_all(g_dir, ignored);
	fs::create_directories(g_dir);

	TestTakeover();
	TestStaleOwner();
	TestHostLockAcrossProcesses();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
