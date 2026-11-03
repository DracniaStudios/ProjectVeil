#pragma once
#ifndef NET_GNS_TRANSPORT_H
#define NET_GNS_TRANSPORT_H

// GameNetworkingSockets behind INetTransport (DedicatedServerPlan.md Step 4.2;
// MultiplayerSplitPlan.md N4, N8; rules R6, P-7).
//
// Rule R6: no GNS type leaves src/net/gns/. This header has none: the
// transport is made by a factory and the library is started and stopped by
// plain functions. P-7: src/net/gns/ calls only SteamNetworkingSockets_Lib()
// and SteamNetworkingUtils_Lib(), and no .cpp here includes the Steamworks SDK,
// because both define an inline SteamNetworkingSockets() returning different
// versions.

#include <net/NetTransport.h>

#include <memory>
#include <string>

namespace Net::Gns {

	// Starts the library, once per process: only on Start Server or Join (§5.6).
	// A second call while it runs only counts; Kill must be called as many times.
	bool Init(std::string& error);

	// Stops the library once the last user is done (§5.6 step 3). Every
	// transport must have closed its connections first.
	void Kill();

	bool IsRunning();

	// Faked latency (ms) and loss (percent) on everything this process sends,
	// through GNS's own network simulation (Step 4.11, N8). Set the same on both
	// games and it is the latency and loss each way. Dev builds.
	void SetFakeNetwork(int lagMs, float lossPercent);

	// A transport: server side (Listen) or client side (Connect), not both.
	// Init must have succeeded first.
	std::unique_ptr<INetTransport> CreateTransport();
}

#endif
