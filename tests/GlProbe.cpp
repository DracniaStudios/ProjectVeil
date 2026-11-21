// Standalone probe: does SDL3 give a working OpenGL 3.3 core context here (B-02)?
// Not part of the game. Builds only with VEIL_BACKEND=sdl3gl. Portable: no platform code.
//
//   veil_gl_probe [--hidden]
//
// Prints one line per step. Exits 0 if every step passed, otherwise prints the failing
// step and exits 1.

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

#include <cstdio>
#include <cstring>

namespace {

int g_step = 0;

void Pass(const char* what) {
	std::printf("[step %d] PASS  %s\n", g_step, what);
}

int Fail(const char* what, const char* detail = nullptr) {
	std::printf("[step %d] FAIL  %s%s%s\n", g_step, what, detail ? ": " : "", detail ? detail : "");
	std::fflush(stdout);
	return 1;
}

const char* GlString(GLenum name) {
	const GLubyte* s = glGetString(name);
	return s ? reinterpret_cast<const char*>(s) : "(null)";
}

GLuint CompileShader(GLenum type, const char* source, char* log, int logSize) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
	GLint ok = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glGetShaderInfoLog(shader, logSize, nullptr, log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

const char* kVertexShader =
	"#version 330\n"
	"in vec3 vertexPosition;\n"
	"uniform mat4 mvp;\n"
	"void main() { gl_Position = mvp*vec4(vertexPosition, 1.0); }\n";

const char* kFragmentShader =
	"#version 330\n"
	"out vec4 finalColor;\n"
	"void main() { finalColor = vec4(1.0, 0.5, 0.25, 1.0); }\n";

} // namespace

int main(int argc, char** argv) {
	bool hidden = false;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--hidden") == 0) hidden = true;
	}
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	// 1. SDL_Init
	g_step = 1;
	SDL_SetMainReady();
	if (!SDL_Init(SDL_INIT_VIDEO)) return Fail("SDL_Init(SDL_INIT_VIDEO)", SDL_GetError());
	Pass("SDL_Init(SDL_INIT_VIDEO)");

	// 2. GL attributes
	g_step = 2;
	bool attributesOk =
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) &&
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) &&
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) &&
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) &&
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24) &&
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	if (!attributesOk) return Fail("SDL_GL_SetAttribute", SDL_GetError());
	Pass("GL attributes: 3.3 core, double buffer, depth 24, stencil 8");

	// 3. Window
	g_step = 3;
	SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
	if (hidden) flags |= SDL_WINDOW_HIDDEN;
	SDL_Window* window = SDL_CreateWindow("veil_gl_probe", 640, 360, flags);
	if (!window) return Fail("SDL_CreateWindow 640x360", SDL_GetError());
	Pass(hidden ? "window 640x360 (hidden)" : "window 640x360");

	// 4. Context, make current, swap interval
	g_step = 4;
	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (!context) return Fail("SDL_GL_CreateContext", SDL_GetError());
	if (!SDL_GL_MakeCurrent(window, context)) return Fail("SDL_GL_MakeCurrent", SDL_GetError());
	if (!SDL_GL_SetSwapInterval(0)) {
		// Not fatal on every driver, but the work order asks for it: report it as a failure.
		return Fail("SDL_GL_SetSwapInterval(0)", SDL_GetError());
	}
	Pass("context created, made current, swap interval 0");

	// 5. glad
	g_step = 5;
	int version = gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress));
	if (version == 0) return Fail("gladLoadGL returned 0");
	int major = GLAD_VERSION_MAJOR(version);
	int minor = GLAD_VERSION_MINOR(version);
	if (major < 3 || (major == 3 && minor < 3)) {
		char detail[64];
		std::snprintf(detail, sizeof detail, "loaded GL %d.%d, need 3.3", major, minor);
		return Fail("glad version", detail);
	}
	std::printf("[step 5] PASS  gladLoadGL: GL %d.%d\n", major, minor);

	// 6. Report
	g_step = 6;
	int w = 0, h = 0, pw = 0, ph = 0;
	SDL_GetWindowSize(window, &w, &h);
	SDL_GetWindowSizeInPixels(window, &pw, &ph);
	const char* driver = SDL_GetCurrentVideoDriver();
	std::printf("[step 6] GL_VERSION                  %s\n", GlString(GL_VERSION));
	std::printf("[step 6] GL_RENDERER                 %s\n", GlString(GL_RENDERER));
	std::printf("[step 6] GL_SHADING_LANGUAGE_VERSION %s\n", GlString(GL_SHADING_LANGUAGE_VERSION));
	std::printf("[step 6] video driver                %s\n", driver ? driver : "(null)");
	std::printf("[step 6] window size                 %d x %d\n", w, h);
	std::printf("[step 6] window size in pixels       %d x %d\n", pw, ph);
	std::printf("[step 6] window display scale        %.3f\n", SDL_GetWindowDisplayScale(window));
	Pass("report");

	// 7. Shader pair
	g_step = 7;
	char log[1024] = {};
	GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader, log, sizeof log);
	if (!vs) return Fail("vertex shader compile", log);
	GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader, log, sizeof log);
	if (!fs) return Fail("fragment shader compile", log);
	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glBindAttribLocation(program, 0, "vertexPosition");
	glLinkProgram(program);
	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		glGetProgramInfoLog(program, sizeof log, nullptr, log);
		return Fail("program link", log);
	}
	glDeleteShader(vs);
	glDeleteShader(fs);
	glDeleteProgram(program);
	Pass("#version 330 shader pair compiled and linked");

	// 8. Depth-only framebuffer: the shadow map case
	g_step = 8;
	GLuint depthTexture = 0, framebuffer = 0;
	glGenTextures(1, &depthTexture);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 2048, 2048, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
	glDrawBuffer(GL_NONE); // no colour attachment: depth is the only output
	glReadBuffer(GL_NONE);
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteTextures(1, &depthTexture);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		char detail[64];
		std::snprintf(detail, sizeof detail, "glCheckFramebufferStatus = 0x%04X", static_cast<unsigned>(status));
		return Fail("2048x2048 GL_DEPTH_COMPONENT24 framebuffer", detail);
	}
	Pass("2048x2048 GL_DEPTH_COMPONENT24 framebuffer complete");

	// 9. Clear for 60 frames, read the centre pixel before the last swap
	g_step = 9;
	unsigned char pixel[4] = {};
	for (int frame = 0; frame < 60; ++frame) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {}
		glViewport(0, 0, pw, ph);
		glClearColor(8.0f / 255.0f, 9.0f / 255.0f, 12.0f / 255.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		if (frame == 59) {
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glReadPixels(pw / 2, ph / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
		}
		if (!SDL_GL_SwapWindow(window)) return Fail("SDL_GL_SwapWindow", SDL_GetError());
	}
	GLenum glError = glGetError();
	if (glError != GL_NO_ERROR) {
		char detail[64];
		std::snprintf(detail, sizeof detail, "glGetError = 0x%04X", static_cast<unsigned>(glError));
		return Fail("GL error after 60 frames", detail);
	}
	auto within1 = [](int a, int b) { return a - b <= 1 && b - a <= 1; };
	std::printf("[step 9] centre pixel RGBA           %d %d %d %d (expected 8 9 12 255)\n",
		pixel[0], pixel[1], pixel[2], pixel[3]);
	if (!within1(pixel[0], 8) || !within1(pixel[1], 9) || !within1(pixel[2], 12)) {
		return Fail("centre pixel is not (8, 9, 12)");
	}
	Pass("60 frames cleared, centre pixel is (8, 9, 12)");

	// 10. Done
	g_step = 10;
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
	std::printf("[step 10] PASS  all steps passed\n");
	return 0;
}
