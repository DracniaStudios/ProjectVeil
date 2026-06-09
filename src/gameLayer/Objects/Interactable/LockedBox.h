#ifndef LOCKED_BOX_H
#define LOCKED_BOX_H

#include <GameObject.h>
#include <randomStuff.h>


struct LockedBox : public GameObject, IInteractable
{

public:
	void Interact() override
	{
		std::cout << "Interacted With: " << name;
		OnInteract();
	}
	void OnInteract()
	{
		auto rng = std::ranlux24_base(std::random_device{}());
		defaultColor = Color(
			getRandomInt(rng, 0, 255),
			getRandomInt(rng, 0, 255),
			getRandomInt(rng, 0, 255),
			255
		);
	}
};

#endif
