// GameNetworkingSockets lives here and nowhere else (rule R6). Only the GNS
// headers are included: never the Steamworks SDK (P-7), and never raylib.
#include <gns/steamnetworkingsockets.h>
#include <gns/isteamnetworkingutils.h>

#include "GnsTransport.h"

#include <net/Protocol.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace Net::Gns {

	namespace {

		int initCount = 0;

		ISteamNetworkingSockets* Sockets() { return SteamNetworkingSockets_Lib(); }
		ISteamNetworkingUtils* Utils() { return SteamNetworkingUtils_Lib(); }

		void DebugOutput(ESteamNetworkingSocketsDebugOutputType type, const char* message)
		{
			if (type <= k_ESteamNetworkingSocketsDebugOutputType_Warning) { std::cerr << "[GNS] " << message << "\n"; }
		}

		class GnsTransport;

		// The status callback is a plain function and every transport in this
		// process shares it (the Host runs a server and its own client), so each
		// callback is routed to the transport that owns the connection
		std::vector<GnsTransport*>& Registry()
		{
			static std::vector<GnsTransport*> transports;
			return transports;
		}

		void OnStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);

		class GnsTransport final : public INetTransport
		{
		public:
			GnsTransport() { Registry().push_back(this); }

			~GnsTransport() override
			{
				Shutdown();
				auto& registry = Registry();
				registry.erase(std::remove(registry.begin(), registry.end(), this), registry.end());
			}

			bool Listen(std::uint16_t port) override
			{
				if (listenSocket != k_HSteamListenSocket_Invalid || isClient) { return false; }

				SteamNetworkingIPAddr address;
				address.Clear();
				address.m_port = port;

				std::vector<SteamNetworkingConfigValue_t> options = Options();
				listenSocket = Sockets()->CreateListenSocketIP(address, static_cast<int>(options.size()), options.data());
				if (listenSocket == k_HSteamListenSocket_Invalid)
				{
					std::cerr << "[GNS] Couldn't listen on port " << port << "\n";
					return false;
				}
				pollGroup = Sockets()->CreatePollGroup();
				std::cout << "[GNS] Listening on port " << port << "\n";
				return true;
			}

			ConnectionId Connect(std::string_view address) override
			{
				if (listenSocket != k_HSteamListenSocket_Invalid || serverConnection != k_HSteamNetConnection_Invalid) { return kInvalidConnection; }

				// "ip:port", "ip" or ":port"
				std::string text(address);
				if (text.empty() || text.front() == ':') { text = "127.0.0.1" + text; }
				if (text.find(':') == std::string::npos) { text += ":" + std::to_string(kDefaultPort); }

				SteamNetworkingIPAddr target;
				target.Clear();
				if (!target.ParseString(text.c_str()))
				{
					std::cerr << "[GNS] Not an address: " << text << "\n";
					return kInvalidConnection;
				}

				std::vector<SteamNetworkingConfigValue_t> options = Options();
				isClient = true;
				serverConnection = Sockets()->ConnectByIPAddress(target, static_cast<int>(options.size()), options.data());
				if (serverConnection == k_HSteamNetConnection_Invalid) { return kInvalidConnection; }
				return serverConnection;
			}

			void Disconnect(ConnectionId connection) override
			{
				if (connection == kInvalidConnection) { return; }
				// Linger, so a Reject sent just before still reaches the peer (P-7)
				Sockets()->CloseConnection(connection, 0, "closed", true);
				Forget(connection);
				if (connection == serverConnection) { serverConnection = k_HSteamNetConnection_Invalid; }
			}

			bool Send(ConnectionId connection, std::span<const std::byte> data, Delivery delivery) override
			{
				const int flags = delivery == Delivery::Reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_UnreliableNoNagle;
				const EResult result = Sockets()->SendMessageToConnection(connection, data.data(), static_cast<uint32>(data.size()), flags, nullptr);
				return result == k_EResultOK;
			}

			std::size_t Poll(std::span<NetEvent> out) override
			{
				// What the last Poll handed out is released now
				for (SteamNetworkingMessage_t* message : held) { message->Release(); }
				held.clear();
				delivered.clear();

				Sockets()->RunCallbacks();

				std::size_t written = 0;
				while (written < out.size() && !pending.empty())
				{
					out[written].type = pending.front().type;
					out[written].connection = pending.front().connection;
					out[written].payload = {};
					pending.erase(pending.begin());
					++written;
				}

				const int room = static_cast<int>(out.size() - written);
				if (room <= 0) { return written; }

				std::vector<SteamNetworkingMessage_t*> messages(static_cast<std::size_t>(room), nullptr);
				int received = 0;
				if (pollGroup != k_HSteamNetPollGroup_Invalid) { received = Sockets()->ReceiveMessagesOnPollGroup(pollGroup, messages.data(), room); }
				else if (serverConnection != k_HSteamNetConnection_Invalid) { received = Sockets()->ReceiveMessagesOnConnection(serverConnection, messages.data(), room); }

				for (int i = 0; i < received; ++i)
				{
					SteamNetworkingMessage_t* message = messages[static_cast<std::size_t>(i)];
					held.push_back(message);
					out[written].type = NetEventType::Message;
					out[written].connection = message->m_conn;
					out[written].payload = std::span<const std::byte>(static_cast<const std::byte*>(message->m_pData), static_cast<std::size_t>(message->m_cbSize));
					++written;
				}
				return written;
			}

			NetStats Stats(ConnectionId connection) const override
			{
				NetStats stats;
				SteamNetConnectionRealTimeStatus_t status = {};
				if (Sockets()->GetConnectionRealTimeStatus(connection, &status, 0, nullptr) == k_EResultOK)
				{
					stats.pingMs = static_cast<float>(status.m_nPing);
					stats.packetLoss = std::clamp(1.0f - status.m_flConnectionQualityLocal, 0.0f, 1.0f);
					stats.bytesInPerSec = status.m_flInBytesPerSec;
					stats.bytesOutPerSec = status.m_flOutBytesPerSec;
				}
				return stats;
			}

			// Routed here by OnStatusChanged
			bool Owns(const SteamNetConnectionStatusChangedCallback_t& info) const
			{
				if (info.m_hConn == serverConnection && serverConnection != k_HSteamNetConnection_Invalid) { return true; }
				if (std::find(connections.begin(), connections.end(), info.m_hConn) != connections.end()) { return true; }
				return listenSocket != k_HSteamListenSocket_Invalid && info.m_info.m_hListenSocket == listenSocket;
			}

			void StatusChanged(const SteamNetConnectionStatusChangedCallback_t& info)
			{
				const HSteamNetConnection connection = info.m_hConn;
				switch (info.m_info.m_eState)
				{
				case k_ESteamNetworkingConnectionState_Connecting:
					// A client asking to join this listen socket
					if (listenSocket != k_HSteamListenSocket_Invalid && info.m_info.m_hListenSocket == listenSocket)
					{
						if (Sockets()->AcceptConnection(connection) != k_EResultOK)
						{
							Sockets()->CloseConnection(connection, 0, "accept failed", false);
							break;
						}
						Sockets()->SetConnectionPollGroup(connection, pollGroup);
						connections.push_back(connection);
					}
					break;

				case k_ESteamNetworkingConnectionState_Connected:
					pending.push_back({ NetEventType::Connected, connection });
					break;

				case k_ESteamNetworkingConnectionState_ClosedByPeer:
				case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
				{
					const bool wasOpen = info.m_eOldState == k_ESteamNetworkingConnectionState_Connected
						|| info.m_eOldState == k_ESteamNetworkingConnectionState_Connecting
						|| info.m_eOldState == k_ESteamNetworkingConnectionState_FindingRoute;
					if (wasOpen) { pending.push_back({ NetEventType::Disconnected, connection }); }
					// Free the handle; nothing more can be sent on it
					Sockets()->CloseConnection(connection, 0, nullptr, false);
					Forget(connection);
					if (connection == serverConnection) { serverConnection = k_HSteamNetConnection_Invalid; }
					break;
				}

				default:
					break;
				}
			}

		private:
			struct Pending { NetEventType type; ConnectionId connection; };

			// P-7, on the listen socket and on the client's connect options alike
			std::vector<SteamNetworkingConfigValue_t> Options() const
			{
				std::vector<SteamNetworkingConfigValue_t> options(5);
				options[0].SetInt32(k_ESteamNetworkingConfig_RecvMaxMessageSize, static_cast<int32_t>(kMaxMessage));
				options[1].SetInt32(k_ESteamNetworkingConfig_RecvBufferMessages, 128);
				options[2].SetInt32(k_ESteamNetworkingConfig_RecvBufferSize, 64 * 1024);
				options[3].SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, reinterpret_cast<void*>(&OnStatusChanged));
				// Dev-only: LAN and direct IP with no certificates (Q33). Unencrypted stays 0.
				options[4].SetInt32(k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 1);
				return options;
			}

			void Forget(HSteamNetConnection connection)
			{
				connections.erase(std::remove(connections.begin(), connections.end(), connection), connections.end());
			}

			void Shutdown()
			{
				for (SteamNetworkingMessage_t* message : held) { message->Release(); }
				held.clear();
				if (!IsRunning()) { return; }
				for (const HSteamNetConnection connection : connections) { Sockets()->CloseConnection(connection, 0, "shutdown", true); }
				connections.clear();
				if (serverConnection != k_HSteamNetConnection_Invalid) { Sockets()->CloseConnection(serverConnection, 0, "shutdown", true); }
				serverConnection = k_HSteamNetConnection_Invalid;
				if (listenSocket != k_HSteamListenSocket_Invalid) { Sockets()->CloseListenSocket(listenSocket); }
				listenSocket = k_HSteamListenSocket_Invalid;
				if (pollGroup != k_HSteamNetPollGroup_Invalid) { Sockets()->DestroyPollGroup(pollGroup); }
				pollGroup = k_HSteamNetPollGroup_Invalid;
			}

			HSteamListenSocket listenSocket = k_HSteamListenSocket_Invalid;
			HSteamNetPollGroup pollGroup = k_HSteamNetPollGroup_Invalid;
			HSteamNetConnection serverConnection = k_HSteamNetConnection_Invalid;
			bool isClient = false;

			std::vector<HSteamNetConnection> connections; // a server's clients
			std::vector<Pending> pending;
			std::vector<SteamNetworkingMessage_t*> held;
			std::vector<std::byte> delivered;
		};

		void OnStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
		{
			for (GnsTransport* transport : Registry())
			{
				if (transport->Owns(*info)) { transport->StatusChanged(*info); return; }
			}
		}
	}

	bool Init(std::string& error)
	{
		if (initCount > 0) { ++initCount; return true; }

		SteamNetworkingErrMsg message = {};
		if (!GameNetworkingSockets_Init(nullptr, message))
		{
			error = message;
			return false;
		}
		Utils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Warning, DebugOutput);
		initCount = 1;
		std::cout << "[GNS] Started\n";
		return true;
	}

	void Kill()
	{
		if (initCount == 0) { return; }
		if (--initCount > 0) { return; }
		GameNetworkingSockets_Kill();
		std::cout << "[GNS] Stopped\n";
	}

	bool IsRunning() { return initCount > 0; }

	void SetFakeNetwork(int lagMs, float lossPercent)
	{
		if (!IsRunning()) { return; }
		// On what this process sends only: with both games set the same, every
		// packet is delayed once, so the setting is the latency each way
		Utils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Send, lagMs);
		Utils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_FakePacketLag_Recv, 0);
		Utils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Send, lossPercent);
		Utils()->SetGlobalConfigValueFloat(k_ESteamNetworkingConfig_FakePacketLoss_Recv, 0.0f);
		std::cout << "[GNS] Faked network: " << lagMs << " ms and " << lossPercent << "% loss on everything this game sends\n";
	}

	std::unique_ptr<INetTransport> CreateTransport()
	{
		return std::make_unique<GnsTransport>();
	}
}
