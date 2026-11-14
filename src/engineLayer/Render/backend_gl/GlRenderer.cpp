// Render on OpenGL 3.3 core (B-15). Every function declared in Render.h exists. Apart
// from clearing the screen in BeginFrame, none draws yet: each stub logs one line the
// first time it is called and names the work order that implements it.
#include <Render/Render.h>
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

	ready = true;
	return true;
}

void Shutdown() { ready = false; } // nothing to free yet
bool IsReady() { return ready; }

LoadedModel LoadModelFile(const std::string&) { VEIL_STUB("B-20"); return {}; }
TextureHandle LoadTextureFile(const std::string&) { VEIL_STUB("B-20"); return {}; }
ModelHandle UnitCube() { VEIL_STUB("B-20"); return {}; }
TextureHandle DiffuseTexture(ModelHandle, int) { VEIL_STUB("B-20"); return {}; }
std::uintptr_t NativeTextureId(TextureHandle) { VEIL_STUB("B-20"); return 0; }

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
}

void EndFrame() {} // nothing is batched yet
void BeginWorld(const Camera3D&) { VEIL_STUB("B-21"); }
void EndWorld() {}
void SetDebugDepthTest(bool) { VEIL_STUB("B-23"); }
void Begin2D(const Camera2D&) { VEIL_STUB("B-18"); }
void End2D() {}
void CaptureScreenshot(const std::string&) { VEIL_STUB("B-18"); }

bool SetShadowMapSize(int size) { VEIL_STUB("B-22"); return size == 0; }
int ShadowMapSize() { return 0; }
bool BeginShadowPass(const Camera3D&) { VEIL_STUB("B-22"); return false; }
void EndShadowPass() {}
Matrix ShadowViewProjection() { VEIL_STUB("B-22"); return MatrixIdentity(); }

void SetLighting(const LightingFrame&) { VEIL_STUB("B-21"); }
unsigned int LightingShaderId() { return 0; }

void Rect(Rectangle, Color) { VEIL_STUB("B-18"); }
void RectOutline(Rectangle, float, Color) { VEIL_STUB("B-18"); }
void Circle(Vector2, float, Color) { VEIL_STUB("B-18"); }
void Text(const std::string&, Vector2, int, Color) { VEIL_STUB("B-18"); }
int TextWidth(const std::string&, int) { VEIL_STUB("B-18"); return 0; }

}
