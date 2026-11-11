#include "DevOptions.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace Client {

	DevOptions& Dev()
	{
		static DevOptions options;
		return options;
	}

	Sim::PlayerCommand BotCommand(std::uint32_t sequence, std::uint32_t clientTick)
	{
		Sim::PlayerCommand command;
		command.sequence = sequence;
		command.clientTick = clientTick;

		// A 12 s loop: walk 4 s, turn 1 s, sprint 3 s, stand 2 s, crouch-walk 2 s
		const std::uint32_t t = clientTick % (12 * 60);
		if (t < 4 * 60) { command.moveY = 1; }
		else if (t < 5 * 60) { command.yawDelta = 0.025f; }
		else if (t < 8 * 60) { command.moveY = 1; command.buttons = Sim::BUTTON_SPRINT; }
		else if (t < 10 * 60) { }
		else { command.moveY = 1; command.buttons = Sim::BUTTON_CROUCH; command.yawDelta = -0.01f; }
		if (clientTick % (5 * 60) == 0) { command.buttons |= Sim::BUTTON_FLASHLIGHT; }
		return command;
	}

	void ParseDevOptions(int argc, char** argv)
	{
#if defined(VEIL_DEV_NET) && !(defined(PRODUCTION_BUILD) && PRODUCTION_BUILD == 1)
		DevOptions& dev = Dev();
		for (int i = 1; i < argc; ++i)
		{
			const std::string_view arg = argv[i];
			const bool hasValue = i + 1 < argc;
			if (arg == "--host") { dev.host = true; }
			else if (arg == "--bot") { dev.bot = true; }
			else if (arg == "--join" && hasValue) { dev.join = argv[++i]; }
			else if (arg == "--name" && hasValue) { dev.name = argv[++i]; }
			else if (arg == "--world-id" && hasValue) { dev.worldId = argv[++i]; }
			else if (arg == "--fake-lag" && hasValue) { dev.fakeLagMs = std::atoi(argv[++i]); }
			else if (arg == "--fake-loss" && hasValue) { dev.fakeLossPercent = static_cast<float>(std::atof(argv[++i])); }
			else if (arg == "--quit-after" && hasValue) { dev.quitAfterSeconds = std::atof(argv[++i]); }
			else if (arg == "--window-x" && hasValue) { dev.windowX = std::atoi(argv[++i]); }
			else if (arg == "--screenshot-at" && hasValue) { dev.screenshotAtSeconds = std::atof(argv[++i]); }
			else { std::cerr << "[Dev] Unknown switch ignored: " << arg << "\n"; }
		}
#else
		(void)argc;
		(void)argv;
#endif
	}
}
