// Platform on raylib (B-04). One raylib call per Platform call, as the work order lists.
#include <Platform.h>

#include <algorithm>

namespace {
	// BeginFrame entry, EndFrame entry, and the previous BeginFrame entry, in seconds
	double beginTime = 0.0;
	double lastBeginTime = 0.0;
	bool haveBegin = false;

	int statFrames = 0;
	int statFrameIntervals = 0;
	double sumWorkMs = 0.0, maxWorkMs = 0.0;
	double sumFrameMs = 0.0, maxFrameMs = 0.0;
}

// The Platform enums carry raylib's own values, so every conversion below is a cast
static_assert(static_cast<int>(Platform::Key::Null) == KEY_NULL);
static_assert(static_cast<int>(Platform::Key::Apostrophe) == KEY_APOSTROPHE);
static_assert(static_cast<int>(Platform::Key::Comma) == KEY_COMMA);
static_assert(static_cast<int>(Platform::Key::Minus) == KEY_MINUS);
static_assert(static_cast<int>(Platform::Key::Period) == KEY_PERIOD);
static_assert(static_cast<int>(Platform::Key::Slash) == KEY_SLASH);
static_assert(static_cast<int>(Platform::Key::Zero) == KEY_ZERO);
static_assert(static_cast<int>(Platform::Key::One) == KEY_ONE);
static_assert(static_cast<int>(Platform::Key::Two) == KEY_TWO);
static_assert(static_cast<int>(Platform::Key::Three) == KEY_THREE);
static_assert(static_cast<int>(Platform::Key::Four) == KEY_FOUR);
static_assert(static_cast<int>(Platform::Key::Five) == KEY_FIVE);
static_assert(static_cast<int>(Platform::Key::Six) == KEY_SIX);
static_assert(static_cast<int>(Platform::Key::Seven) == KEY_SEVEN);
static_assert(static_cast<int>(Platform::Key::Eight) == KEY_EIGHT);
static_assert(static_cast<int>(Platform::Key::Nine) == KEY_NINE);
static_assert(static_cast<int>(Platform::Key::Semicolon) == KEY_SEMICOLON);
static_assert(static_cast<int>(Platform::Key::Equal) == KEY_EQUAL);
static_assert(static_cast<int>(Platform::Key::A) == KEY_A);
static_assert(static_cast<int>(Platform::Key::B) == KEY_B);
static_assert(static_cast<int>(Platform::Key::C) == KEY_C);
static_assert(static_cast<int>(Platform::Key::D) == KEY_D);
static_assert(static_cast<int>(Platform::Key::E) == KEY_E);
static_assert(static_cast<int>(Platform::Key::F) == KEY_F);
static_assert(static_cast<int>(Platform::Key::G) == KEY_G);
static_assert(static_cast<int>(Platform::Key::H) == KEY_H);
static_assert(static_cast<int>(Platform::Key::I) == KEY_I);
static_assert(static_cast<int>(Platform::Key::J) == KEY_J);
static_assert(static_cast<int>(Platform::Key::K) == KEY_K);
static_assert(static_cast<int>(Platform::Key::L) == KEY_L);
static_assert(static_cast<int>(Platform::Key::M) == KEY_M);
static_assert(static_cast<int>(Platform::Key::N) == KEY_N);
static_assert(static_cast<int>(Platform::Key::O) == KEY_O);
static_assert(static_cast<int>(Platform::Key::P) == KEY_P);
static_assert(static_cast<int>(Platform::Key::Q) == KEY_Q);
static_assert(static_cast<int>(Platform::Key::R) == KEY_R);
static_assert(static_cast<int>(Platform::Key::S) == KEY_S);
static_assert(static_cast<int>(Platform::Key::T) == KEY_T);
static_assert(static_cast<int>(Platform::Key::U) == KEY_U);
static_assert(static_cast<int>(Platform::Key::V) == KEY_V);
static_assert(static_cast<int>(Platform::Key::W) == KEY_W);
static_assert(static_cast<int>(Platform::Key::X) == KEY_X);
static_assert(static_cast<int>(Platform::Key::Y) == KEY_Y);
static_assert(static_cast<int>(Platform::Key::Z) == KEY_Z);
static_assert(static_cast<int>(Platform::Key::LeftBracket) == KEY_LEFT_BRACKET);
static_assert(static_cast<int>(Platform::Key::Backslash) == KEY_BACKSLASH);
static_assert(static_cast<int>(Platform::Key::RightBracket) == KEY_RIGHT_BRACKET);
static_assert(static_cast<int>(Platform::Key::Grave) == KEY_GRAVE);
static_assert(static_cast<int>(Platform::Key::Space) == KEY_SPACE);
static_assert(static_cast<int>(Platform::Key::Escape) == KEY_ESCAPE);
static_assert(static_cast<int>(Platform::Key::Enter) == KEY_ENTER);
static_assert(static_cast<int>(Platform::Key::Tab) == KEY_TAB);
static_assert(static_cast<int>(Platform::Key::Backspace) == KEY_BACKSPACE);
static_assert(static_cast<int>(Platform::Key::Insert) == KEY_INSERT);
static_assert(static_cast<int>(Platform::Key::Delete) == KEY_DELETE);
static_assert(static_cast<int>(Platform::Key::Right) == KEY_RIGHT);
static_assert(static_cast<int>(Platform::Key::Left) == KEY_LEFT);
static_assert(static_cast<int>(Platform::Key::Down) == KEY_DOWN);
static_assert(static_cast<int>(Platform::Key::Up) == KEY_UP);
static_assert(static_cast<int>(Platform::Key::PageUp) == KEY_PAGE_UP);
static_assert(static_cast<int>(Platform::Key::PageDown) == KEY_PAGE_DOWN);
static_assert(static_cast<int>(Platform::Key::Home) == KEY_HOME);
static_assert(static_cast<int>(Platform::Key::End) == KEY_END);
static_assert(static_cast<int>(Platform::Key::CapsLock) == KEY_CAPS_LOCK);
static_assert(static_cast<int>(Platform::Key::ScrollLock) == KEY_SCROLL_LOCK);
static_assert(static_cast<int>(Platform::Key::NumLock) == KEY_NUM_LOCK);
static_assert(static_cast<int>(Platform::Key::PrintScreen) == KEY_PRINT_SCREEN);
static_assert(static_cast<int>(Platform::Key::Pause) == KEY_PAUSE);
static_assert(static_cast<int>(Platform::Key::F1) == KEY_F1);
static_assert(static_cast<int>(Platform::Key::F2) == KEY_F2);
static_assert(static_cast<int>(Platform::Key::F3) == KEY_F3);
static_assert(static_cast<int>(Platform::Key::F4) == KEY_F4);
static_assert(static_cast<int>(Platform::Key::F5) == KEY_F5);
static_assert(static_cast<int>(Platform::Key::F6) == KEY_F6);
static_assert(static_cast<int>(Platform::Key::F7) == KEY_F7);
static_assert(static_cast<int>(Platform::Key::F8) == KEY_F8);
static_assert(static_cast<int>(Platform::Key::F9) == KEY_F9);
static_assert(static_cast<int>(Platform::Key::F10) == KEY_F10);
static_assert(static_cast<int>(Platform::Key::F11) == KEY_F11);
static_assert(static_cast<int>(Platform::Key::F12) == KEY_F12);
static_assert(static_cast<int>(Platform::Key::Kp0) == KEY_KP_0);
static_assert(static_cast<int>(Platform::Key::Kp1) == KEY_KP_1);
static_assert(static_cast<int>(Platform::Key::Kp2) == KEY_KP_2);
static_assert(static_cast<int>(Platform::Key::Kp3) == KEY_KP_3);
static_assert(static_cast<int>(Platform::Key::Kp4) == KEY_KP_4);
static_assert(static_cast<int>(Platform::Key::Kp5) == KEY_KP_5);
static_assert(static_cast<int>(Platform::Key::Kp6) == KEY_KP_6);
static_assert(static_cast<int>(Platform::Key::Kp7) == KEY_KP_7);
static_assert(static_cast<int>(Platform::Key::Kp8) == KEY_KP_8);
static_assert(static_cast<int>(Platform::Key::Kp9) == KEY_KP_9);
static_assert(static_cast<int>(Platform::Key::KpDecimal) == KEY_KP_DECIMAL);
static_assert(static_cast<int>(Platform::Key::KpDivide) == KEY_KP_DIVIDE);
static_assert(static_cast<int>(Platform::Key::KpMultiply) == KEY_KP_MULTIPLY);
static_assert(static_cast<int>(Platform::Key::KpSubtract) == KEY_KP_SUBTRACT);
static_assert(static_cast<int>(Platform::Key::KpAdd) == KEY_KP_ADD);
static_assert(static_cast<int>(Platform::Key::KpEnter) == KEY_KP_ENTER);
static_assert(static_cast<int>(Platform::Key::KpEqual) == KEY_KP_EQUAL);
static_assert(static_cast<int>(Platform::Key::LeftShift) == KEY_LEFT_SHIFT);
static_assert(static_cast<int>(Platform::Key::LeftControl) == KEY_LEFT_CONTROL);
static_assert(static_cast<int>(Platform::Key::LeftAlt) == KEY_LEFT_ALT);
static_assert(static_cast<int>(Platform::Key::LeftSuper) == KEY_LEFT_SUPER);
static_assert(static_cast<int>(Platform::Key::RightShift) == KEY_RIGHT_SHIFT);
static_assert(static_cast<int>(Platform::Key::RightControl) == KEY_RIGHT_CONTROL);
static_assert(static_cast<int>(Platform::Key::RightAlt) == KEY_RIGHT_ALT);
static_assert(static_cast<int>(Platform::Key::RightSuper) == KEY_RIGHT_SUPER);
static_assert(static_cast<int>(Platform::Key::KbMenu) == KEY_KB_MENU);
static_assert(Platform::kKeyCount == KEY_KB_MENU + 1);
static_assert(static_cast<int>(Platform::MouseButton::Left) == MOUSE_BUTTON_LEFT);
static_assert(static_cast<int>(Platform::MouseButton::Right) == MOUSE_BUTTON_RIGHT);
static_assert(static_cast<int>(Platform::MouseButton::Middle) == MOUSE_BUTTON_MIDDLE);
static_assert(static_cast<int>(Platform::MouseButton::Side) == MOUSE_BUTTON_SIDE);
static_assert(static_cast<int>(Platform::MouseButton::Extra) == MOUSE_BUTTON_EXTRA);
static_assert(static_cast<int>(Platform::MouseButton::Forward) == MOUSE_BUTTON_FORWARD);
static_assert(static_cast<int>(Platform::MouseButton::Back) == MOUSE_BUTTON_BACK);
static_assert(static_cast<int>(Platform::GamepadButton::Unknown) == GAMEPAD_BUTTON_UNKNOWN);
static_assert(static_cast<int>(Platform::GamepadButton::LeftFaceUp) == GAMEPAD_BUTTON_LEFT_FACE_UP);
static_assert(static_cast<int>(Platform::GamepadButton::LeftFaceRight) == GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
static_assert(static_cast<int>(Platform::GamepadButton::LeftFaceDown) == GAMEPAD_BUTTON_LEFT_FACE_DOWN);
static_assert(static_cast<int>(Platform::GamepadButton::LeftFaceLeft) == GAMEPAD_BUTTON_LEFT_FACE_LEFT);
static_assert(static_cast<int>(Platform::GamepadButton::RightFaceUp) == GAMEPAD_BUTTON_RIGHT_FACE_UP);
static_assert(static_cast<int>(Platform::GamepadButton::RightFaceRight) == GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
static_assert(static_cast<int>(Platform::GamepadButton::RightFaceDown) == GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
static_assert(static_cast<int>(Platform::GamepadButton::RightFaceLeft) == GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
static_assert(static_cast<int>(Platform::GamepadButton::LeftTrigger1) == GAMEPAD_BUTTON_LEFT_TRIGGER_1);
static_assert(static_cast<int>(Platform::GamepadButton::LeftTrigger2) == GAMEPAD_BUTTON_LEFT_TRIGGER_2);
static_assert(static_cast<int>(Platform::GamepadButton::RightTrigger1) == GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
static_assert(static_cast<int>(Platform::GamepadButton::RightTrigger2) == GAMEPAD_BUTTON_RIGHT_TRIGGER_2);
static_assert(static_cast<int>(Platform::GamepadButton::MiddleLeft) == GAMEPAD_BUTTON_MIDDLE_LEFT);
static_assert(static_cast<int>(Platform::GamepadButton::Middle) == GAMEPAD_BUTTON_MIDDLE);
static_assert(static_cast<int>(Platform::GamepadButton::MiddleRight) == GAMEPAD_BUTTON_MIDDLE_RIGHT);
static_assert(static_cast<int>(Platform::GamepadButton::LeftThumb) == GAMEPAD_BUTTON_LEFT_THUMB);
static_assert(static_cast<int>(Platform::GamepadButton::RightThumb) == GAMEPAD_BUTTON_RIGHT_THUMB);

namespace Platform {

bool Init(const WindowDesc& desc)
{
	// All leaves raylib at its own default (LOG_INFO), which is what the game logged before
	// this interface existed; LOG_ALL would add raylib's TRACE and DEBUG lines
	switch (desc.backendLog) {
		case LogLevel::All: break;
		case LogLevel::Warning: SetTraceLogLevel(LOG_WARNING); break;
		case LogLevel::Error: SetTraceLogLevel(LOG_ERROR); break;
		case LogLevel::Fatal: SetTraceLogLevel(LOG_FATAL); break;
	}

	unsigned int flags = 0;
	if (desc.resizable) { flags |= FLAG_WINDOW_RESIZABLE; }
	if (desc.hidden) { flags |= FLAG_WINDOW_HIDDEN; }
	SetConfigFlags(flags);

	InitWindow(desc.width, desc.height, desc.title);
	if (!IsWindowReady()) { return false; }

	if (desc.x >= 0) { SetWindowPosition(desc.x, desc.y); }
	if (desc.minWidth != 0 || desc.minHeight != 0) { SetWindowMinSize(desc.minWidth, desc.minHeight); }
	if (desc.maxWidth != 0 || desc.maxHeight != 0) { SetWindowMaxSize(desc.maxWidth, desc.maxHeight); }
	return true;
}

void Shutdown() { CloseWindow(); }
bool IsReady() { return IsWindowReady(); }

void BeginFrame()
{
	beginTime = GetTime();
	if (haveBegin) {
		const double frameMs = (beginTime - lastBeginTime) * 1000.0;
		sumFrameMs += frameMs;
		maxFrameMs = std::max(maxFrameMs, frameMs);
		++statFrameIntervals;
	}
	lastBeginTime = beginTime;
	haveBegin = true;
	BeginDrawing();
}

void EndFrame()
{
	const double workMs = (GetTime() - beginTime) * 1000.0;
	sumWorkMs += workMs;
	maxWorkMs = std::max(maxWorkMs, workMs);
	++statFrames;
	EndDrawing();
}

bool ShouldClose() { return WindowShouldClose(); }
void SetTargetFps(int fps) { SetTargetFPS(fps); }

int ScreenWidth() { return GetScreenWidth(); }
int ScreenHeight() { return GetScreenHeight(); }
int RenderWidth() { return GetRenderWidth(); }
int RenderHeight() { return GetRenderHeight(); }
void ToggleFullscreen() { ::ToggleFullscreen(); }
void FocusWindow() { SetWindowFocused(); }

float FrameTime() { return GetFrameTime(); }
double Time() { return GetTime(); }
int Fps() { return GetFPS(); }

void LockCursor() { DisableCursor(); }
void UnlockCursor() { EnableCursor(); }
bool IsCursorLocked() { return IsCursorHidden(); }

// Names below match raylib's global functions, so each raylib call is :: qualified
void SetExitKey(Key key) { ::SetExitKey(static_cast<int>(key)); }
bool IsKeyDown(Key key) { return ::IsKeyDown(static_cast<int>(key)); }
bool IsKeyPressed(Key key) { return ::IsKeyPressed(static_cast<int>(key)); }
bool IsKeyReleased(Key key) { return ::IsKeyReleased(static_cast<int>(key)); }
Key PopKeyPressed() { return static_cast<Key>(GetKeyPressed()); }
Vector2 MousePosition() { return GetMousePosition(); }
Vector2 MouseDelta() { return GetMouseDelta(); }
bool IsMouseButtonDown(MouseButton button) { return ::IsMouseButtonDown(static_cast<int>(button)); }
bool IsMouseButtonPressed(MouseButton button) { return ::IsMouseButtonPressed(static_cast<int>(button)); }
bool IsMouseButtonReleased(MouseButton button) { return ::IsMouseButtonReleased(static_cast<int>(button)); }
bool IsGamepadButtonDown(int gamepad, GamepadButton button) { return ::IsGamepadButtonDown(gamepad, static_cast<int>(button)); }
bool IsGamepadButtonPressed(int gamepad, GamepadButton button) { return ::IsGamepadButtonPressed(gamepad, static_cast<int>(button)); }
bool IsGamepadButtonReleased(int gamepad, GamepadButton button) { return ::IsGamepadButtonReleased(gamepad, static_cast<int>(button)); }

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
