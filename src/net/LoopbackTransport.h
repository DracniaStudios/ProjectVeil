#pragma once
#ifndef NET_LOOPBACK_TRANSPORT_H
#define NET_LOOPBACK_TRANSPORT_H

#include "NetTransport.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <utility>
#include <vector>

namespace Net {

	/**
	 * Single-player's transport (DedicatedServerPlan.md Step 2.3; MultiplayerSplitPlan.md
	 * §5.1, N1): two in-memory queues joining the game's client to its in-process
	 * server. No socket, no thread; both ends are polled from the main loop, and a
	 * message sent before the other end polls arrives in that same Poll.
	 *
	 * Loopback is for single-player only. On a Host, the Host's own client connects
	 * to 127.0.0.1 through GnsTransport, so the server handles one transport.
	 *
	 * In dev builds, SetFakeLoss drops a share of unreliable messages, so the
	 * unreliable path is exercised before real networking exists. Reliable
	 * messages always arrive, once, in order.
	 */
	class LoopbackTransport final : public INetTransport
	{
	public:
		// A joined pair: the first end is the server's, the second the client's
		static std::pair<std::unique_ptr<LoopbackTransport>, std::unique_ptr<LoopbackTransport>> CreatePair();

		// The one connection id either end ever sees
		static constexpr ConnectionId kConnection = 1;

		bool         Listen(std::uint16_t port) override;
		ConnectionId Connect(std::string_view address) override;
		void         Disconnect(ConnectionId connection) override;
		bool         Send(ConnectionId connection, std::span<const std::byte> data, Delivery delivery) override;
		std::size_t  Poll(std::span<NetEvent> out) override;
		NetStats     Stats(ConnectionId connection) const override;

		// Drops this share (0..1) of the unreliable messages this end sends, with a
		// seeded generator so a test is repeatable. 0 turns it off.
		void SetFakeLoss(float share, std::uint32_t seed = 1);

		// Unreliable messages this end has dropped on purpose
		std::size_t Dropped() const { return dropped; }

		~LoopbackTransport() override;

	private:
		struct Pending
		{
			NetEventType type = NetEventType::Message;
			std::vector<std::byte> bytes;
		};

		LoopbackTransport() = default;

		void Push(Pending&& item) { inbox.push_back(std::move(item)); }

		LoopbackTransport* peer = nullptr; // the other end; cleared when either goes
		bool listening = false;
		bool connected = false;

		std::deque<Pending> inbox;
		std::vector<Pending> delivered; // what the last Poll handed out; spans point here

		float lossShare = 0.0f;
		std::mt19937 lossRandom{ 1 };
		std::size_t dropped = 0;
		std::uint64_t bytesSent = 0;
		std::uint64_t bytesReceived = 0;
	};
}

#endif
