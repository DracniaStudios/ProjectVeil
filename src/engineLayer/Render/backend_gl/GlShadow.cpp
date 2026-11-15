// The directional shadow map on OpenGL 3.3 core (B-22): a depth-only framebuffer whose depth
// attachment is a texture, and the pass that fills it. The reference is the raylib backend and
// raylib 6.0's own code under it:
//   RaylibRenderer.cpp  LoadShadowMapTarget, UnloadShadowMapTarget, SetShadowMapSize,
//                       BeginShadowPass, EndShadowPass, BeginWorld
//   rlgl.h              rlLoadFramebuffer, rlLoadTextureDepth (unsized GL_DEPTH_COMPONENT,
//                       GL_UNSIGNED_INT, GL_NEAREST, GL_CLAMP_TO_EDGE), rlFramebufferAttach,
//                       rlFramebufferComplete, rlUnloadFramebuffer
//   rcore.c             BeginTextureMode, EndTextureMode, ClearBackground, BeginMode3D (aspect is
//                       the target's width over its height, 1 here), EndMode3D
// The light's matrices come from GlLit::BeginCamera, the same code as the world pass.
#include "GlShared.h"

#include <Platform.h>

#include <glad/gl.h>

#include <iostream>

namespace {

// The texture unit of the shadow map, as in the raylib backend: model draws own the units below 12
constexpr int kShadowUnit = 15;

GLuint framebuffer = 0;
GLuint depthTexture = 0;
int mapSize = 0;
Matrix lightViewProjection = {};

// rlUnloadFramebuffer: the depth texture goes with the framebuffer
void Unload(GLuint fbo, GLuint depth)
{
	if (fbo == 0) { return; }
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (depth != 0) { glDeleteTextures(1, &depth); }
	glDeleteFramebuffers(1, &fbo);
}

void Create(int resolution)
{
	// Without loaded GL functions raylib's rlLoadFramebuffer also returns 0
	if (glGenFramebuffers == nullptr) {
		std::cerr << "[Lighting] Failed to create the shadow map framebuffer\n";
		return;
	}

	GLuint fbo = 0;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (fbo == 0) {
		std::cerr << "[Lighting] Failed to create the shadow map framebuffer\n";
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// rlLoadTextureDepth(resolution, resolution, false)
	GLuint depth = 0;
	glGenTextures(1, &depth);
	glBindTexture(GL_TEXTURE_2D, depth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);

	// raylib leaves the draw and read buffers at their default, COLOR_ATTACHMENT0, which this
	// framebuffer does not have. A core profile requires them to be NONE for a depth-only
	// target to be complete (INCOMPLETE_DRAW_BUFFER / INCOMPLETE_READ_BUFFER otherwise); some
	// drivers accept it anyway. The setting belongs to this framebuffer and changes no pixel.
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (!complete) {
		std::cerr << "[Lighting] Shadow map framebuffer is incomplete \xE2\x80\x94 shadows disabled\n";
		Unload(fbo, depth);
		return;
	}

	framebuffer = fbo;
	depthTexture = depth;
	mapSize = resolution;
	std::cout << "[Lighting] Shadow map ready: " << resolution << "x" << resolution << "\n";
}

void Destroy()
{
	Unload(framebuffer, depthTexture);
	framebuffer = 0;
	depthTexture = 0;
	mapSize = 0;
}

}

namespace GlShadow {

int Size() { return framebuffer != 0 ? mapSize : 0; }

bool SetSize(int size)
{
	if (size == Size()) { return true; }

	Destroy();
	if (size > 0) { Create(size); }
	return Size() == size;
}

bool Begin(const Camera3D& lightCamera)
{
	if (framebuffer == 0) { return false; }

	// BeginTextureMode flushes what is batched first
	Gl2D::Flush();

	// depthPass makes the fragment shader return before it reads shadowMap, which is this
	// pass's render target. raylib leaves the map bound to unit 15 from the last world pass; it
	// is taken off here so that no sampler of the program reads the texture being drawn to.
	GlLit::SetDepthPass(true);
	glActiveTexture(GL_TEXTURE0 + kShadowUnit);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	// BeginTextureMode and ClearBackground(WHITE)
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glViewport(0, 0, mapSize, mapSize);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClearDepth(1.0);
	glDepthMask(GL_TRUE); // a clear writes only where the mask is on
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// BeginMode3D: the aspect is the map's width over its height
	GlLit::BeginCamera(lightCamera, static_cast<float>(mapSize) / static_cast<float>(mapSize));
	lightViewProjection = GlLit::ViewProjection();
	return true;
}

void End()
{
	// EndMode3D leaves the depth test off, EndTextureMode binds the window again and sets its viewport
	glDisable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, Platform::RenderWidth(), Platform::RenderHeight());

	GlLit::SetDepthPass(false);
	GlLit::SetLightViewProjection(lightViewProjection);
}

Matrix ViewProjection() { return lightViewProjection; }

void BindForWorld()
{
	if (depthTexture == 0) { return; }

	glActiveTexture(GL_TEXTURE0 + kShadowUnit);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glActiveTexture(GL_TEXTURE0);
	GlLit::SetShadowSampler(kShadowUnit);
}

void Shutdown()
{
	Destroy();
	lightViewProjection = Matrix{};
}

}
