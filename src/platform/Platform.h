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

	// work = BeginFrame to EndFrame entry, so swap and wait are excluded
	struct FrameStats { int frames = 0; double meanWorkMs = 0, maxWorkMs = 0, meanFrameMs = 0, maxFrameMs = 0; };
	FrameStats GetFrameStats();
}
#endif
