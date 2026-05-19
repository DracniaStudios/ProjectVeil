#pragma once
#ifndef INVENTORY_H
#define INVENTORY_H

#include <vector>
#include <Item.h>
#include <raylib.h>

struct AssetManager;


struct Inventory
{
	
	const int maxSlots = 5;
	
	Item currentItemHeld;
	Rectangle rectangle;

	std::vector<Item> inventoryList;

	void render(AssetManager& assetManager);
	void addItem();
	void removeItem();
};
struct InventorySlot
{
	Item item;
	
	void render(AssetManager& assetManager, Inventory& inventory, Rectangle rect);
	void update();
	void addItem();
	void removeItem();

};

#endif
