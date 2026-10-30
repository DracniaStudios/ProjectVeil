#include <Settings.h>

#include <SaveStore.h>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <raylib.h>

namespace Settings {

	using Json = nlohmann::json;
	constexpr int VERSION = 1;
	constexpr const char* SETTINGS_DIRECTORY = RESOURCES_PATH "../saves/";
	constexpr const char* STORE_NAME = "settings";

#pragma region Descriptors

	static Descriptor Toggle(std::string key, std::string label, std::string section, bool on)
	{
		Descriptor descriptor;
		descriptor.key = std::move(key);
		descriptor.label = std::move(label);
		descriptor.section = std::move(section);
		descriptor.kind = Kind::Toggle;
		descriptor.defaultValue = on ? 1.0 : 0.0;
		return descriptor;
	}

	static Descriptor Choice(std::string key, std::string label, std::string section,
		std::vector<std::string> options, int defaultIndex)
	{
		Descriptor descriptor;
		descriptor.key = std::move(key);
		descriptor.label = std::move(label);
		descriptor.section = std::move(section);
		descriptor.kind = Kind::Choice;
		descriptor.options = std::move(options);
		descriptor.defaultValue = defaultIndex;
		return descriptor;
	}

	static Descriptor Range(std::string key, std::string label, std::string section,
		double min, double max, double step, double defaultValue, bool percent)
	{
		Descriptor descriptor;
		descriptor.key = std::move(key);
		descriptor.label = std::move(label);
		descriptor.section = std::move(section);
		descriptor.kind = Kind::Range;
		descriptor.min = min;
		descriptor.max = max;
		descriptor.step = step;
		descriptor.defaultValue = defaultValue;
		descriptor.percent = percent;
		return descriptor;
	}

	// Keys are the names settings.json has always used, "hideHUD" included, so
	// a file written before descriptors existed still loads.
	static std::vector<Descriptor> GameDescriptors()
	{
		// Every quality selection is 0-4, and 0 switches the feature off.
		// Textures and view distance can't be off, so their lowest step is Lowest.
		const std::vector<std::string> quality = { "Off", "Low", "Medium", "High", "Ultra" };
		const std::vector<std::string> detail = { "Lowest", "Low", "Medium", "High", "Ultra" };

		return {
			/* Gameplay */
			Choice("Difficulty", "Difficulty", "Gameplay",{ "Easy", "Normal", "Hard" }, 1),
			Toggle("InvertedCameraYAxis", "Invert Camera Y", "Gameplay", false),
			Toggle("InvertedCameraXAxis", "Invert Camera X", "Gameplay", false),
			Range("CameraSpeed", "Camera Speed", "Gameplay", 0.25, 2.0, 0.25, 1.0, true),
			Toggle("CameraSmoothEffect", "Camera Smoothing", "Gameplay", false),
			Toggle("CameraShadeEffect", "Camera Shade", "Gameplay", false),
			Toggle("HideClueVFX", "Hide Clue Effects", "Gameplay", false),

			/* Display */
			Choice("Resolution", "Resolution", "Display",{ "1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160" }, 1),
			Range("RenderResolution", "Render Resolution", "Display", 0.5, 1.0, 0.25, 1.0, true),
			Choice("FrameRate", "Frame Rate", "Display", { "30", "60", "120", "240", "360" }, 1),
			// The lighting's exposure; 0 would render a black frame
			Range("Luminosity", "Luminosity", "Display", 0.25, 2.0, 0.25, 1.0, true),
			Choice("DisplayMode", "Display Mode", "Display", { "Windowed", "Borderless", "Fullscreen" }, 0),
			Toggle("Vsync", "VSync", "Display", true),
			Range("HudSize", "HUD Size", "Display", 0.5, 1.5, 0.25, 1.0, true),
			Toggle("hideHUD", "Hide HUD", "Display", false),

			/* Video */
			Toggle("HighContrastDisplay", "High Contrast", "Video", false),
			Choice("Shadows", "Shadows", "Video", quality, 4),
			Choice("Textures", "Textures", "Video", detail, 4),
			Choice("ViewDistance", "View Distance", "Video", detail, 4),
			Choice("Effects", "Effects", "Video", quality, 4),
			Choice("PostProcessing", "Post Processing", "Video", quality, 4),
			Choice("Foliage", "Foliage", "Video", quality, 4),
			Choice("AntiAliasing", "Anti-Aliasing", "Video", quality, 0),

			/* Sound */
			Range("MasterVolume", "Master Volume", "Sound", 0.0, 1.0, 0.1, 0.75, true),
			Range("MusicVolume", "Music Volume", "Sound", 0.0, 1.0, 0.1, 1.0, true),
			Range("SFXVolume", "SFX Volume", "Sound", 0.0, 1.0, 0.1, 1.0, true),
			Range("DialogueVolume", "Dialogue Volume", "Sound", 0.0, 1.0, 0.1, 1.0, true),
			Toggle("GameplaySFX", "Gameplay Sounds", "Sound", true),
			Toggle("MonoOnly", "Mono Audio", "Sound", false),

			/* Language */
			// Subtitles
			// Text Language
			// Subtitle Opacity

			/* KeyBinds */
			// Vibration
		};
	}

#pragma endregion

#pragma region Values

	// Rounds off float noise, so settings.json reads 0.8 rather than 0.8000000000000002
	static double Tidy(double value)
	{
		return std::round(value * 1e6) / 1e6;
	}

	// The nearest value the setting can hold
	static double Validate(const Descriptor& descriptor, double value)
	{
		switch (descriptor.kind)
		{
		case Kind::Toggle:
			return value != 0.0 ? 1.0 : 0.0;
		case Kind::Choice:
		{
			const double last = descriptor.options.empty() ? 0.0 : static_cast<double>(descriptor.options.size() - 1);
			return std::clamp(std::round(value), 0.0, last);
		}
		case Kind::Range:
			return Tidy(std::clamp(value, descriptor.min, descriptor.max));
		}
		return descriptor.defaultValue;
	}

	// Whether the value is one the setting holds as it is, without rounding or
	// clamping. A loaded value has to be, or the default is kept instead.
	static bool CanHold(const Descriptor& descriptor, double value)
	{
		if (!std::isfinite(value)) { return false; }

		switch (descriptor.kind)
		{
		case Kind::Toggle: return value == 0.0 || value == 1.0;
		case Kind::Choice: return value == std::floor(value) && value >= 0.0 && value < static_cast<double>(descriptor.options.size());
		case Kind::Range:  return value >= descriptor.min && value <= descriptor.max;
		}
		return false;
	}

	std::string FormatValue(const Descriptor& descriptor, double value)
	{
		switch (descriptor.kind)
		{
		case Kind::Toggle:
			return value != 0.0 ? "On" : "Off";
		case Kind::Choice:
		{
			const long index = std::lround(value);
			if (index >= 0 && static_cast<size_t>(index) < descriptor.options.size()) { return descriptor.options[index]; }
			return std::to_string(index);
		}
		case Kind::Range:
		{
			if (descriptor.percent) { return std::to_string(std::lround(value * 100.0)) + "%"; }

			char text[32] = {};
			std::snprintf(text, sizeof(text), "%g", value);
			return text;
		}
		}
		return {};
	}

#pragma endregion

	Settings::Settings(SaveSystem::SaveStore& store)
		: store(store), descriptors(GameDescriptors())
	{
		values.reserve(descriptors.size());
		for (const Descriptor& descriptor : descriptors) { values.push_back(descriptor.defaultValue); }
	}

	Settings& Settings::getInstance()
	{
		static SaveSystem::FileSaveStore store(SETTINGS_DIRECTORY);
		static Settings instance(store);
		return instance;
	}

	const Descriptor* Settings::Find(const std::string& key) const
	{
		const auto found = std::find_if(descriptors.begin(), descriptors.end(),
			[&](const Descriptor& descriptor) { return descriptor.key == key; });
		return found != descriptors.end() ? &*found : nullptr;
	}

	double Settings::ValueOf(const std::string& key) const
	{
		const Descriptor* descriptor = Find(key);
		if (descriptor == nullptr)
		{
			std::cerr << "[Settings] No setting named: " << key << "\n";
			return 0.0;
		}
		return values[descriptor - descriptors.data()];
	}

	bool Settings::Set(const std::string& key, double value)
	{
		const Descriptor* descriptor = Find(key);
		if (descriptor == nullptr)
		{
			std::cerr << "[Settings] No setting named: " << key << "\n";
			return false;
		}
		if (!std::isfinite(value)) { return false; }

		double& held = values[descriptor - descriptors.data()];
		const double validated = Validate(*descriptor, value);
		if (validated == held) { return true; }

		held = validated;
		Save();
		Notify({ key });
		return true;
	}

	bool Settings::Cycle(const std::string& key)
	{
		const Descriptor* descriptor = Find(key);
		if (descriptor == nullptr)
		{
			std::cerr << "[Settings] No setting named: " << key << "\n";
			return false;
		}

		const double value = values[descriptor - descriptors.data()];

		switch (descriptor->kind)
		{
		case Kind::Toggle:
			return Set(key, value != 0.0 ? 0.0 : 1.0);

		case Kind::Choice:
		{
			const double next = value + 1.0;
			return Set(key, next < static_cast<double>(descriptor->options.size()) ? next : 0.0);
		}

		case Kind::Range:
		{
			// Steps land on min + n * step, so a value saved between them (0.75 with
			// a step of 0.1) moves up to the next one (0.8) rather than off the grid.
			// The tolerance keeps a value already on a step from counting as below it.
			// Every Range's max is itself a step, so stepping past it wraps to min.
			constexpr double tolerance = 1e-6;
			const double stepsTaken = std::floor((value - descriptor->min) / descriptor->step + tolerance);
			const double next = descriptor->min + (stepsTaken + 1.0) * descriptor->step;

			return Set(key, next > descriptor->max + tolerance ? descriptor->min : next);
		}
		}
		return false;
	}

	void Settings::Subscribe(std::vector<std::string> keys, std::function<void()> onChange)
	{
		subscribers.push_back(Subscriber{ std::move(keys), std::move(onChange) });
	}

	void Settings::Notify(const std::vector<std::string>& changedKeys) const
	{
		for (const Subscriber& subscriber : subscribers)
		{
			const bool heard = std::any_of(subscriber.keys.begin(), subscriber.keys.end(), [&](const std::string& key) {
				return std::find(changedKeys.begin(), changedKeys.end(), key) != changedKeys.end();
			});
			if (heard) { subscriber.onChange(); }
		}
	}

	bool Settings::Save() const
	{
		Json j;
		j["Version"] = VERSION;

		for (size_t i = 0; i < descriptors.size(); i++) { j[descriptors[i].key] = values[i]; }

		if (!store.Write(STORE_NAME, j))
		{
			std::cerr << "[Settings] Failed to save settings\n";
			return false;
		}

		std::cout << "[Settings] Saved Settings\n";
		return true;
	}

	bool Settings::Load()
	{
		Json j;
		if (!store.Read(STORE_NAME, j)) { return false; }

		if (!j.is_object())
		{
			std::cerr << "[Settings] Settings are not a JSON object\n";
			return false;
		}

		const auto version = j.find("Version");
		if (version != j.end() && !version->is_number())
		{
			std::cerr << "[Settings] Save version is not a number: " << version->dump() << "\n";
			return false;
		}
		if (version != j.end() && version->get<double>() > VERSION)
		{
			std::cerr << "[Settings] Save version is newer than the game supports.\n";
			return false;
		}

		std::vector<std::string> changedKeys;
		for (size_t i = 0; i < descriptors.size(); i++)
		{
			const Descriptor& descriptor = descriptors[i];

			const auto saved = j.find(descriptor.key);
			if (saved == j.end()) { continue; }

			// A hand-edited or corrupted settings.json can hold anything under a
			// key; a value the setting can't hold as written keeps its default
			// instead, so Luminosity 0 (a black frame) never reaches the lighting.
			if (!saved->is_number() || !CanHold(descriptor, saved->get<double>()))
			{
				std::cerr << "[Settings] Ignored " << descriptor.key << ": " << saved->dump()
					<< " is not a value it can hold, keeping " << FormatValue(descriptor, values[i]) << "\n";
				continue;
			}

			const double value = Validate(descriptor, saved->get<double>());
			if (value != values[i])
			{
				values[i] = value;
				changedKeys.push_back(descriptor.key);
			}
		}

		Notify(changedKeys);

		std::cout << "[Settings] Loaded Settings\n";
		return true;
	}

	void Settings::Init()
	{
		if (!Load()) {
			Save();
		}
	}
}
