// The one translation unit that holds glad's implementation (B-15). glad 2 from
// thirdparty/glad-2.0.8; every other file only includes <glad/gl.h> for declarations.
// The loader is called as gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) in GlRenderer.cpp.
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
