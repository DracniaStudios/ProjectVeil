#pragma once
#ifndef UI_H
#define UI_H

#include <raylib.h>
#include <string>
#include <vector>

// 115

namespace UI {

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

	enum WidgetType
	{
		NONE,
		TEXT,
		BUTTON,
		PANEL,
	};

	struct Widget
	{
		// Widget Info
		int type = 0;
		int id = 0;

		// State
		bool isVisible = true;
		bool isHovered = false;
		bool isBeingClicked = false;
		bool isReleased = false;

		// Screen Data
		Rectangle anchorRect = {};
		Rectangle pixelRect = {};

		// Style
		Color defaultColor = { 255, 255, 255, 255 };
	};

	struct Text : Widget
	{
		std::string text = {};
		int textSize = 20;
	};

	struct Button : Widget {
		std::string text = {};
		int textSize = 20;
		Color clickColor = { 100, 100, 100, 255 };
		Color highlightedColor = { 10, 10, 10, 255 };
	};

	struct Panel : Widget {
		
	};

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


		

		/** Get Widget Info **/
		std::vector<Widget> widgets;
		int widgetId = 0;
		int getID() { return widgetId++; }

		/** Update and Render */
		void update();
		void render();

		/** Menus **/
		bool isSettingsEnabled = false;
	private:
		UIEngine() = default;
	};

	// Creates a UI Button
	bool drawButton(const std::string& text, Rectangle pixelRect, Rectangle scaleRect = {0, 0, 1, 1});
	
	// Creates Text for UI Elements
	static void drawText(const std::string& text, Rectangle pixelRect, Rectangle scaleRect = {0, 0, 1, 1}, float yOffset = 0);
	
	// Creates a UI Title
	static void drawTitle(const std::string& text, Rectangle pixelRect, Rectangle scaleRect = {0, 0, 1, 1}, float yOffset = 0);
	
	// Creates a UI Panel
	static void drawPanel(const std::string& text, Rectangle pixelRect, Rectangle scaleRect = {0, 0, 1, 1});
}

inline Vector2 GetScreenSize() { return Vector2(GetScreenWidth(), GetScreenHeight()); }
#endif