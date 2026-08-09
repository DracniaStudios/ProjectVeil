#pragma once
#ifndef ARTIFACT_H
#define ARTIFACT_H

#include <GameObject.h>

struct Artifact : public InteractableObject {

	void render3D() override;
	void update(Scene* scene, float deltaTime) override;
	void onEnable() override;
	void onDisable() override;
	void onInteract() override;

};

#endif
