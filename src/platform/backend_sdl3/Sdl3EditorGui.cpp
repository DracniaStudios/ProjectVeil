// EditorGui on imgui_impl_sdl3 and imgui_impl_opengl3 (B-15)
#include <EditorGui.h>
#include "Sdl3Native.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#include <cmath>

namespace {
	ImGuiContext* context = nullptr;
	bool initialised = false;
}

void Sdl3EditorGuiProcessEvent(const SDL_Event& event)
{
	if (initialised) { ImGui_ImplSDL3_ProcessEvent(&event); }
}

namespace EditorGui {

void Init()
{
	context = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	// The font setup of rlImGui.cpp (rlImGuiBeginInitImGui) for a window without high-DPI
	const float scale = SDL_GetWindowDisplayScale(Sdl3Window());
	ImFontConfig config;
	config.SizePixels = std::ceil(13.0f * scale);
	config.RasterizerMultiply = scale;
	config.PixelSnapH = true;
	io.Fonts->AddFontDefault(&config);

	ImGui::StyleColorsDark();
	ImGui_ImplSDL3_InitForOpenGL(Sdl3Window(), Sdl3GlContext());
	ImGui_ImplOpenGL3_Init("#version 330");
	initialised = true;
}

void NewFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void RenderDrawData()
{
	if (context != nullptr) { ImGui::SetCurrentContext(context); }
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Shutdown()
{
	initialised = false;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	context = nullptr;
}

}
