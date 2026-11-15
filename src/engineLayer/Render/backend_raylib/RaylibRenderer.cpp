// Render on raylib (B-07 lifetime, B-08 model and texture tables and model draws,
// B-11 shadow map, B-12 lighting shader and uniforms).
#include <Render/Render.h>

#include <Geometry.h>
#include <ResourcePaths.h>

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
	bool ready = false;

	// A handle's id is its index + 1. Entries are never removed before Shutdown.
	std::vector<Model> models;
	std::vector<Texture2D> textures;
	// Parallel to `textures`: false for a texture raylib or a model owns (the default
	// white texture, a material's own map), which this renderer must not unload.
	std::vector<bool> textureOwned;

	// The default white texture is texture handle 1, and every texture handle is valid
	// after this, with or without Init (a test may load assets without one).
	void SeedTextures()
	{
		if (!textures.empty()) { return; }
		textures.push_back(Texture2D{ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 });
		textureOwned.push_back(false);
	}

	const Model* ModelOf(Render::ModelHandle handle)
	{
		return (handle.id >= 1 && handle.id <= models.size()) ? &models[handle.id - 1] : nullptr;
	}

	const Texture2D* TextureOf(Render::TextureHandle handle)
	{
		return (handle.id >= 1 && handle.id <= textures.size()) ? &textures[handle.id - 1] : nullptr;
	}

	// raylib's PixelFormat has no depth entries; 19 is UNCOMPRESSED_R32 in the
	// public enum and is what raylib's own shadowmap sample stores for a
	// 24-bit depth attachment. Only the width/height/id fields are ever used.
	constexpr int DEPTH_TEXTURE_FORMAT = 19;

	/**
	 * Texture unit the shadow map is bound to.
	 * raylib's DrawMesh() binds material maps to slots 0..MAX_MATERIAL_MAPS-1 (12)
	 * and unbinds them again afterwards, so any slot below 12 can be clobbered
	 * mid-frame. 15 is the top of OpenGL 3.3's guaranteed 16 texture units.
	 */
	constexpr int SHADOW_MAP_SLOT = 15;

	// The lighting shader (id 0 when it failed to load: models then draw with raylib's
	// own shader, unlit). It is injected into every Material rather than pushed with
	// BeginShaderMode(): raylib's DrawMesh() calls rlEnableShader(material.shader.id)
	// itself, so BeginShaderMode() only affects the rlgl batch (DrawGrid, shapes,
	// 2D) and would leave every model unlit. Materials hold a *copy* of the Shader
	// struct, not ownership, so every model's material shader id dangles once
	// Shutdown unloads it.
	Shader lightShader = {};

	// Cached uniform locations. Custom uniforms cannot live in Shader::locs —
	// that array is exactly RL_MAX_SHADER_LOCATIONS (32) entries and is owned
	// by raylib's own semantics.
	struct Uniforms
	{
		int lightCount = -1;
		int lightType = -1;
		int lightPosition = -1;
		int lightDirection = -1;
		int lightColor = -1;
		int lightRange = -1;
		int lightCone = -1;

		int ambient = -1;
		int fogColor = -1;
		int fogDensity = -1;
		int exposure = -1;
		int specularStrength = -1;
		int shininess = -1;

		int shadowMap = -1;
		int lightVP = -1;
		int shadowTexelSize = -1;
		int shadowBias = -1;
		int shadowSoftness = -1;
		int shadowsEnabled = -1;
		int shadowLightIndex = -1;

		int depthPass = -1;
	};
	Uniforms uniforms = {};

	// One of the lighting shader's files, as the C string raylib wants
	std::string ShaderPath(const char* fileName)
	{
		return (ResourcePaths::Resources() / "shaders" / "glsl330" / fileName).string();
	}

	void CacheUniformLocations()
	{
		uniforms.lightCount = GetShaderLocation(lightShader, "lightCount");
		uniforms.lightType = GetShaderLocation(lightShader, "lightType");
		uniforms.lightPosition = GetShaderLocation(lightShader, "lightPosition");
		uniforms.lightDirection = GetShaderLocation(lightShader, "lightDirection");
		uniforms.lightColor = GetShaderLocation(lightShader, "lightColor");
		uniforms.lightRange = GetShaderLocation(lightShader, "lightRange");
		uniforms.lightCone = GetShaderLocation(lightShader, "lightCone");

		uniforms.ambient = GetShaderLocation(lightShader, "ambient");
		uniforms.fogColor = GetShaderLocation(lightShader, "fogColor");
		uniforms.fogDensity = GetShaderLocation(lightShader, "fogDensity");
		uniforms.exposure = GetShaderLocation(lightShader, "exposure");
		uniforms.specularStrength = GetShaderLocation(lightShader, "specularStrength");
		uniforms.shininess = GetShaderLocation(lightShader, "shininess");

		uniforms.shadowMap = GetShaderLocation(lightShader, "shadowMap");
		uniforms.lightVP = GetShaderLocation(lightShader, "lightVP");
		uniforms.shadowTexelSize = GetShaderLocation(lightShader, "shadowTexelSize");
		uniforms.shadowBias = GetShaderLocation(lightShader, "shadowBias");
		uniforms.shadowSoftness = GetShaderLocation(lightShader, "shadowSoftness");
		uniforms.shadowsEnabled = GetShaderLocation(lightShader, "shadowsEnabled");
		uniforms.shadowLightIndex = GetShaderLocation(lightShader, "shadowLightIndex");

		uniforms.depthPass = GetShaderLocation(lightShader, "depthPass");
	}

	void LoadLightingShader()
	{
		const std::string vertexPath = ShaderPath("lighting.vs");
		const std::string fragmentPath = ShaderPath("lighting.fs");
		lightShader = LoadShader(vertexPath.c_str(), fragmentPath.c_str());
		if (!IsShaderValid(lightShader))
		{
			// LoadShader allocates the locs array even on failure — release it
			UnloadShader(lightShader);
			lightShader = Shader{};
			return;
		}

		// raylib resolves mvp/matModel/matNormal/colDiffuse/texture0 by name, but
		// NOT viewPos — SHADER_LOC_VECTOR_VIEW has to be wired up by hand.
		lightShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightShader, "viewPos");

		CacheUniformLocations();
	}

	RenderTexture2D shadowMap = {};
	Matrix shadowViewProjection = {};

	/**
	 * Build a depth-only framebuffer whose depth attachment is a *texture*.
	 *
	 * LoadRenderTexture() cannot be used here: it attaches depth as a
	 * renderbuffer (rlLoadTextureDepth(w, h, true)), which the GPU can render
	 * into but a shader can never sample. Passing useRenderBuffer=false gives a
	 * real texture instead.
	 *
	 * The returned RenderTexture2D is assembled by hand so BeginTextureMode()
	 * still works — it only reads .id and .texture.width/height.
	 */
	RenderTexture2D LoadShadowMapTarget(int resolution)
	{
		RenderTexture2D target = {};

		target.id = rlLoadFramebuffer();
		if (target.id == 0)
		{
			std::cerr << "[Lighting] Failed to create the shadow map framebuffer\n";
			return target;
		}

		rlEnableFramebuffer(target.id);

		target.depth.id = rlLoadTextureDepth(resolution, resolution, false);
		target.depth.width = resolution;
		target.depth.height = resolution;
		target.depth.format = DEPTH_TEXTURE_FORMAT;
		target.depth.mipmaps = 1;

		// BeginTextureMode() reads these to size the viewport and to give
		// BeginMode3D() an aspect ratio (1.0 for a square map).
		target.texture.width = resolution;
		target.texture.height = resolution;

		rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

		// No colour attachment is created on purpose — the depth pass writes
		// nothing but depth. A depth-only FBO is complete under GL 3.3.
		const bool complete = rlFramebufferComplete(target.id);
		rlDisableFramebuffer();

		if (!complete)
		{
			std::cerr << "[Lighting] Shadow map framebuffer is incomplete — shadows disabled\n";
			rlUnloadFramebuffer(target.id);
			return RenderTexture2D{};
		}

		std::cout << "[Lighting] Shadow map ready: " << resolution << "x" << resolution << "\n";
		return target;
	}

	void UnloadShadowMapTarget(RenderTexture2D& target)
	{
		if (target.id == 0) { return; }

		// rlUnloadFramebuffer queries the depth attachment and deletes the
		// texture itself, so calling UnloadTexture(target.depth) as well would
		// be a double free.
		rlUnloadFramebuffer(target.id);
		target = RenderTexture2D{};
	}

	std::string LowerExtension(const std::filesystem::path& path)
	{
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return ext;
	}

	// raylib's tinyobj triangulates each face into a fixed 64-index buffer
	// (TINYOBJ_MAX_FACES_PER_F_LINE) and overflows the stack past it — reject
	// .obj files with bigger faces instead of crashing (re-export triangulated)
	constexpr int MAX_OBJ_FACE_VERTS = 21; // 3 * 21 stays under the 64-index buffer

	bool objFacesFitLoader(const std::filesystem::path& path)
	{
		std::ifstream file(path);
		if (!file.is_open()) { return false; }

		std::string line;
		while (std::getline(file, line))
		{
			if (line.size() < 2 || line[0] != 'f' || (line[1] != ' ' && line[1] != '\t')) { continue; }

			std::istringstream tokens(line);
			std::string token;
			int vertexCount = -1; // first token is the "f" keyword
			while (tokens >> token) { vertexCount++; }

			if (vertexCount > MAX_OBJ_FACE_VERTS)
			{
				std::cerr << "Model has a face with " << vertexCount << " vertices (max "
					<< MAX_OBJ_FACE_VERTS << "): " << path.string() << " — re-export triangulated\n";
				return false;
			}
		}
		return true;
	}

	// Draws a copy of the stored Model with this draw's pose, shader and diffuse texture.
	// `copy` shares the stored model's materials array, so the shader written below is
	// written to the stored materials too (set again at every draw), and the diffuse
	// texture override is put back after the draw.
	template <typename DrawFn>
	void DrawStoredModel(const Render::ModelDraw& draw, DrawFn&& drawFn)
	{
		const Model* stored = ModelOf(draw.model);
		if (stored == nullptr) { return; }

		Model copy = *stored;

		if (lightShader.id != 0 && copy.materials != nullptr)
		{
			for (int i = 0; i < copy.materialCount; i++) { copy.materials[i].shader = lightShader; }
		}

		const Texture2D* overrideTexture = TextureOf(draw.diffuseOverride);
		const bool overridden = overrideTexture != nullptr && copy.materials != nullptr && copy.materialCount > 0;
		Texture2D saved = {};
		if (overridden)
		{
			saved = copy.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture;
			copy.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = *overrideTexture;
		}

		copy.transform = MatrixMultiply(
			MatrixScale(draw.scale.x, draw.scale.y, draw.scale.z),
			QuaternionToMatrix(draw.rotation));
		drawFn(copy);

		if (overridden) { copy.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = saved; }
	}
}

namespace Render {

bool Init()
{
	if (ready) { return true; }
	SeedTextures();
	LoadLightingShader(); // a missing lighting shader leaves id 0 and the game draws unlit
	models.push_back(LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f))); // the unit cube: model handle 1
	ready = true;
	return true;
}

void Shutdown()
{
	// Reverse order of creation. UnloadModel frees meshes and the materials arrays
	// only; a model's own textures are not in the table as owned ones. The lighting
	// shader is unloaded last, below.
	for (std::size_t i = models.size(); i > 0; --i) { UnloadModel(models[i - 1]); }
	models.clear();
	for (std::size_t i = textures.size(); i > 0; --i)
	{
		if (textureOwned[i - 1]) { UnloadTexture(textures[i - 1]); }
	}
	textures.clear();
	textureOwned.clear();
	UnloadShadowMapTarget(shadowMap);
	if (lightShader.id != 0)
	{
		UnloadShader(lightShader);
		lightShader = Shader{};
	}
	uniforms = Uniforms{};
	ready = false;
}

bool IsReady() { return ready; }

LoadedModel LoadModelFile(const std::string& path)
{
	LoadedModel result;
	const std::filesystem::path file(path);

	if (LowerExtension(file) == ".obj" && !objFacesFitLoader(file))
	{
		result.rejected = true;
		return result;
	}

	std::cout << "Load Object: " << path << "\n";
	Model model = LoadModel(path.c_str());
	if (model.meshCount == 0) { return result; }

	// model.transform is deliberately ignored: DrawModel sets it for every draw,
	// so the mesh's untouched local bounds are what a collider fits.
	for (int i = 0; i < model.meshCount; ++i)
	{
		const Mesh& mesh = model.meshes[i];
		if (mesh.vertexCount <= 0 || mesh.vertices == nullptr) { continue; }
		Geometry::ExtendBounds(result.localBounds, Geometry::GetMeshBoundingBox(mesh.vertices, mesh.vertexCount));
	}

	models.push_back(model);
	result.handle = ModelHandle{ static_cast<std::uint32_t>(models.size()) };
	return result;
}

TextureHandle LoadTextureFile(const std::string& path)
{
	SeedTextures();
	std::cout << "Load Texture: " << path << "\n";
	Texture2D texture = LoadTexture(path.c_str());
	if (texture.id == 0) { return {}; }

	textures.push_back(texture);
	textureOwned.push_back(true);
	return TextureHandle{ static_cast<std::uint32_t>(textures.size()) };
}

ModelHandle UnitCube() { return ready ? ModelHandle{ 1 } : ModelHandle{}; }

TextureHandle DiffuseTexture(ModelHandle model, int material)
{
	const Model* stored = ModelOf(model);
	if (stored == nullptr || stored->materials == nullptr || material < 0 || material >= stored->materialCount) { return {}; }

	const unsigned int id = stored->materials[material].maps[MATERIAL_MAP_DIFFUSE].texture.id;
	if (id == 0) { return {}; }

	SeedTextures();
	for (std::size_t i = 0; i < textures.size(); ++i)
	{
		if (textures[i].id == id) { return TextureHandle{ static_cast<std::uint32_t>(i + 1) }; }
	}
	// A texture the model brought with it: lent out as a handle, never unloaded here
	textures.push_back(stored->materials[material].maps[MATERIAL_MAP_DIFFUSE].texture);
	textureOwned.push_back(false);
	return TextureHandle{ static_cast<std::uint32_t>(textures.size()) };
}

std::uintptr_t NativeTextureId(TextureHandle texture)
{
	const Texture2D* stored = TextureOf(texture);
	return stored != nullptr ? static_cast<std::uintptr_t>(stored->id) : 0;
}

void DrawModel(const ModelDraw& draw)
{
	DrawStoredModel(draw, [&](const Model& model) { ::DrawModel(model, draw.translation, 1.0f, draw.tint); });
}

void DrawModelWires(const ModelDraw& draw)
{
	DrawStoredModel(draw, [&](const Model& model) { ::DrawModelWires(model, draw.translation, 1.0f, draw.tint); });
}

void DebugLine(Vector3 a, Vector3 b, Color color) { DrawLine3D(a, b, color); }
void DebugSphere(Vector3 center, float radius, Color color) { DrawSphere(center, radius, color); }
void DebugSphere(Vector3 center, float radius, int rings, int slices, Color color) { DrawSphereEx(center, radius, rings, slices, color); }
void DebugSphereWires(Vector3 center, float radius, int rings, int slices, Color color) { DrawSphereWires(center, radius, rings, slices, color); }
void DebugCube(Vector3 center, Vector3 size, Color color) { DrawCubeV(center, size, color); }
void DebugBoundingBox(BoundingBox box, Color color) { DrawBoundingBox(box, color); }
void DebugCylinder(Vector3 start, Vector3 end, float startRadius, float endRadius, int sides, Color color) { DrawCylinderEx(start, end, startRadius, endRadius, sides, color); }
void DebugCapsule(Vector3 start, Vector3 end, float radius, int slices, int rings, Color color) { DrawCapsule(start, end, radius, slices, rings, color); }
void DebugGrid(int slices, float spacing) { DrawGrid(slices, spacing); }

void BeginFrame(Color clear) { ClearBackground(clear); }
void EndFrame() {}
void BeginWorld(const Camera3D& camera)
{
	BeginMode3D(camera);

	if (shadowMap.depth.id != 0)
	{
		// Bound to SHADOW_MAP_SLOT (15) because raylib's DrawMesh() owns slots
		// 0..MAX_MATERIAL_MAPS-1 (12) and rebinds them on every draw call.
		rlActiveTextureSlot(SHADOW_MAP_SLOT);
		rlEnableTexture(shadowMap.depth.id);
		rlActiveTextureSlot(0);

		if (lightShader.id != 0)
		{
			// The sampler only needs the unit index, and it persists in the program
			// object across raylib's own rlEnableShader/rlDisableShader calls.
			const int slot = SHADOW_MAP_SLOT;
			SetShaderValue(lightShader, uniforms.shadowMap, &slot, SHADER_UNIFORM_INT);
		}
	}
}
void EndWorld() { EndMode3D(); }
void SetDebugDepthTest(bool on)
{
	rlDrawRenderBatchActive();
	if (on) { rlEnableDepthTest(); } else { rlDisableDepthTest(); }
}
void Begin2D(const Camera2D& camera) { BeginMode2D(camera); }
void End2D() { EndMode2D(); }
void CaptureScreenshot(const std::string& fileName)
{
	rlDrawRenderBatchActive();
	TakeScreenshot(fileName.c_str());
}

void Rect(Rectangle rect, Color color) { DrawRectangleRec(rect, color); }
void RectOutline(Rectangle rect, float thickness, Color color) { DrawRectangleLinesEx(rect, thickness, color); }
void Circle(Vector2 center, float radius, Color color) { DrawCircleV(center, radius, color); }
void Text(const std::string& text, Vector2 topLeft, int fontSize, Color color)
{
	DrawText(text.c_str(), static_cast<int>(topLeft.x), static_cast<int>(topLeft.y), fontSize, color);
}
int TextWidth(const std::string& text, int fontSize) { return MeasureText(text.c_str(), fontSize); }

bool SetShadowMapSize(int size)
{
	if (size == ShadowMapSize()) { return true; }

	UnloadShadowMapTarget(shadowMap);
	if (size > 0) { shadowMap = LoadShadowMapTarget(size); }
	return ShadowMapSize() == size;
}

int ShadowMapSize() { return shadowMap.id != 0 ? shadowMap.texture.width : 0; }

bool BeginShadowPass(const Camera3D& lightCamera)
{
	if (shadowMap.id == 0) { return false; }

	// depthPass makes the fragment shader return immediately. Without it the
	// shader would sample shadowMap while that same texture is attached as the
	// depth target — undefined behaviour in OpenGL.
	if (lightShader.id != 0)
	{
		const int depthPassOn = 1;
		SetShaderValue(lightShader, uniforms.depthPass, &depthPassOn, SHADER_UNIFORM_INT);
	}

	BeginTextureMode(shadowMap);
	ClearBackground(WHITE);
	BeginMode3D(lightCamera);
	// Captured inside BeginMode3D so rlgl holds the light's matrices.
	// MatrixMultiply(view, projection) matches raylib's row-vector
	// convention, so the shader can do lightVP*vec4(worldPos, 1.0).
	shadowViewProjection = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
	return true;
}

void EndShadowPass()
{
	EndMode3D();
	EndTextureMode();

	if (lightShader.id != 0)
	{
		const int depthPassOff = 0;
		SetShaderValue(lightShader, uniforms.depthPass, &depthPassOff, SHADER_UNIFORM_INT);
		SetShaderValueMatrix(lightShader, uniforms.lightVP, shadowViewProjection);
	}
}

Matrix ShadowViewProjection() { return shadowViewProjection; }

unsigned int LightingShaderId() { return lightShader.id; }

void SetLighting(const LightingFrame& frame)
{
	if (lightShader.id == 0) { return; }

	// viewPos drives both the specular term and the fog distance
	SetShaderValue(lightShader, lightShader.locs[SHADER_LOC_VECTOR_VIEW],
		&frame.viewPosition, SHADER_UNIFORM_VEC3);

	// Always sized to kMaxLights and uploaded whole: a shrinking light list
	// would otherwise leave last frame's values in the unused slots.
	int types[kMaxLights] = {};
	float positions[kMaxLights * 3] = {};
	float directions[kMaxLights * 3] = {};
	float colors[kMaxLights * 4] = {};
	float ranges[kMaxLights] = {};
	float cones[kMaxLights * 2] = {};

	for (int slot = 0; slot < kMaxLights; slot++)
	{
		const PackedLight& light = frame.lights[slot];

		types[slot] = light.type;

		positions[slot * 3 + 0] = light.position.x;
		positions[slot * 3 + 1] = light.position.y;
		positions[slot * 3 + 2] = light.position.z;

		directions[slot * 3 + 0] = light.direction.x;
		directions[slot * 3 + 1] = light.direction.y;
		directions[slot * 3 + 2] = light.direction.z;

		colors[slot * 4 + 0] = light.color.x;
		colors[slot * 4 + 1] = light.color.y;
		colors[slot * 4 + 2] = light.color.z;
		colors[slot * 4 + 3] = light.color.w;

		ranges[slot] = light.range;

		cones[slot * 2 + 0] = light.cone.x;
		cones[slot * 2 + 1] = light.cone.y;
	}

	SetShaderValue(lightShader, uniforms.lightCount, &frame.lightCount, SHADER_UNIFORM_INT);
	SetShaderValueV(lightShader, uniforms.lightType, types, SHADER_UNIFORM_INT, kMaxLights);
	SetShaderValueV(lightShader, uniforms.lightPosition, positions, SHADER_UNIFORM_VEC3, kMaxLights);
	SetShaderValueV(lightShader, uniforms.lightDirection, directions, SHADER_UNIFORM_VEC3, kMaxLights);
	SetShaderValueV(lightShader, uniforms.lightColor, colors, SHADER_UNIFORM_VEC4, kMaxLights);
	SetShaderValueV(lightShader, uniforms.lightRange, ranges, SHADER_UNIFORM_FLOAT, kMaxLights);
	SetShaderValueV(lightShader, uniforms.lightCone, cones, SHADER_UNIFORM_VEC2, kMaxLights);

	// The shader compares against the *packed* slot, not the lights[] index —
	// disabled lights and the flashlight shift everything after them.
	SetShaderValue(lightShader, uniforms.shadowLightIndex, &frame.shadowLightSlot, SHADER_UNIFORM_INT);

	const float ambientValues[4] = { frame.ambient.x, frame.ambient.y, frame.ambient.z, frame.ambient.w };
	SetShaderValue(lightShader, uniforms.ambient, ambientValues, SHADER_UNIFORM_VEC4);

	const float fogValues[4] = { frame.fogColor.x, frame.fogColor.y, frame.fogColor.z, frame.fogColor.w };
	SetShaderValue(lightShader, uniforms.fogColor, fogValues, SHADER_UNIFORM_VEC4);

	SetShaderValue(lightShader, uniforms.fogDensity, &frame.fogDensity, SHADER_UNIFORM_FLOAT);
	SetShaderValue(lightShader, uniforms.exposure, &frame.exposure, SHADER_UNIFORM_FLOAT);
	SetShaderValue(lightShader, uniforms.specularStrength, &frame.specularStrength, SHADER_UNIFORM_FLOAT);
	SetShaderValue(lightShader, uniforms.shininess, &frame.shininess, SHADER_UNIFORM_FLOAT);
	SetShaderValue(lightShader, uniforms.shadowTexelSize, &frame.shadowTexelSize, SHADER_UNIFORM_FLOAT);
	SetShaderValue(lightShader, uniforms.shadowBias, &frame.shadowBias, SHADER_UNIFORM_FLOAT);
	SetShaderValue(lightShader, uniforms.shadowSoftness, &frame.shadowSoftness, SHADER_UNIFORM_FLOAT);

	// Single source of truth for the shader's shadow gate: shadows are only
	// sampled when they are enabled, a framebuffer exists, and a directional
	// light actually claimed it this frame. Anything else and the shader would
	// read a stale or never-written depth texture. LightingSystem resolves it.
	const int shadowGate = frame.shadowsEnabled ? 1 : 0;
	SetShaderValue(lightShader, uniforms.shadowsEnabled, &shadowGate, SHADER_UNIFORM_INT);
}

}
