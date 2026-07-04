# ProjectVeil

A C++23 game currently in development, built with [raylib](https://www.raylib.com/) for rendering/input, [Dear ImGui](https://github.com/ocornut/imgui) (via rlImgui) for developer tooling, [FastNoise2](https://github.com/Auburn/FastNoise2) for procedural noise, [nlohmann/json](https://github.com/nlohmann/json) for save data, and [FMOD](https://www.fmod.com/) for audio.

## Building

The project uses CMake (4.0+) and vendors its dependencies under `thirdparty/`.

```
cmake --preset x64-debug
cmake --build --preset x64-debug
```

See `CMakePresets.json` for other available presets (x86/x64, Debug/Release, Windows/Linux/macOS).
