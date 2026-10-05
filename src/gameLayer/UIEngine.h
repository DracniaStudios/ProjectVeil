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
		Color defaultColor = { 255, 255, 255, 255 };
	};

	struct Text : Widget
	{
		std::string text = {};

		int textWidth = 0;
		int textHeight = 0;
		int fontSize = 20;

	};

	struct Button : Widget {
		std::string text = {};

		Color highlightColor = { 100, 100, 100, 255 };
		Color clickedColor = { 10, 10, 10, 255 };
		Color releasedColor = { 50, 60, 60, 255 };
	};

	struct Panel : Widget {
		std::string name;
		std::vector<Widget> children{};
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

		/** UI Helper Functions**/

	// Draws centred text inside a rectangle given in pixels.
		void drawText(const std::string& text, Rectangle pixelRect, Rectangle scaleRect, float yOffset = 0);

		// Draws centered text, pixel rectangle, and scale rectangle. Returns true if the button was clicked.
		void drawText(const std::string& text, Rectangle pixelRect, float yOffset = 0) {
			drawText(text, pixelRect, { 0, 0, 1, 1 }, yOffset);
		};


		// Draw a button with text, pixel rectangle, and scale rectangle. Returns true if the button was clicked.
		bool drawButton(const std::string& text, Rectangle pixelRect, Rectangle scaleRect,
			Color color1 = { 100, 100, 100, 255 }, Color color2 = { 10, 10, 10, 255 }, Color color3 = { 50, 60, 60, 255 });

		// Draw a button with text and pixel rectangle. Returns true if the button was clicked.
		bool drawButton(const std::string& text, Rectangle pixelRect,
			Color color1 = { 255, 255, 255, 255 }, Color color2 = { 100, 100, 100, 255 }, Color color3 = { 50, 60, 60, 255 }) {
			return drawButton(text, pixelRect, { 0, 0, 1, 1 },  color1, color2, color3);
		};

		// Draw a panel with a pixel rectangle and scale rectangle.
		void drawPanel(const std::string& text, Rectangle pixelRect, Rectangle scaleRect);
		// Draw a panel with a pixel rectangle.
		void drawPanel(const std::string& text, Rectangle pixelRect) {
			drawPanel(text, pixelRect, { 0, 0, 1, 1 });
		};


	private:
		UIEngine() = default;
	};

	//void drawSettings();

}

inline Vector2 GetScreenSize() { return Vector2(GetScreenWidth(), GetScreenHeight()); }

#endif
