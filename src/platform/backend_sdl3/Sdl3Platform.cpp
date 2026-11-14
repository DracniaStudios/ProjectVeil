// Platform on SDL3 with an OpenGL 3.3 core context (B-15, B-16). Window rules, frame
// limiter, FPS, fullscreen, focus, cursor lock and input follow the raylib backend (B-17).
#include <Platform.h>
#include "Sdl3Native.h"

// The game has its own main(). SDL_SetMainReady is declared only in SDL_main.h, and
// SDL_MAIN_HANDLED stops that header from taking over main.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>

namespace {
	SDL_Window* window = nullptr;
	SDL_GLContext glContext = nullptr;
	Uint64 timeZeroNs = 0;

	// BeginFrame entry, EndFrame entry and the previous BeginFrame entry, in seconds
	double beginTime = 0.0;
	double lastBeginTime = 0.0;
	bool haveBegin = false;
	bool shouldClose = false;

	// Frame timing as raylib's EndDrawing keeps it: previousStamp is the end of the last
	// EndFrame (after its wait); FrameTime is previousStamp to previousStamp, wait included
	double previousStamp = 0.0;
	double targetSeconds = 0.0; // 0: no limit
	float lastFrameSeconds = 0.0f;
	long long frameCounter = 0; // EndFrame calls, raylib's CORE.Time.frameCounter

	// Frame statistics, counted as RaylibPlatform.cpp does: the game prints them at exit
	int statFrames = 0;
	int statFrameIntervals = 0;
	double sumWorkMs = 0.0, maxWorkMs = 0.0;
	double sumFrameMs = 0.0, maxFrameMs = 0.0;

	// Windowed size and position, saved when entering fullscreen
	int savedW = 0, savedH = 0, savedX = 0, savedY = 0;

	// ---- Input (B-17). The state is raylib's: current and previous arrays, copied at the start
	// of each frame, with the events of that frame's poll applied on top. ----
	constexpr int kMouseButtons = 8;            // raylib MAX_MOUSE_BUTTONS
	constexpr int kMaxGamepads = 4;             // raylib MAX_GAMEPADS
	constexpr int kGamepadButtons = 18;         // Platform::GamepadButton enumerators
	constexpr int kKeyQueueCapacity = 16;       // raylib MAX_KEY_PRESSED_QUEUE
	constexpr Sint16 kTriggerThreshold = 18022; // raylib's > 0.1 on GLFW's -1..1 range, as 0..32767

	// raylib starts with Escape as the exit key until SetExitKey says otherwise
	Platform::Key exitKey = Platform::Key::Escape;

	bool keyDown[Platform::kKeyCount] = {};
	bool keyWasDown[Platform::kKeyCount] = {};
	Platform::Key keyQueue[kKeyQueueCapacity] = {};
	int keyQueueCount = 0;

	bool mouseDown[kMouseButtons] = {};
	bool mouseWasDown[kMouseButtons] = {};
	// Unlocked this is the cursor position in the window. Locked it is raylib's virtual position:
	// it starts at the window centre and accumulates the relative motion, unclamped.
	float mouseX = 0.0f, mouseY = 0.0f;
	float mouseDeltaX = 0.0f, mouseDeltaY = 0.0f; // this frame's relative motion, in counts while locked

	SDL_Gamepad* gamepads[kMaxGamepads] = {};
	SDL_JoystickID gamepadIds[kMaxGamepads] = {};
	bool padDown[kMaxGamepads][kGamepadButtons] = {};
	bool padWasDown[kMaxGamepads][kGamepadButtons] = {};

	// Scancode to Platform::Key by physical position. Unlisted scancodes are Key::Null.
	Platform::Key MapScancode(SDL_Scancode sc)
	{
		using K = Platform::Key;
		if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
			return static_cast<K>(static_cast<int>(K::A) + (sc - SDL_SCANCODE_A));
		}
		if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) {
			return static_cast<K>(static_cast<int>(K::One) + (sc - SDL_SCANCODE_1));
		}
		if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) {
			return static_cast<K>(static_cast<int>(K::F1) + (sc - SDL_SCANCODE_F1));
		}
		if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9) {
			return static_cast<K>(static_cast<int>(K::Kp1) + (sc - SDL_SCANCODE_KP_1));
		}
		switch (sc) {
			case SDL_SCANCODE_0: return K::Zero;
			case SDL_SCANCODE_RETURN: return K::Enter;
			case SDL_SCANCODE_ESCAPE: return K::Escape;
			case SDL_SCANCODE_BACKSPACE: return K::Backspace;
			case SDL_SCANCODE_TAB: return K::Tab;
			case SDL_SCANCODE_SPACE: return K::Space;
			case SDL_SCANCODE_MINUS: return K::Minus;
			case SDL_SCANCODE_EQUALS: return K::Equal;
			case SDL_SCANCODE_LEFTBRACKET: return K::LeftBracket;
			case SDL_SCANCODE_RIGHTBRACKET: return K::RightBracket;
			case SDL_SCANCODE_BACKSLASH: return K::Backslash;
			case SDL_SCANCODE_SEMICOLON: return K::Semicolon;
			case SDL_SCANCODE_APOSTROPHE: return K::Apostrophe;
			case SDL_SCANCODE_GRAVE: return K::Grave;
			case SDL_SCANCODE_COMMA: return K::Comma;
			case SDL_SCANCODE_PERIOD: return K::Period;
			case SDL_SCANCODE_SLASH: return K::Slash;
			case SDL_SCANCODE_CAPSLOCK: return K::CapsLock;
			case SDL_SCANCODE_PRINTSCREEN: return K::PrintScreen;
			case SDL_SCANCODE_SCROLLLOCK: return K::ScrollLock;
			case SDL_SCANCODE_PAUSE: return K::Pause;
			case SDL_SCANCODE_INSERT: return K::Insert;
			case SDL_SCANCODE_HOME: return K::Home;
			case SDL_SCANCODE_PAGEUP: return K::PageUp;
			case SDL_SCANCODE_DELETE: return K::Delete;
			case SDL_SCANCODE_END: return K::End;
			case SDL_SCANCODE_PAGEDOWN: return K::PageDown;
			case SDL_SCANCODE_RIGHT: return K::Right;
			case SDL_SCANCODE_LEFT: return K::Left;
			case SDL_SCANCODE_DOWN: return K::Down;
			case SDL_SCANCODE_UP: return K::Up;
			case SDL_SCANCODE_NUMLOCKCLEAR: return K::NumLock;
			case SDL_SCANCODE_KP_DIVIDE: return K::KpDivide;
			case SDL_SCANCODE_KP_MULTIPLY: return K::KpMultiply;
			case SDL_SCANCODE_KP_MINUS: return K::KpSubtract;
			case SDL_SCANCODE_KP_PLUS: return K::KpAdd;
			case SDL_SCANCODE_KP_ENTER: return K::KpEnter;
			case SDL_SCANCODE_KP_0: return K::Kp0;
			case SDL_SCANCODE_KP_PERIOD: return K::KpDecimal;
			case SDL_SCANCODE_KP_EQUALS: return K::KpEqual;
			case SDL_SCANCODE_APPLICATION: return K::KbMenu;
			case SDL_SCANCODE_LCTRL: return K::LeftControl;
			case SDL_SCANCODE_LSHIFT: return K::LeftShift;
			case SDL_SCANCODE_LALT: return K::LeftAlt;
			case SDL_SCANCODE_LGUI: return K::LeftSuper;
			case SDL_SCANCODE_RCTRL: return K::RightControl;
			case SDL_SCANCODE_RSHIFT: return K::RightShift;
			case SDL_SCANCODE_RALT: return K::RightAlt;
			case SDL_SCANCODE_RGUI: return K::RightSuper;
			default: return K::Null;
		}
	}

	// SDL buttons 1..5 are left, middle, right, X1, X2. GLFW (and so raylib) numbers them
	// left 0, right 1, middle 2, then 3 and 4 for the two side buttons. -1: no equivalent.
	int MapMouseButton(Uint8 sdlButton)
	{
		switch (sdlButton) {
			case SDL_BUTTON_LEFT: return static_cast<int>(Platform::MouseButton::Left);
			case SDL_BUTTON_RIGHT: return static_cast<int>(Platform::MouseButton::Right);
			case SDL_BUTTON_MIDDLE: return static_cast<int>(Platform::MouseButton::Middle);
			case SDL_BUTTON_X1: return static_cast<int>(Platform::MouseButton::Side);
			case SDL_BUTTON_X2: return static_cast<int>(Platform::MouseButton::Extra);
			default: return -1;
		}
	}

	// -1: no equivalent (the extra SDL buttons: paddles, touchpad, misc)
	int MapGamepadButton(int b)
	{
		using G = Platform::GamepadButton;
		switch (b) {
			case SDL_GAMEPAD_BUTTON_DPAD_UP: return static_cast<int>(G::LeftFaceUp);
			case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return static_cast<int>(G::LeftFaceRight);
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return static_cast<int>(G::LeftFaceDown);
			case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return static_cast<int>(G::LeftFaceLeft);
			case SDL_GAMEPAD_BUTTON_NORTH: return static_cast<int>(G::RightFaceUp);
			case SDL_GAMEPAD_BUTTON_EAST: return static_cast<int>(G::RightFaceRight);
			case SDL_GAMEPAD_BUTTON_SOUTH: return static_cast<int>(G::RightFaceDown);
			case SDL_GAMEPAD_BUTTON_WEST: return static_cast<int>(G::RightFaceLeft);
			case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return static_cast<int>(G::LeftTrigger1);
			case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return static_cast<int>(G::RightTrigger1);
			case SDL_GAMEPAD_BUTTON_BACK: return static_cast<int>(G::MiddleLeft);
			case SDL_GAMEPAD_BUTTON_GUIDE: return static_cast<int>(G::Middle);
			case SDL_GAMEPAD_BUTTON_START: return static_cast<int>(G::MiddleRight);
			case SDL_GAMEPAD_BUTTON_LEFT_STICK: return static_cast<int>(G::LeftThumb);
			case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return static_cast<int>(G::RightThumb);
			default: return -1;
		}
	}

	int GamepadSlot(SDL_JoystickID id)
	{
		for (int i = 0; i < kMaxGamepads; ++i) {
			if (gamepads[i] != nullptr && gamepadIds[i] == id) { return i; }
		}
		return -1;
	}

	void ClearGamepadSlot(int slot)
	{
		for (int b = 0; b < kGamepadButtons; ++b) {
			padDown[slot][b] = false;
			padWasDown[slot][b] = false;
		}
	}

	// raylib reads a pad's whole state every poll, so a pad that appears with a button held
	// reports it. SDL sends no button event for that, so read the state when the pad opens.
	void OpenGamepadInFirstFreeSlot(SDL_JoystickID id)
	{
		if (GamepadSlot(id) >= 0) { return; }
		for (int slot = 0; slot < kMaxGamepads; ++slot) {
			if (gamepads[slot] != nullptr) { continue; }
			SDL_Gamepad* pad = SDL_OpenGamepad(id);
			if (pad == nullptr) {
				std::cerr << "[Platform] SDL_OpenGamepad failed: " << SDL_GetError() << std::endl;
				return;
			}
			gamepads[slot] = pad;
			gamepadIds[slot] = id;
			ClearGamepadSlot(slot);
			for (int sdlButton = 0; sdlButton < SDL_GAMEPAD_BUTTON_COUNT; ++sdlButton) {
				const int mapped = MapGamepadButton(sdlButton);
				if (mapped >= 0 && SDL_GetGamepadButton(pad, static_cast<SDL_GamepadButton>(sdlButton))) {
					padDown[slot][mapped] = true;
				}
			}
			padDown[slot][static_cast<int>(Platform::GamepadButton::LeftTrigger2)] =
				SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > kTriggerThreshold;
			padDown[slot][static_cast<int>(Platform::GamepadButton::RightTrigger2)] =
				SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > kTriggerThreshold;
			return;
		}
		// All four slots are taken: the pad stays closed, as raylib ignores pads past 4
	}

	void CloseGamepad(SDL_JoystickID id)
	{
		const int slot = GamepadSlot(id);
		if (slot < 0) { return; }
		SDL_CloseGamepad(gamepads[slot]);
		gamepads[slot] = nullptr;
		gamepadIds[slot] = 0;
		ClearGamepadSlot(slot); // raylib: a pad that is not ready answers false to everything
	}

	void HandleInputEvent(const SDL_Event& event)
	{
		switch (event.type) {
			case SDL_EVENT_KEY_DOWN: {
				if (event.key.repeat) { break; } // raylib records a repeat apart from the press
				const Platform::Key key = MapScancode(event.key.scancode);
				if (key == Platform::Key::Null) { break; }
				keyDown[static_cast<int>(key)] = true;
				if (keyQueueCount < kKeyQueueCapacity) { keyQueue[keyQueueCount++] = key; } // full: dropped
				if (key == exitKey) { shouldClose = true; }
				break;
			}
			case SDL_EVENT_KEY_UP: {
				const Platform::Key key = MapScancode(event.key.scancode);
				if (key != Platform::Key::Null) { keyDown[static_cast<int>(key)] = false; }
				break;
			}
			case SDL_EVENT_MOUSE_MOTION:
				if (SDL_GetWindowRelativeMouseMode(window)) {
					// event.motion.x/y are clamped to the window; raylib's locked position is not
					mouseX += event.motion.xrel;
					mouseY += event.motion.yrel;
				} else {
					mouseX = event.motion.x;
					mouseY = event.motion.y;
				}
				mouseDeltaX += event.motion.xrel;
				mouseDeltaY += event.motion.yrel;
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP: {
				const int button = MapMouseButton(event.button.button);
				if (button >= 0) { mouseDown[button] = event.button.down; }
				break;
			}
			case SDL_EVENT_GAMEPAD_ADDED:
				OpenGamepadInFirstFreeSlot(event.gdevice.which);
				break;
			case SDL_EVENT_GAMEPAD_REMOVED:
				CloseGamepad(event.gdevice.which);
				break;
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			case SDL_EVENT_GAMEPAD_BUTTON_UP: {
				const int slot = GamepadSlot(event.gbutton.which);
				const int button = MapGamepadButton(event.gbutton.button);
				if (slot >= 0 && button >= 0) { padDown[slot][button] = event.gbutton.down; }
				break;
			}
			case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
				const int slot = GamepadSlot(event.gaxis.which);
				if (slot < 0) { break; }
				const bool over = event.gaxis.value > kTriggerThreshold;
				if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
					padDown[slot][static_cast<int>(Platform::GamepadButton::LeftTrigger2)] = over;
				} else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
					padDown[slot][static_cast<int>(Platform::GamepadButton::RightTrigger2)] = over;
				}
				break;
			}
			default:
				break;
		}
	}

	bool KeyInRange(Platform::Key k) { return static_cast<int>(k) > 0 && static_cast<int>(k) < Platform::kKeyCount; }
	bool MouseInRange(Platform::MouseButton b) { return static_cast<int>(b) >= 0 && static_cast<int>(b) < kMouseButtons; }
	bool PadInRange(int pad, Platform::GamepadButton b)
	{
		return pad >= 0 && pad < kMaxGamepads && gamepads[pad] != nullptr &&
			static_cast<int>(b) >= 0 && static_cast<int>(b) < kGamepadButtons;
	}

	void CentreCursor()
	{
		int w = 0, h = 0;
		SDL_GetWindowSize(window, &w, &h);
		SDL_WarpMouseInWindow(window, static_cast<float>(w / 2), static_cast<float>(h / 2));
		// raylib's SetMousePosition: the position becomes the centre and no motion is counted
		mouseX = static_cast<float>(w / 2);
		mouseY = static_cast<float>(h / 2);
		mouseDeltaX = 0.0f;
		mouseDeltaY = 0.0f;
	}
}

SDL_Window* Sdl3Window() { return window; }
SDL_GLContext Sdl3GlContext() { return glContext; }

namespace Platform {

bool Init(const WindowDesc& desc)
{
	// desc.backendLog is raylib's trace log; SDL ignores it
	SDL_SetMainReady();
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		std::cerr << "[Platform] SDL_Init failed: " << SDL_GetError() << std::endl;
		return false;
	}

	const bool attributesOk =
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) &&
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) &&
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) &&
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) &&
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24) &&
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	if (!attributesOk) {
		std::cerr << "[Platform] SDL_GL_SetAttribute failed: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return false;
	}

	// No SDL_WINDOW_HIGH_PIXEL_DENSITY: the window size is the framebuffer size, as in raylib
	SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
	if (desc.resizable) { flags |= SDL_WINDOW_RESIZABLE; }
	if (desc.hidden) { flags |= SDL_WINDOW_HIDDEN; }
	window = SDL_CreateWindow(desc.title, desc.width, desc.height, flags);
	if (window == nullptr) {
		std::cerr << "[Platform] SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return false;
	}

	// The same rules as the raylib backend; 0 is "no limit" for SDL as it is for the descriptor
	if (desc.x >= 0) { SDL_SetWindowPosition(window, desc.x, desc.y); }
	if (desc.minWidth != 0 || desc.minHeight != 0) { SDL_SetWindowMinimumSize(window, desc.minWidth, desc.minHeight); }
	if (desc.maxWidth != 0 || desc.maxHeight != 0) { SDL_SetWindowMaximumSize(window, desc.maxWidth, desc.maxHeight); }

	// Relative motion unscaled by the system pointer speed, as raylib's raw motion is
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "0");

	glContext = SDL_GL_CreateContext(window);
	if (glContext == nullptr || !SDL_GL_MakeCurrent(window, glContext)) {
		std::cerr << "[Platform] OpenGL 3.3 core context failed: " << SDL_GetError() << std::endl;
		Shutdown();
		return false;
	}
	SDL_GL_SetSwapInterval(0);

	timeZeroNs = SDL_GetTicksNS();
	previousStamp = 0.0; // raylib starts its frame clock at InitTimer, so frame 1 includes loading
	frameCounter = 0;
	lastFrameSeconds = 0.0f;

	const int v = SDL_GetVersion();
	const char* driver = SDL_GetCurrentVideoDriver();
	std::cout << "[Platform] SDL " << SDL_VERSIONNUM_MAJOR(v) << "." << SDL_VERSIONNUM_MINOR(v) << "."
		<< SDL_VERSIONNUM_MICRO(v) << ", video " << (driver != nullptr ? driver : "(none)") << std::endl;
	return true;
}

void Shutdown()
{
	for (int i = 0; i < kMaxGamepads; ++i) {
		if (gamepads[i] != nullptr) { SDL_CloseGamepad(gamepads[i]); gamepads[i] = nullptr; }
	}
	if (glContext != nullptr) { SDL_GL_DestroyContext(glContext); glContext = nullptr; }
	if (window != nullptr) { SDL_DestroyWindow(window); window = nullptr; }
	SDL_Quit();
}

bool IsReady() { return window != nullptr && glContext != nullptr; }

void BeginFrame()
{
	beginTime = Time();
	if (haveBegin) {
		const double frameMs = (beginTime - lastBeginTime) * 1000.0;
		sumFrameMs += frameMs;
		maxFrameMs = std::max(maxFrameMs, frameMs);
		++statFrameIntervals;
	}
	lastBeginTime = beginTime;
	haveBegin = true;

	// As raylib's PollInputEvents: previous state, empty queue, no motion, then this frame's events
	std::copy(std::begin(keyDown), std::end(keyDown), std::begin(keyWasDown));
	std::copy(std::begin(mouseDown), std::end(mouseDown), std::begin(mouseWasDown));
	for (int p = 0; p < kMaxGamepads; ++p) {
		std::copy(std::begin(padDown[p]), std::end(padDown[p]), std::begin(padWasDown[p]));
	}
	keyQueueCount = 0;
	mouseDeltaX = 0.0f;
	mouseDeltaY = 0.0f;

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
			shouldClose = true;
		}
		HandleInputEvent(event);
		Sdl3EditorGuiProcessEvent(event);
	}
}

void EndFrame()
{
	const double workMs = (Time() - beginTime) * 1000.0;
	sumWorkMs += workMs;
	maxWorkMs = std::max(maxWorkMs, workMs);
	++statFrames;
	SDL_GL_SwapWindow(window);

	// As raylib's EndDrawing: the frame is previousStamp to now (after the swap); if that is
	// under the target, wait the rest and count the wait in the frame. FrameTime is that total.
	double now = Time();
	double frame = now - previousStamp;
	previousStamp = now;
	if (frame < targetSeconds) {
		SDL_DelayPrecise(static_cast<Uint64>((targetSeconds - frame) * 1.0e9));
		now = Time();
		frame += now - previousStamp;
		previousStamp = now;
	}
	lastFrameSeconds = static_cast<float>(frame);
	++frameCounter;
}

bool ShouldClose() { return shouldClose; }
void SetTargetFps(int fps) { targetSeconds = fps < 1 ? 0.0 : 1.0 / static_cast<double>(fps); }

int ScreenWidth() { int w = 0, h = 0; SDL_GetWindowSize(window, &w, &h); return w; }
int ScreenHeight() { int w = 0, h = 0; SDL_GetWindowSize(window, &w, &h); return h; }
int RenderWidth() { int w = 0, h = 0; SDL_GetWindowSizeInPixels(window, &w, &h); return w; }
int RenderHeight() { int w = 0, h = 0; SDL_GetWindowSizeInPixels(window, &w, &h); return h; }

void ToggleFullscreen()
{
	if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0) {
		SDL_GetWindowSize(window, &savedW, &savedH);
		SDL_GetWindowPosition(window, &savedX, &savedY);
		// Exclusive fullscreen at the desktop mode of the display the window is on
		const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window));
		if (mode == nullptr) {
			std::cerr << "[Platform] SDL_GetDesktopDisplayMode failed: " << SDL_GetError() << std::endl;
			return;
		}
		if (!SDL_SetWindowFullscreenMode(window, mode) || !SDL_SetWindowFullscreen(window, true)) {
			std::cerr << "[Platform] Enter fullscreen failed: " << SDL_GetError() << std::endl;
		}
		SDL_SyncWindow(window);
	} else {
		SDL_SetWindowFullscreen(window, false);
		SDL_SyncWindow(window);
		SDL_SetWindowSize(window, savedW, savedH);
		SDL_SetWindowPosition(window, savedX, savedY);
		SDL_SyncWindow(window);
	}
}

void FocusWindow() { SDL_RaiseWindow(window); }

float FrameTime() { return lastFrameSeconds; }
double Time() { return static_cast<double>(SDL_GetTicksNS() - timeZeroNs) / 1.0e9; }

// raylib's GetFPS: every 0.5 s / 30 = 1/60 s a frame time is written into a ring of 30 slots
// (each as frameTime / 30) and the answer is the rounded inverse of the ring's sum, so it
// is the mean of the last 30 samples spread over at least half a second, not of 30 frames.
int Fps()
{
	constexpr int kCaptures = 30;
	constexpr float kStep = 0.5f / kCaptures;
	static int index = 0;
	static float history[kCaptures] = {};
	static float average = 0.0f, last = 0.0f;
	const float fpsFrame = FrameTime();

	if (frameCounter == 0) { // as raylib: a fresh window starts from zero
		average = 0.0f;
		last = 0.0f;
		index = 0;
		for (float& h : history) { h = 0.0f; }
	}
	if (fpsFrame == 0.0f) { return 0; }

	if ((Time() - last) > kStep) {
		last = static_cast<float>(Time());
		index = (index + 1) % kCaptures;
		average -= history[index];
		history[index] = fpsFrame / kCaptures;
		average += history[index];
	}
	// raylib casts 1/average unguarded; an empty ring would be infinity, so answer 0
	return average > 0.0f ? static_cast<int>(std::roundf(1.0f / average)) : 0;
}

// raylib re-centres the cursor on every call, locked or not, so these do too
void LockCursor()
{
	CentreCursor();
	if (!SDL_SetWindowRelativeMouseMode(window, true)) {
		std::cerr << "[Platform] Lock cursor failed: " << SDL_GetError() << std::endl;
	}
}

void UnlockCursor()
{
	SDL_SetWindowRelativeMouseMode(window, false);
	CentreCursor();
}

bool IsCursorLocked() { return SDL_GetWindowRelativeMouseMode(window); }

void SetExitKey(Key key) { exitKey = key; }
bool IsKeyDown(Key key) { return KeyInRange(key) && keyDown[static_cast<int>(key)]; }
bool IsKeyPressed(Key key) { return KeyInRange(key) && keyDown[static_cast<int>(key)] && !keyWasDown[static_cast<int>(key)]; }
bool IsKeyReleased(Key key) { return KeyInRange(key) && !keyDown[static_cast<int>(key)] && keyWasDown[static_cast<int>(key)]; }

Key PopKeyPressed()
{
	if (keyQueueCount == 0) { return Key::Null; }
	const Key front = keyQueue[0];
	for (int i = 0; i < keyQueueCount - 1; ++i) { keyQueue[i] = keyQueue[i + 1]; }
	--keyQueueCount;
	return front;
}

Vector2 MousePosition() { return { mouseX, mouseY }; }
Vector2 MouseDelta() { return { mouseDeltaX, mouseDeltaY }; }

bool IsMouseButtonDown(MouseButton button) { return MouseInRange(button) && mouseDown[static_cast<int>(button)]; }
bool IsMouseButtonPressed(MouseButton button)
{
	return MouseInRange(button) && mouseDown[static_cast<int>(button)] && !mouseWasDown[static_cast<int>(button)];
}
bool IsMouseButtonReleased(MouseButton button)
{
	return MouseInRange(button) && !mouseDown[static_cast<int>(button)] && mouseWasDown[static_cast<int>(button)];
}

bool IsGamepadButtonDown(int gamepad, GamepadButton button)
{
	return PadInRange(gamepad, button) && padDown[gamepad][static_cast<int>(button)];
}
bool IsGamepadButtonPressed(int gamepad, GamepadButton button)
{
	return PadInRange(gamepad, button) && padDown[gamepad][static_cast<int>(button)] && !padWasDown[gamepad][static_cast<int>(button)];
}
bool IsGamepadButtonReleased(int gamepad, GamepadButton button)
{
	return PadInRange(gamepad, button) && !padDown[gamepad][static_cast<int>(button)] && padWasDown[gamepad][static_cast<int>(button)];
}

FrameStats GetFrameStats()
{
	FrameStats s;
	s.frames = statFrames;
	if (statFrames > 0) { s.meanWorkMs = sumWorkMs / statFrames; }
	s.maxWorkMs = maxWorkMs;
	if (statFrameIntervals > 0) { s.meanFrameMs = sumFrameMs / statFrameIntervals; }
	s.maxFrameMs = maxFrameMs;
	return s;
}

}
