#pragma once
#ifndef VEIL_EDITOR_GUI_H
#define VEIL_EDITOR_GUI_H
namespace EditorGui {
	void Init();           // after Platform::Init (and, from B-07, Render::Init): ImGui context, fonts, backend
	void NewFrame();       // after Platform::BeginFrame
	void RenderDrawData(); // after ImGui::Render(): draws ImGui::GetDrawData()
	void Shutdown();
}
#endif
