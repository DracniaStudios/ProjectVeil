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
