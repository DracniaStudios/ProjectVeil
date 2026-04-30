#include "GameObject.h"

void GameObject::render()
{
	DrawSphere(transform.translation, transform.scale.x, GREEN);
}


void GameObject::update(float deltaTime)
{
	// Update Data
}

