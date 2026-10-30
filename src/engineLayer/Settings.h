#pragma once
#ifndef SETTINGS_H
#define SETTINGS_H

#include <cmath>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace SaveSystem { class SaveStore; }

/**
 * The player's settings: what each one means, the value it holds, and who
 * hears when it changes.
 *
 * Every setting is described once, by key and kind:
 *   Toggle  on or off, held as 1 or 0
 *   Choice  one of a list of options, held as its index
 *   Range   a number from min to max, which the menu steps through
 *
 * Set checks a value against its setting before taking it (a Toggle is on for
 * anything but 0, a Choice is rounded to an option, a Range is clamped), keeps
 * it through the store and tells the setting's subscribers. Loading takes only
 * the values a setting can hold as written and leaves the rest at their
 * defaults, so a hand-edited or outdated settings.json never reaches the game.
 */
namespace Settings {

	enum class Kind { Toggle, Choice, Range };

	struct Descriptor
	{
		std::string key;     // Its name in settings.json, and how callers ask for it
		std::string label;   // Shown in the Settings menu
		std::string section; // The page of the Settings menu it is listed on
		Kind kind = Kind::Toggle;
		double defaultValue = 0.0;

		std::vector<std::string> options; // Choice: one label per index

		// Range
		double min = 0.0;
		double max = 1.0;
		double step = 0.1;    // How far one press in the menu moves it; max is a whole number of steps from min
		bool percent = false; // Shown as a percentage: 0.75 reads "75%"
	};

	// The value as the menu shows it: "On", "High", "75%"
	std::string FormatValue(const Descriptor& descriptor, double value);

	class Settings {
	public:
		// Holds every setting at its default until Load
		explicit Settings(SaveSystem::SaveStore& store);

		Settings(const Settings&) = delete;
		Settings& operator=(const Settings&) = delete;
		Settings(Settings&&) = delete;
		Settings& operator=(Settings&&) = delete;

		// The game's settings, kept in saves/settings.json
		static Settings& getInstance();

		// Loads the saved settings, or saves the defaults when there are none
		void Init();

		// Every setting, in the order the menu lists them
		const std::vector<Descriptor>& Descriptors() const { return descriptors; }
		const Descriptor* Find(const std::string& key) const;

		// bool for a Toggle, int for a Choice's index, float for a Range
		template<typename T>
		T Get(const std::string& key) const
		{
			const double value = ValueOf(key);
			if constexpr (std::is_same_v<T, bool>) { return value != 0.0; }
			else if constexpr (std::is_integral_v<T>) { return static_cast<T>(std::lround(value)); }
			else { return static_cast<T>(value); }
		}

		// Checks the value against the setting and takes it. A change is kept
		// through the store and heard by the setting's subscribers. False for a
		// key no setting has, or a value that is not a number.
		bool Set(const std::string& key, double value);

		// Moves the setting on by one press in the menu: a Toggle flips, a Choice
		// moves to the next option, and a Range steps up to the next multiple of
		// its step. Each goes back to its first value after its last.
		bool Cycle(const std::string& key);

		// Calls onChange once each time Set, Cycle or Load changes any of keys
		void Subscribe(std::vector<std::string> keys, std::function<void()> onChange);

		// False, with every value as it was, when the store has no settings, they
		// are not JSON, or a newer game wrote them
		bool Load();
		bool Save() const;
		void SetData();

	private:
		struct Subscriber
		{
			std::vector<std::string> keys;
			std::function<void()> onChange;
		};

		double ValueOf(const std::string& key) const;
		void Notify(const std::vector<std::string>& changedKeys) const;

		SaveSystem::SaveStore& store;
		std::vector<Descriptor> descriptors;
		std::vector<double> values; // One per descriptor, in the same order
		std::vector<Subscriber> subscribers;
	};
}
#endif
