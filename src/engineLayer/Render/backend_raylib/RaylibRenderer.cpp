// Render on raylib (B-07 lifetime, B-08 model and texture tables and model draws).
#include <Render/Render.h>

#include <Geometry.h>
#include <LightingSystem.h> // temporary: the shader bound at every draw; removed by B-12

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

		Lighting::LightingSystem& lighting = Lighting::LightingSystem::getInstance();
		if (lighting.IsReady() && copy.materials != nullptr)
		{
			for (int i = 0; i < copy.materialCount; i++) { copy.materials[i].shader = lighting.GetShader(); }
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
	models.push_back(LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f))); // the unit cube: model handle 1
	ready = true;
	return true;
}

void Shutdown()
{
	// Reverse order of creation. UnloadModel frees meshes and the materials arrays
	// only; a model's own textures are not in the table as owned ones, and shaders
	// belong to whoever made them (the lighting system releases its own).
	for (std::size_t i = models.size(); i > 0; --i) { UnloadModel(models[i - 1]); }
	models.clear();
	for (std::size_t i = textures.size(); i > 0; --i)
	{
		if (textureOwned[i - 1]) { UnloadTexture(textures[i - 1]); }
	}
	textures.clear();
	textureOwned.clear();
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

void Rect(Rectangle rect, Color color) { DrawRectangleRec(rect, color); }
void RectOutline(Rectangle rect, float thickness, Color color) { DrawRectangleLinesEx(rect, thickness, color); }
void Circle(Vector2 center, float radius, Color color) { DrawCircleV(center, radius, color); }
void Text(const std::string& text, Vector2 topLeft, int fontSize, Color color)
{
	DrawText(text.c_str(), static_cast<int>(topLeft.x), static_cast<int>(topLeft.y), fontSize, color);
}
int TextWidth(const std::string& text, int fontSize) { return MeasureText(text.c_str(), fontSize); }

}
