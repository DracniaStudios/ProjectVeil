#pragma once
#ifndef UI_H
#define UI_H

#include <raylib.h>
#include <algorithm>
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

		int textWidth = 0;
		int textHeight = 0;
		int textSize = 20;

	};

	struct Button : Widget {
		Button() { type = WIDGET_BUTTON; }

		std::string text = {};
		int textSize = 20;
		int textWidth = 0;
		int textHeight = 0;

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
	 * One frame's input, read before any widget is drawn. The game reads it
	 * from raylib (ReadRaylibInput); a test writes its own.
	 */
	struct Input
	{
		Vector2 screenSize = {};
		Vector2 mouse = {};
		bool mouseDown = false;     // Left button held
		bool mouseReleased = false; // Left button let go this frame
		bool mouseBlocked = false;  // Something over the UI has the mouse: the World Editor's windows
	};

	// One thing to draw, recorded by a widget and replayed by the Backend at endFrame
	struct DrawCommand
	{
		enum class Kind { Rect, Outline, Text };

		Kind kind = Kind::Rect;
		Color color = {};
		Rectangle rect = {};    // Rect and Outline, in screen pixels
		float thickness = 1.0f; // Outline
		std::string text = {};  // Text
		Vector2 position = {};  // Text: its top-left corner
		int fontSize = 0;       // Text
	};

	// How the UI reaches the screen: raylib in the game, a stand-in in tests
	class Backend
	{
	public:
		virtual ~Backend() = default;

		virtual int TextWidth(const std::string& text, int fontSize) const = 0;
		virtual void Draw(const std::vector<DrawCommand>& commands) = 0;
	};

	// Everything one frame drew, kept whole once the frame ends
	struct Frame
	{
		std::vector<WidgetEntry> widgets;  // In draw order; a widget's id is its index here
		std::vector<DrawCommand> commands; // What reached the screen, in order
		Vector2 screenSize = {};
		float scale = 1.0f;
		int hoveredId = -1; // The widget the mouse was over as the frame began; -1 for none
	};

	class UIEngine;

	/**
	 * Frame
	 *
	 * Every widget is drawn between beginFrame and endFrame, into the engine
	 * whose frame is open; one drawn outside a frame draws nothing. Widgets
	 * reach the screen at endFrame, so anything drawn straight to raylib in
	 * between lands under all of them.
	 *
	 * Hover is decided at beginFrame against last frame's widgets, since this
	 * frame's aren't drawn yet: the last widget drawn there under the mouse is
	 * the one on top, and only it is hovered. Text never takes the mouse.
	 */
	void beginFrame(UIEngine& ui, const Input& input);
	void endFrame();

	// Opens a Panel. Widgets drawn until the matching endPanel() are placed inside
	// it: their rects are relative to it, and the no-rect overloads stack in rows.
	void beginPanel(const std::string& name, Rectangle pixelRect, Rectangle scaleRect = {});
	void endPanel();

	// Creates a UI Button. Returns true on the frame the mouse is released over
	// it, once per click, never while it is held.
	bool drawButton(const std::string& text, Rectangle pixelRect, Rectangle scaleRect = {});
	bool drawButton(const std::string& text); // In the open panel's next row

	// Creates Text for UI Elements, centred in its rect
	void drawText(const std::string& text, Rectangle pixelRect, Rectangle scaleRect = {});
	void drawText(const std::string& text); // In the open panel's next row

	/**
	 * Immediate-mode UI: what persists between frames, and the frame being drawn.
	 *
	 * Only the look is meant to be changed from outside, by code or the UI
	 * Inspector. Widgets are drawn through the free functions above, and the
	 * last frame is read back through LastFrame().
	 */
	class UIEngine
	{
	public:
		// Draws every frame through backend
		explicit UIEngine(Backend& backend) : backend(backend) {}

		// Delete, copy, and move functions to prevent duplication
		UIEngine(const UIEngine&) = delete;
		UIEngine& operator=(const UIEngine&) = delete;
		UIEngine(UIEngine&&) = delete;
		UIEngine& operator=(UIEngine&&) = delete;

		// The game's UI, drawn through raylib
		static UIEngine& getInstance();

		/** Look **/
		Style style = {};
		Vector2 referenceResolution = { 1920.0f, 1080.0f }; // Resolution pixelRects are authored at

		/** Debug (UI Inspector) **/
		bool showBounds = false; // Outline every widget
		// Outlines one widget at the end of the next frame only, so the inspector
		// asks again each frame it wants the outline kept
		void Highlight(int widgetId) { highlightedId = widgetId; }

		// The last frame drawn, whole. Read-only: the next frame rebuilds it.
		const Frame& LastFrame() const { return lastFrame; }

	private:
		friend void beginFrame(UIEngine& ui, const Input& input);
		friend void endFrame();
		friend void beginPanel(const std::string& name, Rectangle pixelRect, Rectangle scaleRect);
		friend void endPanel();
		friend bool drawButton(const std::string& text, Rectangle pixelRect, Rectangle scaleRect);
		friend bool drawButton(const std::string& text);
		friend void drawText(const std::string& text, Rectangle pixelRect, Rectangle scaleRect);
		friend void drawText(const std::string& text);

		// Where the next widget goes. One per open panel, with the screen at the bottom.
		struct Layout
		{
			int panelId = -1;            // -1 for the screen
			Rectangle contentRect = {};
			int rowCount = 0;
		};

		// Fill in the fields every widget shares and decide where it goes: inside
		// the open panel at its own rects, or in that panel's next row.
		void placeWidget(Widget& widget, Rectangle pixelRect, Rectangle scaleRect);
		void placeNextRow(Widget& widget);

		// Whether the mouse is over this widget: it was on top under the mouse
		// last frame, and the mouse is inside where it is now
		bool hovers(const Widget& widget) const;

		int fitTextSize(const std::string& text, Rectangle rect) const;
		void drawRect(Rectangle rect, Color color);
		void drawLabel(const std::string& text, Rectangle rect, int fontSize, float yOffset);
		bool finishButton(Button& button, const std::string& text);
		void finishText(Text& widget, const std::string& text);

		Backend& backend;
		Input input = {};
		Frame frame = {};     // Being drawn
		Frame lastFrame = {};
		std::vector<Layout> layoutStack;
		int highlightedId = -1;
	};

	/** raylib **/
	class RaylibBackend final : public Backend
	{
	public:
		int TextWidth(const std::string& text, int fontSize) const override;
		void Draw(const std::vector<DrawCommand>& commands) override;
	};

	// The mouse and screen as raylib has them this frame
	Input ReadRaylibInput(bool mouseBlocked);
}

#endif
