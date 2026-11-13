// EditorGui on raylib and rlImGui (B-04)
#include <EditorGui.h>

#include <raylib.h>
#include <imgui.h>
#include <rlImGui.h>
#include <imgui_impl_raylib.h>

namespace {
	// The context rlImGuiSetup made. rlImGuiEnd selected it before drawing; so does RenderDrawData.
	ImGuiContext* context = nullptr;
}

namespace EditorGui {

void Init()
{
	rlImGuiSetup(true);
	context = ImGui::GetCurrentContext();
	ImGuiIO& io = ImGui::GetIO();
	io.AddMousePosEvent(GetMousePosition().x, GetMousePosition().y);
	io.AddMouseButtonEvent(MOUSE_BUTTON_LEFT, IsMouseButtonDown(MOUSE_BUTTON_LEFT));
	io.AddMouseButtonEvent(MOUSE_BUTTON_RIGHT, IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
}

void NewFrame() { rlImGuiBegin(); }

void RenderDrawData()
{
	if (context != nullptr) { ImGui::SetCurrentContext(context); }
	ImGui_ImplRaylib_RenderDrawData(ImGui::GetDrawData());
}

void Shutdown()
{
	rlImGuiShutdown();
	context = nullptr;
}

}
