/**
 * The default font's text widths (work order B-18).
 *
 * The GL backend draws text with a port of raylib's default bitmap font and must
 * measure it exactly as raylib's MeasureText does, or the UI would lay out
 * differently. Every width below was recorded from raylib's own MeasureText, run
 * in the raylib build before the GL port was written.
 *
 * Two builds of this file, chosen by a macro:
 *  - by default, an engine-linked suite in the raylib tree. It opens the test
 *    window and checks Render::TextWidth against the recorded widths. See
 *    tests/run_tests.sh.
 *  - with -DVEIL_DEFAULT_FONT_STANDALONE, a windowless program that includes the
 *    GL backend's DefaultFont.h and is compiled together with
 *    src/engineLayer/Render/backend_gl/DefaultFont.cpp and nothing else. It checks
 *    the GL backend's measuring routine against the same widths.
 *    cl /std:c++latest /EHsc /DVEIL_DEFAULT_FONT_STANDALONE /I src\engineLayer\Render\backend_gl
 *       tests\DefaultFontTests.cpp src\engineLayer\Render\backend_gl\DefaultFont.cpp
 */

#include <cstdio>
#include <string>
#include <vector>

#ifdef VEIL_DEFAULT_FONT_STANDALONE
#include "DefaultFont.h"
#else
#include <Render/Render.h>
#include "TestWindow.h"
#endif

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

static int Measure(const std::string& text, int fontSize)
{
#ifdef VEIL_DEFAULT_FONT_STANDALONE
	return DefaultFont::TextWidth(text.c_str(), fontSize);
#else
	return Render::TextWidth(text, fontSize);
#endif
}

struct Case
{
	const char* what;
	std::string text;
	int fontSize;
	int width; // recorded from raylib's MeasureText
};

// Widths recorded from raylib 6.0's MeasureText (the raylib build's Render::TextWidth calls it)
static const std::vector<Case> kCases = {
	{ "empty string", "", 10, 0 },
	{ "single narrow glyph", "i", 10, 1 },
	{ "single wide glyph", "W", 10, 7 },
	{ "single glyph, size 20", "m", 20, 10 },
	{ "lower case", "hello world", 10, 54 },
	{ "mixed case", "HeLLo WoRLd", 10, 67 },
	{ "digits", "0123456789", 10, 56 },
	{ "punctuation", "Hello, World! (1+2=3)?", 10, 107 },
	{ "only spaces", "   ", 10, 11 },
	{ "FPS counter text", "60 FPS", 20, 72 },
	{ "below 10, size 0", "Main Menu", 0, 51 },
	{ "below 10, size 5", "Main Menu", 5, 51 },
	{ "below 10, size 9", "Main Menu", 9, 51 },
	{ "size 10", "Main Menu", 10, 51 },
	{ "size 20", "Main Menu", 20, 102 },
	{ "size 30", "Main Menu", 30, 153 },
	{ "large size", "Project Veil", 64, 386 },
	{ "a newline: the wider line counts", "Line one\nLine two is longer", 10, 89 },
	{ "a trailing newline", "ab\ncd\n", 20, 22 },
	{ "a newline alone (raylib returns -1: its spacing term is (0 - 1) * 1)", "\n", 10, -1 },
	{ "a control character is drawn as ?", "Tab\there", 10, 50 },
	{ "a code point outside the font (U+20AC)", "\xE2\x82\xAC" "uro", 20, 48 },
	{ "a 3-byte code point outside the font (U+4E2D)", "a\xE4\xB8\xAD" "b", 10, 18 },
	{ "a 4-byte code point (U+1F600)", "\xF0\x9F\x98\x80!", 10, 8 },
	{ "a UTF-8 multi-byte character in the font (U+00E9)", "caf\xC3\xA9", 20, 44 },
	{ "a Latin-1 character as 2 bytes (U+00FF)", "\xC3\xBF" "y", 30, 33 },
	{ "a malformed sequence", "\x80" "x", 10, 12 },
	{ "a truncated multi-byte sequence", "ab\xC3", 10, 18 },
};

static void TestWidthsMatchRaylib()
{
	for (const Case& c : kCases)
	{
		const int got = Measure(c.text, c.fontSize);
		Check(got == c.width, std::string(c.what) + " at size " + std::to_string(c.fontSize)
			+ ": width " + std::to_string(got) + ", raylib says " + std::to_string(c.width));
	}
}

int main()
{
#ifndef VEIL_DEFAULT_FONT_STANDALONE
	OpenTestWindow("veil-default-font-tests", false, true);
#endif

	std::printf("Default font tests\n\n");

	TestWidthsMatchRaylib();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

#ifndef VEIL_DEFAULT_FONT_STANDALONE
	CloseTestWindow();
#endif
	return g_failures == 0 ? 0 : 1;
}
