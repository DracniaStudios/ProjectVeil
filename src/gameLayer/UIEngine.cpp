#include "UIEngine.h"

#include <Settings.h>

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


#pragma region Settings Menu
void drawSettings() {

	Settings::Settings* settings = &Settings::Settings::getInstance();

	drawPanel("Settings", Rectangle{ 0, 0, 100, 50 }, Rectangle{ 0.9f, 0.9f, 0.1f, 0.1f }, UIEngine::getInstance());

	if (drawButton("VSync", Rectangle{ 0, 0, 100, 50 }, Rectangle{ 0.4f, 0.3f, 0.2f, 0.1f }, UIEngine::getInstance())) {
		// Handle Audio Settings

		settings->vsync.value = !settings->vsync.value;

	}
};
#pragma endregion

// UIEngine Implementation
#pragma region UIEngine Implementation
void UIEngine::update()
{
	widgets.clear();
	widgetId = 0;
}

void UIEngine::render()
{
	// Widgets are drawn immediately by addButton/addTitle during the scene's
	// draw2D, which also records them in `widgets`. Replaying that list here
	// would call addButton/addTitle again while iterating the vector they
	// push_back into (iterator invalidation) and draw every widget twice.

	if (isSettingsEnabled) { drawSettings(); }

}
#pragma endregion

// UIEngine Helper Functions

// Draws centred text inside a rectangle given in pixels.
static void drawTextPixels(const std::string& text, Rectangle pixelSize, float yOffset)
{
	int fontSize = static_cast<int>(pixelSize.height * 0.5f);

	int textWidth = MeasureText(text.c_str(), fontSize);
	int textHeight = fontSize;

	float textX = pixelSize.x + (pixelSize.width - textWidth) / 2.f;
	float textY = pixelSize.y + (pixelSize.height - textHeight) / 2.f;

	Color shadowColor = { 0, 0, 0, 200 };
	DrawText(text.c_str(), textX - fontSize * 0.08f, textY + fontSize * 0.08f + yOffset, fontSize, shadowColor);

	DrawText(text.c_str(), textX, textY + yOffset, fontSize, WHITE);
	
}

// Draws centred text inside a rectangle given as fractions of the screen (0..1).
void drawTextScaled(std::string text, Rectangle scaleRect, float yOffset)
{
	Rectangle pixelSize = {
		GetScreenWidth() * scaleRect.x,
		GetScreenHeight() * scaleRect.y,
		GetScreenWidth() * scaleRect.width,
		GetScreenHeight() * scaleRect.height
	};
	drawTextPixels(text, pixelSize, yOffset);
}

bool drawButton(std::string text, Rectangle pixelRect, Rectangle scaleRect, UIEngine& ui)
{
	UIEngine::Widget widget;
	widget.type = UIEngine::BUTTON;
	widget.text = text;
	widget.id = ui.getID();

	float w = GetScreenSize().x;
	float h = GetScreenSize().y;

	// Get Base Rectangle
	Rectangle sourceRect = {
		(w * scaleRect.x) + pixelRect.x,
		(h * scaleRect.y) + pixelRect.y,
		(w * scaleRect.width) + pixelRect.width,
		(h * scaleRect.height) + pixelRect.height
	};

	//sourceRect.width = std::min(sourceRect.width, sourceRect.height * 8.f);
	//sourceRect.height = std::min(sourceRect.height, sourceRect.width / 8.f);

	widget.anchorRect = scaleRect;
	widget.pixelRect = pixelRect;

	ui.widgets.push_back(widget);


	// Shrink the rectangle slightly to avoid drawing over the edges of the button
	Rectangle smallerRect = shrinkRectanglePercentage(sourceRect, 0.01f, 0.01f);
	smallerRect.y += smallerRect.height * widget.id;

	// Button Style
	const float clickOffset = 0.05f;
	Color clickColor = { 120, 120, 134, 205 };
	Color defaultColor = { 90, 90, 110, 205 };

	// Button Rules
	bool isHovered = CheckCollisionPointRec(GetMousePosition(), smallerRect);
	bool isBeingClicked = isHovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
	bool isReleased = isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

	// widget was already pushed above (its id determines this button's slot),
	// so the state just computed is written back onto that same entry rather
	// than a copy that goes out of scope when this function returns.

	ui.widgets.back().isHovered = isHovered;
	ui.widgets.back().isBeingClicked = isBeingClicked;
	ui.widgets.back().isReleased = isReleased;

	if (isBeingClicked)
	{
		DrawRectangle(smallerRect.x, smallerRect.y, smallerRect.width, smallerRect.height, clickColor);
	}
	else
	{
		if (isHovered)
		{
			DrawRectangle(smallerRect.x, smallerRect.y, smallerRect.width, smallerRect.height, clickColor);
		}
		else
		{
			DrawRectangle(smallerRect.x, smallerRect.y, smallerRect.width, smallerRect.height, defaultColor);
		}
	}

	if (isBeingClicked)
	{
		drawTextPixels(text, smallerRect, smallerRect.height * clickOffset);
	}
	else
	{
		drawTextPixels(text, smallerRect, 0);
	}

	// Fires once, on release, rather than every frame the mouse stays down —
	// isBeingClicked here would re-trigger the caller's action each frame a
	// button is held instead of once per click.
	return isReleased;
}

void drawTitle(std::string text, UIEngine& ui)
{
	UIEngine::Widget widget;
	widget.type = UIEngine::TITLE;
	widget.text = text;
	widget.id = ui.getID();
	ui.widgets.push_back(widget);

	float w = GetScreenWidth();
	float h = GetScreenHeight();

	// Get Base Rectangle
	Rectangle oneButtonRectangle{};
	oneButtonRectangle.width = w * 0.8f;
	oneButtonRectangle.height = h / (ui.widgets.size() + 1);

	oneButtonRectangle.height = std::min(oneButtonRectangle.height, oneButtonRectangle.width / 8.f);

	oneButtonRectangle = placeRectangleCenterTop(oneButtonRectangle, w);
	oneButtonRectangle.y += oneButtonRectangle.height / 2.f;

	Rectangle smallerRect = shrinkRectanglePercentage(oneButtonRectangle, 0.01f, 0.01f);
	smallerRect.y += smallerRect.height * widget.id;
	drawTextPixels(text, smallerRect, 0);
};

void drawPanel(std::string text, Rectangle pixelRect, Rectangle scaleRect, UIEngine& ui)
{
	UIEngine::Widget widget;
	widget.type = UIEngine::PANEL;
	widget.text = text;
	widget.id = ui.getID();
	ui.widgets.push_back(widget);

	// Get Base Rectangle
	float w = GetScreenSize().x;
	float h = GetScreenSize().y;
	Rectangle sourceRect = {
		(w * scaleRect.x) + pixelRect.x,
		(h * scaleRect.y) + pixelRect.y,
		(w * scaleRect.width) + pixelRect.width,
		(h * scaleRect.height) + pixelRect.height
	};
	// Shrink the rectangle slightly to avoid drawing over the edges of the button
	Rectangle smallerRect = shrinkRectanglePercentage(sourceRect, 0.01f, 0.01f);
	smallerRect.y += smallerRect.height * widget.id;
	Color panelColor = { 50, 50, 50, 200 };
	DrawRectangle(smallerRect.x, smallerRect.y, smallerRect.width, smallerRect.height, panelColor);
	drawTextPixels(text, smallerRect, 0);
};