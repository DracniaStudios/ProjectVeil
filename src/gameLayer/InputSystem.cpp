#include "InputSystem.h"

#include <iostream>

InputAction::InputAction() {
	InputSystem::getInstance().inputActions.push_back(*this);
}

InputAction::InputAction(int id, int k, int b) {
	this->id = id;
	key = k;
	button = b;
	InputSystem::getInstance().inputActions.push_back(*this);
}

InputSystem::InputSystem() {
	// Load From File else Set Default
	SetDefaultActions();
}

void InputSystem::SetAction(int action, int key, int button) {
	if (auto input = &inputActions[action]) {
		input->key = key;
		input->button = button;
	}
	else { CreateAction(action, key, button); }
}

void InputSystem::CreateAction(int action, int key, int button) {
	if (inputActions[action].id == action) { std::cout << "Existing Action: " << action << "\n"; return; }
	InputAction newAction(action, key, button);
}

void InputSystem::SetDefaultActions() {

	// Move Action
	CreateAction(ACTION_MOVE_FORWARD, KEY_W, GAMEPAD_AXIS_LEFT_Y);
	CreateAction(ACTION_MOVE_BACKWARD, KEY_S, GAMEPAD_AXIS_LEFT_Y);
	CreateAction(ACTION_MOVE_RIGHT, KEY_D, GAMEPAD_AXIS_LEFT_X);
	CreateAction(ACTION_MOVE_LEFT, KEY_A, GAMEPAD_AXIS_LEFT_X);
	CreateAction(ACTION_MOVE_CROUCH, KEY_LEFT_CONTROL , GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
	CreateAction(ACTION_MOVE_SPRINT, KEY_LEFT_SHIFT , GAMEPAD_AXIS_RIGHT_TRIGGER);
	
	// UI Action
	CreateAction(ACTION_UI_PAUSE, KEY_TAB , GAMEPAD_BUTTON_MIDDLE_RIGHT);
	
}