#include "Overlays.h"

#include <Scene.h>
#include <SceneManager.h>
#include <Settings.h>
#include <UIEngine.h>
#include <gameMain.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace UI {

	// Closes the panel it sits on, landing on whatever overlay is beneath it
	static void returnButton(OverlayStack& overlays, Overlay overlay) {
		if (drawButton("Return")) { overlays.Close(overlay); }
	}

	static void drawTitle(SceneManagement::Scene& scene) {

		// Each row stacks under the last inside the panel, so a new button is one more call here.
		beginPanel("Title", Rectangle{}, Rectangle{ 0.4f, 0.3f, 0.2f, 0.4f });
		{
			drawText("Main Menu");

			if (drawButton("Play"))
			{
				std::cout << "Play Button Clicked\n";
				SceneManagement::SceneManager_push(&SceneManagement::SceneManager::getInstance(), SCENE_TUTORIAL);
			}
			if (drawButton("Multiplayer"))
			{
				std::cout << "Open Multiplayer Menu\n";
				scene.overlays.Open(Overlay::Multiplayer);
			}
			if (drawButton("Settings"))
			{
				std::cout << "Open Settings Menu\n";
				scene.overlays.Open(Overlay::Settings);
			}
			if (drawButton("Exit"))
			{
				std::cout << "Exit Button Clicked\n";
				request_exit();
			}
		}
		endPanel();
	}

	// The page of the Settings menu on show. Every setting at once overflows the panel.
	static size_t settingsSection = 0;

	static void drawSettings(OverlayStack& overlays) {

		Settings::Settings& settings = Settings::Settings::getInstance();

		// The pages, in the order their first setting is listed
		std::vector<std::string> sections;
		for (const Settings::Descriptor& descriptor : settings.Descriptors()) {
			if (std::find(sections.begin(), sections.end(), descriptor.section) == sections.end()) {
				sections.push_back(descriptor.section);
			}
		}
		if (sections.empty()) { return; }
		settingsSection %= sections.size();

		// The sum of (X + width), (Y + height) is the size of the panel.
		beginPanel("Settings", Rectangle{}, Rectangle{ 0.1f, 0.1f, 0.8f, 0.8f });

		drawText("Settings");

		returnButton(overlays, Overlay::Settings);

		if (drawButton("Section: " + sections[settingsSection])) {
			settingsSection = (settingsSection + 1) % sections.size();
		}

		// A press moves the setting on by one, as its kind says: flip, next option, or next step
		for (const Settings::Descriptor& descriptor : settings.Descriptors()) {
			if (descriptor.section != sections[settingsSection]) { continue; }

			const std::string row = descriptor.label + ": " + Settings::FormatValue(descriptor, settings.Get<double>(descriptor.key));
			if (drawButton(row)) {
				settings.Cycle(descriptor.key);
			}
		}

		endPanel();
	}

	static void drawMultiplayer(OverlayStack& overlays) {

		beginPanel("Multiplayer", Rectangle{}, Rectangle{ 0.1f, 0.1f, 0.8f, 0.8f });

		drawText("Multiplayer");

		returnButton(overlays, Overlay::Multiplayer);

		if (drawButton("Start Server")) {
			// Start Server
		}

		if (drawButton("Join Server")) {
			// Join Server
		}

		endPanel();
	}

	void drawOverlays(SceneManagement::Scene& scene) {

		OverlayStack& overlays = scene.overlays;

		/// Background
		if (overlays.DimsWorld()) {
			DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 20, 20, 20, 200 });
		}

		const std::optional<Overlay> top = overlays.Top();
		if (!top) { return; }

		switch (*top)
		{
		case Overlay::Title:       drawTitle(scene); break;
		case Overlay::Settings:    drawSettings(overlays); break;
		case Overlay::Multiplayer: drawMultiplayer(overlays); break;
		case Overlay::MiniGame:    scene.miniGameSession.Draw(); break;
		}
	}
}
