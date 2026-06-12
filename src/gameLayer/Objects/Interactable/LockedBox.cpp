#include "LockedBox.h"

void LockedBox::onEnable()
{
	isInteractable = true;
	GameObject::onEnable();
}

void LockedBox::onInteract()
{
	if (!isInteractable) { return; }
	auto rng = std::ranlux24_base(std::random_device{}());

	defaultColor = Color{
		static_cast<unsigned char>(getRandomInt(rng, 0, 255)),
		static_cast<unsigned char>(getRandomInt(rng, 0, 255)),
		static_cast<unsigned char>(getRandomInt(rng, 0, 255)),
		255
	};
	std::cout << name << " was Interacted with.\n";
}
