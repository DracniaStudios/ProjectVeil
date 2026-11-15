// The one translation unit that defines the single-header libraries' implementations (B-15).
// Headers come from the upstream copies in thirdparty/ (stb, cgltf), not from raylib.
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
