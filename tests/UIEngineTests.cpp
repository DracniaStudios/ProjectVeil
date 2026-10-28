/**
 * Standalone tests for the UIEngine.
 *
 * Each frame is handed its input (mouse, buttons, screen size) and every widget
 * records what it draws, which a Backend replays at endFrame. These tests write
 * the input themselves and keep what a stand-in Backend is given, so hover,
 * clicks, layout and drawing are all checked without a window. UIEngine.cpp
 * calls no raylib function, so this links no library at all: raylib's header
 * is included for its Rectangle, Vector2 and Color types only.
 *
 * If UIEngine.cpp ever calls into raylib again, its rules have leaked out of the
 * seam and this file stops linking.
 *
 * Build and run: tests/run_tests.sh
 */

#include <UIEngine.h>

#include <cmath>
#include <cstdio>
#include <string>

static int g_failures = 0;
static int g_checks = 0;

static void Check(bool condition, const std::string& what)
{
	++g_checks;
	if (!condition)
	{
		++g_failures;
		std::printf("  FAIL  %s\n", what.c_str());
	}
}

static bool Near(float a, float b)
{
	return std::fabs(a - b) < 0.01f;
}

static bool SameRect(Rectangle a, Rectangle b)
{
	return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.width, b.width) && Near(a.height, b.height);
}

static bool SameColor(Color a, Color b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// Keeps what it is asked to draw. Every character is half its font size wide.
class RecordingBackend final : public UI::Backend
{
public:
	int TextWidth(const std::string& text, int fontSize) const override { return static_cast<int>(text.size()) * fontSize / 2; }
	void Draw(const std::vector<UI::DrawCommand>& commands) override { drawn = commands; ++frames; }

	std::vector<UI::DrawCommand> drawn;
	int frames = 0;
};

// The mouse resting at (x, y) on a 1920x1080 screen, the reference resolution
static UI::Input MouseAt(float x, float y)
{
	UI::Input input;
	input.screenSize = Vector2{ 1920.0f, 1080.0f };
	input.mouse = Vector2{ x, y };
	return input;
}

static UI::Input Held(float x, float y)
{
	UI::Input input = MouseAt(x, y);
	input.mouseDown = true;
	return input;
}

static UI::Input Released(float x, float y)
{
	UI::Input input = MouseAt(x, y);
	input.mouseReleased = true;
	return input;
}

static const Rectangle OK_RECT = { 100.0f, 100.0f, 200.0f, 60.0f };

// One frame holding one button; whether it fired
static bool ButtonFrame(UI::UIEngine& ui, const UI::Input& input, Rectangle rect = OK_RECT)
{
	UI::beginFrame(ui, input);
	const bool fired = UI::drawButton("OK", rect);
	UI::endFrame();
	return fired;
}

static const UI::Button* ButtonAt(const UI::Frame& frame, int id)
{
	if (id < 0 || id >= static_cast<int>(frame.widgets.size())) { return nullptr; }
	return std::get_if<UI::Button>(&frame.widgets[id]);
}

static void TestAButtonFiresOnceOnRelease()
{
	std::printf("a button fires once, on the frame the mouse is released over it (regression: it fired every frame it was held)\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);

	Check(!ButtonFrame(ui, MouseAt(150, 130)), "resting over it does not fire");
	Check(!ButtonFrame(ui, Held(150, 130)), "pressing it does not fire");
	Check(ButtonAt(ui.LastFrame(), 0) && ButtonAt(ui.LastFrame(), 0)->isBeingClicked, "it shows as being clicked while held");
	Check(!ButtonFrame(ui, Held(150, 130)), "holding it a second frame does not fire");
	Check(!ButtonFrame(ui, Held(150, 130)), "holding it a third frame does not fire");
	Check(ButtonFrame(ui, Released(150, 130)), "letting go over it fires");
	Check(!ButtonFrame(ui, MouseAt(150, 130)), "the frame after does not fire again");
}

static void TestHoverNeedsALastFrame()
{
	std::printf("a new button is hovered from its second frame, since hover is decided against last frame\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);

	Check(!ButtonFrame(ui, Released(150, 130)), "a button released over on its first frame does not fire");
	Check(ui.LastFrame().hoveredId == -1, "nothing was under the mouse before it existed");

	ButtonFrame(ui, MouseAt(150, 130));
	Check(ui.LastFrame().hoveredId == 0, "the next frame finds it under the mouse");
	Check(ButtonAt(ui.LastFrame(), 0) && ButtonAt(ui.LastFrame(), 0)->isHovered, "and it is hovered");
}

static void TestTheTopmostButtonTakesTheClick()
{
	std::printf("of two overlapping buttons, only the one drawn on top fires\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);
	bool underFired = false;
	bool topFired = false;

	for (const UI::Input& input : { MouseAt(175, 125), Released(175, 125) })
	{
		UI::beginFrame(ui, input);
		underFired = UI::drawButton("Under", Rectangle{ 100, 100, 200, 60 });
		topFired = UI::drawButton("Top", Rectangle{ 150, 110, 200, 60 });
		UI::endFrame();
	}

	Check(topFired, "the button drawn last fires");
	Check(!underFired, "the one beneath it does not");
	Check(ui.LastFrame().hoveredId == 1, "the one on top is the one under the mouse");
}

static void TestTextNeverBlocksAButton()
{
	std::printf("text drawn over a button never keeps it from being clicked\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);
	bool fired = false;

	for (const UI::Input& input : { MouseAt(150, 130), Released(150, 130) })
	{
		UI::beginFrame(ui, input);
		fired = UI::drawButton("Button", OK_RECT);
		UI::drawText("A label over it", OK_RECT);
		UI::endFrame();
	}

	Check(fired, "the button under the text fires");
}

static void TestAPanelCoversWhatIsBeneathIt()
{
	std::printf("a panel takes the mouse from what is drawn beneath it, and gives it to what is drawn inside it\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);
	bool beneathFired = false;
	bool insideFired = false;

	auto frame = [&](const UI::Input& input)
	{
		UI::beginFrame(ui, input);
		beneathFired = UI::drawButton("Beneath", OK_RECT);
		UI::beginPanel("Cover", Rectangle{ 50, 50, 400, 300 });
		insideFired = UI::drawButton("Inside", Rectangle{ 0, 0, 200, 60 }); // At (66, 66), inside the padding
		UI::endPanel();
		UI::endFrame();
	};

	frame(MouseAt(150, 130));
	frame(Released(150, 130));
	Check(!beneathFired, "a button under the panel does not fire");
	Check(ui.LastFrame().hoveredId == 1, "the panel is what the mouse is over");

	frame(MouseAt(100, 90));
	frame(Released(100, 90));
	Check(insideFired, "a button inside the panel does");
}

static void TestAMovedButtonIsNotHoveredWhereItWas()
{
	std::printf("a button that moved since last frame is not hovered where it used to be\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);
	const Rectangle moved = { 600, 100, 200, 60 };

	ButtonFrame(ui, MouseAt(150, 130));
	Check(!ButtonFrame(ui, Released(150, 130), moved), "released where it was, it does not fire from its new place");
	Check(ButtonFrame(ui, Released(650, 130), moved), "released where it is now, it fires");
}

static void TestABlockedMouseReachesNothing()
{
	std::printf("while something over the UI has the mouse, no widget is hovered or fires\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);

	ButtonFrame(ui, MouseAt(150, 130));

	UI::Input blocked = Released(150, 130);
	blocked.mouseBlocked = true;
	Check(!ButtonFrame(ui, blocked), "a release over the button does not fire");
	Check(ui.LastFrame().hoveredId == -1, "nothing is under the mouse");
	Check(ButtonAt(ui.LastFrame(), 0) && !ButtonAt(ui.LastFrame(), 0)->isHovered, "the button is not hovered");
}

static void TestAnOverflowingRowIsNeitherDrawnNorHovered()
{
	std::printf("a row past the bottom of its panel is not drawn, hovered or fired\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);
	bool lastRowFired = false;

	// Content is 16..184 high: rows start at 16, 86 and 156, and the fourth at 226 is past it
	auto frame = [&](const UI::Input& input)
	{
		UI::beginFrame(ui, input);
		UI::beginPanel("Short", Rectangle{ 0, 0, 400, 200 });
		UI::drawButton("One");
		UI::drawButton("Two");
		UI::drawButton("Three");
		lastRowFired = UI::drawButton("Four");
		UI::endPanel();
		UI::endFrame();
	};

	frame(MouseAt(100, 250));
	frame(Released(100, 250));

	const UI::Button* four = ButtonAt(ui.LastFrame(), 4);
	Check(four && !four->isVisible, "the fourth row is not visible");
	Check(!lastRowFired, "it does not fire");
	Check(ui.LastFrame().hoveredId == -1, "and is not under the mouse");
	Check(backend.drawn.size() == 1 + 3 * 3, "only the panel and the three rows that fit are drawn");
}

static void TestRowsStackInsideTheirPanel()
{
	std::printf("rows stack down the panel's padded content, scaled with the screen\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);

	auto frame = [&](Vector2 screen)
	{
		UI::Input input = MouseAt(0, 0);
		input.screenSize = screen;
		UI::beginFrame(ui, input);
		UI::beginPanel("Settings", Rectangle{}, Rectangle{ 0.1f, 0.1f, 0.8f, 0.8f });
		UI::drawText("Settings");
		UI::drawButton("Return");
		UI::endPanel();
		UI::endFrame();
	};

	frame(Vector2{ 1920, 1080 });
	const UI::Frame& full = ui.LastFrame();
	Check(full.widgets.size() == 3, "a panel and two rows are drawn");
	Check(SameRect(UI::getWidget(full.widgets[0]).screenRect, Rectangle{ 192, 108, 1536, 864 }), "the panel covers its share of the screen");
	Check(SameRect(std::get<UI::Panel>(full.widgets[0]).contentRect, Rectangle{ 208, 124, 1504, 832 }), "its content is inset by its padding");
	Check(SameRect(UI::getWidget(full.widgets[1]).screenRect, Rectangle{ 208, 124, 1504, 60 }), "the first row sits at the top of the content");
	Check(SameRect(UI::getWidget(full.widgets[2]).screenRect, Rectangle{ 208, 194, 1504, 60 }), "the second sits one row and one gap below it");
	Check(UI::getWidget(full.widgets[2]).parentId == 0 && UI::getWidget(full.widgets[2]).depth == 1, "both rows belong to the panel");
	Check(UI::getWidget(full.widgets[2]).isAutoPlaced, "and say they were placed in rows");
	Check(std::get<UI::Panel>(full.widgets[0]).rowCount == 2, "the panel counts two rows");

	frame(Vector2{ 960, 540 });
	const UI::Frame& half = ui.LastFrame();
	Check(Near(half.scale, 0.5f), "half the reference resolution draws at half scale");
	Check(SameRect(UI::getWidget(half.widgets[1]).screenRect, Rectangle{ 104, 62, 752, 30 }), "with half the padding and half-height rows");
	Check(SameRect(UI::getWidget(half.widgets[2]).screenRect, Rectangle{ 104, 97, 752, 30 }), "and half the gap between them");
}

static void TestScaleFollowsTheNarrowerAxis()
{
	std::printf("the UI scales by whichever screen axis is smaller against the reference\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);

	UI::Input wide = MouseAt(0, 0);
	wide.screenSize = Vector2{ 1920, 540 };
	UI::beginFrame(ui, wide);
	UI::endFrame();
	Check(Near(ui.LastFrame().scale, 0.5f), "a screen half as tall draws at half scale, however wide");

	UI::Input big = MouseAt(0, 0);
	big.screenSize = Vector2{ 3840, 2160 };
	UI::beginFrame(ui, big);
	UI::endFrame();
	Check(Near(ui.LastFrame().scale, 2.0f), "a 4K screen draws at twice the scale");
}

static void TestAButtonDrawsAsItsStateSays()
{
	std::printf("a panel draws before its children, and a button's colour and label follow its state\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);
	const UI::Style& style = ui.style;

	auto frame = [&](const UI::Input& input)
	{
		UI::beginFrame(ui, input);
		UI::beginPanel("Menu", Rectangle{ 0, 0, 400, 200 });
		UI::drawButton("Go");
		UI::endPanel();
		UI::endFrame();
	};

	frame(MouseAt(1000, 1000));
	Check(backend.drawn.size() == 4, "a panel and a button are four commands: panel, button, shadow, label");
	if (backend.drawn.size() != 4) { return; }

	Check(backend.drawn[0].kind == UI::DrawCommand::Kind::Rect && SameColor(backend.drawn[0].color, style.panelColor), "the panel is drawn first");
	Check(backend.drawn[1].kind == UI::DrawCommand::Kind::Rect && SameColor(backend.drawn[1].color, style.buttonColor), "then the button, in its resting colour");
	Check(backend.drawn[2].kind == UI::DrawCommand::Kind::Text && SameColor(backend.drawn[2].color, style.textShadowColor), "then the label's shadow");
	Check(backend.drawn[3].kind == UI::DrawCommand::Kind::Text && SameColor(backend.drawn[3].color, style.textColor) && backend.drawn[3].text == "Go", "then the label");
	const float restingY = backend.drawn[3].position.y;

	frame(MouseAt(100, 40));
	Check(SameColor(backend.drawn[1].color, style.buttonHighlightedColor), "hovered, the button is drawn highlighted");

	frame(Held(100, 40));
	Check(SameColor(backend.drawn[1].color, style.buttonClickColor), "held, it is drawn in its click colour");
	Check(Near(backend.drawn[3].position.y - restingY, 60 * style.buttonClickOffset), "and its label drops by the click offset");
}

static void TestALongLabelShrinksToFit()
{
	std::printf("a label too long for its button shrinks to fit\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);

	UI::beginFrame(ui, MouseAt(0, 0));
	UI::drawButton("Go", Rectangle{ 0, 0, 100, 60 });
	UI::drawButton("A long label", Rectangle{ 0, 100, 100, 60 });
	UI::endFrame();

	const UI::Button* shortLabel = ButtonAt(ui.LastFrame(), 0);
	const UI::Button* longLabel = ButtonAt(ui.LastFrame(), 1);
	Check(shortLabel && shortLabel->textSize == 30, "a short label fills half the button's height");
	Check(longLabel && longLabel->textSize == 15, "a long one shrinks until it is no wider than 90% of the button");
	Check(backend.drawn.size() == 6 && backend.drawn[5].fontSize == 15, "and is drawn at that size");
}

static void TestLastFrameIsTheWholeOfTheFrameBefore()
{
	std::printf("LastFrame is the whole of the last finished frame, even while the next is drawn\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);

	UI::beginFrame(ui, MouseAt(0, 0));
	UI::drawText("One");
	UI::drawButton("Two");
	UI::endFrame();

	Check(ui.LastFrame().widgets.size() == 2, "both widgets are kept");
	Check(ui.LastFrame().commands.size() == backend.drawn.size() && backend.frames == 1, "with exactly what the backend was given");

	UI::beginFrame(ui, MouseAt(0, 0));
	UI::drawButton("Only");
	Check(ui.LastFrame().widgets.size() == 2, "a frame being drawn does not change it");
	UI::endFrame();
	Check(ui.LastFrame().widgets.size() == 1, "the next finished frame replaces it");
}

static void TestHighlightLastsOneFrame()
{
	std::printf("a highlighted widget is outlined for one frame; Show Bounds outlines them all\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);

	auto frame = [&]
	{
		UI::beginFrame(ui, MouseAt(0, 0));
		UI::drawButton("One", OK_RECT);
		UI::drawButton("Two", Rectangle{ 400, 100, 200, 60 });
		UI::endFrame();
	};

	auto outlines = [&]
	{
		int count = 0;
		for (const UI::DrawCommand& command : backend.drawn) { if (command.kind == UI::DrawCommand::Kind::Outline) { ++count; } }
		return count;
	};

	ui.Highlight(1);
	frame();
	Check(outlines() == 1, "one outline is drawn");
	Check(backend.drawn.back().kind == UI::DrawCommand::Kind::Outline && SameRect(backend.drawn.back().rect, Rectangle{ 400, 100, 200, 60 }),
		"over the highlighted widget, after everything else");

	frame();
	Check(outlines() == 0, "the frame after, it is gone");

	ui.showBounds = true;
	frame();
	Check(outlines() == 2, "Show Bounds outlines every widget");
}

static void TestUnbalancedPanelsRecover()
{
	std::printf("a stray endPanel is ignored, and a missing one is forgotten by the next frame\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);

	UI::beginFrame(ui, MouseAt(0, 0));
	UI::endPanel();
	UI::drawButton("After", OK_RECT);
	UI::endFrame();
	Check(ui.LastFrame().widgets.size() == 1 && UI::getWidget(ui.LastFrame().widgets[0]).parentId == -1,
		"a widget after a stray endPanel sits on the screen");

	UI::beginFrame(ui, MouseAt(0, 0));
	UI::beginPanel("Left Open", Rectangle{ 0, 0, 400, 200 });
	UI::endFrame();

	UI::beginFrame(ui, MouseAt(0, 0));
	UI::drawButton("Next Frame", OK_RECT);
	UI::endFrame();
	Check(UI::getWidget(ui.LastFrame().widgets[0]).parentId == -1, "the next frame starts on the screen again");
}

static void TestWidgetsOutsideAFrameDrawNothing()
{
	std::printf("a widget drawn outside a frame draws nothing and never fires\n");

	RecordingBackend backend;
	UI::UIEngine ui(backend);

	Check(!UI::drawButton("Lost", OK_RECT), "a button outside a frame does not fire");
	UI::beginPanel("Lost", OK_RECT);
	UI::endPanel();
	UI::drawText("Lost");
	UI::endFrame();

	Check(backend.frames == 0, "nothing reaches the backend");
	Check(ui.LastFrame().widgets.empty(), "and nothing is recorded");
}

int main()
{
	std::printf("UIEngine tests\n\n");

	TestAButtonFiresOnceOnRelease();
	TestHoverNeedsALastFrame();
	TestTheTopmostButtonTakesTheClick();
	TestTextNeverBlocksAButton();
	TestAPanelCoversWhatIsBeneathIt();
	TestAMovedButtonIsNotHoveredWhereItWas();
	TestABlockedMouseReachesNothing();
	TestAnOverflowingRowIsNeitherDrawnNorHovered();
	TestRowsStackInsideTheirPanel();
	TestScaleFollowsTheNarrowerAxis();
	TestAButtonDrawsAsItsStateSays();
	TestALongLabelShrinksToFit();
	TestLastFrameIsTheWholeOfTheFrameBefore();
	TestHighlightLastsOneFrame();
	TestUnbalancedPanelsRecover();
	TestWidgetsOutsideAFrameDrawNothing();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
