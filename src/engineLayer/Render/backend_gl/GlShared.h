#pragma once
// Backend-private helpers shared by the GL backend's translation units (B-18).
// Not included by anything outside backend_gl/.
#include <raylib.h> // vocabulary types only

#include <string>

namespace Gl2D {

// Creates the flat shader, the batch buffers, the default white texture and the font texture.
// Needs a current GL context with functions loaded. False when the shader did not link.
bool Init();
void Shutdown();

// Frame hooks. BeginFrame puts the camera back to identity; Flush draws what the batch holds.
void BeginFrame();
void Flush();

// Camera pass. Draws outside it use the identity camera (screen pixels, origin top left).
void Begin2D(const Camera2D& camera);
void End2D();

void Rect(Rectangle rect, Color color);
void RectOutline(Rectangle rect, float thickness, Color color);
void Circle(Vector2 center, float radius, Color color);
void Text(const std::string& text, Vector2 topLeft, int fontSize, Color color);

// Flushes, then writes the back buffer as a PNG. False when nothing could be written.
bool CaptureScreenshot(const std::string& fileName);

// The 1x1 RGBA 255 texture and the flat shader program, for later work orders' unlit draws
unsigned int WhiteTexture();
unsigned int FlatProgram();

}
