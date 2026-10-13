#include "WorldEditor.h"

#include <UIEngine.h>

namespace Editor {

namespace
{
	const ImVec4 HEADER_COLOR = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
	const ImVec4 SECTION_COLOR = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);

	// RGBA, unlike the Lighting inspector's RGB helper: UI colours lean on alpha
	// for their translucency.
	bool EditColor(const char* label, Color& color)
	{
		float values[4] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
		if (!ImGui::ColorEdit4(label, values, ImGuiColorEditFlags_AlphaPreviewHalf)) { return false; }

		color.r = static_cast<unsigned char>(Clamp(values[0], 0.0f, 1.0f) * 255.0f);
		color.g = static_cast<unsigned char>(Clamp(values[1], 0.0f, 1.0f) * 255.0f);
		color.b = static_cast<unsigned char>(Clamp(values[2], 0.0f, 1.0f) * 255.0f);
		color.a = static_cast<unsigned char>(Clamp(values[3], 0.0f, 1.0f) * 255.0f);
		return true;
	}

	// Widget fields are a record of last frame's draw calls; the next call would
	// overwrite anything typed here. So they are shown, never offered for editing.
	void ShowFlag(const char* label, bool value)
	{
		ImGui::BeginDisabled();
		ImGui::Checkbox(label, &value);
		ImGui::EndDisabled();
	}

	void ShowRect(const char* label, Rectangle rect)
	{
		ImGui::Text("%s: (%.2f, %.2f, %.2f, %.2f)", label, rect.x, rect.y, rect.width, rect.height);
	}

	void ShowColor(const char* label, Color color)
	{
		const ImVec4 value(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
		ImGui::ColorButton(label, value, ImGuiColorEditFlags_AlphaPreviewHalf);
		ImGui::SameLine();
		ImGui::Text("%s (%d, %d, %d, %d)", label, color.r, color.g, color.b, color.a);
	}

	// What a widget shows on screen, or a panel's name, to tell rows apart
	const char* WidgetLabel(const UI::WidgetEntry& entry)
	{
		if (const auto* text = std::get_if<UI::Text>(&entry)) { return text->text.c_str(); }
		if (const auto* button = std::get_if<UI::Button>(&entry)) { return button->text.c_str(); }
		if (const auto* panel = std::get_if<UI::Panel>(&entry)) { return panel->name.c_str(); }
		return "";
	}

	void ShowWidgetDetails(const UI::WidgetEntry& entry)
	{
		const UI::Widget& widget = UI::getWidget(entry);
		const auto* text = std::get_if<UI::Text>(&entry);
		const auto* button = std::get_if<UI::Button>(&entry);
		const auto* panel = std::get_if<UI::Panel>(&entry);

		ImGui::TextColored(SECTION_COLOR, "Data");
		ImGui::Text("Widget ID: %d", widget.id);
		ImGui::Text("Widget Type: %s", UI::widgetTypeToString(widget.type));
		if (widget.parentId < 0) { ImGui::Text("Parent: Screen"); }
		else { ImGui::Text("Parent: Panel #%d", widget.parentId); }
		ImGui::Text("Depth: %d", widget.depth);

		if (text) { ImGui::Text("Text: \"%s\" (size %d)", text->text.c_str(), text->textSize); }
		if (button) { ImGui::Text("Text: \"%s\" (size %d)", button->text.c_str(), button->textSize); }
		if (panel)
		{
			ImGui::Text("Name: %s", panel->name.c_str());
			ImGui::Text("Rows: %d", panel->rowCount);
		}
		ImGui::Separator();

		ImGui::TextColored(SECTION_COLOR, "States");
		ShowFlag("Is Visible", widget.isVisible);
		ShowFlag("Is Hovered", widget.isHovered);
		ShowFlag("Is Being Clicked", widget.isBeingClicked);
		ShowFlag("Is Released", widget.isReleased);
		ImGui::Separator();

		ImGui::TextColored(SECTION_COLOR, "Screen Data");
		ShowFlag("Is Auto Placed", widget.isAutoPlaced);
		ShowRect("Anchor Rect", widget.anchorRect);
		ShowRect("Pixel Rect", widget.pixelRect);
		ShowRect("Screen Rect", widget.screenRect);
		if (panel) { ShowRect("Content Rect", panel->contentRect); }
		ImGui::Separator();

		ImGui::TextColored(SECTION_COLOR, "Style");
		ShowColor("Default Color", widget.defaultColor);
		if (button)
		{
			ShowColor("Highlighted Color", button->highlightedColor);
			ShowColor("Click Color", button->clickColor);
		}
	}
}

/**
 * UI inspector.
 *
 * Lists every widget the UIEngine drew last frame, in draw order. The list is
 * rebuilt from the draw calls each frame, so widget data is read-only here.
 * What does persist — the shared Style and the reference resolution — is
 * editable, and restyles every widget at once.
 */
void WorldEditor::ShowUIData()
{
	auto& ui = UI::UIEngine::getInstance();

	ImGui::Begin("UI Data");

	// --- Scaling ----------------------------------------------------------
	ImGui::TextColored(HEADER_COLOR, "Scaling");
	ImGui::Text("Screen: %d x %d", GetScreenWidth(), GetScreenHeight());
	ImGui::DragFloat2("Reference resolution", &ui.referenceResolution.x, 1.0f, 1.0f, 7680.0f, "%.0f");
	ImGui::Text("UI scale: %.3f", ui.getScale());
	ImGui::Separator();

	// --- Style ------------------------------------------------------------
	if (ImGui::CollapsingHeader("Style"))
	{
		UI::Style& style = ui.style;

		ImGui::TextColored(SECTION_COLOR, "Text");
		EditColor("Text color", style.textColor);
		EditColor("Shadow color", style.textShadowColor);
		ImGui::DragFloat("Height ratio", &style.textHeightRatio, 0.01f, 0.05f, 1.0f);

		ImGui::TextColored(SECTION_COLOR, "Button");
		EditColor("Default", style.buttonColor);
		EditColor("Highlighted", style.buttonHighlightedColor);
		EditColor("Click", style.buttonClickColor);
		ImGui::DragFloat("Click offset", &style.buttonClickOffset, 0.005f, 0.0f, 0.5f);

		ImGui::TextColored(SECTION_COLOR, "Panel");
		EditColor("Panel color", style.panelColor);
		ImGui::DragFloat("Padding", &style.panelPadding, 0.5f, 0.0f, 200.0f);

		ImGui::TextColored(SECTION_COLOR, "Rows");
		ImGui::DragFloat("Row height", &style.rowHeight, 0.5f, 1.0f, 400.0f);
		ImGui::DragFloat("Row spacing", &style.rowSpacing, 0.5f, 0.0f, 200.0f);

		if (ImGui::Button("Reset Style")) { style = {}; }
	}
	ImGui::Separator();

	// --- Widgets ----------------------------------------------------------
	ImGui::TextColored(HEADER_COLOR, "Widgets (%d)", static_cast<int>(ui.widgets.size()));
	if (ui.hoveredId < 0) { ImGui::Text("Under mouse: none"); }
	else { ImGui::Text("Under mouse: #%d", ui.hoveredId); }
	ImGui::Checkbox("Show bounds in game", &ui.showBounds);
	ImGui::TextDisabled("Hover a row to outline that widget in game.");

	ImGui::BeginChild("##Widgets");
	for (const UI::WidgetEntry& entry : ui.widgets)
	{
		const UI::Widget& widget = UI::getWidget(entry);
		ImGui::PushID(widget.id);

		// A panel's children follow it in draw order, so indenting by depth
		// reads as the panel tree without rebuilding one.
		const float indent = widget.depth * ImGui::GetStyle().IndentSpacing;
		if (indent > 0.0f) { ImGui::Indent(indent); }

		const bool isOpen = ImGui::TreeNode("##Widget", "%s #%d  %s",
			UI::widgetTypeToString(widget.type), widget.id, WidgetLabel(entry));
		if (ImGui::IsItemHovered()) { ui.highlightedId = widget.id; }

		if (isOpen)
		{
			ShowWidgetDetails(entry);
			ImGui::TreePop();
		}

		if (indent > 0.0f) { ImGui::Unindent(indent); }
		ImGui::PopID();
	}
	ImGui::EndChild();

	ImGui::End();
}

}
