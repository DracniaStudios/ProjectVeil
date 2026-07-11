#pragma once
#ifndef SETTINGS_H
#define SETTINGS_H

#include <raylib.h>
#include <string>
#include <vector>

struct GameSettings {
	GameSettings();
	std::string name = "Default_Settings_Name";
	float value = 0;// False for Bools
};

class Settings {
private:
	Settings() = default;
public:
	Settings(const Settings&) = delete;
	Settings& operator=(const Settings&) = delete;
	Settings(Settings&&) = delete;
	Settings& operator=(Settings&&) = delete;

	static Settings& getInstance() {
		static Settings instance;
		return instance;
	}


	void Init();
	void Update();
	bool Load();
	bool Save();

	std::vector<GameSettings*> settingsList = {};

	/* Gameplay Settings */
	GameSettings difficulty = {}; // Selection
	GameSettings invertedCameraYAxis = {}; // Bool
	GameSettings invertedCameraXAxis = {}; // Bool
	GameSettings cameraSpeed = {};
	GameSettings cameraSmoothEffect = {};
	GameSettings cameraShadeEffect = {};
	GameSettings hideClueVFX = {};

	/* Display Settings */
	GameSettings resolution = {};
	GameSettings renderResolution = {};
	GameSettings luminosity = {};
	GameSettings displayMode = {};
	GameSettings vsync = {}; // Bool
	GameSettings hudSize = {}; // By %
	GameSettings hideHUD = {}; // Bool
	//GameSettings* hudFraming = {}; Adjust HUD Display

	/* Video */
	GameSettings highContrastDisplay = {}; // Bool
	GameSettings shadows = {}; // Selection
	GameSettings textures = {}; // Selection
	GameSettings viewDistance = {}; // Selection
	GameSettings effects = {}; // Selection
	GameSettings postProcessing = {}; // Selection
	GameSettings foliage = {}; // Selection
	GameSettings antiAliasing = {}; // Selection
	// Selection: Low, Medium, High, Ultra

	/* Sound */
	GameSettings masterVolume = {};
	GameSettings musicVolume = {};
	GameSettings sfxVolume = {};
	GameSettings dialogueVolume = {};
	GameSettings gameplaySFX = {}; // Bool
	GameSettings monoOnly = {}; // Bool

	/* Language */
	// Subtitles
	// Text Language
	// Subtitle Opacity

	/* KeyBinds */
	// Vibration

};
#endif	