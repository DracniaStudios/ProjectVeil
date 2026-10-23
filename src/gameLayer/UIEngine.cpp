#include "UIEngine.h"

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

	// Same rules as raylib's CheckCollisionPointRec and CheckCollisionRecs, kept
	// here so the UI's rules link without raylib
	static bool contains(Rectangle rect, Vector2 point)
	{
		return point.x >= rect.x && point.x < rect.x + rect.width
			&& point.y >= rect.y && point.y < rect.y + rect.height;
	}

	static bool overlaps(Rectangle a, Rectangle b)
	{
		return a.x < b.x + b.width && a.x + a.width > b.x
			&& a.y < b.y + b.height && a.y + a.height > b.y;
	}

	// The engine whose frame is open, nullptr between frames
	static UIEngine* openFrame = nullptr;

	// A widget drawn outside a frame has nowhere to go. Reported once, not every frame.
	static UIEngine* frameFor(const char* what)
	{
		static bool reported = false;
		if (openFrame == nullptr && !reported)
		{
			std::cerr << "[UI] " << what << " outside beginFrame/endFrame draws nothing\n";
			reported = true;
		}
		return openFrame;
	}

	// UIEngine Implementation
#pragma region Frame
	void beginFrame(UIEngine& ui, const Input& input)
	{
		if (openFrame != nullptr)
		{
			std::cerr << "[UI] beginFrame while a frame is open; ending that one first\n";
			endFrame();
		}
		openFrame = &ui;
		ui.input = input;

		Frame& frame = ui.frame;
		frame = Frame{};
		frame.screenSize = input.screenSize;
		frame.scale = computeScale(input.screenSize, ui.referenceResolution);

		// The last one drawn is on top, so the first hit walking backwards is the
		// only one the mouse is really over. Text never blocks: a label drawn over
		// a button should not make the button unclickable. Nor does a widget that
		// was not drawn, such as a row overflowing the bottom of its panel.
		if (!input.mouseBlocked)
		{
			const std::vector<WidgetEntry>& last = ui.lastFrame.widgets;
			for (auto it = last.rbegin(); it != last.rend(); ++it)
			{
				const Widget& widget = getWidget(*it);
				if (widget.type == WIDGET_TEXT || !widget.isVisible) { continue; }
				if (contains(widget.screenRect, input.mouse)) { frame.hoveredId = widget.id; break; }
			}
		}

		ui.layoutStack.assign(1, UIEngine::Layout{ -1, Rectangle{ 0, 0, input.screenSize.x, input.screenSize.y }, 0 });
	}

	void endFrame()
	{
		if (openFrame == nullptr)
		{
			std::cerr << "[UI] endFrame without a beginFrame\n";
			return;
		}
		UIEngine& ui = *openFrame;
		openFrame = nullptr;

		// Reported once rather than every frame; beginFrame resets the stack, so
		// the damage is limited to whatever was drawn after the missing endPanel.
		static bool reportedUnclosedPanel = false;
		if (ui.layoutStack.size() > 1 && !reportedUnclosedPanel)
		{
			std::cerr << "[UI] beginPanel without a matching endPanel\n";
			reportedUnclosedPanel = true;
		}

		for (const WidgetEntry& entry : ui.frame.widgets)
		{
			const Widget& widget = getWidget(entry);

			DrawCommand outline;
			outline.kind = DrawCommand::Kind::Outline;
			outline.rect = widget.screenRect;
			if (widget.id == ui.highlightedId) { outline.thickness = 3.0f; outline.color = RED; }
			else if (ui.showBounds) { outline.thickness = 1.0f; outline.color = YELLOW; }
			else { continue; }
			ui.frame.commands.push_back(std::move(outline));
		}

		// The inspector asks again every frame it is open, so closing it clears the outline
		ui.highlightedId = -1;

		ui.backend.Draw(ui.frame.commands);
		ui.lastFrame = std::move(ui.frame);
		ui.frame = Frame{};
		ui.layoutStack.clear();
	}
#pragma endregion

#pragma region Layout
	void UIEngine::placeWidget(Widget& widget, Rectangle pixelRect, Rectangle scaleRect)
	{
		const Layout& parent = layoutStack.back();

		widget.id = static_cast<int>(frame.widgets.size());
		widget.parentId = parent.panelId;
		widget.depth = static_cast<int>(layoutStack.size()) - 1;

		widget.anchorRect = scaleRect;
		widget.pixelRect = pixelRect;
		widget.screenRect = resolveRect(parent.contentRect, pixelRect, scaleRect, frame.scale);

		widget.isVisible = widget.screenRect.width > 0 && widget.screenRect.height > 0
			&& overlaps(widget.screenRect, parent.contentRect);
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

	bool UIEngine::hovers(const Widget& widget) const
	{
		// Both are checked: hoveredId is last frame's, so on its own it would
		// still hold a button that has since moved out from under the mouse.
		return widget.isVisible && widget.id == frame.hoveredId && contains(widget.screenRect, input.mouse);
	}
#pragma endregion

#pragma region Drawing
	// Largest font that fills heightRatio of the rect without spilling past its
	// width, so a long label shrinks instead of overflowing a narrow button.
	int UIEngine::fitTextSize(const std::string& text, Rectangle rect) const
	{
		int fontSize = static_cast<int>(rect.height * style.textHeightRatio);

		const float maxWidth = rect.width * 0.9f;
		const int textWidth = backend.TextWidth(text, fontSize);
		if (textWidth > maxWidth && textWidth > 0)
		{
			fontSize = static_cast<int>(fontSize * maxWidth / textWidth);
		}

		return std::max(fontSize, 1);
	}

	void UIEngine::drawRect(Rectangle rect, Color color)
	{
		DrawCommand command;
		command.kind = DrawCommand::Kind::Rect;
		command.rect = rect;
		command.color = color;
		frame.commands.push_back(std::move(command));
	}

	// Text with a drop shadow, centred in a rect given in screen pixels
	void UIEngine::drawLabel(const std::string& text, Rectangle rect, int fontSize, float yOffset)
	{
		const int textWidth = backend.TextWidth(text, fontSize);

		const float textX = rect.x + (rect.width - textWidth) / 2.f;
		const float textY = rect.y + (rect.height - fontSize) / 2.f + yOffset;
		const float shadow = fontSize * 0.08f;

		DrawCommand label;
		label.kind = DrawCommand::Kind::Text;
		label.text = text;
		label.fontSize = fontSize;

		label.position = Vector2{ textX - shadow, textY + shadow };
		label.color = style.textShadowColor;
		frame.commands.push_back(label);

		label.position = Vector2{ textX, textY };
		label.color = style.textColor;
		frame.commands.push_back(std::move(label));
	}

	// Shared by both drawButton overloads once the button has been placed
	bool UIEngine::finishButton(Button& button, const std::string& text)
	{
		button.text = text;
		button.defaultColor = style.buttonColor;
		button.highlightedColor = style.buttonHighlightedColor;
		button.clickColor = style.buttonClickColor;
		button.textSize = fitTextSize(text, button.screenRect);

		if (button.isVisible)
		{
			button.isHovered = hovers(button);
			button.isBeingClicked = button.isHovered && input.mouseDown;
			button.isReleased = button.isHovered && input.mouseReleased;

			Color color = button.defaultColor;
			if (button.isBeingClicked) { color = button.clickColor; }
			else if (button.isHovered) { color = button.highlightedColor; }
			drawRect(button.screenRect, color);

			const float yOffset = button.isBeingClicked ? button.screenRect.height * style.buttonClickOffset : 0.0f;
			drawLabel(text, button.screenRect, button.textSize, yOffset);
		}

		const bool isReleased = button.isReleased;
		frame.widgets.push_back(std::move(button));

		// Fires once, on release, rather than every frame the mouse stays down —
		// isBeingClicked here would re-trigger the caller's action each frame a
		// button is held instead of once per click.
		return isReleased;
	}

	void UIEngine::finishText(Text& widget, const std::string& text)
	{
		widget.text = text;
		widget.defaultColor = style.textColor;
		widget.textSize = fitTextSize(text, widget.screenRect);

		if (widget.isVisible) { drawLabel(text, widget.screenRect, widget.textSize, 0.0f); }

		frame.widgets.push_back(std::move(widget));
	}
#pragma endregion

#pragma region Widgets
	void beginPanel(const std::string& name, Rectangle pixelRect, Rectangle scaleRect)
	{
		UIEngine* ui = frameFor("beginPanel");
		if (ui == nullptr) { return; }

		Panel panel;
		ui->placeWidget(panel, pixelRect, scaleRect);
		panel.name = name;
		panel.defaultColor = ui->style.panelColor;
		// Shown so the inspector can say which panel is swallowing the mouse
		panel.isHovered = ui->hovers(panel);

		const float padding = ui->style.panelPadding * ui->frame.scale;
		panel.contentRect = shrinkRectanglePixels(panel.screenRect, padding * 2.0f, padding * 2.0f);

		// Drawn now, before its children, so they land on top of it
		if (panel.isVisible) { ui->drawRect(panel.screenRect, panel.defaultColor); }

		ui->layoutStack.push_back(UIEngine::Layout{ panel.id, panel.contentRect, 0 });
		ui->frame.widgets.push_back(std::move(panel));
	}

	void endPanel()
	{
		UIEngine* ui = frameFor("endPanel");
		if (ui == nullptr) { return; }

		// The screen layout at the bottom of the stack is never popped
		if (ui->layoutStack.size() <= 1)
		{
			std::cerr << "[UI] endPanel without a matching beginPanel\n";
			return;
		}

		const UIEngine::Layout& layout = ui->layoutStack.back();
		std::get<Panel>(ui->frame.widgets[layout.panelId]).rowCount = layout.rowCount;
		ui->layoutStack.pop_back();
	}

	bool drawButton(const std::string& text, Rectangle pixelRect, Rectangle scaleRect)
	{
		UIEngine* ui = frameFor("drawButton");
		if (ui == nullptr) { return false; }

		Button button;
		ui->placeWidget(button, pixelRect, scaleRect);
		return ui->finishButton(button, text);
	}

	bool drawButton(const std::string& text)
	{
		UIEngine* ui = frameFor("drawButton");
		if (ui == nullptr) { return false; }

		Button button;
		ui->placeNextRow(button);
		return ui->finishButton(button, text);
	}

	void drawText(const std::string& text, Rectangle pixelRect, Rectangle scaleRect)
	{
		UIEngine* ui = frameFor("drawText");
		if (ui == nullptr) { return; }

		Text widget;
		ui->placeWidget(widget, pixelRect, scaleRect);
		ui->finishText(widget, text);
	}

	void drawText(const std::string& text)
	{
		UIEngine* ui = frameFor("drawText");
		if (ui == nullptr) { return; }

		Text widget;
		ui->placeNextRow(widget);
		ui->finishText(widget, text);
	}
#pragma endregion
}
