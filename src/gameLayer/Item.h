#pragma once
#ifndef ITEM_H
#define ITEM_H

#include <GameObject.h>


enum ItemType
{
	ITEM_NONE = 0,
	ITEM_KEY = 1,
	ITEM_ARTIFACT = 2,
	ITEM_OBJECT = 3,
	ITEM_POTION = 4
};

struct Item : public InteractableObject
{
	ItemType type = ITEM_NONE;
	int id = 0;
	int quantity = 1;

	bool isStackable() const { return type == ITEM_KEY || type == ITEM_ARTIFACT || type == ITEM_OBJECT; }
	bool isConsumable() const { return type == ITEM_POTION; }
	bool isValid() const { return type != ITEM_NONE; }
	bool isEmpty() const { return quantity == 0; }
	bool isFull() const { return quantity >= 99; }

	bool AddQuantity(int amount) { return quantity = isFull() ? 99 : quantity + amount; }
	bool RemoveQuantity(int amount) { return quantity = isEmpty() ? 0 : quantity - amount; }
	bool canStackWith(const Item& other) const { return type == other.type && id == other.id; }
	bool canCombineWith(const Item& other) const { return isStackable() && other.isStackable() && canStackWith(other); }

	void OnInteract();

	Item();

};

inline const char* itemTypeToString(ItemType type) {
	switch (type) {
		case ITEM_NONE: return "None";
		case ITEM_KEY: return "Key";
		case ITEM_ARTIFACT: return "Artifact";
		case ITEM_OBJECT: return "Object";
		case ITEM_POTION: return "Potion";
		default: return "Unknown";
	}
};
#endif // ITEM_H