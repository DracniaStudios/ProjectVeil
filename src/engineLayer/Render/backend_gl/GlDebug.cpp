// The GL backend's debug shapes (B-23): lines, spheres, cubes, bounding boxes, cylinders,
// capsules and the grid, batched and drawn with the flat shader under the world's matrices.
//
// Ported from raylib 6.0, so that the shapes match the raylib build: the same vertices, in the
// same order, from the same arithmetic, and the same batch bookkeeping, so that the batch is
// drawn at the same moments relative to the model draws:
//   rmodels.c  DrawLine3D, DrawCube, DrawCubeV, DrawCubeWires, DrawSphere, DrawSphereEx,
//              DrawSphereWires, DrawCylinderEx, DrawCapsule, DrawGrid, DrawBoundingBox
//   rlgl.h     rlBegin, rlEnd, rlVertex3f, rlColor3f, rlPushMatrix, rlPopMatrix, rlTranslatef,
//              rlScalef (the CPU-side transform), rlCheckRenderBatchLimit, rlDrawRenderBatch
//              (the MVP, the draw loop over the draw calls, the reset), rlEnableDepthTest,
//              rlDisableDepthTest
// raylib's copyright lines in those files:
//   rmodels.c: Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
//   rlgl.h: Copyright (c) 2014-2026 Ramon Santamaria (@raysan5)
// Used under raylib's zlib/libpng licence, which permits use and alteration with this
// notice kept; this file is an altered version and is not raylib's original source.
//
// The flush points are raylib's: the batch is drawn when it is full, when the draw call table
// is full, at SetDebugDepthTest and at EndWorld (EndMode3D). raylib's DrawMesh never flushes
// it, so in both builds the batched shapes reach the GPU after every model drawn since the
// last flush.
#include "GlShared.h"

#include <raymath.h> // vector and matrix math, PI, DEG2RAD: inline only

#include <glad/gl.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

// rlgl.h's defaults
constexpr int kBatchElements = 8192;                // RL_DEFAULT_BATCH_BUFFER_ELEMENTS
constexpr int kBatchVertices = kBatchElements * 4;
constexpr int kBatchDrawCalls = 256;                // RL_DEFAULT_BATCH_DRAWCALLS

constexpr int kLines = 0x0001;                      // RL_LINES
constexpr int kTriangles = 0x0004;                  // RL_TRIANGLES
constexpr int kQuads = 0x0007;                      // RL_QUADS

struct Vertex
{
	float x, y, z;
	std::uint8_t r, g, b, a;
};

struct DrawCall
{
	int mode = kQuads;
	int vertexCount = 0;
	int vertexAlignment = 0;
};

struct State
{
	bool ready = false;
	bool inWorld = false;
	bool depthTest = true;

	GLuint program = 0;
	GLint mvpLocation = -1;
	GLint textureLocation = -1;
	GLuint vao = 0;
	GLuint vbo = 0;

	// The batch: vertices.size() is RLGL.State.vertexCounter, padding included
	std::vector<Vertex> vertices;
	DrawCall draws[kBatchDrawCalls];
	int drawCounter = 1;

	// RLGL.State.transform and transformRequired, which rlPushMatrix switches on
	Matrix transform = MatrixIdentity();
	bool transformRequired = false;

	Color color = WHITE;
};

State g;

int VertexCounter() { return static_cast<int>(g.vertices.size()); }

// rlDrawRenderBatch, with the state it needs set again, whatever the last draw left
void DrawBatch()
{
	if (g.ready && g.program != 0 && !g.vertices.empty()) {
		glUseProgram(g.program);

		const Matrix mvp = GlLit::ViewProjection(); // rlMatrixMultiply(modelview, projection)
		const float16 mvpFloats = MatrixToFloatV(mvp);
		glUniformMatrix4fv(g.mvpLocation, 1, GL_FALSE, mvpFloats.v);
		glUniform1i(g.textureLocation, 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Gl2D::WhiteTexture());

		glBindVertexArray(g.vao);
		glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(g.vertices.size() * sizeof(Vertex)), g.vertices.data());
		// The attributes the batch does not feed: texcoord (0,0), as raylib's batch buffer holds
		glVertexAttrib2f(1, 0.0f, 0.0f);

		// The state of a raylib world pass
		glDepthFunc(GL_LEQUAL);
		if (g.depthTest) { glEnable(GL_DEPTH_TEST); } else { glDisable(GL_DEPTH_TEST); }
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquation(GL_FUNC_ADD);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glLineWidth(1.0f);

		int vertexOffset = 0;
		for (int i = 0; i < g.drawCounter; i++) {
			const DrawCall& draw = g.draws[i];
			if ((draw.mode == kLines || draw.mode == kTriangles) && draw.vertexCount > 0) {
				glDrawArrays(draw.mode == kLines ? GL_LINES : GL_TRIANGLES, vertexOffset, draw.vertexCount);
			}
			vertexOffset += (draw.vertexCount + draw.vertexAlignment);
		}

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
	}

	// The reset: raylib clears the mode and the count of every draw call, not the alignment
	g.vertices.clear();
	for (DrawCall& draw : g.draws) {
		draw.mode = kQuads;
		draw.vertexCount = 0;
	}
	g.drawCounter = 1;
}

// rlCheckRenderBatchLimit
bool CheckLimit(int vertexCount)
{
	if (VertexCounter() + vertexCount >= kBatchVertices) {
		const int currentMode = g.draws[g.drawCounter - 1].mode;
		DrawBatch();
		g.draws[g.drawCounter - 1].mode = currentMode;
		return true;
	}
	return false;
}

// rlBegin
void Begin(int mode)
{
	DrawCall& current = g.draws[g.drawCounter - 1];
	if (current.mode != mode) {
		if (current.vertexCount > 0) {
			if (current.mode == kLines) { current.vertexAlignment = (current.vertexCount < 4) ? current.vertexCount : current.vertexCount % 4; }
			else if (current.mode == kTriangles) { current.vertexAlignment = (current.vertexCount < 4) ? 1 : (4 - (current.vertexCount % 4)); }
			else { current.vertexAlignment = 0; }

			if (!CheckLimit(current.vertexAlignment)) {
				// The alignment vertices are not drawn but count as an offset for the next draw
				g.vertices.resize(g.vertices.size() + static_cast<std::size_t>(current.vertexAlignment));
				g.drawCounter++;
			}
		}

		if (g.drawCounter >= kBatchDrawCalls) { DrawBatch(); }

		g.draws[g.drawCounter - 1].mode = mode;
	}
}

// rlColor4ub
void SetColor(Color color) { g.color = color; }

// rlVertex3f
void Vertex3f(float x, float y, float z)
{
	float tx = x;
	float ty = y;
	float tz = z;

	// Transform provided vector if required
	if (g.transformRequired) {
		tx = g.transform.m0*x + g.transform.m4*y + g.transform.m8*z + g.transform.m12;
		ty = g.transform.m1*x + g.transform.m5*y + g.transform.m9*z + g.transform.m13;
		tz = g.transform.m2*x + g.transform.m6*y + g.transform.m10*z + g.transform.m14;
	}

	if (VertexCounter() > (kBatchVertices - 4)) {
		DrawCall& current = g.draws[g.drawCounter - 1];
		if (current.mode == kLines && current.vertexCount % 2 == 0) { CheckLimit(2 + 1); }
		else if (current.mode == kTriangles && current.vertexCount % 3 == 0) { CheckLimit(3 + 1); }
		else if (current.mode == kQuads && current.vertexCount % 4 == 0) { CheckLimit(4 + 1); }
	}

	g.vertices.push_back(Vertex{ tx, ty, tz, g.color.r, g.color.g, g.color.b, g.color.a });
	g.draws[g.drawCounter - 1].vertexCount++;
}

// rlPushMatrix, outside any earlier push: the transform it saves is the identity
void PushMatrix()
{
	g.transformRequired = true;
	g.transform = MatrixIdentity();
}

// rlPopMatrix back to an empty stack
void PopMatrix()
{
	g.transform = MatrixIdentity();
	g.transformRequired = false;
}

// rlTranslatef
void Translate(float x, float y, float z)
{
	Matrix matTranslation = MatrixIdentity();
	matTranslation.m12 = x;
	matTranslation.m13 = y;
	matTranslation.m14 = z;
	g.transform = MatrixMultiply(matTranslation, g.transform);
}

// rlScalef
void Scale(float x, float y, float z)
{
	Matrix matScale = MatrixIdentity();
	matScale.m0 = x;
	matScale.m5 = y;
	matScale.m10 = z;
	g.transform = MatrixMultiply(matScale, g.transform);
}

// rmodels.c DrawCubeWires
void CubeWires(Vector3 position, float width, float height, float length, Color color)
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	PushMatrix();
		Translate(position.x, position.y, position.z);

		Begin(kLines);
			SetColor(color);

			// Front face
			// Bottom line
			Vertex3f(x - width/2, y - height/2, z + length/2);  // Bottom left
			Vertex3f(x + width/2, y - height/2, z + length/2);  // Bottom right

			// Left line
			Vertex3f(x + width/2, y - height/2, z + length/2);  // Bottom right
			Vertex3f(x + width/2, y + height/2, z + length/2);  // Top right

			// Top line
			Vertex3f(x + width/2, y + height/2, z + length/2);  // Top right
			Vertex3f(x - width/2, y + height/2, z + length/2);  // Top left

			// Right line
			Vertex3f(x - width/2, y + height/2, z + length/2);  // Top left
			Vertex3f(x - width/2, y - height/2, z + length/2);  // Bottom left

			// Back face
			// Bottom line
			Vertex3f(x - width/2, y - height/2, z - length/2);  // Bottom left
			Vertex3f(x + width/2, y - height/2, z - length/2);  // Bottom right

			// Left line
			Vertex3f(x + width/2, y - height/2, z - length/2);  // Bottom right
			Vertex3f(x + width/2, y + height/2, z - length/2);  // Top right

			// Top line
			Vertex3f(x + width/2, y + height/2, z - length/2);  // Top right
			Vertex3f(x - width/2, y + height/2, z - length/2);  // Top left

			// Right line
			Vertex3f(x - width/2, y + height/2, z - length/2);  // Top left
			Vertex3f(x - width/2, y - height/2, z - length/2);  // Bottom left

			// Top face
			// Left line
			Vertex3f(x - width/2, y + height/2, z + length/2);  // Top left front
			Vertex3f(x - width/2, y + height/2, z - length/2);  // Top left back

			// Right line
			Vertex3f(x + width/2, y + height/2, z + length/2);  // Top right front
			Vertex3f(x + width/2, y + height/2, z - length/2);  // Top right back

			// Bottom face
			// Left line
			Vertex3f(x - width/2, y - height/2, z + length/2);  // Top left front
			Vertex3f(x - width/2, y - height/2, z - length/2);  // Top left back

			// Right line
			Vertex3f(x + width/2, y - height/2, z + length/2);  // Top right front
			Vertex3f(x + width/2, y - height/2, z - length/2);  // Top right back
	PopMatrix();
}

// rmodels.c DrawCube
void Cube(Vector3 position, float width, float height, float length, Color color)
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	PushMatrix();
		Translate(position.x, position.y, position.z);

		Begin(kTriangles);
			SetColor(color);

			// Front face
			Vertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left
			Vertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
			Vertex3f(x - width/2, y + height/2, z + length/2);  // Top Left

			Vertex3f(x + width/2, y + height/2, z + length/2);  // Top Right
			Vertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
			Vertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right

			// Back face
			Vertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Left
			Vertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
			Vertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right

			Vertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
			Vertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
			Vertex3f(x - width/2, y + height/2, z - length/2);  // Top Left

			// Top face
			Vertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
			Vertex3f(x - width/2, y + height/2, z + length/2);  // Bottom Left
			Vertex3f(x + width/2, y + height/2, z + length/2);  // Bottom Right

			Vertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
			Vertex3f(x - width/2, y + height/2, z - length/2);  // Top Left
			Vertex3f(x + width/2, y + height/2, z + length/2);  // Bottom Right

			// Bottom face
			Vertex3f(x - width/2, y - height/2, z - length/2);  // Top Left
			Vertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
			Vertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left

			Vertex3f(x + width/2, y - height/2, z - length/2);  // Top Right
			Vertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Right
			Vertex3f(x - width/2, y - height/2, z - length/2);  // Top Left

			// Right face
			Vertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
			Vertex3f(x + width/2, y + height/2, z - length/2);  // Top Right
			Vertex3f(x + width/2, y + height/2, z + length/2);  // Top Left

			Vertex3f(x + width/2, y - height/2, z + length/2);  // Bottom Left
			Vertex3f(x + width/2, y - height/2, z - length/2);  // Bottom Right
			Vertex3f(x + width/2, y + height/2, z + length/2);  // Top Left

			// Left face
			Vertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right
			Vertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
			Vertex3f(x - width/2, y + height/2, z - length/2);  // Top Right

			Vertex3f(x - width/2, y - height/2, z + length/2);  // Bottom Left
			Vertex3f(x - width/2, y + height/2, z + length/2);  // Top Left
			Vertex3f(x - width/2, y - height/2, z - length/2);  // Bottom Right
	PopMatrix();
}

}

namespace GlDebug {

bool Init()
{
	if (g.ready) { return true; }

	g.program = Gl2D::FlatProgram();
	if (g.program == 0) { return false; }
	g.mvpLocation = glGetUniformLocation(g.program, "mvp");
	g.textureLocation = glGetUniformLocation(g.program, "texture0");

	glGenVertexArrays(1, &g.vao);
	glGenBuffers(1, &g.vbo);
	glBindVertexArray(g.vao);
	glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(kBatchVertices) * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, x)));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, r)));
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	g.vertices.reserve(kBatchVertices);
	g.ready = true;
	return true;
}

void Shutdown()
{
	if (!g.ready) { return; }
	glDeleteBuffers(1, &g.vbo);
	glDeleteVertexArrays(1, &g.vao);
	g = State{};
}

void BeginWorld()
{
	if (!g.ready) { return; }
	// Nothing is batched across passes: BeginMode3D flushed
	g.vertices.clear();
	for (DrawCall& draw : g.draws) { draw = DrawCall{}; }
	g.drawCounter = 1;
	g.transform = MatrixIdentity();
	g.transformRequired = false;
	g.depthTest = true;
	g.inWorld = true;
}

void EndWorld()
{
	if (!g.ready) { return; }
	// EndMode3D draws the batch with the depth test as it stands
	DrawBatch();
	g.inWorld = false;
}

void SetDepthTest(bool on)
{
	if (!g.ready || !g.inWorld) { return; }
	DrawBatch();
	g.depthTest = on;
	if (on) { glEnable(GL_DEPTH_TEST); } else { glDisable(GL_DEPTH_TEST); }
}

// DrawLine3D
void Line(Vector3 startPos, Vector3 endPos, Color color)
{
	if (!g.ready || !g.inWorld) { return; }
	Begin(kLines);
		SetColor(color);
		Vertex3f(startPos.x, startPos.y, startPos.z);
		Vertex3f(endPos.x, endPos.y, endPos.z);
}

// DrawCubeV
void CubeV(Vector3 position, Vector3 size, Color color)
{
	if (!g.ready || !g.inWorld) { return; }
	Cube(position, size.x, size.y, size.z, color);
}

// DrawBoundingBox
void BoundingBoxWires(BoundingBox box, Color color)
{
	if (!g.ready || !g.inWorld) { return; }

	Vector3 size = { 0 };

	size.x = fabsf(box.max.x - box.min.x);
	size.y = fabsf(box.max.y - box.min.y);
	size.z = fabsf(box.max.z - box.min.z);

	Vector3 center = { box.min.x + size.x/2.0f, box.min.y + size.y/2.0f, box.min.z + size.z/2.0f };

	CubeWires(center, size.x, size.y, size.z, color);
}

// DrawSphereEx (DrawSphere is 16 rings and 16 slices)
void SphereEx(Vector3 centerPos, float radius, int rings, int slices, Color color)
{
	if (!g.ready || !g.inWorld) { return; }

	PushMatrix();
		// NOTE: Transformation is applied in inverse order (scale -> translate)
		Translate(centerPos.x, centerPos.y, centerPos.z);
		Scale(radius, radius, radius);

		Begin(kTriangles);
			SetColor(color);

			float ringangle = DEG2RAD*(180.0f/(rings + 1)); // Angle between latitudinal parallels
			float sliceangle = DEG2RAD*(360.0f/slices); // Angle between longitudinal meridians

			float cosring = cosf(ringangle);
			float sinring = sinf(ringangle);
			float cosslice = cosf(sliceangle);
			float sinslice = sinf(sliceangle);

			Vector3 vertices[4] = { 0 }; // Required to store face vertices
			vertices[2] = Vector3{ 0, 1, 0 };
			vertices[3] = Vector3{ sinring, cosring, 0 };

			for (int i = 0; i < rings + 1; i++)
			{
				for (int j = 0; j < slices; j++)
				{
					vertices[0] = vertices[2]; // Rotate around y axis to set up vertices for next face
					vertices[1] = vertices[3];
					vertices[2] = Vector3{ cosslice*vertices[2].x - sinslice*vertices[2].z, vertices[2].y, sinslice*vertices[2].x + cosslice*vertices[2].z }; // Rotation matrix around y axis
					vertices[3] = Vector3{ cosslice*vertices[3].x - sinslice*vertices[3].z, vertices[3].y, sinslice*vertices[3].x + cosslice*vertices[3].z };

					Vertex3f(vertices[0].x, vertices[0].y, vertices[0].z);
					Vertex3f(vertices[3].x, vertices[3].y, vertices[3].z);
					Vertex3f(vertices[1].x, vertices[1].y, vertices[1].z);

					Vertex3f(vertices[0].x, vertices[0].y, vertices[0].z);
					Vertex3f(vertices[2].x, vertices[2].y, vertices[2].z);
					Vertex3f(vertices[3].x, vertices[3].y, vertices[3].z);
				}

				vertices[2] = vertices[3]; // Rotate around z axis to set up  starting vertices for next ring
				vertices[3] = Vector3{ cosring*vertices[3].x + sinring*vertices[3].y, -sinring*vertices[3].x + cosring*vertices[3].y, vertices[3].z }; // Rotation matrix around z axis
			}
	PopMatrix();
}

// DrawSphereWires
void SphereWires(Vector3 centerPos, float radius, int rings, int slices, Color color)
{
	if (!g.ready || !g.inWorld) { return; }

	PushMatrix();
		// NOTE: Transformation is applied in inverse order (scale -> translate)
		Translate(centerPos.x, centerPos.y, centerPos.z);
		Scale(radius, radius, radius);

		Begin(kLines);
			SetColor(color);

			for (int i = 0; i < (rings + 2); i++)
			{
				for (int j = 0; j < slices; j++)
				{
					Vertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
					         sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
					         cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
					Vertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
					         sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
					         cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));

					Vertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*(j + 1)/slices)),
					         sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
					         cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*(j + 1)/slices)));
					Vertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*j/slices)),
					         sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
					         cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*j/slices)));

					Vertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*sinf(DEG2RAD*(360.0f*j/slices)),
					         sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1))),
					         cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*(i + 1)))*cosf(DEG2RAD*(360.0f*j/slices)));
					Vertex3f(cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*sinf(DEG2RAD*(360.0f*j/slices)),
					         sinf(DEG2RAD*(270 + (180.0f/(rings + 1))*i)),
					         cosf(DEG2RAD*(270 + (180.0f/(rings + 1))*i))*cosf(DEG2RAD*(360.0f*j/slices)));
				}
			}
	PopMatrix();
}

// DrawCylinderEx
void CylinderEx(Vector3 startPos, Vector3 endPos, float startRadius, float endRadius, int sides, Color color)
{
	if (!g.ready || !g.inWorld) { return; }

	if (sides < 3) sides = 3;

	Vector3 direction = { endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };
	if ((direction.x == 0) && (direction.y == 0) && (direction.z == 0)) return; // Security check

	// Construct a basis of the base and the top face:
	Vector3 b1 = Vector3Normalize(Vector3Perpendicular(direction));
	Vector3 b2 = Vector3Normalize(Vector3CrossProduct(b1, direction));

	float baseAngle = (2.0f*PI)/sides;

	Begin(kTriangles);
		SetColor(color);

		for (int i = 0; i < sides; i++)
		{
			// Compute the four vertices
			float s1 = sinf(baseAngle*(i + 0))*startRadius;
			float c1 = cosf(baseAngle*(i + 0))*startRadius;
			Vector3 w1 = { startPos.x + s1*b1.x + c1*b2.x, startPos.y + s1*b1.y + c1*b2.y, startPos.z + s1*b1.z + c1*b2.z };
			float s2 = sinf(baseAngle*(i + 1))*startRadius;
			float c2 = cosf(baseAngle*(i + 1))*startRadius;
			Vector3 w2 = { startPos.x + s2*b1.x + c2*b2.x, startPos.y + s2*b1.y + c2*b2.y, startPos.z + s2*b1.z + c2*b2.z };
			float s3 = sinf(baseAngle*(i + 0))*endRadius;
			float c3 = cosf(baseAngle*(i + 0))*endRadius;
			Vector3 w3 = { endPos.x + s3*b1.x + c3*b2.x, endPos.y + s3*b1.y + c3*b2.y, endPos.z + s3*b1.z + c3*b2.z };
			float s4 = sinf(baseAngle*(i + 1))*endRadius;
			float c4 = cosf(baseAngle*(i + 1))*endRadius;
			Vector3 w4 = { endPos.x + s4*b1.x + c4*b2.x, endPos.y + s4*b1.y + c4*b2.y, endPos.z + s4*b1.z + c4*b2.z };

			if (startRadius > 0)
			{
				Vertex3f(startPos.x, startPos.y, startPos.z); // |
				Vertex3f(w2.x, w2.y, w2.z);                   // T0
				Vertex3f(w1.x, w1.y, w1.z);                   // |
			}

			Vertex3f(w1.x, w1.y, w1.z);                       // |
			Vertex3f(w2.x, w2.y, w2.z);                       // T1
			Vertex3f(w3.x, w3.y, w3.z);                       // |

			Vertex3f(w2.x, w2.y, w2.z);                       // |
			Vertex3f(w4.x, w4.y, w4.z);                       // T2
			Vertex3f(w3.x, w3.y, w3.z);                       // |

			if (endRadius > 0)
			{
				Vertex3f(endPos.x, endPos.y, endPos.z);       // |
				Vertex3f(w3.x, w3.y, w3.z);                   // T3
				Vertex3f(w4.x, w4.y, w4.z);                   // |
			}
		}
}

// DrawCapsule
void Capsule(Vector3 startPos, Vector3 endPos, float radius, int slices, int rings, Color color)
{
	if (!g.ready || !g.inWorld) { return; }

	if (slices < 3) slices = 3;

	Vector3 direction = { endPos.x - startPos.x, endPos.y - startPos.y, endPos.z - startPos.z };

	// draw a sphere if start and end points are the same
	bool sphereCase = (direction.x == 0) && (direction.y == 0) && (direction.z == 0);
	if (sphereCase) direction = Vector3{0.0f, 1.0f, 0.0f};

	// Construct a basis of the base and the caps:
	Vector3 b0 = Vector3Normalize(direction);
	Vector3 b1 = Vector3Normalize(Vector3Perpendicular(direction));
	Vector3 b2 = Vector3Normalize(Vector3CrossProduct(b1, direction));
	Vector3 capCenter = endPos;

	float baseSliceAngle = (2.0f*PI)/slices;
	float baseRingAngle  = PI*0.5f/rings;

	Begin(kTriangles);
		SetColor(color);

		// render both caps
		for (int c = 0; c < 2; c++)
		{
			for (int i = 0; i < rings; i++)
			{
				for (int j = 0; j < slices; j++)
				{
					// Building up the rings from capCenter in the direction of the 'direction' vector computed earlier

					// Compute the four vertices
					float ringSin1 = sinf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 0 ));
					float ringCos1 = cosf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 0 ));
					Vector3 w1 = Vector3{
						capCenter.x + (sinf(baseRingAngle*( i + 0 ))*b0.x + ringSin1*b1.x + ringCos1*b2.x)*radius,
						capCenter.y + (sinf(baseRingAngle*( i + 0 ))*b0.y + ringSin1*b1.y + ringCos1*b2.y)*radius,
						capCenter.z + (sinf(baseRingAngle*( i + 0 ))*b0.z + ringSin1*b1.z + ringCos1*b2.z)*radius
					};
					float ringSin2 = sinf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 0 ));
					float ringCos2 = cosf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 0 ));
					Vector3 w2 = Vector3{
						capCenter.x + (sinf(baseRingAngle*( i + 0 ))*b0.x + ringSin2*b1.x + ringCos2*b2.x)*radius,
						capCenter.y + (sinf(baseRingAngle*( i + 0 ))*b0.y + ringSin2*b1.y + ringCos2*b2.y)*radius,
						capCenter.z + (sinf(baseRingAngle*( i + 0 ))*b0.z + ringSin2*b1.z + ringCos2*b2.z)*radius
					};

					float ringSin3 = sinf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 1 ));
					float ringCos3 = cosf(baseSliceAngle*(j + 0))*cosf(baseRingAngle*( i + 1 ));
					Vector3 w3 = Vector3{
						capCenter.x + (sinf(baseRingAngle*( i + 1 ))*b0.x + ringSin3*b1.x + ringCos3*b2.x)*radius,
						capCenter.y + (sinf(baseRingAngle*( i + 1 ))*b0.y + ringSin3*b1.y + ringCos3*b2.y)*radius,
						capCenter.z + (sinf(baseRingAngle*( i + 1 ))*b0.z + ringSin3*b1.z + ringCos3*b2.z)*radius
					};
					float ringSin4 = sinf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 1 ));
					float ringCos4 = cosf(baseSliceAngle*(j + 1))*cosf(baseRingAngle*( i + 1 ));
					Vector3 w4 = Vector3{
						capCenter.x + (sinf(baseRingAngle*( i + 1 ))*b0.x + ringSin4*b1.x + ringCos4*b2.x)*radius,
						capCenter.y + (sinf(baseRingAngle*( i + 1 ))*b0.y + ringSin4*b1.y + ringCos4*b2.y)*radius,
						capCenter.z + (sinf(baseRingAngle*( i + 1 ))*b0.z + ringSin4*b1.z + ringCos4*b2.z)*radius
					};

					// Make sure cap triangle normals are facing outwards
					if (c == 0)
					{
						Vertex3f(w1.x, w1.y, w1.z);
						Vertex3f(w2.x, w2.y, w2.z);
						Vertex3f(w3.x, w3.y, w3.z);

						Vertex3f(w2.x, w2.y, w2.z);
						Vertex3f(w4.x, w4.y, w4.z);
						Vertex3f(w3.x, w3.y, w3.z);
					}
					else
					{
						Vertex3f(w1.x, w1.y, w1.z);
						Vertex3f(w3.x, w3.y, w3.z);
						Vertex3f(w2.x, w2.y, w2.z);

						Vertex3f(w2.x, w2.y, w2.z);
						Vertex3f(w3.x, w3.y, w3.z);
						Vertex3f(w4.x, w4.y, w4.z);
					}
				}
			}
			capCenter = startPos;
			b0 = Vector3Scale(b0, -1.0f);
		}
		// render middle
		if (!sphereCase)
		{
			for (int j = 0; j < slices; j++)
			{
				// compute the four vertices
				float ringSin1 = sinf(baseSliceAngle*(j + 0))*radius;
				float ringCos1 = cosf(baseSliceAngle*(j + 0))*radius;
				Vector3 w1 = {
					startPos.x + ringSin1*b1.x + ringCos1*b2.x,
					startPos.y + ringSin1*b1.y + ringCos1*b2.y,
					startPos.z + ringSin1*b1.z + ringCos1*b2.z
				};
				float ringSin2 = sinf(baseSliceAngle*(j + 1))*radius;
				float ringCos2 = cosf(baseSliceAngle*(j + 1))*radius;
				Vector3 w2 = {
					startPos.x + ringSin2*b1.x + ringCos2*b2.x,
					startPos.y + ringSin2*b1.y + ringCos2*b2.y,
					startPos.z + ringSin2*b1.z + ringCos2*b2.z
				};

				float ringSin3 = sinf(baseSliceAngle*(j + 0))*radius;
				float ringCos3 = cosf(baseSliceAngle*(j + 0))*radius;
				Vector3 w3 = {
					endPos.x + ringSin3*b1.x + ringCos3*b2.x,
					endPos.y + ringSin3*b1.y + ringCos3*b2.y,
					endPos.z + ringSin3*b1.z + ringCos3*b2.z
				};
				float ringSin4 = sinf(baseSliceAngle*(j + 1))*radius;
				float ringCos4 = cosf(baseSliceAngle*(j + 1))*radius;
				Vector3 w4 = {
					endPos.x + ringSin4*b1.x + ringCos4*b2.x,
					endPos.y + ringSin4*b1.y + ringCos4*b2.y,
					endPos.z + ringSin4*b1.z + ringCos4*b2.z
				};

				Vertex3f(w1.x, w1.y, w1.z);
				Vertex3f(w2.x, w2.y, w2.z);
				Vertex3f(w3.x, w3.y, w3.z);

				Vertex3f(w2.x, w2.y, w2.z);
				Vertex3f(w4.x, w4.y, w4.z);
				Vertex3f(w3.x, w3.y, w3.z);
			}
		}
}

// DrawGrid: rlColor3f is (unsigned char)(x*255) with an alpha of 255
void Grid(int slices, float spacing)
{
	if (!g.ready || !g.inWorld) { return; }

	int halfSlices = slices/2;

	Begin(kLines);
		for (int i = -halfSlices; i <= halfSlices; i++)
		{
			if (i == 0)
			{
				SetColor(Color{ (unsigned char)(0.5f*255), (unsigned char)(0.5f*255), (unsigned char)(0.5f*255), 255 });
			}
			else
			{
				SetColor(Color{ (unsigned char)(0.75f*255), (unsigned char)(0.75f*255), (unsigned char)(0.75f*255), 255 });
			}

			Vertex3f((float)i*spacing, 0.0f, (float)-halfSlices*spacing);
			Vertex3f((float)i*spacing, 0.0f, (float)halfSlices*spacing);

			Vertex3f((float)-halfSlices*spacing, 0.0f, (float)i*spacing);
			Vertex3f((float)halfSlices*spacing, 0.0f, (float)i*spacing);
		}
}

}
