#include "InputSystem.h"

#include <Platform.h>

#include <algorithm>
#include <iostream>

namespace InputSystem {

	namespace {
		// InputAction stores plain ints, so the defaults convert once here
		constexpr int K(Platform::Key key) { return static_cast<int>(key); }
		constexpr int B(Platform::GamepadButton button) { return static_cast<int>(button); }
	}

	InputSystem::InputSystem() {
		inputActions.resize(MAX_ACTION); // One slot per ActionType, indexed by action id

		// Load From File else Set Default
		SetDefaultActions();
	}

	void InputSystem::Update() {
		pressedKeys.clear();

		// Drain the backend's key queue to get every key pressed this frame
		for (Platform::Key pressed = Platform::PopKeyPressed(); pressed != Platform::Key::Null; pressed = Platform::PopKeyPressed()) {
			const int key = static_cast<int>(pressed);
			pressedKeys.push_back(key);
			if (std::find(heldKeys.begin(), heldKeys.end(), key) == heldKeys.end()) {
				heldKeys.push_back(key);
			}
		}

		std::erase_if(heldKeys, [](int key) { return !Platform::IsKeyDown(static_cast<Platform::Key>(key)); });
	}

	InputAction* InputSystem::GetAction(int action) {
		if (action <= NO_ACTION || action >= MAX_ACTION) { return nullptr; }
		return &inputActions[action];
	}

	bool InputSystem::IsActionDown(int action) {
		const auto input = GetAction(action);
		if (!input || !input->isEnabled) { return false; }
		return Platform::IsKeyDown(static_cast<Platform::Key>(input->key)) || (input->button >= 0 && Platform::IsGamepadButtonDown(gamepadIndex, static_cast<Platform::GamepadButton>(input->button)));
	}

	bool InputSystem::IsActionPressed(int action) {
		const auto input = GetAction(action);
		if (!input || !input->isEnabled) { return false; }
		return Platform::IsKeyPressed(static_cast<Platform::Key>(input->key)) || (input->button >= 0 && Platform::IsGamepadButtonPressed(gamepadIndex, static_cast<Platform::GamepadButton>(input->button)));
	}

	bool InputSystem::IsActionReleased(int action) {
		const auto input = GetAction(action);
		if (!input || !input->isEnabled) { return false; }
		return Platform::IsKeyReleased(static_cast<Platform::Key>(input->key)) || (input->button >= 0 && Platform::IsGamepadButtonReleased(gamepadIndex, static_cast<Platform::GamepadButton>(input->button)));
	}

	void InputSystem::SetAction(int action, int key, int button) {
		const auto input = GetAction(action);
		if (!input) { std::cout << "Unknown Action: " << action << "\n"; return; }
		input->key = key;
		input->button = button;
	}

	void InputSystem::CreateAction(int action, const std::string& name, int key, int button) {
		const auto input = GetAction(action);
		if (!input) { std::cout << "Invalid Action: " << action << "\n"; return; }
		if (input->id != NO_ACTION) { std::cout << "Existing Action: " << input->name << "\n"; return; }

		input->id = action;
		input->name = name;
		input->key = key;
		input->button = button;
	}

	void InputSystem::SetDefaultActions() {

		// Future Game Pad Errors
		// Implement Axis Movement

		// Move Action
		CreateAction(ACTION_MOVE_FORWARD, "Move Forward", K(Platform::Key::W), B(Platform::GamepadButton::LeftFaceUp));
		CreateAction(ACTION_MOVE_BACKWARD, "Move Backward", K(Platform::Key::S), B(Platform::GamepadButton::LeftFaceDown));
		CreateAction(ACTION_MOVE_RIGHT, "Move Right", K(Platform::Key::D), B(Platform::GamepadButton::LeftFaceRight));
		CreateAction(ACTION_MOVE_LEFT, "Move Left", K(Platform::Key::A), B(Platform::GamepadButton::LeftFaceLeft));

		CreateAction(ACTION_MOVE_CROUCH, "Crouch", K(Platform::Key::LeftControl), B(Platform::GamepadButton::RightFaceRight));
		CreateAction(ACTION_MOVE_SPRINT, "Sprint", K(Platform::Key::LeftShift), B(Platform::GamepadButton::LeftThumb));
		CreateAction(ACTION_MOVE_INTERACT, "Interact", K(Platform::Key::F), B(Platform::GamepadButton::RightFaceLeft));
		CreateAction(ACTION_MOVE_JUMP, "Jump", K(Platform::Key::Space), B(Platform::GamepadButton::RightFaceDown));

		CreateAction(ACTION_USE_ARTIFACT, "Artifact_Interact", K(Platform::Key::E), B(Platform::GamepadButton::MiddleLeft));
		CreateAction(ACTION_USE_ARTIFACT_RIGHT, "Artifact_Right", K(Platform::Key::R), B(Platform::GamepadButton::RightThumb));
		CreateAction(ACTION_USE_ARTIFACT_LEFT, "Artifact_Left", K(Platform::Key::Q), B(Platform::GamepadButton::Middle));

		// Item Actions
		CreateAction(ACTION_USE_ITEM, "Item_Left", K(Platform::Key::T), B(Platform::GamepadButton::LeftTrigger2));
		CreateAction(ACTION_USE_ITEM2, "Item_Right", K(Platform::Key::G), B(Platform::GamepadButton::RightTrigger2));
		CreateAction(ACTION_USE_FLASHLIGHT, "Flashlight", K(Platform::Key::L), B(Platform::GamepadButton::RightFaceUp));

		// Editor Actions
		// KEY_LEFT_CONTROL is also the modifier every Ctrl+ editor hotkey reads
		// directly via IsKeyDown (panel toggles, undo, duplicate — see
		// WorldEditor::UpdateHotkeys); binding it here too meant holding Ctrl for
		// any of those also flew the camera upward for as long as it was held.
		CreateAction(ACTION_EDITOR_UP, "Editor_Up", K(Platform::Key::Space), B(Platform::GamepadButton::RightTrigger1));
		CreateAction(ACTION_EDITOR_DOWN, "Editor_Down", K(Platform::Key::LeftAlt), B(Platform::GamepadButton::LeftTrigger1));

		// UI Action
		CreateAction(ACTION_UI_BACK, "Back", K(Platform::Key::Tab), B(Platform::GamepadButton::MiddleLeft));
		CreateAction(ACTION_UI_PAUSE, "Pause", K(Platform::Key::Escape), B(Platform::GamepadButton::MiddleRight));

	}
}