#pragma once
#ifndef NET_TRANSPORT_H
#define NET_TRANSPORT_H

// The only network surface the game and the server see. No library type
// (GameNetworkingSockets, Steam, a future custom UDP layer) crosses this
// header (DedicatedServerPlan.md Step 2.3, rule R6).
//
// Implementations:
//   LoopbackTransport — single-player; no sockets (MultiplayerSplitPlan.md §5.1).
//   GnsTransport      — GameNetworkingSockets: the Host, its own client and
//                       every joined client (src/net/gns/).
//   UdpTransport      — custom UDP layer; written plan only, not before 2028.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace Net {

	using ConnectionId = std::uint32_t;
	inline constexpr ConnectionId kInvalidConnection = 0;

	enum class Delivery : std::uint8_t {
		Unreliable, // may drop or arrive out of order — commands, snapshots
		Reliable,   // arrives once, in order — handshake, station events, caught, round over
	};

	enum class NetEventType : std::uint8_t { Connected, Disconnected, Message };

	struct NetEvent {
		NetEventType               type       = NetEventType::Message;
		ConnectionId               connection = kInvalidConnection;
		std::span<const std::byte> payload    = {}; // valid until the next Poll()
	};

	struct NetStats {
		float pingMs         = 0.0f;
		float packetLoss     = 0.0f; // 0..1
		float bytesInPerSec  = 0.0f;
		float bytesOutPerSec = 0.0f;
	};

	class INetTransport {
	public:
		virtual ~INetTransport() = default;

		virtual bool         Listen(std::uint16_t port) = 0;          // server side
		virtual ConnectionId Connect(std::string_view address) = 0;   // client side, "ip:port"
		virtual void         Disconnect(ConnectionId connection) = 0;

		virtual bool Send(ConnectionId connection, std::span<const std::byte> data, Delivery delivery) = 0;

		// Writes up to out.size() events into `out` and returns how many were written.
		virtual std::size_t Poll(std::span<NetEvent> out) = 0;

		virtual NetStats Stats(ConnectionId connection) const = 0;
	};
}

#endif
