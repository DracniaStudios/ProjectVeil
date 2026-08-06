#pragma once
#ifndef ARTIFACT_H
#define ARTIFACT_H

#include <GameObject.h>

struct Artifact : public InteractableObject {

	override void render3D() {
		if (!this->isEnabled) { return; }
		GameObject::render3D();
	}
	override void update3D(float deltaTime) {
		if (!this->isEnabled) { return; }
		rigidBody3D.angularVelocity = Vector3(0.1f, 0.0f, 0.1f);

		// Make Selected Mode based on looking at defualt left/right side of artifact

		GameObject::update3D(deltaTime);
	}

	override void onEnable() {
		GameObject::onEnable();
		name = "Artifact";

		/// Flags
		isDestructible = false;
		// Shadow is disabled because the artifact would make a shadow over everything.
		castsShadow = false;
		
		/// Data
		rigidBody3D.scale = Vector3(0.1f, 0.1f, 0.1f);
		rigidBody3D.canCollide = false;
		rigidBody3D.SetGravity(0.0f, 0.0f, 0.0f);
		defaultSound = "Artifact_Load_Up";

		interactType = INTERACT_ITEM;
		interactValue = 2;
	}

	override void onDisable() {
		GameObject::onDisable();
	}
	
	override void onInteract() {

	}

};

#endif
