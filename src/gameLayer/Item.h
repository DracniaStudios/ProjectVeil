#pragma once
#ifndef ITEM_H
#define ITEM_H

#include <AssetManager.h>

bool isItem(int type);

struct Item
{
	/// Item List
	enum
	{
		low_health_potion,
		medium_health_potion,
		high_health_potion,

		LAST_ITEM
	};

	int type = 0;
	int count = 1;
	
	bool isItem() { return::isItem(type); }
	// check if is block type

	virtual int getMaxStack() { return 9999; }
};

Texture getTextureForItemType(int itemType, AssetManager& assetManager);

Rectangle getTextureCoordinatesForItemType(int itemType);


#endif