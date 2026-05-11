#include "ui.h"

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

void UIEngine::updateAndRender()
{
	/*
	for (size_t i = 0; i < widgets.size(); i++)
	{
		auto w = widgets[i];
		w.id = i;
	}
	*/
	widgets.clear();
	widgetId = 0;
}

void drawText(std::string text, Rectangle smallerRect, float yOffset)
{
	int fontSize = static_cast<int>(smallerRect.height * 0.5f);

	int textWidth = MeasureText(text.c_str(), fontSize);
	int textHeight = fontSize;

	float textX = smallerRect.x + (smallerRect.width - textWidth) / 2.f;
	float textY = smallerRect.y + (smallerRect.height - textHeight) / 2.f;

	Color shadowColor = { 0, 0, 0, 200 };
	DrawText(text.c_str(), textX - fontSize * 0.08f, textY + fontSize * 0.08f + yOffset, fontSize, shadowColor);

	DrawText(text.c_str(), textX, textY + yOffset, fontSize, WHITE);
	
}

bool addButton(std::string text, UIEngine &ui)
{
	UIEngine::Widget widget;
	widget.type = UIEngine::button;
	widget.text = text;
	widget.id = ui.getID();
	ui.widgets.push_back(widget);


	float w = GetScreenWidth();
	float h = GetScreenHeight();

	// Get Base Rectangle
	Rectangle oneButtonRectangle;
	oneButtonRectangle.width = w * 0.8f;
	oneButtonRectangle.height = h / (ui.widgets.size() + 1);

	oneButtonRectangle.height = std::min(oneButtonRectangle.height, oneButtonRectangle.width / 8.f);

	oneButtonRectangle = placeRectangleCenterTop(oneButtonRectangle, w);
	oneButtonRectangle.y += oneButtonRectangle.height / 2.f;

	int fontSize = static_cast<int>(oneButtonRectangle.height * 0.5f);

	Rectangle smallerRect = shrinkRectanglePercentage(oneButtonRectangle, 0.01f, 0.01f);
	smallerRect.y += smallerRect.height * widget.id;


	// Button Style
	const float clickOffset = 0.05f;
	Color clickColor = { 120, 120, 134, 205 };
	Color defaultColor = { 90, 90, 110, 205 };

	// Button Rules
	bool isHovered = CheckCollisionPointRec(GetMousePosition(), smallerRect);
	bool isBeingClicked = isHovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
	bool isReleased = isHovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

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
		drawText(text, smallerRect, smallerRect.height * clickOffset);
		return true;
	}
	else
	{
		drawText(text, smallerRect);
		return false;
	}
}

void addTitle(std::string text, UIEngine &ui)
{
	UIEngine::Widget widget;
	widget.type = UIEngine::title;
	widget.text = text;
	widget.id = ui.getID();
	ui.widgets.push_back(widget);

	float w = GetScreenWidth();
	float h = GetScreenHeight();

	// Get Base Rectangle
	Rectangle oneButtonRectangle;
	oneButtonRectangle.width = w * 0.8f;
	oneButtonRectangle.height = h / (ui.widgets.size() + 1);

	oneButtonRectangle.height = std::min(oneButtonRectangle.height, oneButtonRectangle.width / 8.f);

	oneButtonRectangle = placeRectangleCenterTop(oneButtonRectangle, w);
	oneButtonRectangle.y += oneButtonRectangle.height / 2.f;

	int fontSize = static_cast<int>(oneButtonRectangle.height * 0.5f);

	Rectangle smallerRect = shrinkRectanglePercentage(oneButtonRectangle, 0.01f, 0.01f);
	smallerRect.y += smallerRect.height * widget.id;
	drawText(text, smallerRect);
}
