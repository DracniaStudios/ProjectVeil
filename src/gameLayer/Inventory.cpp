#include "Inventory.h"

#include <Player.h>

bool Inventory::AddItem(Item item) {
	if (currentSize >= maxSize) {
		std::cout << "[Inventory] Cannot add item: " << item.name << ". Inventory is full.\n";
		return false; // Inventory is full
	}

	if (ContainsItem(item.id)) {
		for (int i = 0; i < currentSize; ++i) {
			if (items[i].id == item.id) {
				if (items[i].isFull()) {
					std::cout << "[Inventory] Cannot add item: " << item.name << ". Item is full.\n";
					return false; // Item is full
				}
				items[i].AddQuantity(item.quantity);
				return true; // Item quantity increased
			}
		}
	}

	items[currentSize] = item;
	currentSize++;
	return true;
}

bool Inventory::RemoveItem(int itemId) {
	
	// if inventory contains item, remove it and shift items down
	if (ContainsItem(itemId)) {
		for (int i = 0; i < currentSize; ++i) {
			if (items[i].id == itemId) {

				// Check Quanity
				if (items[i].RemoveQuantity(1)) {

					// Check Empty
					if (items[i].isEmpty()) {
					
						// Shift items down to fill the gap
						for (int j = i; j < currentSize - 1; ++j) {
							items[j] = items[j + 1];
						}
						currentSize--;
					}
					return true; // Item removed successfully
				}
			}
		}
	}
	
	std::cout << "[Inventory] Cannot remove item with ID: " << itemId << ". Item not found.\n";
	return false; // Item not found
}