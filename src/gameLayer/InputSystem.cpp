#include "InputSystem.h"

#include <algorithm>
#include <iostream>

InputSystem::InputSystem() {
	inputActions.resize(MAX_ACTION); // One slot per ActionType, indexed by action id

	// Load From File else Set Default
	SetDefaultActions();
}

void InputSystem::Update() {
	pressedKeys.clear();

	// Drain raylib's key queue to get every key pressed this frame
	for (int key = GetKeyPressed(); key != KEY_NULL; key = GetKeyPressed()) {
		pressedKeys.push_back(key);
		if (std::find(heldKeys.begin(), heldKeys.end(), key) == heldKeys.end()) {
			heldKeys.push_back(key);
		}
	}

	std::erase_if(heldKeys, [](int key) { return !IsKeyDown(key); });
}

InputAction* InputSystem::GetAction(int action) {
	if (action <= NO_ACTION || action >= MAX_ACTION) { return nullptr; }
	return &inputActions[action];
}

bool InputSystem::IsActionDown(int action) {
	const auto input = GetAction(action);
	if (!input || !input->isEnabled) { return false; }
	return IsKeyDown(input->key) || (input->button >= 0 && IsGamepadButtonDown(gamepadIndex, input->button));
}

bool InputSystem::IsActionPressed(int action) {
	const auto input = GetAction(action);
	if (!input || !input->isEnabled) { return false; }
	return IsKeyPressed(input->key) || (input->button >= 0 && IsGamepadButtonPressed(gamepadIndex, input->button));
}

bool InputSystem::IsActionReleased(int action) {
	const auto input = GetAction(action);
	if (!input || !input->isEnabled) { return false; }
	return IsKeyReleased(input->key) || (input->button >= 0 && IsGamepadButtonReleased(gamepadIndex, input->button));
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
	CreateAction(ACTION_MOVE_FORWARD, "Move Forward", KEY_W, GAMEPAD_BUTTON_LEFT_FACE_UP);
	CreateAction(ACTION_MOVE_BACKWARD, "Move Backward", KEY_S, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
	CreateAction(ACTION_MOVE_RIGHT, "Move Right", KEY_D, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
	CreateAction(ACTION_MOVE_LEFT, "Move Left", KEY_A, GAMEPAD_BUTTON_LEFT_FACE_LEFT);

	CreateAction(ACTION_MOVE_CROUCH, "Crouch", KEY_LEFT_CONTROL, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
	CreateAction(ACTION_MOVE_SPRINT, "Sprint", KEY_LEFT_SHIFT, GAMEPAD_BUTTON_LEFT_THUMB);
	CreateAction(ACTION_MOVE_INTERACT, "Interact", KEY_F, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
	CreateAction(ACTION_MOVE_JUMP, "Jump", KEY_SPACE, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);


	CreateAction(ACTION_USE_ARTIFACT, "Artifact_Interact", KEY_F, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
	CreateAction(ACTION_USE_ARTIFACT_RIGHT, "Artifact_Right", KEY_E, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
	CreateAction(ACTION_USE_ARTIFACT_LEFT, "Artifact_Left", KEY_Q, GAMEPAD_BUTTON_LEFT_FACE_LEFT);

	// Change to use mouse Buttons
	CreateAction(ACTION_USE_ITEM, "Item_Left", KEY_T, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
	CreateAction(ACTION_USE_ITEM2, "Item_Right", KEY_G, GAMEPAD_BUTTON_LEFT_TRIGGER_2);
	
	
	// UI Action
	CreateAction(ACTION_UI_PAUSE, "Pause", KEY_TAB, GAMEPAD_BUTTON_MIDDLE_RIGHT);

}
