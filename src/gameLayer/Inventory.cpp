#include "Inventory.h"

bool Slot::addItemToSlot(GameObject* item)
{ 
	if (this->item == item)
	{
		this->itemCount++;
	}
	else
	{
		this->item = item;
		
	}
	return true;
}

bool Slot::removeItemFromSlot(GameObject* item)
{
	if (this->item == item)
	{
		if (itemCount > 1)
		{
			itemCount--;
			return true;
		}

		this->item = nullptr;
	}

	return false;

}

bool Inventory::addItem(GameObject* item)
{
	for (auto& slot : slots)
	{
		if (slot->item == item) {
			return slot->addItemToSlot(item);
		}
	}
	return false;
}

bool Inventory::removeItem(GameObject* item)
{
	for (auto& slot : slots)
	{
		if (slot->item == item)
		{
			return slot->removeItemFromSlot(item);
		}
	}
	return false;
}

GameObject* Inventory::getItem(const GameObject* item)
{
	for (const auto& slot : slots)
	{
		if (slot->item == item)
		{
			return slot->getItemFromSlot();
		}
	}
	return nullptr;
}
