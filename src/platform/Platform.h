#pragma once
#ifndef VEIL_PLATFORM_H
#define VEIL_PLATFORM_H
#include <raylib.h> // Vector2 only
#include <cstdint>

namespace Platform {
	enum class LogLevel { All, Warning, Error, Fatal };

	struct WindowDesc {
		int width = 1600;
		int height = 900;
		const char* title = "Project: Veil";
		bool resizable = true;
		bool hidden = false;
		int minWidth = 0, minHeight = 0;  // 0: no limit
		int maxWidth = 0, maxHeight = 0;  // 0: no limit
		int x = -1, y = -1;               // -1: the OS places the window
		LogLevel backendLog = LogLevel::All; // raylib's trace log; SDL ignores it
	};

	bool Init(const WindowDesc& desc); // window plus an OpenGL 3.3 context
	void Shutdown();
	bool IsReady();

	void BeginFrame();          // input read after this is this frame's
	void EndFrame();            // presents, then waits out the target frame time
	bool ShouldClose();         // close requested or exit key pressed
	void SetTargetFps(int fps);

	int ScreenWidth();          // window size in screen coordinates (raylib GetScreenWidth)
	int ScreenHeight();
	int RenderWidth();          // framebuffer size in pixels
	int RenderHeight();
	void ToggleFullscreen();
	void FocusWindow();

	float FrameTime();          // seconds the last whole frame took, wait included
	double Time();              // seconds since Init
	int Fps();                  // as raylib's GetFPS

	void LockCursor();          // hidden, held, relative motion (raylib DisableCursor)
	void UnlockCursor();        // raylib EnableCursor
	bool IsCursorLocked();

	// raylib KeyboardKey values (GLFW key codes): physical keys named after the US layout
	enum class Key : std::int32_t {
		Null = 0, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
		Zero = 48, One, Two, Three, Four, Five, Six, Seven, Eight, Nine,
		Semicolon = 59, Equal = 61,
		A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		LeftBracket = 91, Backslash = 92, RightBracket = 93, Grave = 96, Space = 32,
		Escape = 256, Enter = 257, Tab = 258, Backspace = 259, Insert = 260, Delete = 261,
		Right = 262, Left = 263, Down = 264, Up = 265, PageUp = 266, PageDown = 267, Home = 268, End = 269,
		CapsLock = 280, ScrollLock = 281, NumLock = 282, PrintScreen = 283, Pause = 284,
		F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		Kp0 = 320, Kp1, Kp2, Kp3, Kp4, Kp5, Kp6, Kp7, Kp8, Kp9,
		KpDecimal = 330, KpDivide = 331, KpMultiply = 332, KpSubtract = 333, KpAdd = 334, KpEnter = 335, KpEqual = 336,
		LeftShift = 340, LeftControl = 341, LeftAlt = 342, LeftSuper = 343,
		RightShift = 344, RightControl = 345, RightAlt = 346, RightSuper = 347, KbMenu = 348,
	};
	inline constexpr int kKeyCount = 349;
	enum class MouseButton : std::int32_t { Left = 0, Right = 1, Middle = 2, Side = 3, Extra = 4, Forward = 5, Back = 6 };
	// raylib GamepadButton values
	enum class GamepadButton : std::int32_t {
		Unknown = 0, LeftFaceUp, LeftFaceRight, LeftFaceDown, LeftFaceLeft,
		RightFaceUp, RightFaceRight, RightFaceDown, RightFaceLeft,
		LeftTrigger1, LeftTrigger2, RightTrigger1, RightTrigger2,
		MiddleLeft, Middle, MiddleRight, LeftThumb, RightThumb,
	};

	void SetExitKey(Key key);               // Key::Null: none
	bool IsKeyDown(Key key);
	bool IsKeyPressed(Key key);             // down now, up last frame; repeats ignored
	bool IsKeyReleased(Key key);
	Key  PopKeyPressed();                   // this frame's presses in order, then Key::Null
	Vector2 MousePosition();                // screen coordinates
	Vector2 MouseDelta();                   // since last frame; raw counts while locked
	bool IsMouseButtonDown(MouseButton button);
	bool IsMouseButtonPressed(MouseButton button);
	bool IsMouseButtonReleased(MouseButton button);
	bool IsGamepadButtonDown(int gamepad, GamepadButton button);
	bool IsGamepadButtonPressed(int gamepad, GamepadButton button);
	bool IsGamepadButtonReleased(int gamepad, GamepadButton button);

	// work = BeginFrame to EndFrame entry, so swap and wait are excluded
	struct FrameStats { int frames = 0; double meanWorkMs = 0, maxWorkMs = 0, meanFrameMs = 0, maxFrameMs = 0; };
	FrameStats GetFrameStats();
}
#endif
