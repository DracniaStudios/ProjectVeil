// The GL backend's 2D renderer (B-18): a flat shader, one dynamic vertex buffer drawn as
// triangles, rectangles, outlines, circles, text in raylib's default font, the 2D camera
// pass and screenshots.
//
// Ported from raylib 6.0, so that the 2D output matches the raylib build pixel for pixel:
//   rshapes.c  DrawRectanglePro, DrawRectangleLinesEx, DrawCircleSector (quads mode)
//   rtextures.c  DrawTexturePro (its quad and texture coordinates, used per glyph)
//   rtext.c    DrawTextEx and DrawTextCodepoint (through DefaultFont.cpp)
//   rcore.c    BeginMode2D, GetCameraMatrix2D, TakeScreenshot
//   rlgl.h     rlDrawRenderBatch (the MVP and blend state), rlReadScreenPixels,
//              and the default shader's source
// raylib's copyright lines in those files:
//   rshapes.c, rtext.c, rtextures.c, rcore.c: Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
//   (rcore.c adds "and contributors")
//   rlgl.h: Copyright (c) 2014-2026 Ramon Santamaria (@raysan5)
// Used under raylib's zlib/libpng licence, which permits use and alteration with this
// notice kept; this file is an altered version and is not raylib's original source.
#include "GlShared.h"
#include "DefaultFont.h"

#include <Platform.h>
#include <raymath.h> // matrix math, DEG2RAD: inline only

#include <glad/gl.h>
#include <stb_image_write.h> // declarations only; the implementation is in ThirdPartyImpl.cpp

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

// raylib's default shader, with its colDiffuse (always 1) dropped
const char* const kFlatVertex =
	"#version 330\n"
	"in vec3 vertexPosition;\n"
	"in vec2 vertexTexCoord;\n"
	"in vec4 vertexColor;\n"
	"out vec2 fragTexCoord;\n"
	"out vec4 fragColor;\n"
	"uniform mat4 mvp;\n"
	"void main()\n"
	"{\n"
	"    fragTexCoord = vertexTexCoord;\n"
	"    fragColor = vertexColor;\n"
	"    gl_Position = mvp*vec4(vertexPosition, 1.0);\n"
	"}\n";

const char* const kFlatFragment =
	"#version 330\n"
	"in vec2 fragTexCoord;\n"
	"in vec4 fragColor;\n"
	"out vec4 finalColor;\n"
	"uniform sampler2D texture0;\n"
	"void main()\n"
	"{\n"
	"    finalColor = texture(texture0, fragTexCoord)*fragColor;\n"
	"}\n";

struct Vertex
{
	float x, y, z;
	float u, v;
	std::uint8_t r, g, b, a;
};

constexpr int kMaxVertices = 8192 * 6; // as many as 8192 quads
constexpr float kDepth = -0.5f;        // inside the ortho range 0..1 of the 2D projection; the depth test is off

struct State
{
	bool ready = false;
	GLuint program = 0;
	GLint mvpLocation = -1;
	GLint textureLocation = -1;
	GLuint vao = 0;
	GLuint vbo = 0;
	GLuint whiteTexture = 0;
	GLuint fontTexture = 0;

	Matrix modelview = MatrixIdentity(); // identity outside Begin2D
	GLuint currentTexture = 0;
	std::vector<Vertex> vertices;
};

State g;

GLuint CompileStage(GLenum type, const char* source)
{
	const GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	GLint ok = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024] = {};
		glGetShaderInfoLog(shader, sizeof(log) - 1, nullptr, log);
		std::cerr << "[Render] flat shader stage failed to compile: " << log << std::endl;
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

GLuint MakeTexture(const void* rgba, int width, int height)
{
	GLint previousAlignment = 4;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	GLuint id = 0;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	glBindTexture(GL_TEXTURE_2D, 0);

	glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
	return id;
}

// rcore.c GetCameraMatrix2D
Matrix CameraMatrix2D(const Camera2D& camera)
{
	const Matrix matOrigin = MatrixTranslate(-camera.target.x, -camera.target.y, 0.0f);
	const Matrix matRotation = MatrixRotate(Vector3{ 0.0f, 0.0f, 1.0f }, camera.rotation * DEG2RAD);
	const Matrix matScale = MatrixScale(camera.zoom, camera.zoom, 1.0f);
	const Matrix matTranslation = MatrixTranslate(camera.offset.x, camera.offset.y, 0.0f);

	return MatrixMultiply(MatrixMultiply(matOrigin, MatrixMultiply(matScale, matRotation)), matTranslation);
}

void SetTexture(GLuint texture)
{
	if (texture != g.currentTexture) {
		Gl2D::Flush();
		g.currentTexture = texture;
	}
}

void Reserve(std::size_t count)
{
	if (g.vertices.size() + count > static_cast<std::size_t>(kMaxVertices)) { Gl2D::Flush(); }
}

void Push(float x, float y, float u, float v, Color c)
{
	g.vertices.push_back(Vertex{ x, y, kDepth, u, v, c.r, c.g, c.b, c.a });
}

// raylib's quads: vertices 0 1 2 3 become triangles (0 1 2) and (0 2 3); with raylib's
// ortho (y down) and its top left, bottom left, bottom right, top right order they
// face the viewer counter-clockwise and survive back-face culling
struct QuadVertex { float x, y, u, v; };

void PushQuad(const QuadVertex& a, const QuadVertex& b, const QuadVertex& c, const QuadVertex& d, Color color)
{
	Reserve(6);
	Push(a.x, a.y, a.u, a.v, color);
	Push(b.x, b.y, b.u, b.v, color);
	Push(c.x, c.y, c.u, c.v, color);
	Push(a.x, a.y, a.u, a.v, color);
	Push(c.x, c.y, c.u, c.v, color);
	Push(d.x, d.y, d.u, d.v, color);
}

}

namespace Gl2D {

bool Init()
{
	if (g.ready) { return true; }

	const GLuint vs = CompileStage(GL_VERTEX_SHADER, kFlatVertex);
	const GLuint fs = CompileStage(GL_FRAGMENT_SHADER, kFlatFragment);
	if (vs == 0 || fs == 0) {
		if (vs != 0) { glDeleteShader(vs); }
		if (fs != 0) { glDeleteShader(fs); }
		return false;
	}

	g.program = glCreateProgram();
	glAttachShader(g.program, vs);
	glAttachShader(g.program, fs);
	// The attribute locations of ADR B section 7
	glBindAttribLocation(g.program, 0, "vertexPosition");
	glBindAttribLocation(g.program, 1, "vertexTexCoord");
	glBindAttribLocation(g.program, 3, "vertexColor");
	glLinkProgram(g.program);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint linked = 0;
	glGetProgramiv(g.program, GL_LINK_STATUS, &linked);
	if (!linked) {
		char log[1024] = {};
		glGetProgramInfoLog(g.program, sizeof(log) - 1, nullptr, log);
		std::cerr << "[Render] flat shader failed to link: " << log << std::endl;
		glDeleteProgram(g.program);
		g.program = 0;
		return false;
	}
	g.mvpLocation = glGetUniformLocation(g.program, "mvp");
	g.textureLocation = glGetUniformLocation(g.program, "texture0");

	glGenVertexArrays(1, &g.vao);
	glGenBuffers(1, &g.vbo);
	glBindVertexArray(g.vao);
	glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(kMaxVertices) * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, x)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, u)));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, r)));
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	const std::uint8_t white[4] = { 255, 255, 255, 255 };
	g.whiteTexture = MakeTexture(white, 1, 1);
	g.fontTexture = MakeTexture(DefaultFont::AtlasPixels().data(), DefaultFont::kAtlasSize, DefaultFont::kAtlasSize);

	g.vertices.clear();
	g.vertices.reserve(kMaxVertices);
	g.modelview = MatrixIdentity();
	g.currentTexture = g.whiteTexture;
	g.ready = true;
	return true;
}

void Shutdown()
{
	if (!g.ready) { return; }
	glDeleteTextures(1, &g.fontTexture);
	glDeleteTextures(1, &g.whiteTexture);
	glDeleteBuffers(1, &g.vbo);
	glDeleteVertexArrays(1, &g.vao);
	glDeleteProgram(g.program);
	g = State{};
}

void BeginFrame()
{
	g.vertices.clear();
	g.modelview = MatrixIdentity();
	g.currentTexture = g.whiteTexture;
}

// rlDrawRenderBatch: set everything this draw needs, whatever the last draw or ImGui left
void Flush()
{
	if (!g.ready || g.vertices.empty()) { return; }

	glUseProgram(g.program);

	const Matrix projection = MatrixOrtho(0.0, static_cast<double>(Platform::ScreenWidth()),
		static_cast<double>(Platform::ScreenHeight()), 0.0, 0.0, 1.0);
	const Matrix mvp = MatrixMultiply(g.modelview, projection);
	const float16 mvpFloats = MatrixToFloatV(mvp);
	glUniformMatrix4fv(g.mvpLocation, 1, GL_FALSE, mvpFloats.v);
	glUniform1i(g.textureLocation, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g.currentTexture);

	glBindVertexArray(g.vao);
	glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(g.vertices.size() * sizeof(Vertex)), g.vertices.data());

	// The state as Render::BeginFrame documents it, with the depth test off for 2D
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);

	glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(g.vertices.size()));

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	g.vertices.clear();
}

// BeginMode2D: flush, then the camera becomes the modelview
void Begin2D(const Camera2D& camera)
{
	Flush();
	g.modelview = CameraMatrix2D(camera);
}

// EndMode2D
void End2D()
{
	Flush();
	g.modelview = MatrixIdentity();
}

// DrawRectanglePro with no rotation and no origin
void Rect(Rectangle rect, Color color)
{
	if (!g.ready) { return; }

	const float x = rect.x - 0.0f;
	const float y = rect.y - 0.0f;
	const QuadVertex topLeft = { x, y, 0.0f, 0.0f };
	const QuadVertex bottomLeft = { x, y + rect.height, 0.0f, 1.0f };
	const QuadVertex bottomRight = { x + rect.width, y + rect.height, 1.0f, 1.0f };
	const QuadVertex topRight = { x + rect.width, y, 1.0f, 0.0f };

	SetTexture(g.whiteTexture);
	PushQuad(topLeft, bottomLeft, bottomRight, topRight, color);
}

// DrawRectangleLinesEx: four rectangles
void RectOutline(Rectangle rec, float lineThick, Color color)
{
	if ((lineThick > rec.width) || (lineThick > rec.height)) {
		if (rec.width >= rec.height) { lineThick = rec.height / 2; }
		else if (rec.width <= rec.height) { lineThick = rec.width / 2; }
	}

	const Rectangle top = { rec.x, rec.y, rec.width, lineThick };
	const Rectangle bottom = { rec.x, rec.y - lineThick + rec.height, rec.width, lineThick };
	const Rectangle left = { rec.x, rec.y + lineThick, lineThick, rec.height - lineThick * 2.0f };
	const Rectangle right = { rec.x - lineThick + rec.width, rec.y + lineThick, lineThick, rec.height - lineThick * 2.0f };

	Rect(top, color);
	Rect(bottom, color);
	Rect(left, color);
	Rect(right, color);
}

// DrawCircleV: DrawCircleSector(center, radius, 0, 360, 36, color) in quads mode.
// The segment count is above the minimum of 4, so raylib never recomputes it, and 36 is even,
// so there is no odd last piece.
void Circle(Vector2 center, float radius, Color color)
{
	if (!g.ready) { return; }
	if (radius <= 0.0f) { radius = 0.1f; }

	const int segments = 36;
	const float stepLength = (360.0f - 0.0f) / static_cast<float>(segments);
	float angle = 0.0f;

	SetTexture(g.whiteTexture);

	// Every quad is two segments
	for (int i = 0; i < segments / 2; i++) {
		const QuadVertex a = { center.x, center.y, 0.0f, 0.0f };
		const QuadVertex b = {
			center.x + cosf(DEG2RAD * (angle + stepLength * 2.0f)) * radius,
			center.y + sinf(DEG2RAD * (angle + stepLength * 2.0f)) * radius, 1.0f, 0.0f };
		const QuadVertex c = {
			center.x + cosf(DEG2RAD * (angle + stepLength)) * radius,
			center.y + sinf(DEG2RAD * (angle + stepLength)) * radius, 1.0f, 1.0f };
		const QuadVertex d = {
			center.x + cosf(DEG2RAD * angle) * radius,
			center.y + sinf(DEG2RAD * angle) * radius, 0.0f, 1.0f };
		PushQuad(a, b, c, d, color);

		angle += (stepLength * 2.0f);
	}
}

// DrawText and DrawTextEx: one DrawTexturePro quad per glyph
void Text(const std::string& text, Vector2 topLeft, int fontSize, Color color)
{
	if (!g.ready) { return; }

	// Render::Text truncates the position to whole pixels, as the raylib backend's static_cast<int> does
	const float posX = static_cast<float>(static_cast<int>(topLeft.x));
	const float posY = static_cast<float>(static_cast<int>(topLeft.y));

	static std::vector<DefaultFont::PlacedGlyph> glyphs;
	DefaultFont::Layout(text.c_str(), fontSize, glyphs);
	if (glyphs.empty()) { return; }

	SetTexture(g.fontTexture);

	const float width = static_cast<float>(DefaultFont::kAtlasSize);
	const float height = static_cast<float>(DefaultFont::kAtlasSize);
	for (const DefaultFont::PlacedGlyph& glyph : glyphs) {
		// DrawTextCodepoint's destination rectangle (the glyph padding is 0)
		const float dstX = posX + glyph.offsetX;
		const float dstY = posY + glyph.offsetY;

		const float x = dstX - 0.0f;
		const float y = dstY - 0.0f;
		const QuadVertex tl = { x, y, glyph.srcX / width, glyph.srcY / height };
		const QuadVertex bl = { x, y + glyph.height, glyph.srcX / width, (glyph.srcY + glyph.srcH) / height };
		const QuadVertex br = { x + glyph.width, y + glyph.height, (glyph.srcX + glyph.srcW) / width, (glyph.srcY + glyph.srcH) / height };
		const QuadVertex tr = { x + glyph.width, y, (glyph.srcX + glyph.srcW) / width, glyph.srcY / height };
		PushQuad(tl, bl, br, tr, color);
	}
}

// TakeScreenshot and rlReadScreenPixels: the back buffer, alpha forced to 255, rows flipped
bool CaptureScreenshot(const std::string& fileName)
{
	Flush();

	const int width = Platform::RenderWidth();
	const int height = Platform::RenderHeight();
	if (width <= 0 || height <= 0) { return false; }

	GLint previousReadFramebuffer = 0, previousReadBuffer = GL_BACK, previousAlignment = 4;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
	glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
	glGetIntegerv(GL_PACK_ALIGNMENT, &previousAlignment);

	std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

	glPixelStorei(GL_PACK_ALIGNMENT, previousAlignment);
	glReadBuffer(static_cast<GLenum>(previousReadBuffer));
	glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));

	// glReadPixels starts at the bottom left. Flip the rows and make every pixel opaque.
	const std::size_t stride = static_cast<std::size_t>(width) * 4;
	std::vector<std::uint8_t> flipped(pixels.size());
	for (int row = 0; row < height; row++) {
		const std::uint8_t* source = &pixels[static_cast<std::size_t>(height - 1 - row) * stride];
		std::uint8_t* target = &flipped[static_cast<std::size_t>(row) * stride];
		for (std::size_t i = 0; i < stride; i += 4) {
			target[i] = source[i];
			target[i + 1] = source[i + 1];
			target[i + 2] = source[i + 2];
			target[i + 3] = 255;
		}
	}

	return stbi_write_png(fileName.c_str(), width, height, 4, flipped.data(), width * 4) != 0;
}

unsigned int WhiteTexture() { return g.whiteTexture; }
unsigned int FlatProgram() { return g.program; }

}
