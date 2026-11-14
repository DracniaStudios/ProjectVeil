// Platform on SDL3 with an OpenGL 3.3 core context (B-15, B-16). Window rules, frame
// limiter, FPS, fullscreen, focus and cursor lock follow the raylib backend; input is B-17.
#include <Platform.h>
#include "Sdl3Native.h"

// The game has its own main(). SDL_SetMainReady is declared only in SDL_main.h, and
// SDL_MAIN_HANDLED stops that header from taking over main.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <iostream>

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

	// Stub state (B-17): recorded, not acted on
	Platform::Key exitKey = Platform::Key::Null;

	void CentreCursor()
	{
		int w = 0, h = 0;
		SDL_GetWindowSize(window, &w, &h);
		SDL_WarpMouseInWindow(window, static_cast<float>(w / 2), static_cast<float>(h / 2));
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

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
			shouldClose = true;
		}
		// B-17: key, mouse and gamepad events update the input state here
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

void SetExitKey(Key key) { exitKey = key; } // B-17
bool IsKeyDown(Key) { return false; }       // B-17
bool IsKeyPressed(Key) { return false; }    // B-17
bool IsKeyReleased(Key) { return false; }   // B-17
Key  PopKeyPressed() { return Key::Null; }  // B-17
Vector2 MousePosition() { return { 0.0f, 0.0f }; } // B-17
Vector2 MouseDelta() { return { 0.0f, 0.0f }; }    // B-17
bool IsMouseButtonDown(MouseButton) { return false; }     // B-17
bool IsMouseButtonPressed(MouseButton) { return false; }  // B-17
bool IsMouseButtonReleased(MouseButton) { return false; } // B-17
bool IsGamepadButtonDown(int, GamepadButton) { return false; }     // B-17
bool IsGamepadButtonPressed(int, GamepadButton) { return false; }  // B-17
bool IsGamepadButtonReleased(int, GamepadButton) { return false; } // B-17

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
