/**
 * The Host's World store (MultiplayerSplitPlan.md W5; rules W-1 to W-11;
 * decisions Q48, Q49, Q60).
 *
 * Live, against the Host PC's SpacetimeDB with veil_world published (skipped
 * when nothing answers on 127.0.0.1:3000):
 *   - the JSON round trip: the store's reducer calls are accepted by the module
 *     and the rows it reads back parse (Q51's boundary test)
 *   - completed stations survive the store closing and a new one opening
 *     (a Host restart); a Full wipe deletes them
 *   - a store with another identity is not the owner and Fails
 *   - host.lock keeps a second Host off the world
 *
 * Against a fake SpacetimeDB on 127.0.0.1:3901 (an httplib server in this test):
 *   - an outage leaves the store Degraded with the writes queued, and they
 *     flush when it comes back
 *   - a 530 (the reducer's Err) or a 400 sets Failed with no retry loop
 *   - Close keeps to its deadline when the server never answers
 *
 * No raylib header here: cpp-httplib brings <windows.h>.
 */

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif
#include <httplib.h>

#include <net/spacetime/SpacetimeHttp.h>
#include <net/spacetime/SpacetimeWorldStore.h>
#include <World/WorldKey.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
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

namespace st = Net::Spacetime;
using World::WorldStoreStatus;

static bool WaitFor(const std::function<bool()>& done, double seconds)
{
	const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
	while (std::chrono::steady_clock::now() < end)
	{
		if (done()) { return true; }
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return done();
}

static std::filesystem::path TempDir()
{
	const auto dir = std::filesystem::temp_directory_path() / "veil-world-store-tests";
	std::error_code ignored;
	std::filesystem::create_directories(dir, ignored);
	return dir;
}

static std::string FreshKey(const char* id)
{
	const auto now = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
	return World::MakeWorldKey(id, now);
}

static st::StoreConfig Config(const std::string& identityFile, int port = 3000)
{
	st::StoreConfig config;
	config.port = port;
	config.identityFile = TempDir() / identityFile;
	config.lockFile = TempDir() / (identityFile + ".lock");
	config.firstBackoff = std::chrono::milliseconds(100);
	config.maxBackoff = std::chrono::milliseconds(400);
	return config;
}

static bool Opened(st::SpacetimeWorldStore& store)
{
	WaitFor([&] { return store.Status() != WorldStoreStatus::Opening; }, 10.0);
	return store.Status() == WorldStoreStatus::Ready;
}

// ---- Live -----------------------------------------------------------------

static void TestLive()
{
	std::printf("live: veil_world on 127.0.0.1:3000\n");
	const std::string key = FreshKey("veil_store_test");

	{
		st::SpacetimeWorldStore store(Config("host_identity.json"));
		store.BeginOpen(key);
		Check(Opened(store), "a fresh world opens (" + store.LastError() + ")");
		World::WorldState state;
		Check(store.ReadState(state) && state.completedStations.empty(), "with no station completed");

		store.SubmitStationCompleted(302);
		store.SubmitStationCompleted(312);
		store.SubmitStationCompleted(302); // twice is the same row: the reducer is idempotent
		Check(WaitFor([&] { return store.Pending() == 0; }, 10.0), "the writes go through");
		Check(store.Confirmed() == 3 && store.Status() == WorldStoreStatus::Ready, "all three confirmed, still Ready");
		store.Close(std::chrono::seconds(5));
		Check(store.Status() == WorldStoreStatus::Closed, "closed");
	}

	{
		// The Host restarts: a new store on the same world key
		st::SpacetimeWorldStore store(Config("host_identity.json"));
		store.BeginOpen(key);
		Check(Opened(store), "the world opens again after the restart");
		World::WorldState state;
		store.ReadState(state);
		const std::set<std::uint64_t> done(state.completedStations.begin(), state.completedStations.end());
		Check(done == std::set<std::uint64_t>{ 302, 312 }, "both completed stations survived the restart");

		// Another Host on this PC is kept out by host.lock (W-11)
		st::SpacetimeWorldStore second(Config("host_identity.json"));
		second.BeginOpen(key);
		WaitFor([&] { return second.Status() != WorldStoreStatus::Opening; }, 10.0);
		Check(second.Status() == WorldStoreStatus::Failed && second.LastError().find("another Host") != std::string::npos, "a second Host on the same PC Fails: " + second.LastError());
		second.Close(std::chrono::milliseconds(0));

		store.SubmitFullWipe();
		Check(WaitFor([&] { return store.Pending() == 0; }, 10.0), "the wipe goes through");
		store.Close(std::chrono::seconds(5));
	}

	{
		st::SpacetimeWorldStore store(Config("host_identity.json"));
		store.BeginOpen(key);
		Check(Opened(store), "and opens once more");
		World::WorldState state;
		store.ReadState(state);
		Check(state.completedStations.empty(), "the Full wipe deleted the rows");
		store.Close(std::chrono::seconds(1));
	}

	{
		// Someone else's identity: not the owner (Q49)
		std::error_code ignored;
		std::filesystem::remove(TempDir() / "other_identity.json", ignored);
		st::SpacetimeWorldStore stranger(Config("other_identity.json"));
		stranger.BeginOpen(key);
		WaitFor([&] { return stranger.Status() != WorldStoreStatus::Opening; }, 10.0);
		Check(stranger.Status() == WorldStoreStatus::Failed && stranger.LastError().find("not the owner") != std::string::npos,
			"another identity can't open the world: " + stranger.LastError());
		World::WorldState state;
		Check(!stranger.ReadState(state), "and reads nothing");
		stranger.Close(std::chrono::milliseconds(0));
	}

	{
		st::SpacetimeWorldStore bad(Config("host_identity.json"));
		bad.BeginOpen("Not A Key");
		Check(bad.Status() == WorldStoreStatus::Failed, "a malformed world key Fails at once");
	}
}

// ---- A fake SpacetimeDB ---------------------------------------------------

struct FakeSpacetime
{
	int port = 3901;
	std::unique_ptr<httplib::Server> server;
	std::thread thread;
	std::mutex lock;
	std::set<std::uint64_t> completed;
	std::atomic<int> callStatus{ 200 }; // what complete_station answers
	std::string callBody;
	std::atomic<int> calls{ 0 };

	void Start()
	{
		server = std::make_unique<httplib::Server>();
		server->Post("/v1/identity", [](const httplib::Request&, httplib::Response& res) {
			res.set_content(R"({"identity":"c200aa","token":"fake-token"})", "application/json");
		});
		server->Post(R"(/v1/database/([^/]+)/call/(\w+))", [this](const httplib::Request& req, httplib::Response& res) {
			const std::string reducer = req.matches[2];
			const nlohmann::json args = nlohmann::json::parse(req.body, nullptr, false);
			if (reducer == "open_world") { res.status = 200; return; }
			++calls;
			if (callStatus != 200) { res.status = callStatus; res.set_content(callBody, "text/plain"); return; }
			std::lock_guard guard(lock);
			if (reducer == "complete_station" && args.is_array() && args.size() == 2) { completed.insert(args[1].get<std::uint64_t>()); }
			if (reducer == "reset_world") { completed.clear(); }
			res.status = 200;
		});
		// One statement per request, as SpacetimeDB takes them
		server->Post(R"(/v1/database/([^/]+)/sql)", [this](const httplib::Request& req, httplib::Response& res) {
			std::lock_guard guard(lock);
			const auto column = [](const char* name) { return nlohmann::json{ { "name", { { "some", name } } } }; };
			nlohmann::json result;
			if (req.body.find("FROM station_state") != std::string::npos)
			{
				nlohmann::json stations = nlohmann::json::array();
				for (const auto id : completed) { stations.push_back(nlohmann::json::array({ "k/" + std::to_string(id), "k", id })); }
				result = { { "schema", { { "elements", nlohmann::json::array({ column("key"), column("world_key"), column("station_id") }) } } }, { "rows", stations } };
			}
			else
			{
				result = { { "schema", { { "elements", nlohmann::json::array({ column("world_key"), column("owner") }) } } },
					{ "rows", nlohmann::json::array({ nlohmann::json::array({ "k", nlohmann::json::array({ "0xC200AA" }) }) }) } };
			}
			res.set_content(nlohmann::json::array({ result }).dump(), "application/json");
		});
		httplib::Server* s = server.get();
		thread = std::thread([s, p = port] { s->listen("127.0.0.1", p); });
		server->wait_until_ready();
	}

	void Stop()
	{
		if (!server) { return; }
		server->stop();
		thread.join();
		server.reset();
	}

	~FakeSpacetime() { Stop(); }
};

static void TestOutageQueuesThenFlushes()
{
	std::printf("fake: an outage queues the writes; they flush when it's back\n");
	std::error_code ignored;
	std::filesystem::remove(TempDir() / "fake_identity.json", ignored);

	FakeSpacetime fake;
	fake.Start();
	st::SpacetimeWorldStore store(Config("fake_identity.json", fake.port));
	store.BeginOpen("fake_world:0000000000000001");
	Check(Opened(store), "the store opens against the fake (" + store.LastError() + ")");

	fake.Stop(); // SpacetimeDB goes away mid-session
	const auto started = std::chrono::steady_clock::now();
	store.SubmitStationCompleted(7);
	store.SubmitStationCompleted(8);
	const double submitMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
	Check(submitMs < 5.0, "Submit returns at once even with the server gone (" + std::to_string(submitMs) + " ms)");
	Check(WaitFor([&] { return store.Status() == WorldStoreStatus::Degraded; }, 5.0), "the store goes Degraded");
	World::WorldState state;
	Check(store.ReadState(state), "and can still be read: play goes on");
	Check(store.Pending() == 2, "both writes wait in the queue");

	fake.Start(); // and comes back
	Check(WaitFor([&] { return store.Pending() == 0; }, 10.0), "the queue flushes once it's back");
	Check(store.Status() == WorldStoreStatus::Ready, "and the store is Ready again");
	{
		std::lock_guard guard(fake.lock);
		Check(fake.completed == std::set<std::uint64_t>{ 7, 8 }, "both writes landed, in order");
	}
	store.Close(std::chrono::seconds(1));
	fake.Stop();
}

static void TestRefusalsFailWithoutRetrying()
{
	std::printf("fake: a 530 or a 400 sets Failed, with no retry loop\n");
	for (const int code : { 530, 400 })
	{
		FakeSpacetime fake;
		fake.Start();
		st::SpacetimeWorldStore store(Config("fake_identity.json", fake.port));
		store.BeginOpen("fake_world:0000000000000002");
		Opened(store);
		fake.callStatus = code;
		fake.callBody = code == 530 ? "not the owner of this world" : "invalid arguments";
		store.SubmitStationCompleted(9);
		Check(WaitFor([&] { return store.Status() == WorldStoreStatus::Failed; }, 5.0), "HTTP " + std::to_string(code) + " sets Failed");
		std::this_thread::sleep_for(std::chrono::milliseconds(600));
		Check(fake.calls == 1, "after exactly one try (" + std::to_string(fake.calls.load()) + ")");
		Check(store.LastError().find(fake.callBody) != std::string::npos, "and the reducer's message is shown: " + store.LastError());
		store.Close(std::chrono::milliseconds(0));
		fake.Stop();
	}
}

static void TestCloseKeepsItsDeadline()
{
	std::printf("fake: Close stops at its deadline when the server never answers\n");
	FakeSpacetime fake;
	fake.Start();
	st::SpacetimeWorldStore store(Config("fake_identity.json", fake.port));
	store.BeginOpen("fake_world:0000000000000003");
	Opened(store);
	fake.Stop();
	store.SubmitStationCompleted(11);

	const auto started = std::chrono::steady_clock::now();
	store.Close(std::chrono::milliseconds(500));
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
	Check(ms >= 450.0 && ms < 3500.0, "Close took about its deadline (" + std::to_string(ms) + " ms)");
	Check(store.Status() == WorldStoreStatus::Closed && store.Pending() == 0, "and the store is Closed");
	store.Close(std::chrono::milliseconds(0));
	Check(store.Status() == WorldStoreStatus::Closed, "Close twice is fine");
	store.SubmitStationCompleted(12);
	Check(store.Pending() == 0, "a Submit after Close is ignored");
}

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::printf("SpacetimeDB World store tests\n\n");

	st::HttpClient probe("127.0.0.1", 3000);
	if (probe.Sql("veil-world", "SELECT * FROM world WHERE world_key = 'none:0000000000000000'", "").transportOk) { TestLive(); }
	else { std::printf("  SKIPPED live: no SpacetimeDB on 127.0.0.1:3000\n"); }

	TestOutageQueuesThenFlushes();
	TestRefusalsFailWithoutRetrying();
	TestCloseKeepsItsDeadline();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
