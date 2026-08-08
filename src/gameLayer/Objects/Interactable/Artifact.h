#pragma once
#ifndef ARTIFACT_H
#define ARTIFACT_H

#include <GameObject.h>

struct Artifact : public Item {

	void render3D();
	void update(Scene* scene, float deltaTime);
	void onEnable();
	void onDisable();
	void onInteract();

};

#endif
