#include <Settings.h>

#include <SaveSystem.h>
#include <SceneManager.h>
#include <filesystem>
#include <fstream>

GameSettings::GameSettings() {
	name = "Undefined";
	value = 0;
}

using Json = nlohmann::json;
constexpr int VERSION = 1;
constexpr const char* SETTINGS_SAVE_PATH = RESOURCES_PATH "../saves/settings.json";
constexpr const char* SETTINGS_SAVE_BAK = RESOURCES_PATH "../saves/settings.json.bak";

static bool JsonToSettings(Json& j, std::vector<GameSettings*> list) {
	if (j.value("Version", 0) > VERSION) {
		std::cerr << "[Settings] Save version is newer than the game supports.\n";
		return false;
	}

	for (auto& setting : list) {
		if (j.contains(setting->name)) {
			setting->value = j[setting->name];
		}
	}
	return true;
}

bool Settings::Save() {
	
	const std::filesystem::path path = SETTINGS_SAVE_PATH;
	const std::filesystem::path tmpPath = path.string() + ".tmp";
	const std::filesystem::path bakPath = path.string() + ".bak";

	std::error_code errorCode;
	if (path.has_parent_path()) {
		std::filesystem::create_directories(path.parent_path(), errorCode);
	}

	// Use Temp To Avoid Crash Corruption
	std::ofstream file(tmpPath, std::ios::binary);
	if (!file.is_open()) { return false; }

	Json j;
	j["Version"] = VERSION;

	for (auto& setting : settingsList) {
		j[setting->name] = setting->value;
		std::cout << "[Settings] Saved: " << setting->name << " With Value: " << setting->value << "\n";
	}

	file << j.dump(2);
	file.close();

	std::filesystem::rename(path, bakPath, errorCode);
	std::filesystem::rename(tmpPath, path, errorCode);

	std::cout << "[Settings] Saved Settings: " << path.string() << "\n";
	return true;
}
bool Settings::Load() {
	std::ifstream file(SETTINGS_SAVE_PATH, std::ios::binary);

	if (!file.is_open()) {
		std::cerr << "[Settings] Failed to open save file: " << SETTINGS_SAVE_PATH << "\n";
		return false;
	}

	Json j = Json::parse(file, nullptr, false);
	file.close();

	if (j.is_discarded()) {
		std::cerr << "[Settings] Save file Is corrupted: " << SETTINGS_SAVE_PATH << "\n";
		return false;
	}

	if (!JsonToSettings(j, settingsList)) return false;

	std::cout << "[Settings] Loaded Settings: " << SETTINGS_SAVE_PATH << "\n";
	return true;

}


void Settings::Init() {

	// Create Base Files 
	
	/* Gameplay*/
	{
		difficulty.name = "Difficulty";
		settingsList.push_back(&difficulty);

		invertedCameraYAxis.name = "InvertedCameraYAxis";
		settingsList.push_back(&invertedCameraYAxis);

		invertedCameraXAxis.name = "InvertedCameraXAxis";
		settingsList.push_back(&invertedCameraXAxis);

		cameraSpeed.name = "CameraSpeed";
		settingsList.push_back(&cameraSpeed);

		cameraSmoothEffect.name = "CameraSmoothEffect";
		settingsList.push_back(&cameraSmoothEffect);

		cameraShadeEffect.name = "CameraShadeEffect"; 
		settingsList.push_back(&cameraShadeEffect);

		hideClueVFX.name = "HideClueVFX"; 
		settingsList.push_back(&hideClueVFX);
	}

	/* Display */
	{
		resolution.name = "Resolution";
		settingsList.push_back(&resolution);
		
		renderResolution.name = "RenderResolution";
		settingsList.push_back(&renderResolution);
		
		luminosity.name = "Luminosity";
		settingsList.push_back(&luminosity);
		
		displayMode.name = "DisplayMode";
		settingsList.push_back(&displayMode);
		
		vsync.name = "Vsync"; vsync.value = 1;
		settingsList.push_back(&vsync);
		
		hudSize.name = "HudSize";
		settingsList.push_back(&hudSize);

		hideHUD.name = "hideHUD";
		settingsList.push_back(&hideHUD);
	}

	/* Video */
	{
		highContrastDisplay.name = "HighContrastDisplay";
		settingsList.push_back(&highContrastDisplay);

		shadows.name = "Shadows";
		shadows.value = 4;
		settingsList.push_back(&shadows);

		textures.name = "Textures";
		textures.value = 4;
		settingsList.push_back(&textures);

		viewDistance.name = "ViewDistance";
		viewDistance.value = 4;
		settingsList.push_back(&viewDistance);

		effects.name = "Effects"; effects.value = 4;
		settingsList.push_back(&effects);

		postProcessing.name = "PostProcessing";
		postProcessing.value = 4;
		settingsList.push_back(&postProcessing);

		foliage.name = "Foliage";
		foliage.value = 4;
		settingsList.push_back(&foliage);

		antiAliasing.name = "AntiAliasing";
		settingsList.push_back(&antiAliasing);
	}

	/* Audio */
	{
		masterVolume.name = "MasterVolume";
		masterVolume.value = 0.75f;
		settingsList.push_back(&masterVolume);

		musicVolume.name = "MusicVolume";
		musicVolume.value = 1;
		settingsList.push_back(&musicVolume);

		sfxVolume.name = "SFXVolume";
		sfxVolume.value = 1;
		settingsList.push_back(&sfxVolume);

		dialogueVolume.name = "DialogueVolume";
		dialogueVolume.value = 1;
		settingsList.push_back(&dialogueVolume);

		gameplaySFX.name = "GameplaySFX";
		gameplaySFX.value = 1;
		settingsList.push_back(&gameplaySFX);

		monoOnly.name = "MonoOnly";
		settingsList.push_back(&monoOnly);
	}

	if (!Load()) {
		Save();
	}
}
void Settings::Update() {

}