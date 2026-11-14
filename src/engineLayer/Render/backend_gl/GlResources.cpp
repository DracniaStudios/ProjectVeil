// Render's resource functions on OpenGL 3.3 core (B-20): textures, meshes, models and the
// unit cube, uploaded from the CPU-side decoder (AssetDecode.h) and kept in tables behind
// the handles. Drawing the models is B-21.
//
// The answers match the raylib backend's: a handle's id is its table index + 1, handle 1 of
// the texture table is the default white texture, handle 1 of the model table is the unit
// cube (made at Init), repeated loads of one path are separate entries, nothing is removed
// before Shutdown.
//
// The texture formats, swizzles and parameters follow raylib 6.0's rlLoadTexture and
// rlGetGlTextureFormats (rlgl.h, GL 3.3 branch). The mesh attribute layout follows UploadMesh
// (rmodels.c). The unit cube tables are ported from GenMeshCube (rmodels.c, CUSTOM_MESH_GEN_CUBE):
//   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
// Used under raylib's zlib/libpng licence, which permits use and alteration with this notice
// kept; this file is an altered version and is not raylib's original source.
#include <Render/Render.h>
#include "GlShared.h"
#include "AssetDecode.h"

#include <glad/gl.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

namespace {
	using Render::ModelHandle;
	using Render::TextureHandle;

	struct TextureEntry {
		unsigned int id = 0;
		int width = 0, height = 0;
		bool owned = false; // false for the default white texture, which Gl2D owns
	};

	bool ready = false;
	std::vector<TextureEntry> textures;
	std::vector<GlResources::Model> models;

	// Where each attribute lives in the shaders (ADR B section 7) and in Mesh::buffers
	constexpr unsigned int kPosition = 0, kTexcoord = 1, kNormal = 2, kColor = 3, kIndex = 4;

	void DrainErrors() { while (glGetError() != GL_NO_ERROR) {} }

	std::string LowerExtension(const std::string& path)
	{
		std::string ext = std::filesystem::path(path).extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return ext;
	}

	TextureHandle WhiteHandle() { return TextureHandle{ 1 }; }

	// 0 on failure. Leaves the texture binding and the unpack alignment as they were.
	unsigned int UploadTexture(const Render::Decode::Image& image)
	{
		if (image.width <= 0 || image.height <= 0 || image.channels < 1 || image.channels > 4) { return 0; }
		const std::size_t needed = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * static_cast<std::size_t>(image.channels);
		if (image.pixels.size() < needed) { return 0; }

		// raylib's GL 3.3 table: 1 -> R8, 2 -> RG8, 3 -> RGB8, 4 -> RGBA8
		GLint internalFormat = GL_RGBA8;
		GLenum format = GL_RGBA;
		switch (image.channels) {
		case 1: internalFormat = GL_R8;   format = GL_RED;  break;
		case 2: internalFormat = GL_RG8;  format = GL_RG;   break;
		case 3: internalFormat = GL_RGB8; format = GL_RGB;  break;
		default: break;
		}

		DrainErrors();
		GLint previousAlignment = 4, previousBinding = 0;
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousBinding);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		GLuint id = 0;
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0, format, GL_UNSIGNED_BYTE, image.pixels.data());

		if (image.channels == 1) {
			const GLint swizzle[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
			glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
		} else if (image.channels == 2) {
			const GLint swizzle[] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
			glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousBinding));
		glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);

		const GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			std::cerr << "[Render] texture upload failed (GL error 0x" << std::hex << error << std::dec << ")" << std::endl;
			glDeleteTextures(1, &id);
			DrainErrors();
			return 0;
		}
		return id;
	}

	TextureHandle AddTexture(unsigned int id, int width, int height)
	{
		textures.push_back(TextureEntry{ id, width, height, true });
		return TextureHandle{ static_cast<std::uint32_t>(textures.size()) };
	}

	void ReleaseMesh(GlResources::Mesh& mesh)
	{
		for (unsigned int& buffer : mesh.buffers) {
			if (buffer != 0) { glDeleteBuffers(1, &buffer); buffer = 0; }
		}
		if (mesh.vao != 0) { glDeleteVertexArrays(1, &mesh.vao); mesh.vao = 0; }
	}

	GLuint MakeBuffer(GLenum target, const void* data, std::size_t bytes)
	{
		GLuint buffer = 0;
		glGenBuffers(1, &buffer);
		glBindBuffer(target, buffer);
		glBufferData(target, static_cast<GLsizeiptr>(bytes), data, GL_STATIC_DRAW);
		return buffer;
	}

	// One VAO, a separate buffer per attribute. An absent attribute's array stays disabled.
	// False on a GL error (the mesh is released). Leaves no VAO or array buffer bound.
	bool UploadMesh(const Render::Decode::MeshData& data, GlResources::Mesh& mesh)
	{
		DrainErrors();
		mesh = GlResources::Mesh{};
		mesh.vertexCount = static_cast<int>(data.positions.size() / 3);
		mesh.indexCount = static_cast<int>(data.indices.size());
		mesh.material = data.material;
		mesh.hasTexcoords = !data.texcoords.empty();
		mesh.hasNormals = !data.normals.empty();
		mesh.hasColors = !data.colors.empty();

		glGenVertexArrays(1, &mesh.vao);
		glBindVertexArray(mesh.vao);

		mesh.buffers[kPosition] = MakeBuffer(GL_ARRAY_BUFFER, data.positions.data(), data.positions.size() * sizeof(float));
		glVertexAttribPointer(kPosition, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		glEnableVertexAttribArray(kPosition);

		if (mesh.hasTexcoords) {
			mesh.buffers[kTexcoord] = MakeBuffer(GL_ARRAY_BUFFER, data.texcoords.data(), data.texcoords.size() * sizeof(float));
			glVertexAttribPointer(kTexcoord, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
			glEnableVertexAttribArray(kTexcoord);
		}
		if (mesh.hasNormals) {
			mesh.buffers[kNormal] = MakeBuffer(GL_ARRAY_BUFFER, data.normals.data(), data.normals.size() * sizeof(float));
			glVertexAttribPointer(kNormal, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
			glEnableVertexAttribArray(kNormal);
		}
		if (mesh.hasColors) {
			mesh.buffers[kColor] = MakeBuffer(GL_ARRAY_BUFFER, data.colors.data(), data.colors.size());
			glVertexAttribPointer(kColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, nullptr);
			glEnableVertexAttribArray(kColor);
		}
		if (mesh.indexCount > 0) {
			// Bound while the VAO is, which records it
			mesh.buffers[kIndex] = MakeBuffer(GL_ELEMENT_ARRAY_BUFFER, data.indices.data(), data.indices.size() * sizeof(std::uint32_t));
		}

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		const GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			std::cerr << "[Render] mesh upload failed (GL error 0x" << std::hex << error << std::dec << ")" << std::endl;
			ReleaseMesh(mesh);
			DrainErrors();
			return false;
		}
		return true;
	}

	void ReleaseModel(GlResources::Model& model)
	{
		for (GlResources::Mesh& mesh : model.meshes) { ReleaseMesh(mesh); }
		model.meshes.clear();
	}

	// raylib's GenMeshCube(1, 1, 1): 24 vertices, 6 faces of 4, 36 indices
	Render::Decode::MeshData UnitCubeData()
	{
		constexpr float w = 1.0f, h = 1.0f, l = 1.0f;
		const float vertices[] = {
			-w/2, -h/2,  l/2,   w/2, -h/2,  l/2,   w/2,  h/2,  l/2,  -w/2,  h/2,  l/2,
			-w/2, -h/2, -l/2,  -w/2,  h/2, -l/2,   w/2,  h/2, -l/2,   w/2, -h/2, -l/2,
			-w/2,  h/2, -l/2,  -w/2,  h/2,  l/2,   w/2,  h/2,  l/2,   w/2,  h/2, -l/2,
			-w/2, -h/2, -l/2,   w/2, -h/2, -l/2,   w/2, -h/2,  l/2,  -w/2, -h/2,  l/2,
			 w/2, -h/2, -l/2,   w/2,  h/2, -l/2,   w/2,  h/2,  l/2,   w/2, -h/2,  l/2,
			-w/2, -h/2, -l/2,  -w/2, -h/2,  l/2,  -w/2,  h/2,  l/2,  -w/2,  h/2, -l/2,
		};
		const float texcoords[] = {
			0, 0,  1, 0,  1, 1,  0, 1,
			1, 0,  1, 1,  0, 1,  0, 0,
			0, 1,  0, 0,  1, 0,  1, 1,
			1, 1,  0, 1,  0, 0,  1, 0,
			1, 0,  1, 1,  0, 1,  0, 0,
			0, 0,  1, 0,  1, 1,  0, 1,
		};
		const float normals[] = {
			0, 0, 1,   0, 0, 1,   0, 0, 1,   0, 0, 1,
			0, 0,-1,   0, 0,-1,   0, 0,-1,   0, 0,-1,
			0, 1, 0,   0, 1, 0,   0, 1, 0,   0, 1, 0,
			0,-1, 0,   0,-1, 0,   0,-1, 0,   0,-1, 0,
			1, 0, 0,   1, 0, 0,   1, 0, 0,   1, 0, 0,
		   -1, 0, 0,  -1, 0, 0,  -1, 0, 0,  -1, 0, 0,
		};

		Render::Decode::MeshData cube;
		cube.positions.assign(std::begin(vertices), std::end(vertices));
		cube.texcoords.assign(std::begin(texcoords), std::end(texcoords));
		cube.normals.assign(std::begin(normals), std::end(normals));
		for (std::uint32_t face = 0; face < 6; ++face) {
			const std::uint32_t k = 4 * face;
			for (const std::uint32_t offset : { 0u, 1u, 2u, 0u, 2u, 3u }) { cube.indices.push_back(k + offset); }
		}
		cube.material = 0;
		return cube;
	}

	void ReleaseEverything()
	{
		for (std::size_t i = models.size(); i > 0; --i) { ReleaseModel(models[i - 1]); }
		models.clear();
		for (std::size_t i = textures.size(); i > 0; --i) {
			if (textures[i - 1].owned) { glDeleteTextures(1, &textures[i - 1].id); }
		}
		textures.clear();
	}
}

namespace GlResources {

bool Init()
{
	if (ready) { return true; }
	ReleaseEverything();

	// Texture handle 1: the default white texture, which Gl2D owns
	textures.push_back(TextureEntry{ Gl2D::WhiteTexture(), 1, 1, false });

	// Model handle 1: the unit cube, one default material
	Model cube;
	cube.materials.push_back(Material{ WhiteHandle(), WHITE });
	cube.meshes.resize(1);
	if (!UploadMesh(UnitCubeData(), cube.meshes[0])) {
		ReleaseEverything();
		return false;
	}
	models.push_back(std::move(cube));
	ready = true;
	return true;
}

void Shutdown()
{
	ReleaseEverything();
	ready = false;
}

const Model* FindModel(Render::ModelHandle handle)
{
	return (handle.id >= 1 && handle.id <= models.size()) ? &models[handle.id - 1] : nullptr;
}

unsigned int TextureName(Render::TextureHandle handle)
{
	return (handle.id >= 1 && handle.id <= textures.size()) ? textures[handle.id - 1].id : 0;
}

}

namespace Render {

LoadedModel LoadModelFile(const std::string& path)
{
	LoadedModel result;
	if (!ready) { return result; }

	if (LowerExtension(path) == ".obj") {
		// Refused before loading, as the raylib build refuses an oversized .obj face: the reason is on
		// stderr, so the AssetManager skips the file without a second "Failed to load model" line
		std::cerr << "[Render] OBJ is not supported in the sdl3gl build yet (B-25)" << std::endl;
		result.rejected = true;
		return result;
	}

	std::cout << "Load Object: " << path << "\n";
	std::optional<Decode::ModelData> decoded = Decode::LoadGltf(path); // reports its own failures
	if (!decoded) { return result; }

	GlResources::Model model;
	model.meshes.resize(decoded->meshes.size());
	std::vector<std::size_t> addedTextures; // table indices of this model's textures, for a rollback
	bool failed = false;

	// Materials first: a texture that fails to upload leaves the material on the white default
	for (const Decode::MaterialData& source : decoded->materials) {
		GlResources::Material material;
		material.color = source.diffuseColor;
		material.diffuse = WhiteHandle();
		if (source.diffuseImage) {
			const unsigned int id = UploadTexture(*source.diffuseImage);
			if (id != 0) {
				material.diffuse = AddTexture(id, source.diffuseImage->width, source.diffuseImage->height);
				addedTextures.push_back(textures.size() - 1);
			} else {
				std::cerr << "[Render] a texture of " << path << " could not be uploaded; its material stays white" << std::endl;
			}
		}
		model.materials.push_back(material);
	}

	for (std::size_t i = 0; i < decoded->meshes.size(); ++i) {
		if (decoded->meshes[i].material < 0 || static_cast<std::size_t>(decoded->meshes[i].material) >= model.materials.size()) {
			decoded->meshes[i].material = 0;
		}
		if (!UploadMesh(decoded->meshes[i], model.meshes[i])) { failed = true; break; }
		// The CPU copy is not needed once it is on the GPU
		decoded->meshes[i] = Decode::MeshData{ {}, {}, {}, {}, {}, decoded->meshes[i].material };
	}

	if (failed) {
		ReleaseModel(model);
		for (std::size_t i = addedTextures.size(); i > 0; --i) {
			glDeleteTextures(1, &textures[addedTextures[i - 1]].id);
		}
		// The failed model's textures are the newest entries: drop them
		if (!addedTextures.empty()) { textures.resize(addedTextures.front()); }
		return result;
	}

	result.localBounds = decoded->localBounds;
	models.push_back(std::move(model));
	result.handle = ModelHandle{ static_cast<std::uint32_t>(models.size()) };
	return result;
}

TextureHandle LoadTextureFile(const std::string& path)
{
	if (!ready) { return {}; }
	std::cout << "Load Texture: " << path << "\n";
	const std::optional<Decode::Image> image = Decode::LoadImageFile(path); // reports its own failures
	if (!image) { return {}; }

	const unsigned int id = UploadTexture(*image);
	if (id == 0) { return {}; }
	return AddTexture(id, image->width, image->height);
}

ModelHandle UnitCube() { return ready ? ModelHandle{ 1 } : ModelHandle{}; }

TextureHandle DiffuseTexture(ModelHandle model, int material)
{
	const GlResources::Model* stored = GlResources::FindModel(model);
	if (stored == nullptr || material < 0 || static_cast<std::size_t>(material) >= stored->materials.size()) { return {}; }
	return stored->materials[material].diffuse;
}

std::uintptr_t NativeTextureId(TextureHandle texture)
{
	return static_cast<std::uintptr_t>(GlResources::TextureName(texture));
}

}
