// Platform on SDL3 with an OpenGL 3.3 core context (B-15). Window, context, quit, time
// and size only; input, the frame limiter, cursor lock and fullscreen are B-16 and B-17.
#include <Platform.h>
#include "Sdl3Native.h"

// The game has its own main(). SDL_SetMainReady is declared only in SDL_main.h, and
// SDL_MAIN_HANDLED stops that header from taking over main.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <iostream>

namespace {
	SDL_Window* window = nullptr;
	SDL_GLContext glContext = nullptr;
	Uint64 timeZeroNs = 0;

	// BeginFrame entry, EndFrame entry and the previous BeginFrame entry, in seconds
	double beginTime = 0.0;
	double lastBeginTime = 0.0;
	bool haveBegin = false;
	float lastFrameSeconds = 0.0f;
	bool shouldClose = false;

	// Frame statistics, counted as RaylibPlatform.cpp does: the game prints them at exit
	int statFrames = 0;
	int statFrameIntervals = 0;
	double sumWorkMs = 0.0, maxWorkMs = 0.0;
	double sumFrameMs = 0.0, maxFrameMs = 0.0;

	// Stub state (B-16, B-17): recorded, not acted on
	int targetFps = 0;
	bool cursorLocked = false;
	Platform::Key exitKey = Platform::Key::Null;
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

	// Minimum and maximum size (desc.minWidth to desc.maxHeight): B-16
	if (desc.x != -1 || desc.y != -1) {
		int x = desc.x, y = desc.y;
		int currentX = 0, currentY = 0;
		SDL_GetWindowPosition(window, &currentX, &currentY);
		if (x == -1) { x = currentX; }
		if (y == -1) { y = currentY; }
		SDL_SetWindowPosition(window, x, y);
	}

	glContext = SDL_GL_CreateContext(window);
	if (glContext == nullptr || !SDL_GL_MakeCurrent(window, glContext)) {
		std::cerr << "[Platform] OpenGL 3.3 core context failed: " << SDL_GetError() << std::endl;
		Shutdown();
		return false;
	}
	SDL_GL_SetSwapInterval(0);

	timeZeroNs = SDL_GetTicksNS();

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
		const double frameSeconds = beginTime - lastBeginTime;
		lastFrameSeconds = static_cast<float>(frameSeconds);
		const double frameMs = frameSeconds * 1000.0;
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
	// B-16: wait out the target frame time here
}

bool ShouldClose() { return shouldClose; }
void SetTargetFps(int fps) { targetFps = fps; } // B-16

int ScreenWidth() { int w = 0, h = 0; SDL_GetWindowSize(window, &w, &h); return w; }
int ScreenHeight() { int w = 0, h = 0; SDL_GetWindowSize(window, &w, &h); return h; }
int RenderWidth() { int w = 0, h = 0; SDL_GetWindowSizeInPixels(window, &w, &h); return w; }
int RenderHeight() { int w = 0, h = 0; SDL_GetWindowSizeInPixels(window, &w, &h); return h; }
void ToggleFullscreen() {} // B-16
void FocusWindow() {}      // B-16

float FrameTime() { return lastFrameSeconds; }
double Time() { return static_cast<double>(SDL_GetTicksNS() - timeZeroNs) / 1.0e9; }
int Fps() { return 0; } // B-16

void LockCursor() { cursorLocked = true; }     // B-16
void UnlockCursor() { cursorLocked = false; }  // B-16
bool IsCursorLocked() { return cursorLocked; } // B-16

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
