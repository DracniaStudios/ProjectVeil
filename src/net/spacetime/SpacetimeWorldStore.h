#pragma once
#ifndef NET_SPACETIME_WORLD_STORE_H
#define NET_SPACETIME_WORLD_STORE_H

// The Host's World store: the World state in the Host PC's SpacetimeDB, read
// at session start and written through as it changes (MultiplayerSplitPlan.md
// W5; decisions S2, S3, Q44, Q48, Q49, Q50; rules W-1 to W-11).
//
// Every HTTP call happens on one worker thread. The tick only appends to a
// queue (W-1), so SpacetimeDB being slow or gone never stalls the simulation:
// a mid-session outage leaves the store Degraded, keeps play going, queues the
// writes and retries until they go through (Q49).
//
// No HTTP-library or SpacetimeDB type appears here (rule R8): the client is
// behind SpacetimeHttp's pimpl.

#include <World/WorldStore.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

namespace Net::Spacetime {

	class HttpClient;

	struct StoreConfig
	{
		std::string host = "127.0.0.1";  // plain HTTP, the Host PC only (Q54)
		int port = 3000;
		std::string database = "veil-world";
		std::filesystem::path identityFile; // saves/dev_identity.json (W-10)
		std::filesystem::path lockFile;     // saves/host.lock (W-11)

		// Backoff after a failed write: 1 s, doubling to 8 s (W-2)
		std::chrono::milliseconds firstBackoff{ 1000 };
		std::chrono::milliseconds maxBackoff{ 8000 };
	};

	class SpacetimeWorldStore final : public World::WorldStore
	{
	public:
		explicit SpacetimeWorldStore(StoreConfig config);
		~SpacetimeWorldStore() override;

		SpacetimeWorldStore(const SpacetimeWorldStore&) = delete;
		SpacetimeWorldStore& operator=(const SpacetimeWorldStore&) = delete;

		void BeginOpen(std::string_view worldKey) override;
		bool ReadState(World::WorldState& out) const override;
		void SubmitStationCompleted(std::uint64_t stationId) override;
		void SubmitFullWipe() override;
		World::WorldStoreStatus Status() const override { return status.load(std::memory_order_acquire); }
		std::size_t Pending() const override { return pending.load(std::memory_order_acquire); }
		void Close(std::chrono::milliseconds flushDeadline) override;

		// Why the store Failed, for the UI: the reducer's own message when there
		// is one ("not the owner of this world"). Never contains the token.
		std::string LastError() const;

		// Writes confirmed so far, for the log and tests
		std::size_t Confirmed() const { return confirmed.load(std::memory_order_acquire); }

	private:
		struct Entry
		{
			enum class Kind : std::uint8_t { StationCompleted, FullWipe } kind = Kind::StationCompleted;
			std::uint64_t stationId = 0;
			std::uint64_t serial = 0; // tells two identical submissions apart (W-3)
			bool operator==(const Entry&) const = default;
		};

		enum class Outcome { Confirmed, Retry, Fail };

		void Submit(Entry entry);
		void Run(std::stop_token stop);
		bool Open();
		Outcome Send(const Entry& entry);
		void Fail(const std::string& why);

		bool AcquireLock();
		void ReleaseLock();

		const StoreConfig config;

		std::atomic<World::WorldStoreStatus> status{ World::WorldStoreStatus::Closed };
		std::atomic<std::size_t> pending{ 0 };
		std::atomic<std::size_t> confirmed{ 0 };

		mutable std::mutex lock;          // queue, loaded, key, error, flags
		std::condition_variable_any wake;
		std::deque<Entry> queue;
		World::WorldState loaded;
		std::string key;
		std::string identity;
		std::string token;
		std::string error;
		std::uint64_t nextSerial = 1;
		bool opened = false;
		bool closing = false;
		bool closed = false;

		std::unique_ptr<HttpClient> http;
		void* lockHandle = nullptr; // the host.lock file, held open while the store runs
		int lockFd = -1;

		// Declared last so it stops and joins before anything it uses is destroyed (W-2)
		std::jthread worker;
	};
}

#endif
