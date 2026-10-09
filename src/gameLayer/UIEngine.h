#pragma once
#ifndef UI_H
#define UI_H

#include <raylib.h>
#include <string>
#include <variant>
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

	/**
	 * Scaling
	 *
	 * Every widget is placed by two rectangles, both relative to its parent (the
	 * open panel, or the screen when none is open).
	 */
	Rectangle resolveRect(Rectangle parentRect, Rectangle pixelRect, Rectangle scaleRect, float scale);

	// Ratio of the screen to the reference resolution. Uses the smaller axis, so
	// a layout authored at the reference never overflows a narrower window.
	float computeScale(Vector2 screenSize, Vector2 referenceResolution);

	enum WidgetType
	{
		WIDGET_NONE,
		WIDGET_TEXT,
		WIDGET_BUTTON,
		WIDGET_PANEL,
	};

	inline const char* widgetTypeToString(int type)
	{
		switch (type)
		{
		case WIDGET_TEXT:   return "Text";
		case WIDGET_BUTTON: return "Button";
		case WIDGET_PANEL:  return "Panel";
		default:            return "None";
		}
	}

	/**
	 * The look shared by every widget.
	 *
	 * Lives on the UIEngine rather than on each call so one edit, in code or in the
	 * UI Inspector, restyles the whole UI. Lengths are reference pixels.
	 */
	struct Style
	{
		// Text
		Color textColor = WHITE;
		Color textShadowColor = { 0, 0, 0, 200 };
		float textHeightRatio = 0.5f; // Font size as a fraction of the widget's height

		// Button
		Color buttonColor = { 90, 90, 110, 205 };
		Color buttonHighlightedColor = { 120, 120, 134, 205 };
		Color buttonClickColor = { 60, 60, 75, 230 };
		float buttonClickOffset = 0.05f; // Label drop while held, as a fraction of the button height

		// Panel
		Color panelColor = { 20, 20, 25, 200 };
		float panelPadding = 16.0f; // Between a panel's edge and its children

		// Rows (auto-placed widgets)
		float rowHeight = 60.0f;
		float rowSpacing = 10.0f;
	};

	struct Widget
	{
		// Widget Info
		int type = WIDGET_NONE;
		int id = 0;        // Draw order this frame; a higher id is drawn on top
		int parentId = -1; // Panel it was placed in; -1 is the screen
		int depth = 0;     // Panels open around it

		// State
		bool isVisible = true; // False when it lands off-screen or has no area
		bool isHovered = false;
		bool isBeingClicked = false;
		bool isReleased = false;

		// Screen Data
		Rectangle anchorRect = {}; // The anchor points on the screen
		Rectangle pixelRect = {}; // Scale in pixel size
		Rectangle screenRect = {}; // Where it ended up, in screen pixels
		bool isAutoPlaced = false; // Took its parent's next row instead of using its own rects

		// Style
		Color defaultColor = { 255, 255, 255, 255 }; // Background, or the glyph colour for Text
	};

	struct Text : Widget
	{
		Text() { type = WIDGET_TEXT; }

		std::string text = {};
		int textSize = 20;
	};

	struct Button : Widget {
		Button() { type = WIDGET_BUTTON; }

		std::string text = {};
		int textSize = 20;
		Color clickColor = { 100, 100, 100, 255 };
		Color highlightedColor = { 10, 10, 10, 255 };
	};

	struct Panel : Widget {
		Panel() { type = WIDGET_PANEL; }

		std::string name = {};      // Identifies the panel in the UI Inspector; not drawn
		Rectangle contentRect = {}; // Inside the padding; children are placed relative to this
		int rowCount = 0;           // Rows handed out to auto-placed children
	};

	// One drawn widget. A variant rather than std::vector<Widget> so a Button keeps
	// its text and colours instead of being sliced down to the base fields.
	using WidgetEntry = std::variant<Text, Button, Panel>;

	// The fields every widget shares, whichever kind the entry holds
	inline const Widget& getWidget(const WidgetEntry& entry)
	{
		return std::visit([](const auto& widget) -> const Widget& { return widget; }, entry);
	}

	inline Widget& getWidget(WidgetEntry& entry)
	{
		return std::visit([](auto& widget) -> Widget& { return widget; }, entry);
	}

	/**
	 * Immediate-mode UI.
	 */
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

		/** Widgets **/
		// Everything drawn this frame, in draw order; an id is its index here.
		std::vector<WidgetEntry> widgets;

		/** Style & Scaling **/
		Style style = {};
		Vector2 referenceResolution = { 1920.0f, 1080.0f }; // Resolution pixelRects are authored at
		float getScale() const { return scale; }

		/** Frame **/
		void beginFrame(); // Before the first widget of the frame
		void endFrame();   // After the last
		void render();     // Engine-owned menus (Settings)

		/** Layout **/
		// Fill in the fields every widget shares and decide where it goes: inside
		// the open panel at its own rects, or in that panel's next row.
		void placeWidget(Widget& widget, Rectangle pixelRect, Rectangle scaleRect);
		void placeNextRow(Widget& widget);

		void pushPanel(Panel panel);
		void popPanel();

		/** Input **/
		int hoveredId = -1;
		bool isMouseBlocked = false; // Set by the World Editor while ImGui owns the mouse

		/** Debug (UI Inspector) **/
		bool showBounds = false; // Outline every widget
		int highlightedId = -1;  // Outlined for one frame; the inspector sets it on hover

		/** Menus **/
		bool isSettingsEnabled = false;
	private:
		UIEngine() = default;

		// Where the next widget goes. One per open panel, with the screen at the bottom.
		struct Layout
		{
			int panelId = -1;            // -1 for the screen
			Rectangle contentRect = {};
			int rowCount = 0;
		};

		// Starts with a root so a widget drawn before the first beginFrame() gets a
		// zero-sized parent instead of reading back() off an empty vector
		std::vector<Layout> layoutStack = { Layout{} };
		float scale = 1.0f;
	};

	// Opens a Panel. Widgets drawn until the matching endPanel() are placed inside
	// it: their rects are relative to it, and the no-rect overloads stack in rows.
	void beginPanel(const std::string& name, Rectangle pixelRect, Rectangle scaleRect = {});
	void endPanel();

	// Creates a UI Button. Returns true on the frame the mouse is released over it.
	bool drawButton(const std::string& text, Rectangle pixelRect, Rectangle scaleRect = {});
	bool drawButton(const std::string& text); // In the open panel's next row

	// Creates Text for UI Elements, centred in its rect
	void drawText(const std::string& text, Rectangle pixelRect, Rectangle scaleRect = {});
	void drawText(const std::string& text); // In the open panel's next row
}

inline Vector2 GetScreenSize() { return Vector2(GetScreenWidth(), GetScreenHeight()); }
#endif
