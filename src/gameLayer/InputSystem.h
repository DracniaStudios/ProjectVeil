#pragma once
#ifndef INPUT_SYSTEM_H
#define INPUT_SYSTEM_H

#include <raylib.h>
#include <cstdint>
#include <vector>
#include <string>

enum ActionType : uint8_t {
	NO_ACTION = 0,

	// Move Actions
	ACTION_MOVE_FORWARD,
	ACTION_MOVE_BACKWARD,
	ACTION_MOVE_RIGHT,
	ACTION_MOVE_LEFT,

	ACTION_MOVE_CROUCH,
	ACTION_MOVE_SPRINT,
	ACTION_MOVE_INTERACT,
	ACTION_MOVE_JUMP,

	ACTION_USE_ARTIFACT,
	ACTION_USE_ARTIFACT_RIGHT,
	ACTION_USE_ARTIFACT_LEFT,

	// Menu Actions
	ACTION_UI_PAUSE,

	MAX_ACTION
};

struct InputAction {
	std::string name = "";
	bool isEnabled = true;
	int id = NO_ACTION;
	int key = KEY_NULL;
	int button = -1; // -1 = no gamepad binding
};

class InputSystem
{
private:
	InputSystem();

	std::vector<int> pressedKeys; // Keys pressed this frame, in press order
	std::vector<int> heldKeys;    // Keys currently held down
public:
	InputSystem(const InputSystem&) = delete;
	InputSystem& operator=(const InputSystem&) = delete;
	InputSystem(InputSystem&&) = delete;
	InputSystem& operator=(InputSystem&&) = delete;

	static InputSystem& getInstance() {
		static InputSystem instance;
		return instance;
	}
	std::vector<InputAction> inputActions; // Indexed by ActionType
	int gamepadIndex = 0;

	// Call once per frame, before the scene update
	void Update();

	// Action Functions
	bool IsActionDown(int action);
	bool IsActionPressed(int action);
	bool IsActionReleased(int action);

	InputAction* GetAction(int action);

	void SetDefaultActions(void);
	void SetAction(int action, int key, int button);
	void CreateAction(int action, const std::string& name, int key, int button);

	// Key Functions
	const std::vector<int>& GetPressedKeys() const { return pressedKeys; } // Keys pressed this frame
	const std::vector<int>& GetHeldKeys() const { return heldKeys; }       // Keys currently held down
	int GetLastPressedKey() const { return pressedKeys.empty() ? KEY_NULL : pressedKeys.back(); } // KEY_NULL if none this frame
};

#endif
