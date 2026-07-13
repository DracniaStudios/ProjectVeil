#pragma once
#ifndef INPUT_SYSTEM_H
#define INPUT_SYSTEM_H

#include <raylib.h>
#include <vector>
#include <string>

typedef enum ActionType : uint8_t {
	NO_ACTION = 0,

	// Move Actions
	ACTION_MOVE_FORWARD,
	ACTION_MOVE_BACKWARD,
	ACTION_MOVE_RIGHT,
	ACTION_MOVE_LEFT,

	ACTION_MOVE_CROUCH,
	ACTION_MOVE_SPRINT,
	
	// Menu Actions
	ACTION_UI_PAUSE,
	
	MAX_ACTION
};

struct InputAction {

	InputAction();
	InputAction(int id, int k, int b);
	std::string name = "";
	bool isEnabled = true;
	int id = 0;
	int key = 0;
	int button = 0;
};

class InputSystem
{
private:
	InputSystem();
public:
	InputSystem(const InputSystem&) = delete;
	InputSystem& operator=(const InputSystem&) = delete;
	InputSystem(InputSystem&&) = delete;
	InputSystem& operator=(InputSystem&&) = delete;

	static InputSystem& getInstance() {
		static InputSystem instance;
		return instance;
	}
	std::vector<InputAction> inputActions;
	int gamepadIndex = 0;

	// Functions
	bool IsActionDown(int action) { return IsKeyDown(inputActions[action].key) || IsGamepadButtonDown(gamepadIndex, inputActions[action].button); }
	bool IsActionPressed(int action) { return IsKeyPressed(inputActions[action].key) || IsGamepadButtonPressed(gamepadIndex, inputActions[action].button); }
	bool IsActionReleased(int action) { return IsKeyReleased(inputActions[action].key) || IsGamepadButtonReleased(gamepadIndex, inputActions[action].button); }

	void SetDefaultActions(void);
	void SetAction(int action, int key, int button);
	void CreateAction(int action, int key, int button);
};

#endif
