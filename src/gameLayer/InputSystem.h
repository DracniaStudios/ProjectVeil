#pragma once
#ifndef INPUT_SYSTEM_H
#define INPUT_SYSTEM_H


struct InputAction {
	// Key
	bool isEnabled;
	bool isPressed;
	bool isHeld;
};

class InputSystem
{
private:
	InputSystem() = default;
public:
	InputSystem(const InputSystem&) = delete;
	InputSystem& operator=(const InputSystem&) = delete;
	InputSystem(InputSystem&&) = delete;
	InputSystem& operator=(InputSystem&&) = delete;

	static InputSystem& getInstance() {
		static InputSystem instance;
		return instance;
	}

};

#endif
