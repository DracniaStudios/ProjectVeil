// CPU-side glTF and image decoding for the GL backend (B-19). See AssetDecode.h.
//
// No GL, SDL, window, Platform:: or Render:: call here. raylib's headers are used for the
// Color/BoundingBox/Matrix types and raymath's inline maths only; raylib is not linked.
// stb_image and cgltf come from the upstream copies in thirdparty/ (declarations here, the
// implementations in ThirdPartyImpl.cpp).
//
// The rules are those of raylib 6.0's LoadGLTF (rmodels.c) and LoadImageFromCgltfImage, so
// the meshes, materials and bounds equal what LoadModel gives. Where this file does
// something else on purpose it says so:
//  - 32-bit indices stay 32-bit (raylib truncates them to 16 bits).
//  - Accessor reads use byte offsets and strides (raylib divides the offsets by the
//    component size, which is wrong for unaligned views). Identical for valid aligned files.
//  - Only the base-colour image of each material is decoded; raylib also loads the
//    metallic-roughness, normal, occlusion and emissive images, which the GL backend
//    never samples.
//  - A read past the end of its buffer, a missing buffer view, an unsupported index type,
//    or an attribute whose count differs from the position count fails softly (the
//    attribute is dropped, or the load returns nullopt for indices) where raylib would
//    read out of bounds.
#include "AssetDecode.h"

#include <raymath.h>

#include <stb_image.h>
#include <cgltf.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

namespace Render::Decode {
namespace {

void Report(const std::string& what, const std::string& reason)
{
	std::cerr << "[AssetDecode] " << what << ": " << reason << "\n";
}

std::filesystem::path Utf8Path(const std::string& path)
{
	return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
}

bool ReadFile(const std::string& path, std::vector<std::uint8_t>& bytes)
{
	std::ifstream in(Utf8Path(path), std::ios::binary | std::ios::ate);
	if (!in) { return false; }
	const std::streamoff size = in.tellg();
	if (size <= 0) { return false; }
	bytes.resize(static_cast<std::size_t>(size));
	in.seekg(0);
	in.read(reinterpret_cast<char*>(bytes.data()), size);
	return static_cast<bool>(in);
}

std::string DirectoryOf(const std::string& path)
{
	const std::size_t slash = path.find_last_of("/\\");
	return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

std::string LowerExtension(const std::string& path)
{
	const std::size_t dot = path.find_last_of('.');
	if (dot == std::string::npos || path.find_last_of("/\\") > dot) { return {}; }
	std::string ext = path.substr(dot);
	for (char& c : ext) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
	return ext;
}

std::optional<Image> DecodeImage(std::span<const std::uint8_t> bytes, const std::string& what)
{
	if (bytes.empty() || bytes.size() > static_cast<std::size_t>(INT_MAX))
	{
		Report(what, "no image data");
		return std::nullopt;
	}

	// stbi_load_from_memory(..., 0): the file's own channel count, as raylib's LoadImageFromMemory does
	int w = 0, h = 0, channels = 0;
	stbi_uc* decoded = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &channels, 0);
	if (decoded == nullptr)
	{
		const char* why = stbi_failure_reason();
		Report(what, std::string("not a PNG or BMP image this decoder reads (") + (why != nullptr ? why : "unknown") + ")");
		return std::nullopt;
	}

	Image image;
	image.width = w;
	image.height = h;
	image.channels = channels;
	image.pixels.assign(decoded, decoded + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * static_cast<std::size_t>(channels));
	stbi_image_free(decoded);
	return image;
}

// --- glTF accessors --------------------------------------------------------------------------

// Pointer to element k of an accessor, or null if the data is missing or out of the buffer.
// Reads go through memcpy, so alignment does not matter.
const std::uint8_t* AccessorBase(const cgltf_accessor* acc, std::size_t componentBytes, std::size_t components)
{
	if (acc == nullptr || acc->buffer_view == nullptr || acc->buffer_view->buffer == nullptr) { return nullptr; }
	const cgltf_buffer* buffer = acc->buffer_view->buffer;
	if (buffer->data == nullptr || acc->count == 0) { return nullptr; }

	const std::size_t start = acc->buffer_view->offset + acc->offset;
	const std::size_t stride = acc->stride;
	const std::size_t elementBytes = componentBytes * components;
	if (stride < elementBytes) { return nullptr; }
	const std::size_t end = start + (acc->count - 1) * stride + elementBytes;
	if (end > buffer->size) { return nullptr; }
	return static_cast<const std::uint8_t*>(buffer->data) + start;
}

// Reads every element of the accessor as `N` components of type Src, converted to Dst
template <class Src, class Dst, int N>
bool ReadAccessor(const cgltf_accessor* acc, std::vector<Dst>& out)
{
	const std::uint8_t* base = AccessorBase(acc, sizeof(Src), N);
	if (base == nullptr) { return false; }
	out.resize(static_cast<std::size_t>(acc->count) * N);
	for (std::size_t k = 0; k < acc->count; ++k)
	{
		const std::uint8_t* element = base + k * acc->stride;
		for (int l = 0; l < N; ++l)
		{
			Src value;
			std::memcpy(&value, element + l * sizeof(Src), sizeof(Src));
			out[k * N + l] = static_cast<Dst>(value);
		}
	}
	return true;
}

std::uint8_t ToByte(float v) { return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 255.0f)); }

bool IsType(const cgltf_accessor* a, cgltf_type type, cgltf_component_type component)
{
	return a->type == type && a->component_type == component;
}

// POSITION: vec3 of float, u16 or i16 (the integer ones raw, as raylib does), then the world matrix
bool LoadPositions(const cgltf_accessor* a, const Matrix& world, std::vector<float>& out)
{
	bool ok = false;
	if (IsType(a, cgltf_type_vec3, cgltf_component_type_r_32f)) { ok = ReadAccessor<float, float, 3>(a, out); }
	else if (IsType(a, cgltf_type_vec3, cgltf_component_type_r_16u)) { ok = ReadAccessor<std::uint16_t, float, 3>(a, out); }
	else if (IsType(a, cgltf_type_vec3, cgltf_component_type_r_16)) { ok = ReadAccessor<std::int16_t, float, 3>(a, out); }
	if (!ok) { return false; }

	for (std::size_t k = 0; k < a->count; ++k)
	{
		const Vector3 v = Vector3Transform(Vector3{ out[3 * k], out[3 * k + 1], out[3 * k + 2] }, world);
		out[3 * k] = v.x; out[3 * k + 1] = v.y; out[3 * k + 2] = v.z;
	}
	return true;
}

// NORMAL: vec3 of float (transformed only), or i16, u8, i8 (raw values, transformed, then normalised)
bool LoadNormals(const cgltf_accessor* a, const Matrix& normalMatrix, std::vector<float>& out)
{
	bool normalise = true;
	bool ok = false;
	if (IsType(a, cgltf_type_vec3, cgltf_component_type_r_32f)) { ok = ReadAccessor<float, float, 3>(a, out); normalise = false; }
	else if (IsType(a, cgltf_type_vec3, cgltf_component_type_r_16)) { ok = ReadAccessor<std::int16_t, float, 3>(a, out); }
	else if (IsType(a, cgltf_type_vec3, cgltf_component_type_r_8u)) { ok = ReadAccessor<std::uint8_t, float, 3>(a, out); }
	else if (IsType(a, cgltf_type_vec3, cgltf_component_type_r_8)) { ok = ReadAccessor<std::int8_t, float, 3>(a, out); }
	if (!ok) { return false; }

	for (std::size_t k = 0; k < a->count; ++k)
	{
		Vector3 v = Vector3Transform(Vector3{ out[3 * k], out[3 * k + 1], out[3 * k + 2] }, normalMatrix);
		if (normalise) { v = Vector3Normalize(v); }
		out[3 * k] = v.x; out[3 * k + 1] = v.y; out[3 * k + 2] = v.z;
	}
	return true;
}

// TEXCOORD_0: vec2 of float, or u8 / u16 divided by 255 / 65535
bool LoadTexcoords(const cgltf_accessor* a, std::vector<float>& out)
{
	if (a->type != cgltf_type_vec2) { return false; }
	if (a->component_type == cgltf_component_type_r_32f) { return ReadAccessor<float, float, 2>(a, out); }
	if (a->component_type == cgltf_component_type_r_8u)
	{
		if (!ReadAccessor<std::uint8_t, float, 2>(a, out)) { return false; }
		for (float& v : out) { v = v / 255.0f; }
		return true;
	}
	if (a->component_type == cgltf_component_type_r_16u)
	{
		if (!ReadAccessor<std::uint16_t, float, 2>(a, out)) { return false; }
		for (float& v : out) { v = v / 65535.0f; }
		return true;
	}
	return false;
}

// COLOR_0: vec3 or vec4 of u8, u16 or float, to rgba8 (alpha 255 for vec3)
bool LoadColors(const cgltf_accessor* a, std::vector<std::uint8_t>& out)
{
	int n = 0;
	if (a->type == cgltf_type_vec3) { n = 3; }
	else if (a->type == cgltf_type_vec4) { n = 4; }
	else { return false; }

	switch (a->component_type)
	{
	case cgltf_component_type_r_8u:
	{
		std::vector<std::uint8_t> raw;
		if (!(n == 3 ? ReadAccessor<std::uint8_t, std::uint8_t, 3>(a, raw) : ReadAccessor<std::uint8_t, std::uint8_t, 4>(a, raw))) { return false; }
		out.resize(a->count * 4);
		for (std::size_t k = 0; k < a->count; ++k)
		{
			for (int l = 0; l < 3; ++l) { out[4 * k + l] = raw[n * k + l]; }
			out[4 * k + 3] = n == 4 ? raw[4 * k + 3] : 255;
		}
		return true;
	}
	case cgltf_component_type_r_16u:
	{
		std::vector<std::uint16_t> raw;
		if (!(n == 3 ? ReadAccessor<std::uint16_t, std::uint16_t, 3>(a, raw) : ReadAccessor<std::uint16_t, std::uint16_t, 4>(a, raw))) { return false; }
		out.resize(a->count * 4);
		for (std::size_t k = 0; k < a->count; ++k)
		{
			for (int l = 0; l < 3; ++l) { out[4 * k + l] = ToByte((static_cast<float>(raw[n * k + l]) / 65535.0f) * 255.0f); }
			out[4 * k + 3] = n == 4 ? ToByte((static_cast<float>(raw[4 * k + 3]) / 65535.0f) * 255.0f) : 255;
		}
		return true;
	}
	case cgltf_component_type_r_32f:
	{
		std::vector<float> raw;
		if (!(n == 3 ? ReadAccessor<float, float, 3>(a, raw) : ReadAccessor<float, float, 4>(a, raw))) { return false; }
		out.resize(a->count * 4);
		for (std::size_t k = 0; k < a->count; ++k)
		{
			for (int l = 0; l < 3; ++l) { out[4 * k + l] = ToByte(raw[n * k + l] * 255.0f); }
			out[4 * k + 3] = n == 4 ? ToByte(raw[4 * k + 3] * 255.0f) : 255;
		}
		return true;
	}
	default:
		return false;
	}
}

// u8, u16 or u32 indices, all to u32
bool LoadIndices(const cgltf_accessor* a, std::vector<std::uint32_t>& out)
{
	if (a->type != cgltf_type_scalar) { return false; }
	switch (a->component_type)
	{
	case cgltf_component_type_r_8u: return ReadAccessor<std::uint8_t, std::uint32_t, 1>(a, out);
	case cgltf_component_type_r_16u: return ReadAccessor<std::uint16_t, std::uint32_t, 1>(a, out);
	case cgltf_component_type_r_32u: return ReadAccessor<std::uint32_t, std::uint32_t, 1>(a, out);
	default: return false;
	}
}

// --- glTF images ----------------------------------------------------------------------------

// As LoadImageFromCgltfImage: a data URI, a file next to the glTF, or a buffer view. The uri
// is used as written (raylib does not percent-decode it).
std::optional<Image> LoadCgltfImage(const cgltf_image* image, const std::string& directory, const std::string& modelPath)
{
	if (image == nullptr) { return std::nullopt; }
	const std::string what = modelPath + " (image)";

	if (image->uri != nullptr)
	{
		const std::string uri = image->uri;
		if (uri.size() > 5 && uri.compare(0, 5, "data:") == 0)
		{
			const std::size_t comma = uri.find(',');
			if (comma == std::string::npos) { Report(what, "glTF data URI is not a valid image"); return std::nullopt; }

			std::size_t base64Size = uri.size() - comma - 1;
			while (base64Size > 0 && uri[comma + base64Size] == '=') { --base64Size; } // optional padding
			const std::size_t outSize = (base64Size * 6 - (base64Size * 6) % 8) / 8;

			cgltf_options options = {};
			void* data = nullptr;
			if (cgltf_load_buffer_base64(&options, outSize, uri.c_str() + comma + 1, &data) != cgltf_result_success)
			{
				Report(what, "glTF data URI could not be decoded");
				return std::nullopt;
			}
			std::optional<Image> decoded = DecodeImage({ static_cast<const std::uint8_t*>(data), outSize }, what);
			std::free(data);
			return decoded;
		}
		return LoadImageFile(directory + "/" + uri);
	}

	if (image->buffer_view != nullptr && image->buffer_view->buffer != nullptr && image->buffer_view->buffer->data != nullptr)
	{
		const cgltf_buffer_view* view = image->buffer_view;
		if (view->offset + view->size > view->buffer->size) { Report(what, "image buffer view is outside its buffer"); return std::nullopt; }

		// raylib accepts PNG and JPEG mime types, but its build has JPEG disabled, so a JPEG fails there too
		const char* mime = image->mime_type;
		const bool png = mime != nullptr && (std::strcmp(mime, "image/png") == 0 || std::strcmp(mime, "image\\/png") == 0);
		if (!png) { Report(what, std::string("image MIME type not recognized or not PNG (") + (mime != nullptr ? mime : "none") + ")"); return std::nullopt; }

		return DecodeImage({ static_cast<const std::uint8_t*>(view->buffer->data) + view->offset, view->size }, what);
	}
	return std::nullopt;
}

// glTF stores matrices column-major, which is raylib's Matrix member order too
Matrix MatrixFromGltf(const cgltf_float* m)
{
	return Matrix{ m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15] };
}

struct GltfDeleter { void operator()(cgltf_data* d) const { cgltf_free(d); } };

} // namespace

std::optional<Image> LoadImageFile(const std::string& path)
{
	std::vector<std::uint8_t> bytes;
	if (!ReadFile(path, bytes)) { Report(path, "cannot read file"); return std::nullopt; }
	return DecodeImage(bytes, path);
}

std::optional<Image> LoadImageMemory(std::span<const std::uint8_t> bytes)
{
	return DecodeImage(bytes, "image from memory");
}

std::optional<ModelData> LoadGltf(const std::string& path)
{
	const std::string ext = LowerExtension(path);
	if (ext != ".glb" && ext != ".gltf") { Report(path, "not a .glb or .gltf file"); return std::nullopt; }

	std::vector<std::uint8_t> file;
	if (!ReadFile(path, file)) { Report(path, "cannot read file"); return std::nullopt; }

	cgltf_options options = {};
	cgltf_data* parsed = nullptr;
	if (cgltf_parse(&options, file.data(), file.size(), &parsed) != cgltf_result_success)
	{
		Report(path, "not a valid glTF file");
		return std::nullopt;
	}
	std::unique_ptr<cgltf_data, GltfDeleter> data(parsed);

	// Fills buffer->data: the GLB's binary chunk is used in place, data URIs and .bin files are loaded
	if (cgltf_load_buffers(&options, data.get(), path.c_str()) != cgltf_result_success)
	{
		Report(path, "failed to load the mesh buffers");
		return std::nullopt;
	}

	for (cgltf_size i = 0; i < data->nodes_count; ++i)
	{
		const cgltf_mesh* mesh = data->nodes[i].mesh;
		if (mesh == nullptr) { continue; }
		for (cgltf_size p = 0; p < mesh->primitives_count; ++p)
		{
			if (mesh->primitives[p].has_draco_mesh_compression)
			{
				Report(path, "Draco compression is not supported");
				return std::nullopt;
			}
		}
	}

	ModelData model;

	// Materials: [0] is the default (white, no image); glTF material i becomes i + 1
	const std::string directory = DirectoryOf(path);
	model.materials.resize(data->materials_count + 1);
	for (cgltf_size i = 0; i < data->materials_count; ++i)
	{
		const cgltf_material& source = data->materials[i];
		MaterialData& material = model.materials[i + 1];
		if (!source.has_pbr_metallic_roughness) { continue; }

		// Only the base-colour image: nothing else is sampled by the GL backend
		if (source.pbr_metallic_roughness.base_color_texture.texture != nullptr)
		{
			material.diffuseImage = LoadCgltfImage(source.pbr_metallic_roughness.base_color_texture.texture->image, directory, path);
		}
		const cgltf_float* factor = source.pbr_metallic_roughness.base_color_factor;
		material.diffuseColor = Color{ ToByte(factor[0] * 255), ToByte(factor[1] * 255), ToByte(factor[2] * 255), ToByte(factor[3] * 255) };
	}

	// Every triangle primitive of every node with a mesh becomes one mesh, the node's world matrix applied
	for (cgltf_size i = 0; i < data->nodes_count; ++i)
	{
		const cgltf_node* node = &data->nodes[i];
		const cgltf_mesh* mesh = node->mesh;
		if (mesh == nullptr) { continue; }

		cgltf_float worldTransform[16];
		cgltf_node_transform_world(node, worldTransform);
		const Matrix world = MatrixFromGltf(worldTransform);
		const Matrix normalMatrix = MatrixTranspose(MatrixInvert(world));

		for (cgltf_size p = 0; p < mesh->primitives_count; ++p)
		{
			const cgltf_primitive& primitive = mesh->primitives[p];
			if (primitive.type != cgltf_primitive_type_triangles) { continue; }

			MeshData out;
			bool havePositions = false, haveNormals = false, haveTexcoords = false, haveColors = false;
			for (cgltf_size j = 0; j < primitive.attributes_count; ++j)
			{
				const cgltf_attribute& attribute = primitive.attributes[j];
				const cgltf_accessor* a = attribute.data;
				if (a == nullptr) { continue; }

				// The first attribute of each kind wins, as in raylib
				if (attribute.type == cgltf_attribute_type_position && !havePositions)
				{
					havePositions = LoadPositions(a, world, out.positions);
					if (!havePositions) { Report(path, "POSITION data is missing, out of range or not vec3 float/u16/i16"); }
				}
				else if (attribute.type == cgltf_attribute_type_normal && !haveNormals)
				{
					haveNormals = LoadNormals(a, normalMatrix, out.normals);
				}
				else if (attribute.type == cgltf_attribute_type_texcoord && attribute.index == 0 && !haveTexcoords)
				{
					haveTexcoords = LoadTexcoords(a, out.texcoords);
				}
				else if (attribute.type == cgltf_attribute_type_color && !haveColors)
				{
					haveColors = LoadColors(a, out.colors);
				}
			}

			// Per-vertex arrays must have one entry per position; a mismatched one is dropped
			const std::size_t vertexCount = out.positions.size() / 3;
			if (haveNormals && out.normals.size() != vertexCount * 3) { out.normals.clear(); }
			if (haveTexcoords && out.texcoords.size() != vertexCount * 2) { out.texcoords.clear(); }
			if (haveColors && out.colors.size() != vertexCount * 4) { out.colors.clear(); }

			if (primitive.indices != nullptr && primitive.indices->buffer_view != nullptr)
			{
				if (!LoadIndices(primitive.indices, out.indices))
				{
					Report(path, "indices are missing, out of range or not u8/u16/u32");
					return std::nullopt;
				}
			}

			// The glTF material index + 1, or 0 for the default
			if (primitive.material != nullptr)
			{
				out.material = static_cast<int>(cgltf_material_index(data.get(), primitive.material)) + 1;
			}
			model.meshes.push_back(std::move(out));
		}
	}

	if (model.meshes.empty())
	{
		Report(path, "no triangle meshes");
		return std::nullopt;
	}

	// The union of the meshes' position bounds, with raylib's GetMeshBoundingBox rules
	for (const MeshData& mesh : model.meshes)
	{
		if (mesh.positions.empty()) { continue; }
		Vector3 lo = { mesh.positions[0], mesh.positions[1], mesh.positions[2] };
		Vector3 hi = lo;
		for (std::size_t k = 1; k < mesh.positions.size() / 3; ++k)
		{
			const Vector3 v = { mesh.positions[3 * k], mesh.positions[3 * k + 1], mesh.positions[3 * k + 2] };
			lo = Vector3Min(lo, v);
			hi = Vector3Max(hi, v);
		}
		if (!model.localBounds) { model.localBounds = BoundingBox{ lo, hi }; }
		else { model.localBounds = BoundingBox{ Vector3Min(model.localBounds->min, lo), Vector3Max(model.localBounds->max, hi) }; }
	}
	return model;
}

} // namespace Render::Decode
