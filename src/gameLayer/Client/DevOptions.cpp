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
			else { std::cerr << "[Dev] Unknown switch ignored: " << arg << "\n"; }
		}
#else
		(void)argc;
		(void)argv;
#endif
	}
}
