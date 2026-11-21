/**
 * CPU-side asset decoding for the GL backend (work order B-19).
 *
 * The GL backend decodes glTF models and PNG/BMP images itself
 * (src/engineLayer/Render/backend_gl/AssetDecode.cpp) and must produce the data raylib's
 * LoadModel and LoadImage produce, or the two backends would show different worlds.
 * Every constant below was recorded from raylib 6.0's own LoadModel, GetModelBoundingBox
 * and LoadImage, run in the raylib build before the decoder was checked against it (a
 * throwaway program printed them; it is not committed). The default mode of this file
 * re-checks raylib against the constants on every run, so the constants stay proven to be
 * raylib's facts; the standalone mode checks the decoder against the same constants.
 *
 * Two builds of this file, chosen by a macro, as tests/DefaultFontTests.cpp does:
 *  - by default, an engine-linked suite in the raylib tree. It opens the test window
 *    (LoadModel uploads to the GPU, so it needs a GL context) and checks raylib itself
 *    against the constants. See tests/run_tests.sh.
 *  - with -DVEIL_ASSET_DECODE_STANDALONE, a windowless program compiled together with
 *    src/engineLayer/Render/backend_gl/AssetDecode.cpp and ThirdPartyImpl.cpp and nothing
 *    else (raylib's header for types only; raylib is not linked). It reads the resources
 *    folder from the VEIL_RESOURCES_DIR environment variable.
 *    cl /nologo /std:c++latest /EHsc /MDd /Zc:__cplusplus /DVEIL_ASSET_DECODE_STANDALONE
 *       /I src /I src\engineLayer /I src\engineLayer\Render\backend_gl
 *       /I thirdparty\raylib-6.0\src /I thirdparty\stb /I thirdparty\cgltf
 *       tests\AssetDecodeTests.cpp src\engineLayer\Render\backend_gl\AssetDecode.cpp
 *       src\engineLayer\Render\backend_gl\ThirdPartyImpl.cpp /Fe:out\b19\AssetDecodeStandalone.exe /Fo:out\b19\
 *
 * Index counts: raylib reports triangleCount * 3 for every mesh. For an indexed mesh that
 * is the index count; for an unindexed mesh (no index array) it is (vertices / 3) * 3.
 * The decoder's MeshData::indices is empty for an unindexed mesh, so the "indices" fact
 * below is indices.size() when indexed and (vertices / 3) * 3 when not. "indexed" says which.
 */

#include <raylib.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifdef VEIL_ASSET_DECODE_STANDALONE
#include "AssetDecode.h"
#else
#include <ResourcePaths.h>
#include "TestWindow.h"
#endif

static int g_failures = 0;
static int g_checks = 0;

static void Check(bool condition, const std::string& what)
{
	++g_checks;
	if (!condition)
	{
		++g_failures;
		std::printf("  FAIL  %s\n", what.c_str());
	}
}

// ---------------------------------------------------------------------------------------------
// The facts: what is recorded from raylib, and what each mode measures
// ---------------------------------------------------------------------------------------------

struct MeshFacts
{
	int vertices = 0;
	int indices = 0;      // see the note on index counts above
	bool indexed = false;
	int material = 0;     // index into the model's materials; 0 is the default material
	bool normals = false, texcoords = false, colors = false;
	double position[3] = {}; // sums of x, y, z over all vertices
	double normal[3] = {};   // likewise over the normals (0 if absent)
	double uv[2] = {};       // likewise over the texcoords (0 if absent)
	long long colorSum = 0;  // sum of every rgba byte
	long long indexSum = 0;  // sum of every index (raylib's are 16-bit; see the report on u32 indices)
};

struct MaterialFacts
{
	int r = 255, g = 255, b = 255, a = 255; // diffuse colour
	bool hasTexture = false;                // a diffuse image beyond the default white one
	int width = 0, height = 0;
};

struct ModelFacts
{
	std::vector<MeshFacts> meshes;
	std::vector<MaterialFacts> materials; // [0] is the default material
	double boundsMin[3] = {}, boundsMax[3] = {};
};

struct ModelCase { const char* file; ModelFacts facts; };

struct ImageCase
{
	const char* file;
	int width, height, channels;
	long long byteSum; // sum of every pixel byte
};

// Recorded from raylib 6.0 (LoadModel, GetModelBoundingBox): per model the meshes in raylib's order,
// the materials (index 0 is raylib's default), then the bounds min and max.
// Mesh: vertices, indices, indexed, material, normals, texcoords, colours, position sums,
// normal sums, texcoord sums, colour byte sum, index sum.
// Material: diffuse r, g, b, a, has a diffuse image, its width and height.
static const std::vector<ModelCase> kModels = {
	{ "models/environment/Terminal.glb", {
		{
			{ 188, 318, true, 1, true, true, false, { 72.151096075773239, 932.93351936340332, 0 }, { 5.8277477622032166, 24.408039882779121, 0 }, { 123.75987601280212, 41.575337886810303 }, 0LL, 30921LL },
			{ 4, 6, true, 2, true, true, false, { 7.0782222747802734, 31.54210090637207, 0 }, { 1.3333238363265991, -0.015199891291558743, 0 }, { 2.6450393199920654, 0.85744524002075195 }, 0LL, 8LL },
		}, {
			{ 255, 255, 255, 255, false, 0, 0 },
			{ 255, 255, 255, 255, true, 4096, 4096 },
			{ 255, 255, 255, 108, true, 4096, 4096 },
		},
		{ -3, -1, -3 }, { 3, 9.30274105, 3 } } },
	{ "models/items/Artifact/RubixCube0.glb", {
		{
			{ 3105, 15174, true, 1, true, true, true, { -45.657560907304287, -1.20667814463377, 12.789584465324879 }, { -2391.513879776001, 544.67151880264282, 1004.6501083374023 }, { 1491.7471716105938, 1490.7370326519012 }, 3167100LL, 23543416LL },
			{ 36, 54, true, 2, true, true, true, { -6.7055225372314453e-07, 9.0897083282470703e-07, 10.800001502037048 }, { 0, 0, 353.9022102355957 }, { 18, 4.500000536441803 }, 36720LL, 937LL },
		}, {
			{ 255, 255, 255, 255, false, 0, 0 },
			{ 0, 0, 0, 255, false, 0, 0 },
			{ 204, 0, 1, 255, false, 0, 0 },
		},
		{ -0.300000042, -0.299999982, -0.299999982 }, { 0.299999982, 0.300000042, 0.300000042 } } },
	{ "models/items/Artifact/RubixCube1.glb", {
		{
			{ 3105, 15120, true, 1, true, true, true, { -45.657560907304287, -1.20667814463377, 12.789584465324879 }, { -2391.513879776001, 544.67151880264282, 1004.6501083374023 }, { 1491.7471716105938, 1490.7370326519012 }, 3167100LL, 23465221LL },
			{ 36, 54, true, 2, true, true, true, { -6.7055225372314453e-07, 9.0897083282470703e-07, 10.800001502037048 }, { 0, 0, 353.9022102355957 }, { 18, 4.500000536441803 }, 36720LL, 937LL },
			{ 36, 54, true, 3, true, true, true, { 10.799999356269836, 9.2387199401855469e-07, 8.0466270446777344e-07 }, { 353.9022102355957, 0, 0 }, { 18, 13.5 }, 36720LL, 936LL },
		}, {
			{ 255, 255, 255, 255, false, 0, 0 },
			{ 0, 0, 0, 255, false, 0, 0 },
			{ 204, 0, 1, 255, false, 0, 0 },
			{ 0, 0, 255, 255, false, 0, 0 },
		},
		{ -0.300000042, -0.299999982, -0.299999982 }, { 0.299999982, 0.300000042, 0.300000042 } } },
	{ "models/items/Artifact/RubixCube2.glb", {
		{
			{ 3105, 15066, true, 1, true, true, true, { -45.657560907304287, -1.20667814463377, 12.789584465324879 }, { -2391.513879776001, 544.67151880264282, 1004.6501083374023 }, { 1491.7471716105938, 1490.7370326519012 }, 3167100LL, 23361532LL },
			{ 36, 54, true, 2, true, true, true, { -6.7055225372314453e-07, 9.0897083282470703e-07, 10.800001502037048 }, { 0, 0, 353.9022102355957 }, { 18, 4.500000536441803 }, 36720LL, 937LL },
			{ 36, 54, true, 3, true, true, true, { 10.799999356269836, 9.2387199401855469e-07, 8.0466270446777344e-07 }, { 353.9022102355957, 0, 0 }, { 18, 13.5 }, 36720LL, 936LL },
			{ 36, 54, true, 4, true, true, true, { -6.7055225372314453e-07, 9.0897083282470703e-07, -10.799999356269836 }, { 0, 0, -353.9022102355957 }, { 18, 22.500000536441803 }, 36720LL, 945LL },
		}, {
			{ 255, 255, 255, 255, false, 0, 0 },
			{ 0, 0, 0, 255, false, 0, 0 },
			{ 204, 0, 1, 255, false, 0, 0 },
			{ 0, 0, 255, 255, false, 0, 0 },
			{ 255, 54, 0, 255, false, 0, 0 },
		},
		{ -0.300000042, -0.299999982, -0.299999982 }, { 0.299999982, 0.300000042, 0.300000042 } } },
	{ "models/items/Artifact/RubixCube3.glb", {
		{
			{ 3105, 15012, true, 1, true, true, true, { -45.657560907304287, -1.20667814463377, 12.789584465324879 }, { -2391.513879776001, 544.67151880264282, 1004.6501083374023 }, { 1491.7471716105938, 1490.7370326519012 }, 3167100LL, 23276502LL },
			{ 36, 54, true, 2, true, true, true, { -6.7055225372314453e-07, 9.0897083282470703e-07, 10.800001502037048 }, { 0, 0, 353.9022102355957 }, { 18, 4.500000536441803 }, 36720LL, 937LL },
			{ 36, 54, true, 3, true, true, true, { 10.799999356269836, 9.2387199401855469e-07, 8.0466270446777344e-07 }, { 353.9022102355957, 0, 0 }, { 18, 13.5 }, 36720LL, 936LL },
			{ 36, 54, true, 4, true, true, true, { -6.7055225372314453e-07, 9.0897083282470703e-07, -10.799999356269836 }, { 0, 0, -353.9022102355957 }, { 18, 22.500000536441803 }, 36720LL, 945LL },
			{ 36, 54, true, 5, true, true, true, { -10.800001502037048, 8.9406967163085938e-07, 8.3446502685546875e-07 }, { -353.9022102355957, 0, 0 }, { 18, 31.500001072883606 }, 36720LL, 937LL },
		}, {
			{ 255, 255, 255, 255, false, 0, 0 },
			{ 0, 0, 0, 255, false, 0, 0 },
			{ 204, 0, 1, 255, false, 0, 0 },
			{ 0, 0, 255, 255, false, 0, 0 },
			{ 255, 54, 0, 255, false, 0, 0 },
			{ 0, 255, 0, 255, false, 0, 0 },
		},
		{ -0.300000042, -0.299999982, -0.299999982 }, { 0.299999982, 0.300000042, 0.300000042 } } },
	{ "models/items/Artifact/RubixCube4.glb", {
		{
			{ 3105, 14958, true, 1, true, true, true, { -45.657560907304287, -1.20667814463377, 12.789584465324879 }, { -2391.513879776001, 544.67151880264282, 1004.6501083374023 }, { 1491.7471716105938, 1490.7370326519012 }, 3167100LL, 23241121LL },
			{ 36, 54, true, 2, true, true, true, { -6.7055225372314453e-07, 9.0897083282470703e-07, 10.800001502037048 }, { 0, 0, 353.9022102355957 }, { 18, 4.500000536441803 }, 36720LL, 937LL },
			{ 36, 54, true, 3, true, true, true, { 10.799999356269836, 9.2387199401855469e-07, 8.0466270446777344e-07 }, { 353.9022102355957, 0, 0 }, { 18, 13.5 }, 36720LL, 936LL },
			{ 36, 54, true, 4, true, true, true, { -6.7055225372314453e-07, 9.0897083282470703e-07, -10.799999356269836 }, { 0, 0, -353.9022102355957 }, { 18, 22.500000536441803 }, 36720LL, 945LL },
			{ 36, 54, true, 5, true, true, true, { -10.800001502037048, 8.9406967163085938e-07, 8.3446502685546875e-07 }, { -353.9022102355957, 0, 0 }, { 18, 31.500001072883606 }, 36720LL, 937LL },
			{ 36, 54, true, 6, true, true, true, { -6.7055225372314453e-07, -10.799999356269836, 8.0466270446777344e-07 }, { 0, -353.9022102355957, 0 }, { 9.0000004023313522, 13.5 }, 36720LL, 929LL },
		}, {
			{ 255, 255, 255, 255, false, 0, 0 },
			{ 0, 0, 0, 255, false, 0, 0 },
			{ 204, 0, 1, 255, false, 0, 0 },
			{ 0, 0, 255, 255, false, 0, 0 },
			{ 255, 54, 0, 255, false, 0, 0 },
			{ 0, 255, 0, 255, false, 0, 0 },
			{ 255, 255, 0, 255, false, 0, 0 },
		},
		{ -0.300000042, -0.299999982, -0.299999982 }, { 0.299999982, 0.300000042, 0.300000042 } } },
	{ "models/items/Artifact/RubixCube5.glb", {
		{
			{ 3105, 14904, true, 1, true, true, true, { -45.657560907304287, -1.20667814463377, 12.789584465324879 }, { -2391.513879776001, 544.67151880264282, 1004.6501083374023 }, { 1491.7471716105938, 1490.7370326519012 }, 3167100LL, 23113240LL },
			{ 36, 54, true, 2, true, true, true, { -6.7055225372314453e-07, 9.0897083282470703e-07, 10.800001502037048 }, { 0, 0, 353.9022102355957 }, { 18, 4.500000536441803 }, 36720LL, 937LL },
			{ 36, 54, true, 3, true, true, true, { -6.7055225372314453e-07, 10.800001502037048, 8.3446502685546875e-07 }, { 0, 353.9022102355957, 0 }, { 27.000000536441803, 13.5 }, 36720LL, 954LL },
			{ 36, 54, true, 4, true, true, true, { 10.799999356269836, 9.2387199401855469e-07, 8.0466270446777344e-07 }, { 353.9022102355957, 0, 0 }, { 18, 13.5 }, 36720LL, 936LL },
			{ 36, 54, true, 5, true, true, true, { -6.7055225372314453e-07, 9.0897083282470703e-07, -10.799999356269836 }, { 0, 0, -353.9022102355957 }, { 18, 22.500000536441803 }, 36720LL, 945LL },
			{ 36, 54, true, 6, true, true, true, { -10.800001502037048, 8.9406967163085938e-07, 8.3446502685546875e-07 }, { -353.9022102355957, 0, 0 }, { 18, 31.500001072883606 }, 36720LL, 937LL },
			{ 36, 54, true, 7, true, true, true, { -6.7055225372314453e-07, -10.799999356269836, 8.0466270446777344e-07 }, { 0, -353.9022102355957, 0 }, { 9.0000004023313522, 13.5 }, 36720LL, 929LL },
		}, {
			{ 255, 255, 255, 255, false, 0, 0 },
			{ 0, 0, 0, 255, false, 0, 0 },
			{ 204, 0, 1, 255, false, 0, 0 },
			{ 204, 204, 204, 255, false, 0, 0 },
			{ 0, 0, 255, 255, false, 0, 0 },
			{ 255, 54, 0, 255, false, 0, 0 },
			{ 0, 255, 0, 255, false, 0, 0 },
			{ 255, 255, 0, 255, false, 0, 0 },
		},
		{ -0.300000042, -0.299999982, -0.299999982 }, { 0.299999982, 0.300000042, 0.300000042 } } },
};

// Recorded from raylib 6.0's LoadImage. Five PNGs of different kinds: 4 channels (RGBA), 4 channels
// from a 4-bit palette (stb expands it to RGBA), 1 channel from a 1-bit grey, 3 channels (8-bit RGB)
// and 1 channel from a 16-bit grey (stb reduces it to 8 bits). The assets have no 2-channel PNG.
static const std::vector<ImageCase> kImages = {
	{ "ui/Cells01/01.png", 415, 91, 4, 15035087LL },
	{ "blocks/frame.png", 32, 32, 4, 376192LL },
	{ "blocks/Chip001/Chip001_4K-PNG_Metalness.png", 4096, 4096, 1, 694686300LL },
	{ "blocks/Chip001/Chip001_4K-PNG_Color.png", 4096, 4096, 3, 3263249515LL },
	{ "blocks/Bricks097/Bricks097_4K-PNG_Displacement.png", 4096, 2048, 1, 1423329456LL },
};

// ---------------------------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------------------------

static bool Near(double a, double b, double relative)
{
	return std::fabs(a - b) <= relative * std::fmax(std::fabs(a), std::fabs(b)) + 1e-6;
}

static void CompareModel(const char* file, const ModelFacts& want, const ModelFacts& got, const char* who)
{
	const std::string name = std::string(file) + " (" + who + ")";
	Check(got.meshes.size() == want.meshes.size(), name + ": mesh count " + std::to_string(got.meshes.size()) + ", expected " + std::to_string(want.meshes.size()));
	Check(got.materials.size() == want.materials.size(), name + ": material count " + std::to_string(got.materials.size()) + ", expected " + std::to_string(want.materials.size()));

	const std::size_t meshes = std::min(got.meshes.size(), want.meshes.size());
	for (std::size_t i = 0; i < meshes; ++i)
	{
		const MeshFacts& w = want.meshes[i];
		const MeshFacts& g = got.meshes[i];
		const std::string m = name + " mesh " + std::to_string(i) + ": ";
		Check(g.vertices == w.vertices, m + "vertices " + std::to_string(g.vertices) + ", expected " + std::to_string(w.vertices));
		Check(g.indices == w.indices, m + "indices " + std::to_string(g.indices) + ", expected " + std::to_string(w.indices));
		Check(g.indexed == w.indexed, m + "indexed flag");
		Check(g.material == w.material, m + "material " + std::to_string(g.material) + ", expected " + std::to_string(w.material));
		Check(g.normals == w.normals, m + "normals present");
		Check(g.texcoords == w.texcoords, m + "texcoords present");
		Check(g.colors == w.colors, m + "vertex colours present");
		for (int k = 0; k < 3; ++k)
		{
			Check(Near(g.position[k], w.position[k], 1e-5), m + "position sum " + std::to_string(k) + ": " + std::to_string(g.position[k]) + ", expected " + std::to_string(w.position[k]));
			Check(Near(g.normal[k], w.normal[k], 1e-5), m + "normal sum " + std::to_string(k) + ": " + std::to_string(g.normal[k]) + ", expected " + std::to_string(w.normal[k]));
		}
		for (int k = 0; k < 2; ++k)
		{
			Check(Near(g.uv[k], w.uv[k], 1e-5), m + "texcoord sum " + std::to_string(k) + ": " + std::to_string(g.uv[k]) + ", expected " + std::to_string(w.uv[k]));
		}
		Check(g.colorSum == w.colorSum, m + "colour byte sum " + std::to_string(g.colorSum) + ", expected " + std::to_string(w.colorSum));
		Check(g.indexSum == w.indexSum, m + "index sum " + std::to_string(g.indexSum) + ", expected " + std::to_string(w.indexSum));
	}

	const std::size_t materials = std::min(got.materials.size(), want.materials.size());
	for (std::size_t i = 0; i < materials; ++i)
	{
		const MaterialFacts& w = want.materials[i];
		const MaterialFacts& g = got.materials[i];
		const std::string m = name + " material " + std::to_string(i) + ": ";
		Check(g.r == w.r && g.g == w.g && g.b == w.b && g.a == w.a,
			m + "colour " + std::to_string(g.r) + "," + std::to_string(g.g) + "," + std::to_string(g.b) + "," + std::to_string(g.a)
			+ ", expected " + std::to_string(w.r) + "," + std::to_string(w.g) + "," + std::to_string(w.b) + "," + std::to_string(w.a));
		Check(g.hasTexture == w.hasTexture, m + "diffuse texture present");
		Check(g.width == w.width && g.height == w.height,
			m + "texture " + std::to_string(g.width) + "x" + std::to_string(g.height) + ", expected " + std::to_string(w.width) + "x" + std::to_string(w.height));
	}

	for (int k = 0; k < 3; ++k)
	{
		Check(std::fabs(got.boundsMin[k] - want.boundsMin[k]) <= 1e-5, name + ": bounds min " + std::to_string(k) + ": " + std::to_string(got.boundsMin[k]) + ", expected " + std::to_string(want.boundsMin[k]));
		Check(std::fabs(got.boundsMax[k] - want.boundsMax[k]) <= 1e-5, name + ": bounds max " + std::to_string(k) + ": " + std::to_string(got.boundsMax[k]) + ", expected " + std::to_string(want.boundsMax[k]));
	}
}

static void CompareImage(const ImageCase& want, int width, int height, int channels, long long byteSum, const char* who)
{
	const std::string name = std::string(want.file) + " (" + who + "): ";
	Check(width == want.width && height == want.height, name + "size " + std::to_string(width) + "x" + std::to_string(height) + ", expected " + std::to_string(want.width) + "x" + std::to_string(want.height));
	Check(channels == want.channels, name + "channels " + std::to_string(channels) + ", expected " + std::to_string(want.channels));
	Check(byteSum == want.byteSum, name + "pixel byte sum " + std::to_string(byteSum) + ", expected " + std::to_string(want.byteSum));
}

// ---------------------------------------------------------------------------------------------
// Measuring: raylib in the default mode, the decoder in the standalone mode
// ---------------------------------------------------------------------------------------------

static std::string ResourcesDir()
{
#ifdef VEIL_ASSET_DECODE_STANDALONE
	std::string dir;
#ifdef _MSC_VER
	char* value = nullptr;
	std::size_t length = 0;
	if (_dupenv_s(&value, &length, "VEIL_RESOURCES_DIR") == 0 && value != nullptr) { dir = value; }
	std::free(value);
#else
	if (const char* value = std::getenv("VEIL_RESOURCES_DIR")) { dir = value; }
#endif
	return dir.empty() ? std::string("resources") : dir;
#else
	return ResourcePaths::Resources().generic_string();
#endif
}

static std::string ResourceFile(const char* relative) { return ResourcesDir() + "/" + relative; }

#ifndef VEIL_ASSET_DECODE_STANDALONE

static ModelFacts MeasureRaylibModel(const Model& model)
{
	ModelFacts f;
	for (int i = 0; i < model.meshCount; ++i)
	{
		const Mesh& mesh = model.meshes[i];
		MeshFacts m;
		m.vertices = mesh.vertexCount;
		m.indices = mesh.triangleCount * 3;
		m.indexed = mesh.indices != nullptr;
		m.material = model.meshMaterial[i];
		m.normals = mesh.normals != nullptr;
		m.texcoords = mesh.texcoords != nullptr;
		m.colors = mesh.colors != nullptr;
		for (int v = 0; v < mesh.vertexCount; ++v)
		{
			for (int k = 0; k < 3; ++k) { m.position[k] += mesh.vertices[3 * v + k]; }
			if (m.normals) { for (int k = 0; k < 3; ++k) { m.normal[k] += mesh.normals[3 * v + k]; } }
			if (m.texcoords) { for (int k = 0; k < 2; ++k) { m.uv[k] += mesh.texcoords[2 * v + k]; } }
			if (m.colors) { for (int k = 0; k < 4; ++k) { m.colorSum += mesh.colors[4 * v + k]; } }
		}
		if (m.indexed) { for (int k = 0; k < m.indices; ++k) { m.indexSum += mesh.indices[k]; } }
		f.meshes.push_back(m);
	}

	const unsigned int defaultTexture = model.materialCount > 0 ? model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture.id : 0;
	for (int i = 0; i < model.materialCount; ++i)
	{
		const MaterialMap& map = model.materials[i].maps[MATERIAL_MAP_DIFFUSE];
		MaterialFacts m;
		m.r = map.color.r; m.g = map.color.g; m.b = map.color.b; m.a = map.color.a;
		m.hasTexture = map.texture.id != 0 && map.texture.id != defaultTexture;
		m.width = m.hasTexture ? map.texture.width : 0;
		m.height = m.hasTexture ? map.texture.height : 0;
		f.materials.push_back(m);
	}

	const BoundingBox box = GetModelBoundingBox(model);
	f.boundsMin[0] = box.min.x; f.boundsMin[1] = box.min.y; f.boundsMin[2] = box.min.z;
	f.boundsMax[0] = box.max.x; f.boundsMax[1] = box.max.y; f.boundsMax[2] = box.max.z;
	return f;
}

static int RaylibChannels(int format)
{
	switch (format)
	{
	case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: return 1;
	case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: return 2;
	case PIXELFORMAT_UNCOMPRESSED_R8G8B8: return 3;
	case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: return 4;
	default: return 0;
	}
}

static long long RaylibImageSum(const ::Image& image)
{
	long long sum = 0;
	const std::size_t bytes = static_cast<std::size_t>(image.width) * image.height * RaylibChannels(image.format);
	const unsigned char* p = static_cast<const unsigned char*>(image.data);
	for (std::size_t i = 0; i < bytes; ++i) { sum += p[i]; }
	return sum;
}

#else // VEIL_ASSET_DECODE_STANDALONE

static ModelFacts MeasureDecodedModel(const Render::Decode::ModelData& model)
{
	ModelFacts f;
	for (const Render::Decode::MeshData& mesh : model.meshes)
	{
		MeshFacts m;
		m.vertices = static_cast<int>(mesh.positions.size() / 3);
		m.indexed = !mesh.indices.empty();
		m.indices = m.indexed ? static_cast<int>(mesh.indices.size()) : (m.vertices / 3) * 3;
		m.material = mesh.material;
		m.normals = !mesh.normals.empty();
		m.texcoords = !mesh.texcoords.empty();
		m.colors = !mesh.colors.empty();
		for (int v = 0; v < m.vertices; ++v)
		{
			for (int k = 0; k < 3; ++k) { m.position[k] += mesh.positions[3 * v + k]; }
			if (m.normals) { for (int k = 0; k < 3; ++k) { m.normal[k] += mesh.normals[3 * v + k]; } }
			if (m.texcoords) { for (int k = 0; k < 2; ++k) { m.uv[k] += mesh.texcoords[2 * v + k]; } }
		}
		for (std::uint8_t c : mesh.colors) { m.colorSum += c; }
		for (std::uint32_t index : mesh.indices) { m.indexSum += index; }
		f.meshes.push_back(m);
	}

	for (const Render::Decode::MaterialData& material : model.materials)
	{
		MaterialFacts m;
		m.r = material.diffuseColor.r; m.g = material.diffuseColor.g; m.b = material.diffuseColor.b; m.a = material.diffuseColor.a;
		m.hasTexture = material.diffuseImage.has_value();
		m.width = m.hasTexture ? material.diffuseImage->width : 0;
		m.height = m.hasTexture ? material.diffuseImage->height : 0;
		f.materials.push_back(m);
	}

	if (model.localBounds)
	{
		const BoundingBox& box = *model.localBounds;
		f.boundsMin[0] = box.min.x; f.boundsMin[1] = box.min.y; f.boundsMin[2] = box.min.z;
		f.boundsMax[0] = box.max.x; f.boundsMax[1] = box.max.y; f.boundsMax[2] = box.max.z;
	}
	return f;
}

// What the constants cannot say cheaply: the arrays agree with each other and the indices stay inside
static void CheckModelStructure(const char* file, const Render::Decode::ModelData& model)
{
	Check(model.localBounds.has_value(), std::string(file) + ": has local bounds");
	Check(!model.materials.empty(), std::string(file) + ": has the default material");

	for (std::size_t i = 0; i < model.meshes.size(); ++i)
	{
		const Render::Decode::MeshData& mesh = model.meshes[i];
		const std::string m = std::string(file) + " mesh " + std::to_string(i) + ": ";
		Check(mesh.positions.size() % 3 == 0 && !mesh.positions.empty(), m + "positions are whole xyz triples");
		const std::size_t vertices = mesh.positions.size() / 3;
		Check(mesh.normals.empty() || mesh.normals.size() == vertices * 3, m + "normals array length matches the vertex count");
		Check(mesh.texcoords.empty() || mesh.texcoords.size() == vertices * 2, m + "texcoords array length matches the vertex count");
		Check(mesh.colors.empty() || mesh.colors.size() == vertices * 4, m + "colours array length matches the vertex count");
		Check(mesh.material >= 0 && static_cast<std::size_t>(mesh.material) < model.materials.size(), m + "material index is inside the materials");

		bool indicesInside = true;
		for (std::uint32_t index : mesh.indices) { if (index >= vertices) { indicesInside = false; break; } }
		Check(indicesInside, m + "every index is below the vertex count");

		// Not unit length, and deliberately so: like raylib, the decoder only transforms float normals by
		// transpose(inverse(world)), so a node scaled by s gives normals of length 1/s (these models'
		// nodes are scaled by about 0.1). lighting.vs normalises them. So the check is that every
		// normal is finite and non-zero, and that its direction is a proper unit vector once normalised.
		bool usable = true;
		for (std::size_t v = 0; v < mesh.normals.size() / 3; ++v)
		{
			const double x = mesh.normals[3 * v], y = mesh.normals[3 * v + 1], z = mesh.normals[3 * v + 2];
			const double length = std::sqrt(x * x + y * y + z * z);
			if (!std::isfinite(length) || length < 1e-6) { usable = false; break; }
			const double nx = x / length, ny = y / length, nz = z / length;
			if (std::fabs(std::sqrt(nx * nx + ny * ny + nz * nz) - 1.0) > 1e-3) { usable = false; break; }
		}
		Check(usable, m + "normals are finite, non-zero and normalise to unit length within 1e-3");
	}
}

#endif

// ---------------------------------------------------------------------------------------------
// The tests
// ---------------------------------------------------------------------------------------------

using Clock = std::chrono::steady_clock;
static double Milliseconds(Clock::time_point since) { return std::chrono::duration<double, std::milli>(Clock::now() - since).count(); }

static void TestModels()
{
	for (const ModelCase& c : kModels)
	{
		const std::string path = ResourceFile(c.file);
		const auto start = Clock::now();
#ifdef VEIL_ASSET_DECODE_STANDALONE
		const std::optional<Render::Decode::ModelData> model = Render::Decode::LoadGltf(path);
		const double ms = Milliseconds(start);
		Check(model.has_value(), std::string(c.file) + ": LoadGltf succeeds");
		if (!model) { continue; }
		std::printf("  %-34s LoadGltf %9.1f ms\n", c.file, ms);
		CompareModel(c.file, c.facts, MeasureDecodedModel(*model), "decoder");
		CheckModelStructure(c.file, *model);
#else
		Model model = LoadModel(path.c_str());
		const double ms = Milliseconds(start);
		Check(model.meshCount > 0, std::string(c.file) + ": LoadModel succeeds");
		if (model.meshCount <= 0) { continue; }
		std::printf("  %-34s LoadModel %9.1f ms\n", c.file, ms);
		CompareModel(c.file, c.facts, MeasureRaylibModel(model), "raylib");
		UnloadModel(model);
#endif
	}
}

static void TestImages()
{
	for (const ImageCase& c : kImages)
	{
		const std::string path = ResourceFile(c.file);
#ifdef VEIL_ASSET_DECODE_STANDALONE
		const std::optional<Render::Decode::Image> image = Render::Decode::LoadImageFile(path);
		Check(image.has_value(), std::string(c.file) + ": LoadImageFile succeeds");
		if (!image) { continue; }
		long long sum = 0;
		for (std::uint8_t b : image->pixels) { sum += b; }
		Check(image->pixels.size() == static_cast<std::size_t>(image->width) * image->height * image->channels, std::string(c.file) + ": pixel array is width x height x channels");
		CompareImage(c, image->width, image->height, image->channels, sum, "decoder");

#else
		::Image image = LoadImage(path.c_str());
		Check(image.data != nullptr, std::string(c.file) + ": LoadImage succeeds");
		if (image.data == nullptr) { continue; }
		CompareImage(c, image.width, image.height, RaylibChannels(image.format), RaylibImageSum(image), "raylib");
		UnloadImage(image);
#endif
	}
}

#ifdef VEIL_ASSET_DECODE_STANDALONE
static void TestImageMemory()
{
	// The same bytes through LoadImageMemory give the same image as LoadImageFile (the smallest file)
	const ImageCase& c = kImages[0];
	const std::string path = ResourceFile(c.file);
	std::ifstream in(path, std::ios::binary);
	const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	Check(!bytes.empty(), std::string(c.file) + ": read for LoadImageMemory");
	const std::optional<Render::Decode::Image> image = Render::Decode::LoadImageMemory(bytes);
	Check(image.has_value(), std::string(c.file) + ": LoadImageMemory succeeds");
	if (!image) { return; }
	long long sum = 0;
	for (std::uint8_t b : image->pixels) { sum += b; }
	CompareImage(c, image->width, image->height, image->channels, sum, "decoder, from memory");
}

static void TestRefusals()
{
	Check(!Render::Decode::LoadGltf(ResourceFile("models/no_such_model.glb")).has_value(), "a missing .glb is refused");
	Check(!Render::Decode::LoadGltf(ResourceFile("blocks/frame.png")).has_value(), "a PNG is not a glTF");
	Check(!Render::Decode::LoadImageFile(ResourceFile("blocks/no_such_image.png")).has_value(), "a missing image is refused");
	Check(!Render::Decode::LoadImageFile(ResourceFile("models/items/Artifact/RubixCube0.glb")).has_value(), "a glTF is not an image");
	const std::uint8_t junk[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	Check(!Render::Decode::LoadImageMemory(junk).has_value(), "junk bytes are not an image");
}
#endif

int main()
{
#ifndef VEIL_ASSET_DECODE_STANDALONE
	OpenTestWindow("veil-asset-decode-tests", false, true);
	std::printf("Asset decode tests: raylib against the recorded constants\n\n");
#else
	std::printf("Asset decode tests: the decoder against the recorded constants\n\n");
#endif

	TestModels();
	std::printf("\n");
	TestImages();
#ifdef VEIL_ASSET_DECODE_STANDALONE
	TestImageMemory();
	TestRefusals();
#endif

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

#ifndef VEIL_ASSET_DECODE_STANDALONE
	CloseTestWindow();
#endif
	return g_failures == 0 ? 0 : 1;
}
