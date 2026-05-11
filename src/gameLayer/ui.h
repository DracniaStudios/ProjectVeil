#pragma once
#ifndef UI_H
#define UI_H

#include <raylib.h>
#include <string>
#include <vector>

// 115

Rectangle placeRectangleTopRightCorner(Rectangle r, float w);

Rectangle placeRectangleTopLeftCorner(Rectangle r, float w);

Rectangle placeRectangleBottomRightCorner(Rectangle r, float w, float h);

Rectangle placeRectangleBottomLeftCorner(Rectangle r, float w, float h);

Rectangle placeRectangleCenter(Rectangle r, float w, float h);

Rectangle placeRectangleCenterTop(Rectangle r, float w);

Rectangle placeRectangleCenterBottom(Rectangle r, float w, float h);

Rectangle placeRectangleCenterLeft(Rectangle r, float h);

Rectangle plateRectangleCenterRight(Rectangle r, float w, float h);

Rectangle enlargeRectanglePixels(Rectangle r, float pixelX, float pixelY);

Rectangle shrinkRectanglePercentage(Rectangle r, float percentageX, float percentageY);

struct UIEngine
{
	int widgetId;

	enum Type
	{
		none,
		title,
		button,
	};

	struct Widget
	{
		std::string text = {};
		int type = 0;
		int id = 0;

		bool isHovered = false;
		bool isBeingClicked = false;
		bool isReleased = false;
	};

	int getID(){return widgetId++;}

	std::vector<Widget> widgets;


	void updateAndRender();
};

void drawText(std::string text, Rectangle smallerRect, float yOffset = 0);
bool addButton(std::string text, UIEngine &ui);
void addTitle(std::string text, UIEngine &ui);

#endif
