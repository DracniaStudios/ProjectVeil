// Private to src/platform/backend_sdl3 and src/engineLayer/Render/backend_gl (B-15).
// Nothing outside those two folders includes this.
#pragma once
#include <SDL3/SDL.h>

SDL_Window*   Sdl3Window();
SDL_GLContext Sdl3GlContext();
void Sdl3EditorGuiProcessEvent(const SDL_Event& event); // no-op until EditorGui::Init
