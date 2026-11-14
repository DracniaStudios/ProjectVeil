// Render on OpenGL 3.3 core (B-15). Every function declared in Render.h exists. The 2D
// part is real (B-18, Gl2D.cpp and DefaultFont.cpp); the world is not drawn yet: each of
// its stubs logs one line the first time it is called and names the work order that
// implements it.
#include <Render/Render.h>
#include "GlShared.h"
#include "DefaultFont.h"
#include <Platform.h>
#include <raymath.h> // MatrixIdentity: inline math only

#include <glad/gl.h>
#include <SDL3/SDL.h>

#include <iostream>
#include <set>
#include <string>

namespace {
	bool ready = false;

	void LogStubOnce(const char* function, const char* workOrder)
	{
		static std::set<std::string> logged;
		if (logged.insert(function).second) {
			std::cout << "[Render] " << function << " is not implemented in sdl3gl yet (" << workOrder << ")" << std::endl;
		}
	}
}

// Put first in a stub's body. The static makes the call free after the first time.
#define VEIL_STUB(workOrder) do { static const bool logged_ = (LogStubOnce(__func__, workOrder), true); (void)logged_; } while (0)

namespace Render {

bool Init()
{
	if (ready) { return true; }

	const int version = gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));
	if (version == 0) {
		std::cerr << "[Render] gladLoadGL failed: no OpenGL functions could be loaded" << std::endl;
		return false;
	}
	const int major = GLAD_VERSION_MAJOR(version);
	const int minor = GLAD_VERSION_MINOR(version);
	if (major < 3 || (major == 3 && minor < 3)) {
		std::cerr << "[Render] OpenGL " << major << "." << minor << " is too old, 3.3 is required" << std::endl;
		return false;
	}

	const auto text = [](GLenum name) {
		const GLubyte* s = glGetString(name);
		return s != nullptr ? reinterpret_cast<const char*>(s) : "(null)";
	};
	std::cout << "[Render] GL " << text(GL_VERSION) << " on " << text(GL_RENDERER) << std::endl;

	if (!Gl2D::Init()) {
		std::cerr << "[Render] the 2D renderer could not be created" << std::endl;
		return false;
	}

	if (!GlResources::Init()) {
		std::cerr << "[Render] the unit cube could not be created" << std::endl;
		Gl2D::Shutdown();
		return false;
	}

	ready = true;
	return true;
}

void Shutdown()
{
	GlResources::Shutdown(); // before Gl2D: handle 1 is Gl2D's texture and is not deleted here
	Gl2D::Shutdown();
	ready = false;
}
bool IsReady() { return ready; }

void DrawModel(const ModelDraw&) { VEIL_STUB("B-21"); }
void DrawModelWires(const ModelDraw&) { VEIL_STUB("B-21"); }

void DebugLine(Vector3, Vector3, Color) { VEIL_STUB("B-23"); }
void DebugSphere(Vector3, float, Color) { VEIL_STUB("B-23"); }
void DebugSphere(Vector3, float, int, int, Color) { VEIL_STUB("B-23"); }
void DebugSphereWires(Vector3, float, int, int, Color) { VEIL_STUB("B-23"); }
void DebugCube(Vector3, Vector3, Color) { VEIL_STUB("B-23"); }
void DebugBoundingBox(BoundingBox, Color) { VEIL_STUB("B-23"); }
void DebugCylinder(Vector3, Vector3, float, float, int, Color) { VEIL_STUB("B-23"); }
void DebugCapsule(Vector3, Vector3, float, int, int, Color) { VEIL_STUB("B-23"); }
void DebugGrid(int, float) { VEIL_STUB("B-23"); }

void BeginFrame(Color clear)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, Platform::RenderWidth(), Platform::RenderHeight());

	// The state the raylib build draws with
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// The depth write mask must be on, or the clear below leaves the old depth
	glDepthMask(GL_TRUE);
	glClearColor(clear.r / 255.0f, clear.g / 255.0f, clear.b / 255.0f, clear.a / 255.0f);
	glClearDepth(1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	Gl2D::BeginFrame();
}

// Nothing may stay batched: ImGui draws straight after this
void EndFrame() { Gl2D::Flush(); }
void BeginWorld(const Camera3D&) { Gl2D::Flush(); VEIL_STUB("B-21"); }
void EndWorld() {}
void SetDebugDepthTest(bool) { VEIL_STUB("B-23"); }
void Begin2D(const Camera2D& camera) { Gl2D::Begin2D(camera); }
void End2D() { Gl2D::End2D(); }
void CaptureScreenshot(const std::string& fileName) { Gl2D::CaptureScreenshot(fileName); }

bool SetShadowMapSize(int size) { VEIL_STUB("B-22"); return size == 0; }
int ShadowMapSize() { return 0; }
bool BeginShadowPass(const Camera3D&) { VEIL_STUB("B-22"); return false; }
void EndShadowPass() {}
Matrix ShadowViewProjection() { VEIL_STUB("B-22"); return MatrixIdentity(); }

void SetLighting(const LightingFrame&) { VEIL_STUB("B-21"); }
unsigned int LightingShaderId() { return 0; }

void Rect(Rectangle rect, Color color) { Gl2D::Rect(rect, color); }
void RectOutline(Rectangle rect, float thickness, Color color) { Gl2D::RectOutline(rect, thickness, color); }
void Circle(Vector2 center, float radius, Color color) { Gl2D::Circle(center, radius, color); }
void Text(const std::string& text, Vector2 topLeft, int fontSize, Color color) { Gl2D::Text(text, topLeft, fontSize, color); }
// CPU only, so it is right before Init too: the UI measures text while it lays out
int TextWidth(const std::string& text, int fontSize) { return DefaultFont::TextWidth(text.c_str(), fontSize); }

}
