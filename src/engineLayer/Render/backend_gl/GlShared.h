#pragma once
// Backend-private helpers shared by the GL backend's translation units (B-18).
// Not included by anything outside backend_gl/.
#include <Render/Render.h>
#include <raylib.h> // vocabulary types only

#include <string>
#include <vector>

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

// Textures, meshes and models (B-20, GlResources.cpp). The tables behind Render's handles;
// B-21 draws from them. GL names are plain unsigned ints so this header needs no GL include.
namespace GlResources {

// Needs Gl2D::Init first (handle 1 is Gl2D's white texture). Builds the unit cube, model handle 1.
bool Init();
// Deletes every texture, buffer and VAO the tables hold and empties them. Before Gl2D::Shutdown.
void Shutdown();

struct Mesh {
	unsigned int vao = 0;
	unsigned int buffers[5] = {};    // position, texcoord, normal, colour, index; 0 where absent
	int vertexCount = 0;
	int indexCount = 0;              // 0: not indexed, draw vertexCount vertices with glDrawArrays
	int material = 0;                // into Model::materials
	// An absent attribute has its array disabled: set glVertexAttrib* at draw time
	// (texcoord 0,0; normal 0,0,1; colour 1,1,1,1, as raylib's UploadMesh does)
	bool hasTexcoords = false, hasNormals = false, hasColors = false;
};
struct Material {
	Render::TextureHandle diffuse;   // handle 1 (white) when the material has no texture of its own
	Color color = WHITE;
};
struct Model {
	std::vector<Mesh> meshes;
	std::vector<Material> materials; // [0] is the default material
};

const Model* FindModel(Render::ModelHandle handle); // null for none or unknown
unsigned int TextureName(Render::TextureHandle handle); // GL texture name, 0 for none or unknown

}

// The lit world (B-21, GlLit.cpp): the lighting shader, the model draw and the world pass.
namespace GlLit {

// Needs Gl2D::Init and a current context. Compiles the lighting shader (a failure is
// reported and leaves ShaderId() at 0) and the unlit fallback. False only when neither
// program could be built.
bool Init();
void Shutdown();

// The lighting shader program, 0 when it failed to load
unsigned int ShaderId();

// Flushes the 2D batch, then builds the camera matrices and sets the world pass state
void BeginWorld(const Camera3D& camera);
// Puts the state back as Render::BeginFrame documents it
void EndWorld();

void DrawModel(const Render::ModelDraw& draw, bool wires);
void SetLighting(const Render::LightingFrame& frame);

}
