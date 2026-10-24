/**
 * Standalone tests for Settings.
 *
 * Every setting is described by key and kind, and Set, Cycle and Load decide
 * what value it may hold. Settings.cpp includes nothing from raylib or the
 * engine, and keeps its values through a SaveStore, so this suite links the
 * JSON library and nothing else: no window, no FMOD, no files outside a temp
 * directory.
 *
 * If Settings.cpp ever needs a game header to compile, the meaning of a setting
 * has leaked out of it again and this file stops linking.
 *
 * Build and run: tests/run_tests.sh
 */

#include <Settings.h>
#include <SaveStore.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <string>

using Json = nlohmann::json;

static int g_failures = 0;
static int g_checks = 0;

static void Check(bool condition, const std::string& what)
{
	++g_checks;
	if (!condition)
	{
		++g_failures;
		std::printf("  FAIL  %s\n", what.c_str());
	}
}

static bool Near(double a, double b)
{
	return std::fabs(a - b) < 1e-9;
}

// A MemorySaveStore that counts what is written to it
class CountingStore final : public SaveSystem::SaveStore
{
public:
	bool Write(const std::string& name, const Json& save) override { ++writes; return memory.Write(name, save); }
	bool Read(const std::string& name, Json& save) const override { return memory.Read(name, save); }

	int writes = 0;
	SaveSystem::MemorySaveStore memory;
};

// A temp directory of its own, emptied before and after
struct TempDirectory
{
	std::filesystem::path path = std::filesystem::temp_directory_path() / "ProjectVeilSettingsTests";

	TempDirectory() { std::filesystem::remove_all(path); std::filesystem::create_directories(path); }
	~TempDirectory() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
};

// The keys saves/settings.json has always been written with
static const std::set<std::string> SHIPPED_KEYS = {
	"AntiAliasing", "CameraShadeEffect", "CameraSmoothEffect", "CameraSpeed", "DialogueVolume",
	"Difficulty", "DisplayMode", "Effects", "Foliage", "GameplaySFX", "HideClueVFX",
	"HighContrastDisplay", "HudSize", "InvertedCameraXAxis", "InvertedCameraYAxis", "Luminosity",
	"MasterVolume", "MonoOnly", "MusicVolume", "PostProcessing", "RenderResolution", "Resolution",
	"SFXVolume", "Shadows", "Textures", "ViewDistance", "Vsync", "hideHUD",
};

// saves/settings.json as written before settings knew their kind: every value
// never touched is 0, whether or not 0 means anything for that setting
static const char* SHIPPED_SETTINGS = R"({
  "AntiAliasing": 0.0, "CameraShadeEffect": 0.0, "CameraSmoothEffect": 0.0, "CameraSpeed": 0.0,
  "DialogueVolume": 1.0, "Difficulty": 0.0, "DisplayMode": 0.0, "Effects": 4.0, "Foliage": 4.0,
  "GameplaySFX": 1.0, "HideClueVFX": 0.0, "HighContrastDisplay": 0.0, "HudSize": 0.0,
  "InvertedCameraXAxis": 0.0, "InvertedCameraYAxis": 0.0, "Luminosity": 0.0, "MasterVolume": 0.75,
  "MonoOnly": 0.0, "MusicVolume": 1.0, "PostProcessing": 4.0, "RenderResolution": 0.0,
  "Resolution": 0.0, "SFXVolume": 1.0, "Shadows": 4.0, "Textures": 4.0, "Version": 1,
  "ViewDistance": 4.0, "Vsync": 1.0, "hideHUD": 0.0
})";

static void TestEverySettingStartsAtItsDefault()
{
	std::printf("every setting starts at its default\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	for (const Settings::Descriptor& descriptor : settings.Descriptors())
	{
		Check(settings.Get<double>(descriptor.key) == descriptor.defaultValue, descriptor.key + " holds its default");
	}

	Check(settings.Get<bool>("Vsync"), "VSync starts on");
	Check(settings.Get<int>("Shadows") == 4, "Shadows start at Ultra");
	Check(settings.Get<float>("MasterVolume") == 0.75f, "the master volume starts at 75%");
	Check(settings.Get<float>("Luminosity") == 1.0f, "Luminosity starts at 1, not the 0 that rendered black");
}

static void TestDescriptorsAreWellFormed()
{
	std::printf("every setting is described so it can hold its own default\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	std::set<std::string> keys;
	for (const Settings::Descriptor& descriptor : settings.Descriptors())
	{
		const std::string& key = descriptor.key;
		Check(keys.insert(key).second, key + " is described once");
		Check(!descriptor.label.empty(), key + " has a label for the menu");
		Check(!descriptor.section.empty(), key + " is listed on a page of the menu");
		Check(settings.Find(key) == &descriptor, key + " is found by its key");

		switch (descriptor.kind)
		{
		case Settings::Kind::Toggle:
			Check(descriptor.defaultValue == 0.0 || descriptor.defaultValue == 1.0, key + " defaults to on or off");
			break;
		case Settings::Kind::Choice:
			Check(!descriptor.options.empty(), key + " has options");
			Check(descriptor.defaultValue >= 0.0 && descriptor.defaultValue < descriptor.options.size()
				&& descriptor.defaultValue == std::floor(descriptor.defaultValue), key + " defaults to one of its options");
			break;
		case Settings::Kind::Range:
		{
			Check(descriptor.min < descriptor.max, key + " has a min below its max");
			Check(descriptor.step > 0.0, key + " steps forward");
			Check(descriptor.defaultValue >= descriptor.min && descriptor.defaultValue <= descriptor.max, key + " defaults inside its range");

			// Cycle relies on this to wrap from max straight back to min
			const double steps = (descriptor.max - descriptor.min) / descriptor.step;
			Check(Near(steps, std::round(steps)), key + "'s max is a whole number of steps from its min");
			break;
		}
		}
	}

	Check(settings.Find("NoSuchSetting") == nullptr, "an unknown key finds nothing");
}

static void TestKeysAreTheOnesSettingsJsonUses()
{
	std::printf("the keys are the names settings.json has always used\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	std::set<std::string> keys;
	for (const Settings::Descriptor& descriptor : settings.Descriptors()) { keys.insert(descriptor.key); }

	Check(keys == SHIPPED_KEYS, "every shipped key is described, and nothing else");
}

static void TestSetClampsARange()
{
	std::printf("Set clamps a Range into its bounds\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	Check(settings.Set("MasterVolume", 1.7), "a volume above 100% is taken");
	Check(settings.Get<double>("MasterVolume") == 1.0, "and clamped to 100%");

	settings.Set("MasterVolume", -0.3);
	Check(settings.Get<double>("MasterVolume") == 0.0, "a volume below 0% is clamped to 0%");

	settings.Set("MasterVolume", 0.35);
	Check(settings.Get<double>("MasterVolume") == 0.35, "a volume between steps is kept as it is");

	settings.Set("Luminosity", 0.0);
	Check(settings.Get<double>("Luminosity") == 0.25, "Luminosity 0 is clamped to 0.25, so the frame is never black");
}

static void TestSetRoundsAChoiceToAnOption()
{
	std::printf("Set rounds a Choice to one of its options\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	settings.Set("Shadows", 2.6);
	Check(settings.Get<int>("Shadows") == 3, "2.6 rounds to option 3");

	settings.Set("Shadows", 9.0);
	Check(settings.Get<int>("Shadows") == 4, "past the last option is the last option");

	settings.Set("Shadows", -1.0);
	Check(settings.Get<int>("Shadows") == 0, "below the first option is the first option");
}

static void TestSetTurnsAToggleOnForAnythingButZero()
{
	std::printf("Set turns a Toggle on for anything but 0\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	settings.Set("Vsync", 0.0);
	Check(!settings.Get<bool>("Vsync") && settings.Get<double>("Vsync") == 0.0, "0 turns it off");

	settings.Set("Vsync", 0.5);
	Check(settings.Get<double>("Vsync") == 1.0, "0.5 turns it on, held as 1");

	settings.Set("Vsync", 0.0);
	settings.Set("Vsync", -2.0);
	Check(settings.Get<double>("Vsync") == 1.0, "-2 turns it on, held as 1");
}

static void TestSetRefusesUnknownKeysAndNonNumbers()
{
	std::printf("Set refuses a key no setting has, and a value that is not a number\n");

	CountingStore store;
	Settings::Settings settings(store);

	Check(!settings.Set("NoSuchSetting", 1.0), "an unknown key is refused");
	Check(!settings.Cycle("NoSuchSetting"), "an unknown key can't be cycled");
	Check(settings.Get<double>("NoSuchSetting") == 0.0, "an unknown key reads 0");

	Check(!settings.Set("MasterVolume", std::numeric_limits<double>::quiet_NaN()), "NaN is refused");
	Check(!settings.Set("MasterVolume", std::numeric_limits<double>::infinity()), "infinity is refused");
	Check(settings.Get<double>("MasterVolume") == 0.75, "the volume is left as it was");
	Check(store.writes == 0, "nothing refused is saved");
}

static void TestCycleFlipsAToggle()
{
	std::printf("Cycle flips a Toggle\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	settings.Cycle("Vsync");
	Check(!settings.Get<bool>("Vsync"), "on goes off");
	settings.Cycle("Vsync");
	Check(settings.Get<bool>("Vsync"), "off goes on");
}

static void TestCycleWalksEveryOptionOfAChoice()
{
	std::printf("Cycle walks a Choice through every option, then back to the first\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	// The menu used to treat every setting as a bool: Shadows went 4, 1, 0, 1, 0
	// and never reached Medium or High again
	const int expected[] = { 0, 1, 2, 3, 4, 0 };
	for (int shown : expected)
	{
		settings.Cycle("Shadows");
		Check(settings.Get<int>("Shadows") == shown, "Shadows moves on to option " + std::to_string(shown));
	}
}

static void TestCycleStepsARangeAndWraps()
{
	std::printf("Cycle steps a Range up its steps, then wraps to its min\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	// The menu used to treat every setting as a bool: the master volume went
	// from 75% to 100% to 0%, and never came back to anything in between
	const double expected[] = { 0.8, 0.9, 1.0, 0.0, 0.1, 0.2 };
	for (double volume : expected)
	{
		settings.Cycle("MasterVolume");
		Check(Near(settings.Get<double>("MasterVolume"), volume), "the master volume steps on to " + std::to_string(volume));
	}

	bool alwaysInRange = true;
	for (int press = 0; press < 40; press++)
	{
		settings.Cycle("MasterVolume");
		const double volume = settings.Get<double>("MasterVolume");
		alwaysInRange = alwaysInRange && volume >= 0.0 && volume <= 1.0;
	}
	Check(alwaysInRange, "forty more presses never leave 0% to 100%");

	settings.Set("Luminosity", 2.0);
	settings.Cycle("Luminosity");
	Check(settings.Get<double>("Luminosity") == 0.25, "Luminosity wraps from its max to 0.25, not to 0");
}

static void TestCycledValuesAreSavedAsWritten()
{
	std::printf("a stepped value is saved as 0.8, not 0.8000000000000002\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	settings.Cycle("MasterVolume"); // 0.75 -> 0.8
	settings.Cycle("MasterVolume"); // 0.8 -> 0.9

	Json saved;
	Check(store.Read("settings", saved), "the settings were saved");
	Check(saved.is_object() && saved["MasterVolume"].dump() == "0.9", "the saved volume reads 0.9");
}

static void TestSetKeepsTheChange()
{
	std::printf("Set keeps the change through the store\n");

	CountingStore store;
	Settings::Settings settings(store);

	settings.Set("MasterVolume", 0.4);
	Check(store.writes == 1, "one change is one save");

	Settings::Settings reloaded(store);
	Check(reloaded.Load(), "the settings load back");
	Check(reloaded.Get<double>("MasterVolume") == 0.4, "the change is there after a reload");

	settings.Set("MasterVolume", 0.4);
	Check(store.writes == 1, "setting the value it already holds saves nothing");
}

static void ChangeEverySetting(Settings::Settings& settings)
{
	for (const Settings::Descriptor& descriptor : settings.Descriptors())
	{
		settings.Cycle(descriptor.key);
	}
}

static void CheckSameValues(const Settings::Settings& expected, const Settings::Settings& actual, const std::string& through)
{
	for (const Settings::Descriptor& descriptor : expected.Descriptors())
	{
		Check(actual.Get<double>(descriptor.key) == expected.Get<double>(descriptor.key),
			descriptor.key + " round-trips through " + through);
	}
}

static void TestEverySettingRoundTrips()
{
	std::printf("every setting round-trips, in memory and through a file\n");

	{
		SaveSystem::MemorySaveStore store;
		Settings::Settings settings(store);
		ChangeEverySetting(settings);

		Settings::Settings reloaded(store);
		Check(reloaded.Load(), "the settings load back from memory");
		CheckSameValues(settings, reloaded, "memory");
	}

	{
		TempDirectory directory;
		SaveSystem::FileSaveStore store(directory.path);
		Settings::Settings settings(store);
		ChangeEverySetting(settings);

		Check(std::filesystem::exists(directory.path / "settings.json"), "the settings are kept in settings.json");

		Settings::Settings reloaded(store);
		Check(reloaded.Load(), "the settings load back from the file");
		CheckSameValues(settings, reloaded, "a file");
	}
}

static void TestInitSavesTheDefaultsWhenNothingIsSaved()
{
	std::printf("Init saves the defaults when nothing is saved yet\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);
	settings.Init();

	Json saved;
	Check(store.Read("settings", saved), "Init wrote the settings");
	Check(saved.is_object() && saved.value("Version", 0) == 1, "with the version");
	Check(saved.is_object() && saved.size() == SHIPPED_KEYS.size() + 1, "and every setting");
}

static void TestLoadKeepsDefaultsForValuesItCantHold()
{
	std::printf("Load keeps the default for any value its setting can't hold as written\n");

	SaveSystem::MemorySaveStore store;
	Json saved = {
		{ "Version", 1 },
		{ "Luminosity", 0.0 },     // below its min
		{ "MasterVolume", 3.0 },   // above its max
		{ "Shadows", 2.5 },        // not an option
		{ "Difficulty", -1 },      // not an option
		{ "Vsync", "yes" },        // not a number
		{ "MonoOnly", 2 },         // not on or off
		{ "MusicVolume", 0.5 },    // fine
		{ "Foliage", 2 },          // fine
		{ "NoSuchSetting", 7 },    // no setting has it
	};
	store.Write("settings", saved);

	Settings::Settings settings(store);
	Check(settings.Load(), "the settings still load");

	Check(settings.Get<double>("Luminosity") == 1.0, "Luminosity 0 keeps its default of 1, as the lighting used to read it");
	Check(settings.Get<double>("MasterVolume") == 0.75, "a volume of 300% keeps its default");
	Check(settings.Get<int>("Shadows") == 4, "Shadows 2.5 keeps its default");
	Check(settings.Get<int>("Difficulty") == 1, "Difficulty -1 keeps its default");
	Check(settings.Get<bool>("Vsync"), "VSync \"yes\" keeps its default");
	Check(!settings.Get<bool>("MonoOnly"), "Mono Audio 2 keeps its default");
	Check(settings.Get<double>("MusicVolume") == 0.5, "a valid volume is taken");
	Check(settings.Get<int>("Foliage") == 2, "a valid option is taken");
}

static void TestLoadReadsTheShippedSettingsFile()
{
	std::printf("Load reads a settings.json written before settings knew their kind\n");

	SaveSystem::MemorySaveStore store;
	store.Write("settings", Json::parse(SHIPPED_SETTINGS));

	Settings::Settings settings(store);
	Check(settings.Load(), "the shipped file loads");

	Check(settings.Get<double>("MasterVolume") == 0.75, "the master volume is 75%");
	Check(settings.Get<int>("Shadows") == 4, "Shadows are Ultra");
	Check(settings.Get<bool>("Vsync"), "VSync is on");
	Check(settings.Get<bool>("GameplaySFX"), "gameplay sounds are on");
	Check(settings.Get<int>("AntiAliasing") == 0, "anti-aliasing is off");

	// 0 was never a value these could mean, so their defaults stand
	Check(settings.Get<double>("Luminosity") == 1.0, "Luminosity is 1");
	Check(settings.Get<double>("CameraSpeed") == 1.0, "camera speed is 100%");
	Check(settings.Get<double>("RenderResolution") == 1.0, "render resolution is 100%");
	Check(settings.Get<double>("HudSize") == 1.0, "HUD size is 100%");
}

static void TestLoadRefusesWhatItCantRead()
{
	std::printf("Load refuses settings it can't read, and changes nothing\n");

	{
		SaveSystem::MemorySaveStore store;
		Settings::Settings settings(store);
		Check(!settings.Load(), "no saved settings is refused");
		Check(settings.Get<double>("MasterVolume") == 0.75, "and the defaults stand");
	}

	const Json refused[] = {
		Json{ { "Version", 2 }, { "MasterVolume", 0.2 } },     // newer game
		Json{ { "Version", "one" }, { "MasterVolume", 0.2 } }, // version not a number
		Json::array({ 1, 2, 3 }),                              // not an object
	};
	const char* why[] = { "a newer version", "a version that is not a number", "JSON that is not an object" };

	for (int i = 0; i < 3; i++)
	{
		SaveSystem::MemorySaveStore store;
		Settings::Settings settings(store);
		settings.Set("MasterVolume", 0.5);

		store.Write("settings", refused[i]);
		Check(!settings.Load(), std::string("settings with ") + why[i] + " are refused");
		Check(settings.Get<double>("MasterVolume") == 0.5, std::string("settings with ") + why[i] + " change nothing");
	}

	{
		TempDirectory directory;
		{
			std::ofstream file(directory.path / "settings.json");
			file << "{ \"MasterVolume\": 0.2, ";
		}

		SaveSystem::FileSaveStore store(directory.path);
		Settings::Settings settings(store);
		Check(!settings.Load(), "a file that is not JSON is refused");
		Check(settings.Get<double>("MasterVolume") == 0.75, "and the defaults stand");
	}
}

static void TestSubscribersHearTheirOwnKeys()
{
	std::printf("a subscriber hears each change to its own keys, once\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	int audioHeard = 0;
	int lightingHeard = 0;
	settings.Subscribe({ "MasterVolume", "MusicVolume" }, [&] { ++audioHeard; });
	settings.Subscribe({ "Shadows", "Luminosity" }, [&] { ++lightingHeard; });

	settings.Set("MasterVolume", 0.5);
	Check(audioHeard == 1, "a volume change is heard by the audio");
	Check(lightingHeard == 0, "and not by the lighting");

	settings.Set("MasterVolume", 0.5);
	Check(audioHeard == 1, "setting the value it already holds is not heard");

	settings.Cycle("Shadows");
	Check(lightingHeard == 1, "a press on Shadows is heard by the lighting");
	Check(audioHeard == 1, "and not by the audio");

	settings.Set("Vsync", 0.0);
	settings.Set("NoSuchSetting", 1.0);
	Check(audioHeard == 1 && lightingHeard == 1, "a setting neither listens to is heard by neither");
}

static void TestLoadIsHeardOncePerSubscriber()
{
	std::printf("Load is heard once by each subscriber whose settings it changed\n");

	SaveSystem::MemorySaveStore store;
	store.Write("settings", Json{ { "Version", 1 }, { "MasterVolume", 0.2 }, { "MusicVolume", 0.3 }, { "Shadows", 4 } });

	Settings::Settings settings(store);

	int audioHeard = 0;
	int lightingHeard = 0;
	settings.Subscribe({ "MasterVolume", "MusicVolume" }, [&] { ++audioHeard; });
	settings.Subscribe({ "Shadows", "Luminosity" }, [&] { ++lightingHeard; });

	settings.Load();
	Check(audioHeard == 1, "two volume changes are heard once");
	Check(lightingHeard == 0, "Shadows saved at its default is not a change");
}

static void TestFormatValue()
{
	std::printf("each kind of setting shows its value in its own words\n");

	SaveSystem::MemorySaveStore store;
	Settings::Settings settings(store);

	Check(Settings::FormatValue(*settings.Find("Vsync"), 1.0) == "On", "a Toggle on reads On");
	Check(Settings::FormatValue(*settings.Find("Vsync"), 0.0) == "Off", "a Toggle off reads Off");
	Check(Settings::FormatValue(*settings.Find("Shadows"), 4.0) == "Ultra", "Shadows 4 reads Ultra");
	Check(Settings::FormatValue(*settings.Find("Shadows"), 0.0) == "Off", "Shadows 0 reads Off");
	Check(Settings::FormatValue(*settings.Find("Textures"), 0.0) == "Lowest", "Textures 0 reads Lowest, never Off");
	Check(Settings::FormatValue(*settings.Find("MasterVolume"), 0.75) == "75%", "a volume of 0.75 reads 75%");

	Settings::Descriptor plain;
	plain.kind = Settings::Kind::Range;
	plain.min = 0.0;
	plain.max = 4.0;
	plain.step = 0.5;
	Check(Settings::FormatValue(plain, 1.5) == "1.5", "a Range that isn't a percentage reads as a number");
}

int main()
{
	std::printf("Settings tests\n\n");

	TestEverySettingStartsAtItsDefault();
	TestDescriptorsAreWellFormed();
	TestKeysAreTheOnesSettingsJsonUses();
	TestSetClampsARange();
	TestSetRoundsAChoiceToAnOption();
	TestSetTurnsAToggleOnForAnythingButZero();
	TestSetRefusesUnknownKeysAndNonNumbers();
	TestCycleFlipsAToggle();
	TestCycleWalksEveryOptionOfAChoice();
	TestCycleStepsARangeAndWraps();
	TestCycledValuesAreSavedAsWritten();
	TestSetKeepsTheChange();
	TestEverySettingRoundTrips();
	TestInitSavesTheDefaultsWhenNothingIsSaved();
	TestLoadKeepsDefaultsForValuesItCantHold();
	TestLoadReadsTheShippedSettingsFile();
	TestLoadRefusesWhatItCantRead();
	TestSubscribersHearTheirOwnKeys();
	TestLoadIsHeardOncePerSubscriber();
	TestFormatValue();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
