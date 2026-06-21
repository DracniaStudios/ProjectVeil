#ifndef LOCKED_BOX_H
#define LOCKED_BOX_H

#include <GameObject.h>
#include <randomStuff.h>


struct LockedBox : InteractableObject
{
	void onInteract() override;
	void onEnable() override;
};

#endif
