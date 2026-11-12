#include "LoopbackTransport.h"

#include <algorithm>

namespace Net {

	std::pair<std::unique_ptr<LoopbackTransport>, std::unique_ptr<LoopbackTransport>> LoopbackTransport::CreatePair()
	{
		std::unique_ptr<LoopbackTransport> server(new LoopbackTransport);
		std::unique_ptr<LoopbackTransport> client(new LoopbackTransport);
		server->peer = client.get();
		client->peer = server.get();
		return { std::move(server), std::move(client) };
	}

	LoopbackTransport::~LoopbackTransport()
	{
		// The other end sees the connection drop, and forgets this one
		if (peer != nullptr)
		{
			if (connected) { peer->Push({ NetEventType::Disconnected, {} }); peer->connected = false; }
			peer->peer = nullptr;
		}
	}

	bool LoopbackTransport::Listen(std::uint16_t)
	{
		listening = true;
		return true;
	}

	ConnectionId LoopbackTransport::Connect(std::string_view)
	{
		if (peer == nullptr || !peer->listening || connected) { return kInvalidConnection; }

		// Both ends learn of the connection on their next Poll, as with a socket
		connected = true;
		peer->connected = true;
		Push({ NetEventType::Connected, {} });
		peer->Push({ NetEventType::Connected, {} });
		return kConnection;
	}

	void LoopbackTransport::Disconnect(ConnectionId connection)
	{
		if (connection != kConnection || !connected) { return; }
		connected = false;
		Push({ NetEventType::Disconnected, {} });
		if (peer != nullptr && peer->connected)
		{
			peer->connected = false;
			peer->Push({ NetEventType::Disconnected, {} });
		}
	}

	bool LoopbackTransport::Send(ConnectionId connection, std::span<const std::byte> data, Delivery delivery)
	{
		if (connection != kConnection || !connected || peer == nullptr) { return false; }

		if (delivery == Delivery::Unreliable && lossShare > 0.0f)
		{
			std::uniform_real_distribution<float> roll(0.0f, 1.0f);
			if (roll(lossRandom) < lossShare) { ++dropped; return true; } // sent, then lost
		}

		bytesSent += data.size();
		peer->Push({ NetEventType::Message, std::vector<std::byte>(data.begin(), data.end()) });
		return true;
	}

	std::size_t LoopbackTransport::Poll(std::span<NetEvent> out)
	{
		// The spans the last Poll handed out are invalid from here on
		delivered.clear();

		const std::size_t count = std::min(out.size(), inbox.size());
		delivered.reserve(count);
		for (std::size_t i = 0; i < count; ++i)
		{
			delivered.push_back(std::move(inbox.front()));
			inbox.pop_front();
		}

		for (std::size_t i = 0; i < count; ++i)
		{
			const Pending& item = delivered[i];
			out[i].type = item.type;
			out[i].connection = kConnection;
			out[i].payload = std::span<const std::byte>(item.bytes.data(), item.bytes.size());
			bytesReceived += item.bytes.size();
		}
		return count;
	}

	NetStats LoopbackTransport::Stats(ConnectionId) const
	{
		NetStats stats;
		stats.packetLoss = lossShare;
		return stats;
	}

	void LoopbackTransport::SetFakeLoss(float share, std::uint32_t seed)
	{
		lossShare = std::clamp(share, 0.0f, 1.0f);
		lossRandom.seed(seed);
	}
}
