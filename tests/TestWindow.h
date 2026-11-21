#pragma once
#include <Platform.h>
// The 64x64 window the engine-linked suites have always opened
inline bool OpenTestWindow(const char* title, bool hidden, bool quiet)
{
	Platform::WindowDesc desc;
	desc.width = 64; desc.height = 64; desc.title = title;
	desc.resizable = false; desc.hidden = hidden;
	desc.backendLog = quiet ? Platform::LogLevel::Error : Platform::LogLevel::All;
	return Platform::Init(desc);
}
inline void CloseTestWindow() { Platform::Shutdown(); }
