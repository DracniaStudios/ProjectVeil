// The lit world on OpenGL 3.3 (B-21): the lighting shader, Render::SetLighting's uniforms,
// the world pass and the model draws, written to put the same numbers on the GPU as the
// raylib build does. The reference is raylib 6.0's own code, not the raymath helpers it
// could have used, because the order of the matrix products and the precision of the
// projection reach the last bit:
//   rcore.c    BeginMode3D, EndMode3D (the projection in double, then rlFrustum or rlOrtho;
//              the view matrix goes through rlMultMatrixf onto an identity modelview)
//   rlgl.h     rlFrustum, rlOrtho (their float casts), rlMultMatrixf, rlGetMatrixTransform
//   rmodels.c  DrawModel, DrawModelEx (the transform and the integer tint), DrawMesh
//              (the uniforms, the material map binding, the default vertex attributes),
//              DrawModelWiresEx
//   rlgl.h     rlEnableWireMode, rlDisableWireMode, the state of rlglInit
// raylib's copyright lines in those files:
//   rcore.c, rmodels.c: Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
//   rlgl.h: Copyright (c) 2014-2026 Ramon Santamaria (@raysan5)
// Used under raylib's zlib/libpng licence, which permits use and alteration with this
// notice kept; this file is an altered version and is not raylib's original source.
#include "GlShared.h"

#include <Platform.h>
#include <ResourcePaths.h>
#include <raymath.h> // matrix math: inline only

#include <glad/gl.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

// raylib's default shader (rlgl.h), whose colDiffuse is what a tint is on the unlit path
const char* const kFallbackVertex =
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

const char* const kFallbackFragment =
	"#version 330\n"
	"in vec2 fragTexCoord;\n"
	"in vec4 fragColor;\n"
	"out vec4 finalColor;\n"
	"uniform sampler2D texture0;\n"
	"uniform vec4 colDiffuse;\n"
	"void main()\n"
	"{\n"
	"    vec4 texelColor = texture(texture0, fragTexCoord);\n"
	"    finalColor = texelColor*colDiffuse*fragColor;\n"
	"}\n";

// The uniforms of lighting.vs and lighting.fs, as raylib's backend caches them
struct LitProgram
{
	GLuint id = 0;
	GLint mvp = -1, matModel = -1, matNormal = -1, colDiffuse = -1, texture0 = -1, viewPos = -1;
	GLint lightCount = -1, lightType = -1, lightPosition = -1, lightDirection = -1, lightColor = -1, lightRange = -1, lightCone = -1;
	GLint ambient = -1, fogColor = -1, fogDensity = -1, exposure = -1, specularStrength = -1, shininess = -1;
	GLint shadowTexelSize = -1, shadowBias = -1, shadowSoftness = -1, shadowsEnabled = -1, shadowLightIndex = -1;
	GLint depthPass = -1;
};

struct FallbackProgram
{
	GLuint id = 0;
	GLint mvp = -1, colDiffuse = -1, texture0 = -1;
};

LitProgram lit;
FallbackProgram fallback;

// What BeginMode3D leaves in rlgl: the modelview (the view matrix times an identity) and the projection
Matrix modelview = MatrixIdentity();
Matrix projection = MatrixIdentity();

GLuint CompileStage(GLenum type, const char* label, const std::string& source)
{
	const GLuint shader = glCreateShader(type);
	const char* text = source.c_str();
	glShaderSource(shader, 1, &text, nullptr);
	glCompileShader(shader);
	GLint ok = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[2048] = {};
		glGetShaderInfoLog(shader, sizeof(log) - 1, nullptr, log);
		std::cerr << "[Render] " << label << " failed to compile: " << log << std::endl;
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

// The four attribute locations of ADR B section 7 are bound before the link
GLuint BuildProgram(const char* label, const std::string& vertexSource, const std::string& fragmentSource)
{
	const GLuint vs = CompileStage(GL_VERTEX_SHADER, (std::string(label) + " vertex stage").c_str(), vertexSource);
	const GLuint fs = CompileStage(GL_FRAGMENT_SHADER, (std::string(label) + " fragment stage").c_str(), fragmentSource);
	if (vs == 0 || fs == 0) {
		if (vs != 0) { glDeleteShader(vs); }
		if (fs != 0) { glDeleteShader(fs); }
		return 0;
	}

	const GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glBindAttribLocation(program, 0, "vertexPosition");
	glBindAttribLocation(program, 1, "vertexTexCoord");
	glBindAttribLocation(program, 2, "vertexNormal");
	glBindAttribLocation(program, 3, "vertexColor");
	glLinkProgram(program);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint linked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		char log[2048] = {};
		glGetProgramInfoLog(program, sizeof(log) - 1, nullptr, log);
		std::cerr << "[Render] " << label << " failed to link: " << log << std::endl;
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

bool ReadFile(const std::filesystem::path& path, std::string& text)
{
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "[Render] cannot open " << path.string() << std::endl;
		return false;
	}
	std::ostringstream contents;
	contents << file.rdbuf();
	text = contents.str();
	return true;
}

void LoadLighting()
{
	const std::filesystem::path directory = ResourcePaths::Resources() / "shaders" / "glsl330";
	std::string vertexSource, fragmentSource;
	if (!ReadFile(directory / "lighting.vs", vertexSource) || !ReadFile(directory / "lighting.fs", fragmentSource)) { return; }

	const GLuint program = BuildProgram("lighting shader", vertexSource, fragmentSource);
	if (program == 0) { return; }

	lit.id = program;
	const auto at = [program](const char* name) { return glGetUniformLocation(program, name); };
	lit.mvp = at("mvp");
	lit.matModel = at("matModel");
	lit.matNormal = at("matNormal");
	lit.colDiffuse = at("colDiffuse");
	lit.texture0 = at("texture0");
	lit.viewPos = at("viewPos");
	lit.lightCount = at("lightCount");
	lit.lightType = at("lightType");
	lit.lightPosition = at("lightPosition");
	lit.lightDirection = at("lightDirection");
	lit.lightColor = at("lightColor");
	lit.lightRange = at("lightRange");
	lit.lightCone = at("lightCone");
	lit.ambient = at("ambient");
	lit.fogColor = at("fogColor");
	lit.fogDensity = at("fogDensity");
	lit.exposure = at("exposure");
	lit.specularStrength = at("specularStrength");
	lit.shininess = at("shininess");
	lit.shadowTexelSize = at("shadowTexelSize");
	lit.shadowBias = at("shadowBias");
	lit.shadowSoftness = at("shadowSoftness");
	lit.shadowsEnabled = at("shadowsEnabled");
	lit.shadowLightIndex = at("shadowLightIndex");
	lit.depthPass = at("depthPass");
}

void LoadFallback()
{
	const GLuint program = BuildProgram("unlit model shader", kFallbackVertex, kFallbackFragment);
	if (program == 0) { return; }
	fallback.id = program;
	fallback.mvp = glGetUniformLocation(program, "mvp");
	fallback.colDiffuse = glGetUniformLocation(program, "colDiffuse");
	fallback.texture0 = glGetUniformLocation(program, "texture0");
}

// rlgl.h rlFrustum onto the identity matrix BeginMode3D loaded: the float casts are raylib's
Matrix Frustum(double left, double right, double bottom, double top, double znear, double zfar)
{
	Matrix matFrustum = { 0 };

	float rl = (float)(right - left);
	float tb = (float)(top - bottom);
	float fn = (float)(zfar - znear);

	matFrustum.m0 = ((float)znear*2.0f)/rl;
	matFrustum.m1 = 0.0f;
	matFrustum.m2 = 0.0f;
	matFrustum.m3 = 0.0f;

	matFrustum.m4 = 0.0f;
	matFrustum.m5 = ((float)znear*2.0f)/tb;
	matFrustum.m6 = 0.0f;
	matFrustum.m7 = 0.0f;

	matFrustum.m8 = ((float)right + (float)left)/rl;
	matFrustum.m9 = ((float)top + (float)bottom)/tb;
	matFrustum.m10 = -((float)zfar + (float)znear)/fn;
	matFrustum.m11 = -1.0f;

	matFrustum.m12 = 0.0f;
	matFrustum.m13 = 0.0f;
	matFrustum.m14 = -((float)zfar*(float)znear*2.0f)/fn;
	matFrustum.m15 = 0.0f;

	return MatrixMultiply(MatrixIdentity(), matFrustum);
}

// rlgl.h rlOrtho onto the identity matrix
Matrix Ortho(double left, double right, double bottom, double top, double znear, double zfar)
{
	Matrix matOrtho = { 0 };

	float rl = (float)(right - left);
	float tb = (float)(top - bottom);
	float fn = (float)(zfar - znear);

	matOrtho.m0 = 2.0f/rl;
	matOrtho.m1 = 0.0f;
	matOrtho.m2 = 0.0f;
	matOrtho.m3 = 0.0f;
	matOrtho.m4 = 0.0f;
	matOrtho.m5 = 2.0f/tb;
	matOrtho.m6 = 0.0f;
	matOrtho.m7 = 0.0f;
	matOrtho.m8 = 0.0f;
	matOrtho.m9 = 0.0f;
	matOrtho.m10 = -2.0f/fn;
	matOrtho.m11 = 0.0f;
	matOrtho.m12 = -((float)left + (float)right)/rl;
	matOrtho.m13 = -((float)top + (float)bottom)/tb;
	matOrtho.m14 = -((float)zfar + (float)znear)/fn;
	matOrtho.m15 = 1.0f;

	return MatrixMultiply(MatrixIdentity(), matOrtho);
}

void UploadMatrix(GLint location, const Matrix& matrix)
{
	const float16 values = MatrixToFloatV(matrix);
	glUniformMatrix4fv(location, 1, GL_FALSE, values.v);
}

// rlgl's near and far (RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR)
constexpr double kNear = 0.05;
constexpr double kFar = 4000.0;

}

namespace GlLit {

bool Init()
{
	LoadLighting();
	LoadFallback();
	return lit.id != 0 || fallback.id != 0;
}

void Shutdown()
{
	if (lit.id != 0) { glDeleteProgram(lit.id); }
	if (fallback.id != 0) { glDeleteProgram(fallback.id); }
	lit = LitProgram{};
	fallback = FallbackProgram{};
	modelview = MatrixIdentity();
	projection = MatrixIdentity();
}

unsigned int ShaderId() { return lit.id; }

// BeginMode3D
void BeginWorld(const Camera3D& camera)
{
	Gl2D::Flush();

	const float aspect = static_cast<float>(Platform::RenderWidth()) / static_cast<float>(Platform::RenderHeight());
	if (camera.projection == CAMERA_PERSPECTIVE) {
		double top = kNear*tan(camera.fovy*0.5*DEG2RAD);
		double right = top*aspect;
		projection = Frustum(-right, right, -top, top, kNear, kFar);
	} else if (camera.projection == CAMERA_ORTHOGRAPHIC) {
		double top = camera.fovy/2.0;
		double right = top*aspect;
		projection = Ortho(-right, right, -top, top, kNear, kFar);
	} else {
		projection = MatrixIdentity();
	}

	// rlMultMatrixf(MatrixToFloat(view)) onto the identity modelview
	modelview = MatrixMultiply(MatrixLookAt(camera.position, camera.target, camera.up), MatrixIdentity());

	// The state of BeginMode3D and rlglInit
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);

	// The world pass reads no shadow map yet and the depth pass is never on in it
	if (lit.id != 0) {
		glUseProgram(lit.id);
		glUniform1i(lit.depthPass, 0);
		glUseProgram(0);
	}
}

// EndMode3D leaves the depth test off, and nothing of the world pass stays bound
void EndWorld()
{
	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	glDisable(GL_DEPTH_TEST);
}

// DrawStoredModel, DrawModelEx and DrawMesh. With wires, rlEnableWireMode around all of it.
void DrawModel(const Render::ModelDraw& draw, bool wires)
{
	const GlResources::Model* model = GlResources::FindModel(draw.model);
	if (model == nullptr) { return; }

	const bool useLit = lit.id != 0;
	if (!useLit && fallback.id == 0) { return; }

	// DrawStoredModel: model.transform
	Matrix transform = MatrixMultiply(
		MatrixScale(draw.scale.x, draw.scale.y, draw.scale.z),
		QuaternionToMatrix(draw.rotation));

	// DrawModel -> DrawModelEx(position, axis (0,1,0), angle 0, scale (1,1,1))
	const Vector3 scale = { 1.0f, 1.0f, 1.0f };
	const Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };
	const Matrix matScale = MatrixScale(scale.x, scale.y, scale.z);
	const Matrix matRotation = MatrixRotate(rotationAxis, 0.0f*DEG2RAD);
	const Matrix matTranslation = MatrixTranslate(draw.translation.x, draw.translation.y, draw.translation.z);
	const Matrix matTransform = MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);
	transform = MatrixMultiply(transform, matTransform);

	// DrawMesh: rlGetMatrixTransform() is the identity outside rlPushMatrix
	const Matrix matModel = MatrixMultiply(transform, MatrixIdentity());
	const Matrix matView = modelview;
	const Matrix matModelView = MatrixMultiply(matModel, matView);
	const Matrix matModelViewProjection = MatrixMultiply(matModelView, projection);
	const Matrix matNormal = MatrixTranspose(MatrixInvert(matModel));

	const unsigned int overrideTexture = GlResources::TextureName(draw.diffuseOverride);

	const GLuint program = useLit ? lit.id : fallback.id;
	const GLint locDiffuse = useLit ? lit.colDiffuse : fallback.colDiffuse;
	const GLint locMvp = useLit ? lit.mvp : fallback.mvp;
	const GLint locTexture = useLit ? lit.texture0 : fallback.texture0;

	if (wires) { glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); }

	glUseProgram(program);
	for (const GlResources::Mesh& mesh : model->meshes) {
		if (mesh.vao == 0) { continue; }
		const GlResources::Material& material = model->materials[static_cast<std::size_t>(mesh.material)];

		// The tint is applied to the material colour in integers, as DrawModelEx does
		const Color tinted = {
			static_cast<unsigned char>(((int)material.color.r*(int)draw.tint.r)/255),
			static_cast<unsigned char>(((int)material.color.g*(int)draw.tint.g)/255),
			static_cast<unsigned char>(((int)material.color.b*(int)draw.tint.b)/255),
			static_cast<unsigned char>(((int)material.color.a*(int)draw.tint.a)/255) };
		const float colour[4] = {
			(float)tinted.r/255.0f, (float)tinted.g/255.0f, (float)tinted.b/255.0f, (float)tinted.a/255.0f };
		glUniform4fv(locDiffuse, 1, colour);

		if (useLit) {
			UploadMatrix(lit.matModel, matModel);
			UploadMatrix(lit.matNormal, matNormal);
		}

		// The diffuse override replaces material 0's texture only
		unsigned int texture = GlResources::TextureName(material.diffuse);
		if (mesh.material == 0 && overrideTexture != 0) { texture = overrideTexture; }
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);
		glUniform1i(locTexture, 0);

		glBindVertexArray(mesh.vao);
		// An attribute the mesh lacks has its array disabled and gets raylib's default
		if (!mesh.hasTexcoords) { glVertexAttrib2f(1, 0.0f, 0.0f); }
		if (!mesh.hasNormals) { glVertexAttrib3f(2, 0.0f, 0.0f, 1.0f); }
		if (!mesh.hasColors) { glVertexAttrib4f(3, 1.0f, 1.0f, 1.0f, 1.0f); }

		UploadMatrix(locMvp, matModelViewProjection);

		if (mesh.indexCount > 0) { glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr); }
		else { glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount); }
	}

	if (wires) { glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); }
}

// The raylib backend's SetLighting, uniform for uniform and in its order
void SetLighting(const Render::LightingFrame& frame)
{
	if (lit.id == 0) { return; }
	constexpr int kMax = Render::kMaxLights;

	glUseProgram(lit.id);

	// viewPos drives both the specular term and the fog distance
	glUniform3fv(lit.viewPos, 1, &frame.viewPosition.x);

	int types[kMax] = {};
	float positions[kMax * 3] = {};
	float directions[kMax * 3] = {};
	float colors[kMax * 4] = {};
	float ranges[kMax] = {};
	float cones[kMax * 2] = {};

	for (int slot = 0; slot < kMax; slot++) {
		const Render::PackedLight& light = frame.lights[slot];

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

	glUniform1iv(lit.lightCount, 1, &frame.lightCount);
	glUniform1iv(lit.lightType, kMax, types);
	glUniform3fv(lit.lightPosition, kMax, positions);
	glUniform3fv(lit.lightDirection, kMax, directions);
	glUniform4fv(lit.lightColor, kMax, colors);
	glUniform1fv(lit.lightRange, kMax, ranges);
	glUniform2fv(lit.lightCone, kMax, cones);

	glUniform1iv(lit.shadowLightIndex, 1, &frame.shadowLightSlot);

	const float ambientValues[4] = { frame.ambient.x, frame.ambient.y, frame.ambient.z, frame.ambient.w };
	glUniform4fv(lit.ambient, 1, ambientValues);

	const float fogValues[4] = { frame.fogColor.x, frame.fogColor.y, frame.fogColor.z, frame.fogColor.w };
	glUniform4fv(lit.fogColor, 1, fogValues);

	glUniform1fv(lit.fogDensity, 1, &frame.fogDensity);
	glUniform1fv(lit.exposure, 1, &frame.exposure);
	glUniform1fv(lit.specularStrength, 1, &frame.specularStrength);
	glUniform1fv(lit.shininess, 1, &frame.shininess);
	glUniform1fv(lit.shadowTexelSize, 1, &frame.shadowTexelSize);
	glUniform1fv(lit.shadowBias, 1, &frame.shadowBias);
	glUniform1fv(lit.shadowSoftness, 1, &frame.shadowSoftness);

	const int shadowGate = frame.shadowsEnabled ? 1 : 0;
	glUniform1iv(lit.shadowsEnabled, 1, &shadowGate);

	glUseProgram(0);
}

}
