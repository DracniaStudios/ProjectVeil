#include "UIEngine.h"

#include <Settings.h>

#include <algorithm>
#include <iostream>

namespace UI {

#pragma region UIEngine Helper Functions
	// Position
	Rectangle placeRectangleTopRightCorner(Rectangle r, float w)
	{
		r.x = w - r.width;
		r.y = 0;
		return r;
	}

	Rectangle placeRectangleTopLeftCorner(Rectangle r, float w)
	{
		r.x = 0;
		r.y = 0;
		return r;
	}

	Rectangle placeRectangleBottomRightCorner(Rectangle r, float w, float h)
	{
		r.x = w - r.width;
		r.y = h - r.height;
		return r;
	}

	Rectangle placeRectangleBottomLeftCorner(Rectangle r, float w, float h)
	{
		r.x = 0;
		r.y = h - r.height;
		return r;
	}

	Rectangle placeRectangleCenter(Rectangle r, float w, float h)
	{
		r.x = (w - r.width) / 2.0f;
		r.y = (h - r.height) / 2.0f;
		return r;
	}

	Rectangle placeRectangleCenterTop(Rectangle r, float w)
	{
		r.x = (w - r.width) / 2.0f;
		r.y = 0;
		return r;
	}

	Rectangle placeRectangleCenterBottom(Rectangle r, float w, float h)
	{
		r.x = (w - r.width) / 2.0f;
		r.y = h - r.height;
		return r;
	}

	Rectangle placeRectangleCenterLeft(Rectangle r, float h)
	{
		r.x = 0;
		r.y = (h - r.height) / 2.0f;
		return r;
	}

	Rectangle placeRectangleCenterRight(Rectangle r, float w, float h)
	{
		r.x = w - r.width;
		r.y = (h - r.height) / 2.0f;
		return r;
	}

	// Scale


	Rectangle enlargeRectanglePixels(Rectangle r, float pixelX, float pixelY)
	{
		r.width += pixelX;
		r.height += pixelY;

		r.x -= pixelX / 2.f;
		r.y -= pixelY / 2.f;

		return  r;
	}
	Rectangle shrinkRectanglePixels(Rectangle r, float pixelX, float pixelY)
	{
		r.width -= pixelX;
		r.height -= pixelY;

		r.x += pixelX / 2.f;
		r.y += pixelY / 2.f;

		return  r;
	}

	Rectangle shrinkRectanglePercentage(Rectangle r, float percentageX, float percentageY)
	{
		float shrinkX = r.width * percentageX;
		float shrinkY = r.height * percentageY;

		r.width -= shrinkX;
		r.height -= shrinkY;

		r.x += shrinkX / 2.f;
		r.y += shrinkY / 2.f;

		return r;
	}
	Rectangle enlargeRectanglePercentage(Rectangle r, float percentageX, float percentageY)
	{
		float shrinkX = r.width * percentageX;
		float shrinkY = r.height * percentageY;

		r.width += shrinkX;
		r.height += shrinkY;

		r.x -= shrinkX / 2.f;
		r.y -= shrinkY / 2.f;

		return r;
	}
#pragma endregion

#pragma region Scaling
	Rectangle resolveRect(Rectangle parentRect, Rectangle pixelRect, Rectangle scaleRect, float scale)
	{
		return Rectangle{
			parentRect.x + parentRect.width * scaleRect.x + pixelRect.x * scale,
			parentRect.y + parentRect.height * scaleRect.y + pixelRect.y * scale,
			parentRect.width * scaleRect.width + pixelRect.width * scale,
			parentRect.height * scaleRect.height + pixelRect.height * scale
		};
	}

	float computeScale(Vector2 screenSize, Vector2 referenceResolution)
	{
		// A zeroed reference (e.g. dragged to 0 in the inspector) would divide by zero
		if (referenceResolution.x <= 0.0f || referenceResolution.y <= 0.0f) { return 1.0f; }

		return std::min(screenSize.x / referenceResolution.x, screenSize.y / referenceResolution.y);
	}
#pragma endregion

#pragma region Settings Menu
	void drawSettings() {

		Settings::Settings* settings = &Settings::Settings::getInstance();

		beginPanel("Settings", Rectangle{}, Rectangle{ 0.65f, 0.3f, 0.2f, 0.28f });

		drawText("Settings");

		if (drawButton(settings->vsync.value != 0 ? "VSync: On" : "VSync: Off")) {
			settings->vsync.value = !settings->vsync.value;
		}

		endPanel();
	};
#pragma endregion

	// UIEngine Implementation
#pragma region UIEngine Implementation
	void UIEngine::beginFrame()
	{
		// Hover is resolved against last frame's widgets, before they are cleared.
		// The last one drawn is on top, so the first hit walking backwards is the
		// only one the mouse is really over. Text never blocks: a label drawn over
		// a button should not make the button unclickable.
		hoveredId = -1;
		if (!isMouseBlocked)
		{
			const Vector2 mouse = GetMousePosition();
			for (auto it = widgets.rbegin(); it != widgets.rend(); ++it)
			{
				const Widget& widget = getWidget(*it);
				if (widget.type == WIDGET_TEXT) { continue; }
				if (CheckCollisionPointRec(mouse, widget.screenRect)) { hoveredId = widget.id; break; }
			}
		}

		widgets.clear();

		scale = computeScale(GetScreenSize(), referenceResolution);

		layoutStack.clear();
		layoutStack.push_back(Layout{ -1, Rectangle{ 0, 0, GetScreenSize().x, GetScreenSize().y }, 0 });
	}

	void UIEngine::endFrame()
	{
		// Reported once rather than every frame; beginFrame resets the stack, so
		// the damage is limited to whatever was drawn after the missing endPanel.
		static bool reportedUnclosedPanel = false;
		if (layoutStack.size() > 1 && !reportedUnclosedPanel)
		{
			std::cerr << "[UI] beginPanel without a matching endPanel\n";
			reportedUnclosedPanel = true;
		}

		for (const WidgetEntry& entry : widgets)
		{
			const Widget& widget = getWidget(entry);

			if (widget.id == highlightedId) { DrawRectangleLinesEx(widget.screenRect, 3.0f, RED); }
			else if (showBounds) { DrawRectangleLinesEx(widget.screenRect, 1.0f, YELLOW); }
		}

		// The inspector re-sets this every frame it is open, so closing it clears the outline
		highlightedId = -1;
	}

	void UIEngine::render()
	{
		// Widgets are drawn the moment their draw function is called, during the
		// scene's draw2D, so there is nothing to replay here — only the menus the
		// engine owns.

		if (isSettingsEnabled) { drawSettings(); }

	}

	void UIEngine::placeWidget(Widget& widget, Rectangle pixelRect, Rectangle scaleRect)
	{
		const Layout& parent = layoutStack.back();

		widget.id = static_cast<int>(widgets.size());
		widget.parentId = parent.panelId;
		widget.depth = static_cast<int>(layoutStack.size()) - 1;

		widget.anchorRect = scaleRect;
		widget.pixelRect = pixelRect;
		widget.screenRect = resolveRect(parent.contentRect, pixelRect, scaleRect, scale);

		const Rectangle screen = { 0, 0, GetScreenSize().x, GetScreenSize().y };
		widget.isVisible = widget.screenRect.width > 0 && widget.screenRect.height > 0
			&& CheckCollisionRecs(widget.screenRect, screen);
	}

	void UIEngine::placeNextRow(Widget& widget)
	{
		const int row = layoutStack.back().rowCount++;

		// A row is the parent's full width at a fixed reference height, stacked top
		// to bottom. Written as ordinary rects rather than resolved here, so the
		// inspector shows auto-placed widgets in the same terms as every other.
		const Rectangle pixelRect = { 0.0f, row * (style.rowHeight + style.rowSpacing), 0.0f, style.rowHeight };
		const Rectangle scaleRect = { 0.0f, 0.0f, 1.0f, 0.0f };

		placeWidget(widget, pixelRect, scaleRect);
		widget.isAutoPlaced = true;
	}

	void UIEngine::pushPanel(Panel panel)
	{
		const int panelId = panel.id;
		const Rectangle contentRect = panel.contentRect;

		widgets.push_back(std::move(panel));
		layoutStack.push_back(Layout{ panelId, contentRect, 0 });
	}

	void UIEngine::popPanel()
	{
		// The screen layout at the bottom of the stack is never popped
		if (layoutStack.size() <= 1)
		{
			std::cerr << "[UI] endPanel without a matching beginPanel\n";
			return;
		}

		const Layout& layout = layoutStack.back();
		std::get<Panel>(widgets[layout.panelId]).rowCount = layout.rowCount;
		layoutStack.pop_back();
	}
#pragma endregion

	// UIEngine Helper Functions

	// Largest font that fills heightRatio of the rect without spilling past its
	// width, so a long label shrinks instead of overflowing a narrow button.
	static int fitTextSize(const std::string& text, Rectangle rect, float heightRatio)
	{
		int fontSize = static_cast<int>(rect.height * heightRatio);

		const float maxWidth = rect.width * 0.9f;
		const int textWidth = MeasureText(text.c_str(), fontSize);
		if (textWidth > maxWidth && textWidth > 0)
		{
			fontSize = static_cast<int>(fontSize * maxWidth / textWidth);
		}

		return std::max(fontSize, 1);
	}

	// Draws text with a drop shadow, centred in a rect given in screen pixels.
	static void drawLabel(const std::string& text, Rectangle rect, int fontSize, float yOffset, const Style& style)
	{
		const int textWidth = MeasureText(text.c_str(), fontSize);

		const float textX = rect.x + (rect.width - textWidth) / 2.f;
		const float textY = rect.y + (rect.height - fontSize) / 2.f + yOffset;
		const float shadow = fontSize * 0.08f;

		DrawText(text.c_str(), static_cast<int>(textX - shadow), static_cast<int>(textY + shadow), fontSize, style.textShadowColor);
		DrawText(text.c_str(), static_cast<int>(textX), static_cast<int>(textY), fontSize, style.textColor);
	}

	// Shared by both drawButton overloads once the button has been placed
	static bool drawPlacedButton(UIEngine& ui, Button& button, const std::string& text)
	{
		const Style& style = ui.style;

		button.text = text;
		button.defaultColor = style.buttonColor;
		button.highlightedColor = style.buttonHighlightedColor;
		button.clickColor = style.buttonClickColor;
		button.textSize = fitTextSize(text, button.screenRect, style.textHeightRatio);

		// Button Rules
		// Both the mouse and the hover owner are checked: hoveredId is last
		// frame's, so on its own it would still hold a button that has since moved.
		button.isHovered = button.id == ui.hoveredId && CheckCollisionPointRec(GetMousePosition(), button.screenRect);
		button.isBeingClicked = button.isHovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
		button.isReleased = button.isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

		Color color = button.defaultColor;
		if (button.isBeingClicked) { color = button.clickColor; }
		else if (button.isHovered) { color = button.highlightedColor; }

		DrawRectangleRec(button.screenRect, color);

		const float yOffset = button.isBeingClicked ? button.screenRect.height * style.buttonClickOffset : 0.0f;
		drawLabel(text, button.screenRect, button.textSize, yOffset, style);

		const bool isReleased = button.isReleased;
		ui.widgets.push_back(std::move(button));

		// Fires once, on release, rather than every frame the mouse stays down —
		// isBeingClicked here would re-trigger the caller's action each frame a
		// button is held instead of once per click.
		return isReleased;
	}

	static void drawPlacedText(UIEngine& ui, Text& widget, const std::string& text)
	{
		widget.text = text;
		widget.defaultColor = ui.style.textColor;
		widget.textSize = fitTextSize(text, widget.screenRect, ui.style.textHeightRatio);

		drawLabel(text, widget.screenRect, widget.textSize, 0.0f, ui.style);

		ui.widgets.push_back(std::move(widget));
	}

	void beginPanel(const std::string& name, Rectangle pixelRect, Rectangle scaleRect)
	{
		UIEngine& ui = UIEngine::getInstance();

		Panel panel;
		ui.placeWidget(panel, pixelRect, scaleRect);
		panel.name = name;
		panel.defaultColor = ui.style.panelColor;
		// Shown so the inspector can say which panel is swallowing the mouse
		panel.isHovered = panel.id == ui.hoveredId && CheckCollisionPointRec(GetMousePosition(), panel.screenRect);

		const float padding = ui.style.panelPadding * ui.getScale();
		panel.contentRect = shrinkRectanglePixels(panel.screenRect, padding * 2.0f, padding * 2.0f);

		// Drawn now, before its children, so they land on top of it
		DrawRectangleRec(panel.screenRect, panel.defaultColor);

		ui.pushPanel(std::move(panel));
	}

	void endPanel()
	{
		UIEngine::getInstance().popPanel();
	}

	bool drawButton(const std::string& text, Rectangle pixelRect, Rectangle scaleRect)
	{
		UIEngine& ui = UIEngine::getInstance();

		Button button;
		ui.placeWidget(button, pixelRect, scaleRect);
		return drawPlacedButton(ui, button, text);
	}

	bool drawButton(const std::string& text)
	{
		UIEngine& ui = UIEngine::getInstance();

		Button button;
		ui.placeNextRow(button);
		return drawPlacedButton(ui, button, text);
	}

	void drawText(const std::string& text, Rectangle pixelRect, Rectangle scaleRect)
	{
		UIEngine& ui = UIEngine::getInstance();

		Text widget;
		ui.placeWidget(widget, pixelRect, scaleRect);
		drawPlacedText(ui, widget, text);
	}

	void drawText(const std::string& text)
	{
		UIEngine& ui = UIEngine::getInstance();

		Text widget;
		ui.placeNextRow(widget);
		drawPlacedText(ui, widget, text);
	}
}
