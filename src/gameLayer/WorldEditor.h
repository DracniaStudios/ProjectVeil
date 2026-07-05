#pragma once
#ifndef WORLD_EDITOR_H
#define WORLD_EDITOR_H

#include <Scene.h>

class WorldEditor
{
private:
	std::string name = "WorldEditor";
	std::string path;

	bool isEnabled = false;

	WorldEditor() = default; // Private constructor to prevent instantiation
	~WorldEditor() = default; // Private destructor to prevent deletion
public:

	// Delete, copy, and move functions to prevent duplication
	WorldEditor(const WorldEditor&) = delete;
	WorldEditor& operator=(const WorldEditor&) = delete;
	WorldEditor(WorldEditor&&) = delete;
	WorldEditor& operator=(WorldEditor&&) = delete;

	static WorldEditor& getInstance()
	{
		static WorldEditor instance; // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}

	Camera3D editorCamera = {};
	Scene activeScene = {};

	bool isActive = false;
	bool isGridActive = true;
	bool isSnapActive = true;
	float gridSize = 1.0f;

	void Enable();
	void Disable();
	void Update(float deltaTime);
	void Draw2D();
	void Draw3D();
	void DrawGrid(float size, float spacing);
	void DrawGrid3D(float size, float spacing);

};

#endif	// WORLD_EDITOR_H