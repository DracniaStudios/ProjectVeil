#pragma once
#ifndef CLIENT_DEV_OPTIONS_H
#define CLIENT_DEV_OPTIONS_H

// Development command-line switches (dev builds only: VEIL_DEV_NET and not a
// production build). They let two game windows on one PC host and join each
// other without clicking (DedicatedServerPlan Q28; MultiplayerSplitPlan.md N7,
// N8, N9), and let a scripted run end cleanly so its log is complete.
//
//   --host                  Start Server once the game is up
//   --join <ip[:port]>      Join that address once the game is up
//   --name <name>           the name sent in Hello (24 printable ASCII bytes)
//   --world-id <id>         the Host's world_id (default: the World save's name)
//   --fake-lag <ms>         GNS faked latency, each way (N8)
//   --fake-loss <percent>   GNS faked packet loss, each way (N8)
//   --bot                   this client walks a fixed pattern instead of reading input
//   --quit-after <seconds>  exit cleanly after this long
//   --window-x <px>         place the window (two windows side by side)
//   --log-mouse             print the summed mouse delta once a second ([Input] mouse dx .. dy ..)
//   --screenshot-at <s>     save a screenshot this many seconds in (veil_<name>.png)

#include <sim/PlayerCommand.h>

#include <cstdint>
#include <string>

namespace Client {

	struct DevOptions
	{
		bool host = false;
		std::string join;
		std::string name = "Player";
		std::string worldId;
		int fakeLagMs = 0;
		float fakeLossPercent = 0.0f;
		bool bot = false;
		double quitAfterSeconds = 0.0;
		int windowX = -1;
		double screenshotAtSeconds = 0.0;
		bool logMouse = false;
	};

	// The switches this run was started with
	DevOptions& Dev();

	// Reads argv. Unknown switches are reported and ignored. In a production
	// build, or without VEIL_DEV_NET, nothing is read.
	void ParseDevOptions(int argc, char** argv);

	// --bot: a fixed pattern of walking, turning, sprinting (loud, so the stalker
	// hears it) and the flashlight, for runs nobody plays
	Sim::PlayerCommand BotCommand(std::uint32_t sequence, std::uint32_t clientTick);
}

#endif
