#pragma once
#ifndef INVENTORY_H
#define INVENTORY_H

#include <Item.h>
#include <vector>
#include <PoolAllocator.h>

struct Inventory
{
private:
	int maxSize = 10;
	int currentSize = 0;

	std::vector<Item> items; // Use a vector to store items

public:
	Inventory() = default;
	~Inventory() = default;
	bool AddItem(Item item);
	bool RemoveItem(int itemId);
	bool ContainsItem(int itemId) const {
		for (int i = 0; i < currentSize; ++i) {
			if (items[i].id == itemId) {
				return true; // Item found
			}
		}
		return false; // Item not found
	}

	const std::vector<Item>& getItemsVector() const {
		return items;
	}
};

#endif // INVENTORY_H