#pragma once
// raylib's default bitmap font for the GL backend (B-18): the glyph rectangles, the
// atlas pixels and the layout and measuring routines. Pure CPU: no GL, no SDL, no raylib
// header, so tests/DefaultFontTests.cpp can compile DefaultFont.cpp on its own.
// The GL texture is made from AtlasPixels() in Gl2D.cpp.
#include <cstdint>
#include <vector>

namespace DefaultFont {

inline constexpr int kAtlasSize = 128;  // the atlas is kAtlasSize x kAtlasSize
inline constexpr int kGlyphCount = 224; // code points 32 .. 255
inline constexpr int kBaseSize = 10;    // glyph height in pixels, raylib's font.baseSize

struct GlyphRect { float x, y, w, h; }; // a glyph's rectangle in the atlas, in pixels

// 128 x 128 RGBA8, row 0 on top. A set pixel is (255,255,255,255), a clear one (255,255,255,0).
const std::vector<std::uint8_t>& AtlasPixels();

const GlyphRect& Glyph(int index);

// raylib's GetGlyphIndex for this font: an unknown code point gives the index of '?'
int GlyphIndex(int codepoint);

// raylib's GetCodepointNext: decodes one UTF-8 sequence. A malformed one gives '?' and one byte.
int NextCodepoint(const char* text, int* byteCount);

// raylib's DrawText rules: sizes under kBaseSize use kBaseSize, spacing is size / 10 as an int
int EffectiveSize(int fontSize);
int Spacing(int fontSize);

// MeasureText: the width of the longest line, truncated to whole pixels
int TextWidth(const char* text, int fontSize);

// One glyph quad of a laid-out string. The offsets are relative to the text's position:
// the caller adds them to the position as floats, as DrawTextEx does.
struct PlacedGlyph
{
	float offsetX, offsetY; // where the quad's top left lies, from the text position
	float width, height;    // the quad's size on screen
	float srcX, srcY, srcW, srcH; // the glyph's rectangle in the atlas, in pixels
};

// DrawText's layout: one entry per glyph that is drawn (not ' ', '\t' or '\n')
void Layout(const char* text, int fontSize, std::vector<PlacedGlyph>& out);

}
