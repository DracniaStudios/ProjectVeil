// Nothing from raylib here: this file pulls in <windows.h> for the lock file
// (rule R8 keeps it to HTTP, JSON and the OS).
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#include "SpacetimeWorldStore.h"
#include "SpacetimeHttp.h"

#include <World/WorldKey.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>

namespace Net::Spacetime {

	using World::WorldStoreStatus;

	namespace {
		// "0xc200ab" and "c200AB" are the same identity
		std::string NormalizeIdentity(std::string text)
		{
			if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) { text.erase(0, 2); }
			for (char& c : text) { if (c >= 'A' && c <= 'F') { c = static_cast<char>(c - 'A' + 'a'); } }
			return text;
		}

		// The owner column is a Product holding the identity: ["0x..."]
		std::string OwnerOf(const nlohmann::json& value)
		{
			if (value.is_string()) { return NormalizeIdentity(value.get<std::string>()); }
			if (value.is_array() && value.size() == 1 && value[0].is_string()) { return NormalizeIdentity(value[0].get<std::string>()); }
			return {};
		}

		bool Retryable(int status) { return status == 500 || status == 502 || status == 503 || status == 504 || status == 408 || status == 429; }
	}

	SpacetimeWorldStore::SpacetimeWorldStore(StoreConfig configuration)
		: config(std::move(configuration)), http(std::make_unique<HttpClient>(config.host, config.port))
	{
	}

	SpacetimeWorldStore::~SpacetimeWorldStore()
	{
		Close(std::chrono::milliseconds(0));
	}

	std::string SpacetimeWorldStore::LastError() const
	{
		std::lock_guard guard(lock);
		return error;
	}

	void SpacetimeWorldStore::Fail(const std::string& why)
	{
		{
			std::lock_guard guard(lock);
			error = why;
		}
		status.store(WorldStoreStatus::Failed, std::memory_order_release);
		std::cerr << "[WorldStore] Failed: " << why << "\n";
	}

	// ---- The tick's side ----------------------------------------------------

	void SpacetimeWorldStore::BeginOpen(std::string_view worldKey)
	{
		{
			std::lock_guard guard(lock);
			if (opened || closed) { return; } // once per store (W-5)
			opened = true;
			key = std::string(worldKey);
		}

		if (!World::IsValidWorldKey(worldKey))
		{
			Fail("the world key is not <world_id>:<16 hex digits>");
			return;
		}

		status.store(WorldStoreStatus::Opening, std::memory_order_release);
		worker = std::jthread([this](std::stop_token stop) { Run(stop); });
	}

	bool SpacetimeWorldStore::ReadState(World::WorldState& out) const
	{
		const WorldStoreStatus now = Status();
		if (now != WorldStoreStatus::Ready && now != WorldStoreStatus::Degraded) { return false; } // W-6
		std::lock_guard guard(lock);
		out = loaded;
		return true;
	}

	void SpacetimeWorldStore::Submit(Entry entry)
	{
		const WorldStoreStatus now = Status();
		// W-6: the server applies the state, then ticks; nothing is submitted before
		assert((now != WorldStoreStatus::Opening) && "Submit before the World store is Ready (W-6)");
		if (now != WorldStoreStatus::Ready && now != WorldStoreStatus::Degraded) { return; }

		// W-1: a short lock, no I/O, no logging while it is held
		{
			std::lock_guard guard(lock);
			if (closing || closed) { return; } // after Close: ignored (W-11)
			entry.serial = nextSerial++;
			queue.push_back(entry);
			pending.store(queue.size(), std::memory_order_release);
		}
		wake.notify_all();
	}

	void SpacetimeWorldStore::SubmitStationCompleted(std::uint64_t stationId)
	{
		Submit(Entry{ Entry::Kind::StationCompleted, stationId, 0 });
	}

	void SpacetimeWorldStore::SubmitFullWipe()
	{
		Submit(Entry{ Entry::Kind::FullWipe, 0, 0 });
	}

	void SpacetimeWorldStore::Close(std::chrono::milliseconds flushDeadline)
	{
		{
			std::lock_guard guard(lock);
			if (closed) { return; } // idempotent
			closing = true;
		}
		wake.notify_all();

		if (worker.joinable())
		{
			// Flush until the queue is empty or the deadline passes (W-7)
			const auto deadline = std::chrono::steady_clock::now() + flushDeadline;
			while (Pending() > 0 && std::chrono::steady_clock::now() < deadline
				&& Status() != WorldStoreStatus::Failed && Status() != WorldStoreStatus::Opening)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			// Then stop the request in flight. One applied after this is harmless:
			// rows win at the next open.
			worker.request_stop();
			wake.notify_all();
			http->Stop();
			worker.join();
		}

		std::deque<Entry> unsent;
		{
			std::lock_guard guard(lock);
			unsent.swap(queue);
			pending.store(0, std::memory_order_release);
			closed = true;
		}
		if (!unsent.empty())
		{
			// Kind and station id only; never a token (W-10)
			std::ostringstream line;
			line << "[WorldStore] " << unsent.size() << " write(s) not sent:";
			for (const Entry& entry : unsent)
			{
				if (entry.kind == Entry::Kind::FullWipe) { line << " wipe"; }
				else { line << " complete " << entry.stationId; }
			}
			std::cerr << line.str() << "\n";
		}

		ReleaseLock();
		if (Status() != WorldStoreStatus::Failed) { status.store(WorldStoreStatus::Closed, std::memory_order_release); }
	}

	// ---- The worker ---------------------------------------------------------

	void SpacetimeWorldStore::Run(std::stop_token stop)
	{
		// W-2: an exception here sets Failed instead of terminating the game
		try
		{
			if (!Open()) { return; }

			std::chrono::milliseconds backoff = config.firstBackoff;
			while (!stop.stop_requested())
			{
				Entry next;
				{
					std::unique_lock guard(lock);
					wake.wait(guard, stop, [&] { return !queue.empty() || closing; });
					if (stop.stop_requested()) { break; }
					if (queue.empty()) { break; } // closing, and nothing left to flush
					next = queue.front();
				}

				// One request in flight, first in first out (W-3)
				const Outcome outcome = Send(next);
				if (outcome == Outcome::Confirmed)
				{
					{
						std::lock_guard guard(lock);
						if (!queue.empty() && queue.front() == next) { queue.pop_front(); }
						pending.store(queue.size(), std::memory_order_release);
					}
					confirmed.fetch_add(1, std::memory_order_acq_rel);
					backoff = config.firstBackoff;
					WorldStoreStatus degraded = WorldStoreStatus::Degraded;
					if (status.compare_exchange_strong(degraded, WorldStoreStatus::Ready, std::memory_order_acq_rel))
					{
						std::cout << "[WorldStore] SpacetimeDB is back; the queued writes went through\n";
					}
				}
				else if (outcome == Outcome::Fail)
				{
					return; // Send set Failed, with the reason
				}
				else
				{
					// Keep playing, keep the queue, try again later (Q49)
					WorldStoreStatus ready = WorldStoreStatus::Ready;
					if (status.compare_exchange_strong(ready, WorldStoreStatus::Degraded, std::memory_order_acq_rel))
					{
						std::cerr << "[WorldStore] SpacetimeDB isn't answering; writes are queued and retried\n";
					}
					std::unique_lock guard(lock);
					wake.wait_for(guard, stop, backoff, [] { return false; });
					backoff = std::min(backoff * 2, config.maxBackoff);
				}
			}
		}
		catch (const std::exception& e)
		{
			Fail(std::string("the World store's worker stopped: ") + e.what());
		}
		catch (...)
		{
			Fail("the World store's worker stopped on an unknown exception");
		}
	}

	bool SpacetimeWorldStore::Open()
	{
		// One Host per world, here per PC (W-11)
		if (!AcquireLock())
		{
			Fail("another Host is already running (" + config.lockFile.string() + " is held)");
			return false;
		}

		// The identity: read, or minted once and written atomically (W-10)
		Identity id;
		std::string why;
		const Obtained obtained = ObtainIdentity(*http, config.identityFile, id, why);
		if (obtained == Obtained::Failed) { Fail(why); return false; }
		{
			std::lock_guard guard(lock);
			identity = NormalizeIdentity(id.identity);
			token = id.token;
		}
		if (obtained == Obtained::Minted) { std::cout << "[WorldStore] Minted a SpacetimeDB identity for this Host\n"; }

		std::string worldKey;
		{
			std::lock_guard guard(lock);
			worldKey = key;
		}

		// open_world: creates the world with us as owner, or checks that we are
		const HttpResult opened = http->CallReducer(config.database, "open_world", nlohmann::json::array({ worldKey }), id.token);
		if (!opened.transportOk) { Fail("SpacetimeDB isn't answering on " + config.host + ":" + std::to_string(config.port) + " (" + opened.transportError + ")"); return false; }
		if (opened.status != 200) { Fail("open_world: " + (opened.body.empty() ? "HTTP " + std::to_string(opened.status) : opened.body)); return false; }

		// The world row, then its stations. SpacetimeDB's /sql takes one statement
		// per request, so these are two reads, both before Ready is published. The
		// key was checked against the module's own pattern, so it is safe inside
		// the quotes (W-8).
		std::vector<SqlResult> results;
		for (const char* table : { "world", "station_state" })
		{
			const std::string query = std::string("SELECT * FROM ") + table + " WHERE world_key = '" + worldKey + "'";
			const HttpResult read = http->Sql(config.database, query, id.token);
			if (!read.transportOk) { Fail("reading the world failed: " + read.transportError); return false; }
			if (read.status != 200) { Fail("reading " + std::string(table) + " answered HTTP " + std::to_string(read.status) + ": " + read.body); return false; }
			std::vector<SqlResult> one;
			if (!ParseSqlResults(read.body, one) || one.size() != 1) { Fail("reading " + std::string(table) + " returned an unexpected body"); return false; }
			results.push_back(std::move(one[0]));
		}

		// A schema we don't expect is a failure, never "empty" (W-5)
		const SqlResult& world = results[0];
		const SqlResult& stations = results[1];
		if (world.columns != std::vector<std::string>{ "world_key", "owner" }) { Fail("the world table's schema isn't (world_key, owner)"); return false; }
		if (stations.columns != std::vector<std::string>{ "key", "world_key", "station_id" }) { Fail("the station_state table's schema isn't (key, world_key, station_id)"); return false; }
		if (world.rows.size() != 1) { Fail("the world row is missing after open_world"); return false; }
		if (OwnerOf(world.rows[0][1]) != NormalizeIdentity(id.identity)) { Fail("not the owner of this world"); return false; }

		World::WorldState state;
		for (const nlohmann::json& row : stations.rows)
		{
			if (!row[2].is_number_unsigned() && !row[2].is_number_integer()) { Fail("a station_state row has no station id"); return false; }
			state.completedStations.push_back(row[2].get<std::uint64_t>());
		}

		{
			std::lock_guard guard(lock);
			loaded = std::move(state);
		}
		// The state is written before Ready is published (W-5)
		status.store(WorldStoreStatus::Ready, std::memory_order_release);
		std::cout << "[WorldStore] Opened " << worldKey << " in " << config.database << ": " << loaded.completedStations.size() << " station(s) completed\n";
		return true;
	}

	SpacetimeWorldStore::Outcome SpacetimeWorldStore::Send(const Entry& entry)
	{
		std::string worldKey;
		std::string bearer;
		{
			std::lock_guard guard(lock);
			worldKey = key;
			bearer = token;
		}

		// Bodies are built by nlohmann::json, never concatenated (W-8)
		const HttpResult result = entry.kind == Entry::Kind::FullWipe
			? http->CallReducer(config.database, "reset_world", nlohmann::json::array({ worldKey }), bearer)
			: http->CallReducer(config.database, "complete_station", nlohmann::json::array({ worldKey, entry.stationId }), bearer);

		// W-4: retry what can succeed; stop on what can't
		if (!result.transportOk) { return Outcome::Retry; }
		if (result.status == 200) { return Outcome::Confirmed; }
		if (Retryable(result.status)) { return Outcome::Retry; }

		Fail((entry.kind == Entry::Kind::FullWipe ? "reset_world: " : "complete_station: ")
			+ (result.body.empty() ? "HTTP " + std::to_string(result.status) : result.body));
		return Outcome::Fail;
	}

	// ---- host.lock (W-11) ---------------------------------------------------

	bool SpacetimeWorldStore::AcquireLock()
	{
		if (config.lockFile.empty()) { return true; }
		std::error_code ignored;
		std::filesystem::create_directories(config.lockFile.parent_path(), ignored);
#if defined(_WIN32)
		// No sharing: a second Host's open fails while this one holds it, and the
		// file goes when the handle closes, even after a crash
		HANDLE handle = CreateFileW(config.lockFile.wstring().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
			OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
		if (handle == INVALID_HANDLE_VALUE) { return false; }
		lockHandle = handle;
		return true;
#else
		const int fd = open(config.lockFile.c_str(), O_RDWR | O_CREAT, 0600);
		if (fd < 0) { return false; }
		if (flock(fd, LOCK_EX | LOCK_NB) != 0) { close(fd); return false; }
		lockFd = fd;
		return true;
#endif
	}

	void SpacetimeWorldStore::ReleaseLock()
	{
#if defined(_WIN32)
		if (lockHandle != nullptr) { CloseHandle(static_cast<HANDLE>(lockHandle)); lockHandle = nullptr; }
#else
		if (lockFd >= 0) { flock(lockFd, LOCK_UN); close(lockFd); lockFd = -1; unlink(config.lockFile.c_str()); }
#endif
	}
}
