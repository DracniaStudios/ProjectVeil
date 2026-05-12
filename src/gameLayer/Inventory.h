#pragma once
#ifndef INVENTORY_H
#define INVENTORY_H

#include <raylib.h>
#include <GameObject.h>
#include <vector>

struct Slot
{
	int id = -1;

	GameObject* item = {};
	int itemCount = 0;

	bool addItemToSlot(GameObject* item);
	bool removeItemFromSlot(GameObject* item);
	[[nodiscard]] auto* getItemFromSlot() const { return item; }
};

struct Inventory
{
	
	const int maxSlots = 5;
	
	std::vector<Slot*> slots;

	// Scans through all items
	bool addItem(GameObject* item);
	bool removeItem(GameObject* item);
	GameObject* getItem(const GameObject* item);// Item Type

};

#endif
