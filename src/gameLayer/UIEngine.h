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

Rectangle placeRectangleCenterRight(Rectangle r, float w, float h);

Rectangle enlargeRectanglePixels(Rectangle r, float pixelX, float pixelY);

Rectangle shrinkRectanglePercentage(Rectangle r, float percentageX, float percentageY);

// Scale All My Percentage
class UIEngine
{
public:

	// Delete, copy, and move functions to prevent duplication
	UIEngine(const UIEngine&) = delete;
	UIEngine& operator=(const UIEngine&) = delete;
	UIEngine(UIEngine&&) = delete;
	UIEngine& operator=(UIEngine&&) = delete;

	// Global Access point to the UIEngine instance
	static UIEngine& getInstance() {
		static UIEngine instance; // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}


	enum Type
	{
		NONE,
		TITLE,
		BUTTON,
		PANEL,
	};

	struct Widget
	{
		// Widget Info
		std::string text = {};
		int type = 0;
		int id = 0;
		
		// State
		bool isHovered = false;
		bool isBeingClicked = false;
		bool isReleased = false;

		// Screen Data
		Rectangle anchorRect = {};
		Rectangle pixelRect = {};

		// Style
		Color color1 = { 255, 255, 255, 255 };
		Color color2 = { 100, 100, 100, 255 };
		Color color3 = { 10, 10, 10, 255 };
	};

	struct Text : Widget
	{
		std::string text = {};
		int textSize = 20;
	};

	struct Button : Widget {
		std::string text = {};
	};

	struct Panel : Widget {
		std::string text = {};
	};

	/** Get Widget Info **/
	std::vector<Widget> widgets;
	int widgetId = 0;
	int getID(){return widgetId++;}

	/** Update and Render */
	void update();
	void render();

	/** Menus **/
	bool isSettingsEnabled = false;
private:
	UIEngine() = default;
};

inline Vector2 GetScreenSize() { return Vector2(GetScreenWidth(), GetScreenHeight()); }
void drawTextScaled(std::string text, Rectangle scaleRect, float yOffset = 0);
static void drawTextPixels(const std::string& text, Rectangle pixelRect, float yOffset = 0);


bool drawButton(std::string text, Rectangle pixelRect, Rectangle scaleRect, UIEngine &ui);
void drawTitle(std::string text, UIEngine &ui);
void drawPanel(std::string text, Rectangle pixelRect, Rectangle scaleRect, UIEngine& ui);

#endif
