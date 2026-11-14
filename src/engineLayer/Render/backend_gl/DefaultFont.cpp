// raylib's default bitmap font for the GL backend (B-18). No GL call and no GL, SDL or
// raylib header: see DefaultFont.h.
//
// The bitmap data, the glyph widths and the glyph layout loop are ported from raylib 6.0:
//   LoadFontDefault, DrawText, DrawTextEx, DrawTextCodepoint, MeasureText, MeasureTextEx,
//   GetGlyphIndex and GetCodepointNext, all in src/rtext.c.
// raylib's copyright line in that file:
//   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
// Used under raylib's zlib/libpng licence, which permits use and alteration with this
// notice kept; this file is an altered version and is not raylib's original source.
#include "DefaultFont.h"

#include <cstring>

namespace DefaultFont {
namespace {

// Ported from LoadFontDefault (rtext.c): one bit per pixel of the 128 x 128 atlas
const unsigned int defaultFontData[512] = {
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00200020, 0x0001b000, 0x00000000, 0x00000000, 0x8ef92520, 0x00020a00, 0x7dbe8000, 0x1f7df45f,
	0x4a2bf2a0, 0x0852091e, 0x41224000, 0x10041450, 0x2e292020, 0x08220812, 0x41222000, 0x10041450, 0x10f92020, 0x3efa084c, 0x7d22103c, 0x107df7de,
	0xe8a12020, 0x08220832, 0x05220800, 0x10450410, 0xa4a3f000, 0x08520832, 0x05220400, 0x10450410, 0xe2f92020, 0x0002085e, 0x7d3e0281, 0x107df41f,
	0x00200000, 0x8001b000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xc0000fbe, 0xfbf7e00f, 0x5fbf7e7d, 0x0050bee8, 0x440808a2, 0x0a142fe8, 0x50810285, 0x0050a048,
	0x49e428a2, 0x0a142828, 0x40810284, 0x0048a048, 0x10020fbe, 0x09f7ebaf, 0xd89f3e84, 0x0047a04f, 0x09e48822, 0x0a142aa1, 0x50810284, 0x0048a048,
	0x04082822, 0x0a142fa0, 0x50810285, 0x0050a248, 0x00008fbe, 0xfbf42021, 0x5f817e7d, 0x07d09ce8, 0x00008000, 0x00000fe0, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000c0180,
	0xdfbf4282, 0x0bfbf7ef, 0x42850505, 0x004804bf, 0x50a142c6, 0x08401428, 0x42852505, 0x00a808a0, 0x50a146aa, 0x08401428, 0x42852505, 0x00081090,
	0x5fa14a92, 0x0843f7e8, 0x7e792505, 0x00082088, 0x40a15282, 0x08420128, 0x40852489, 0x00084084, 0x40a16282, 0x0842022a, 0x40852451, 0x00088082,
	0xc0bf4282, 0xf843f42f, 0x7e85fc21, 0x3e0900bf, 0x00000000, 0x00000004, 0x00000000, 0x000c0180, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x04000402, 0x41482000, 0x00000000, 0x00000800,
	0x04000404, 0x4100203c, 0x00000000, 0x00000800, 0xf7df7df0, 0x514bef85, 0xbefbefbe, 0x04513bef, 0x14414500, 0x494a2885, 0xa28a28aa, 0x04510820,
	0xf44145f0, 0x474a289d, 0xa28a28aa, 0x04510be0, 0x14414510, 0x494a2884, 0xa28a28aa, 0x02910a00, 0xf7df7df0, 0xd14a2f85, 0xbefbe8aa, 0x011f7be0,
	0x00000000, 0x00400804, 0x20080000, 0x00000000, 0x00000000, 0x00600f84, 0x20080000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0xac000000, 0x00000f01, 0x00000000, 0x00000000, 0x24000000, 0x00000f01, 0x00000000, 0x06000000, 0x24000000, 0x00000f01, 0x00000000, 0x09108000,
	0x24fa28a2, 0x00000f01, 0x00000000, 0x013e0000, 0x2242252a, 0x00000f52, 0x00000000, 0x038a8000, 0x2422222a, 0x00000f29, 0x00000000, 0x010a8000,
	0x2412252a, 0x00000f01, 0x00000000, 0x010a8000, 0x24fbe8be, 0x00000f01, 0x00000000, 0x0ebe8000, 0xac020000, 0x00000f01, 0x00000000, 0x00048000,
	0x0003e000, 0x00000f00, 0x00000000, 0x00008000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000038, 0x8443b80e, 0x00203a03,
	0x02bea080, 0xf0000020, 0xc452208a, 0x04202b02, 0xf8029122, 0x07f0003b, 0xe44b388e, 0x02203a02, 0x081e8a1c, 0x0411e92a, 0xf4420be0, 0x01248202,
	0xe8140414, 0x05d104ba, 0xe7c3b880, 0x00893a0a, 0x283c0e1c, 0x04500902, 0xc4400080, 0x00448002, 0xe8208422, 0x04500002, 0x80400000, 0x05200002,
	0x083e8e00, 0x04100002, 0x804003e0, 0x07000042, 0xf8008400, 0x07f00003, 0x80400000, 0x04000022, 0x00000000, 0x00000000, 0x80400000, 0x04000002,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00800702, 0x1848a0c2, 0x84010000, 0x02920921, 0x01042642, 0x00005121, 0x42023f7f, 0x00291002,
	0xefc01422, 0x7efdfbf7, 0xefdfa109, 0x03bbbbf7, 0x28440f12, 0x42850a14, 0x20408109, 0x01111010, 0x28440408, 0x42850a14, 0x2040817f, 0x01111010,
	0xefc78204, 0x7efdfbf7, 0xe7cf8109, 0x011111f3, 0x2850a932, 0x42850a14, 0x2040a109, 0x01111010, 0x2850b840, 0x42850a14, 0xefdfbf79, 0x03bbbbf7,
	0x001fa020, 0x00000000, 0x00001000, 0x00000000, 0x00002070, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x08022800, 0x00012283, 0x02430802, 0x01010001, 0x8404147c, 0x20000144, 0x80048404, 0x00823f08, 0xdfbf4284, 0x7e03f7ef, 0x142850a1, 0x0000210a,
	0x50a14684, 0x528a1428, 0x142850a1, 0x03efa17a, 0x50a14a9e, 0x52521428, 0x142850a1, 0x02081f4a, 0x50a15284, 0x4a221428, 0xf42850a1, 0x03efa14b,
	0x50a16284, 0x4a521428, 0x042850a1, 0x0228a17a, 0xdfbf427c, 0x7e8bf7ef, 0xf7efdfbf, 0x03efbd0b, 0x00000000, 0x04000000, 0x00000000, 0x00000008,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00200508, 0x00840400, 0x11458122, 0x00014210,
	0x00514294, 0x51420800, 0x20a22a94, 0x0050a508, 0x00200000, 0x00000000, 0x00050000, 0x08000000, 0xfefbefbe, 0xfbefbefb, 0xfbeb9114, 0x00fbefbe,
	0x20820820, 0x8a28a20a, 0x8a289114, 0x3e8a28a2, 0xfefbefbe, 0xfbefbe0b, 0x8a289114, 0x008a28a2, 0x228a28a2, 0x08208208, 0x8a289114, 0x088a28a2,
	0xfefbefbe, 0xfbefbefb, 0xfa2f9114, 0x00fbefbe, 0x00000000, 0x00000040, 0x00000000, 0x00000000, 0x00000000, 0x00000020, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00210100, 0x00000004, 0x00000000, 0x00000000, 0x14508200, 0x00001402, 0x00000000, 0x00000000,
	0x00000010, 0x00000020, 0x00000000, 0x00000000, 0xa28a28be, 0x00002228, 0x00000000, 0x00000000, 0xa28a28aa, 0x000022e8, 0x00000000, 0x00000000,
	0xa28a28aa, 0x000022a8, 0x00000000, 0x00000000, 0xa28a28aa, 0x000022e8, 0x00000000, 0x00000000, 0xbefbefbe, 0x00003e2f, 0x00000000, 0x00000000,
	0x00000004, 0x00002028, 0x00000000, 0x00000000, 0x80000000, 0x00003e0f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 };

const int charsWidth[kGlyphCount] = {
	3, 1, 4, 6, 5, 7, 6, 2, 3, 3, 5, 5, 2, 4, 1, 7, 5, 2, 5, 5, 5, 5, 5, 5, 5, 5, 1, 1, 3, 4, 3, 6,
	7, 6, 6, 6, 6, 6, 6, 6, 6, 3, 5, 6, 5, 7, 6, 6, 6, 6, 6, 6, 7, 6, 7, 7, 6, 6, 6, 2, 7, 2, 3, 5,
	2, 5, 5, 5, 5, 5, 4, 5, 5, 1, 2, 5, 2, 5, 5, 5, 5, 5, 5, 5, 4, 5, 5, 5, 5, 5, 5, 3, 1, 3, 4, 4,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 5, 5, 5, 7, 1, 5, 3, 7, 3, 5, 4, 1, 7, 4, 3, 5, 3, 3, 2, 5, 6, 1, 2, 2, 3, 5, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 7, 6, 6, 6, 6, 6, 3, 3, 3, 3, 7, 6, 6, 6, 6, 6, 6, 5, 6, 6, 6, 6, 6, 6, 4, 6,
	5, 5, 5, 5, 5, 5, 9, 5, 5, 5, 5, 5, 2, 2, 3, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 5 };

constexpr int kCharsHeight = 10;
constexpr int kCharsDivisor = 1; // a 1 pixel divisor between characters, horizontally and vertically
constexpr int kLineSpacing = 2;  // raylib's textLineSpacing default

struct Font
{
	std::vector<std::uint8_t> pixels;
	GlyphRect recs[kGlyphCount];
};

Font BuildFont()
{
	Font font;

	// Image from the bits (rtext.c: "Fill image.data with defaultFontData"). raylib holds gray + alpha
	// and samples it as (gray, gray, gray, alpha); this stores that as RGBA.
	font.pixels.assign(static_cast<std::size_t>(kAtlasSize) * kAtlasSize * 4, 0);
	for (int i = 0, counter = 0; i < kAtlasSize * kAtlasSize; i += 32)
	{
		for (int j = 31; j >= 0; j--)
		{
			std::uint8_t* px = &font.pixels[static_cast<std::size_t>(i + j) * 4];
			px[0] = px[1] = px[2] = 0xff;
			px[3] = (defaultFontData[counter] & (1u << j)) ? 0xff : 0x00;
		}
		counter++;
	}

	// The glyph rectangles (rtext.c: "Reconstruct charSet")
	int currentLine = 0;
	int currentPosX = kCharsDivisor;
	int testPosX = kCharsDivisor;

	for (int i = 0; i < kGlyphCount; i++)
	{
		font.recs[i].x = static_cast<float>(currentPosX);
		font.recs[i].y = static_cast<float>(kCharsDivisor + currentLine * (kCharsHeight + kCharsDivisor));
		font.recs[i].w = static_cast<float>(charsWidth[i]);
		font.recs[i].h = static_cast<float>(kCharsHeight);

		testPosX += static_cast<int>(font.recs[i].w + static_cast<float>(kCharsDivisor));

		if (testPosX >= kAtlasSize)
		{
			currentLine++;
			currentPosX = 2 * kCharsDivisor + charsWidth[i];
			testPosX = currentPosX;

			font.recs[i].x = static_cast<float>(kCharsDivisor);
			font.recs[i].y = static_cast<float>(kCharsDivisor + currentLine * (kCharsHeight + kCharsDivisor));
		}
		else { currentPosX = testPosX; }
	}

	return font;
}

const Font& TheFont()
{
	static const Font font = BuildFont();
	return font;
}

}

const std::vector<std::uint8_t>& AtlasPixels() { return TheFont().pixels; }

const GlyphRect& Glyph(int index) { return TheFont().recs[index]; }

// GetGlyphIndex (rtext.c). The glyphs are code points 32 + i, so a search finds codepoint - 32
int GlyphIndex(int codepoint)
{
	int index = 0;
	int fallbackIndex = 0; // index of '?'

	for (int i = 0; i < kGlyphCount; i++)
	{
		if (32 + i == 63) { fallbackIndex = i; }

		if (32 + i == codepoint)
		{
			index = i;
			break;
		}
	}

	if ((index == 0) && (32 != codepoint)) { index = fallbackIndex; }
	return index;
}

// GetCodepointNext (rtext.c)
int NextCodepoint(const char* text, int* codepointSize)
{
	const char* ptr = text;
	int codepoint = 0x3f; // '?'
	*codepointSize = 1;
	if (text == nullptr) { return codepoint; }

	if (0xf0 == (0xf8 & ptr[0]))
	{
		// 4 byte UTF-8 codepoint
		if (((ptr[1] & 0xC0) ^ 0x80) || ((ptr[2] & 0xC0) ^ 0x80) || ((ptr[3] & 0xC0) ^ 0x80)) { return codepoint; }
		codepoint = ((0x07 & ptr[0]) << 18) | ((0x3f & ptr[1]) << 12) | ((0x3f & ptr[2]) << 6) | (0x3f & ptr[3]);
		*codepointSize = 4;
	}
	else if (0xe0 == (0xf0 & ptr[0]))
	{
		// 3 byte UTF-8 codepoint
		if (((ptr[1] & 0xC0) ^ 0x80) || ((ptr[2] & 0xC0) ^ 0x80)) { return codepoint; }
		codepoint = ((0x0f & ptr[0]) << 12) | ((0x3f & ptr[1]) << 6) | (0x3f & ptr[2]);
		*codepointSize = 3;
	}
	else if (0xc0 == (0xe0 & ptr[0]))
	{
		// 2 byte UTF-8 codepoint
		if ((ptr[1] & 0xC0) ^ 0x80) { return codepoint; }
		codepoint = ((0x1f & ptr[0]) << 6) | (0x3f & ptr[1]);
		*codepointSize = 2;
	}
	else if (0x00 == (0x80 & ptr[0]))
	{
		// 1 byte UTF-8 codepoint
		codepoint = ptr[0];
		*codepointSize = 1;
	}

	return codepoint;
}

int EffectiveSize(int fontSize) { return fontSize < kBaseSize ? kBaseSize : fontSize; }
int Spacing(int fontSize) { return EffectiveSize(fontSize) / kBaseSize; }

// MeasureText and MeasureTextEx (rtext.c), for the default font: no glyph has an advanceX or an offset
int TextWidth(const char* text, int fontSize)
{
	if (text == nullptr || text[0] == '\0') { return 0; }

	const float size = static_cast<float>(EffectiveSize(fontSize));
	const float spacing = static_cast<float>(Spacing(fontSize));
	const Font& font = TheFont();

	const int length = static_cast<int>(std::strlen(text));
	int tempByteCounter = 0; // the most characters on a line
	int byteCounter = 0;

	float textWidth = 0.0f;
	float tempTextWidth = 0.0f; // the widest line

	const float scaleFactor = size / static_cast<float>(kBaseSize);

	for (int i = 0; i < length;)
	{
		byteCounter++;

		int codepointByteCount = 0;
		const int letter = NextCodepoint(&text[i], &codepointByteCount);
		const int index = GlyphIndex(letter);

		i += codepointByteCount;

		if (letter != '\n')
		{
			textWidth += (font.recs[index].w + 0.0f);
		}
		else
		{
			if (tempTextWidth < textWidth) { tempTextWidth = textWidth; }
			byteCounter = 0;
			textWidth = 0;
		}

		if (tempByteCounter < byteCounter) { tempByteCounter = byteCounter; }
	}

	if (tempTextWidth < textWidth) { tempTextWidth = textWidth; }

	const float width = tempTextWidth * scaleFactor + static_cast<float>((tempByteCounter - 1) * spacing);
	return static_cast<int>(width);
}

// DrawText, DrawTextEx and DrawTextCodepoint (rtext.c)
void Layout(const char* text, int fontSize, std::vector<PlacedGlyph>& out)
{
	out.clear();
	if (text == nullptr) { return; }

	const float size = static_cast<float>(EffectiveSize(fontSize));
	const float spacing = static_cast<float>(Spacing(fontSize));
	const Font& font = TheFont();

	const int length = static_cast<int>(std::strlen(text));

	float textOffsetY = 0;     // offset between lines (on '\n')
	float textOffsetX = 0.0f;  // offset to the next character

	const float scaleFactor = size / kBaseSize; // character quad scaling factor

	for (int i = 0; i < length;)
	{
		int codepointByteCount = 0;
		const int codepoint = NextCodepoint(&text[i], &codepointByteCount);
		const int index = GlyphIndex(codepoint);

		if (codepoint == '\n')
		{
			textOffsetY += (size + static_cast<float>(kLineSpacing));
			textOffsetX = 0.0f;
		}
		else
		{
			if ((codepoint != ' ') && (codepoint != '\t'))
			{
				const GlyphRect& rec = font.recs[index];
				PlacedGlyph glyph;
				glyph.offsetX = textOffsetX;
				glyph.offsetY = textOffsetY;
				glyph.width = rec.w * scaleFactor;
				glyph.height = rec.h * scaleFactor;
				glyph.srcX = rec.x;
				glyph.srcY = rec.y;
				glyph.srcW = rec.w;
				glyph.srcH = rec.h;
				out.push_back(glyph);
			}

			textOffsetX += (font.recs[index].w * scaleFactor + spacing);
		}

		i += codepointByteCount;
	}
}

}
