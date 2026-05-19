#include "Inventory.h"

#include <AssetManager.h>
#include <iostream>
#include <ui.h>

Rectangle getInventoryRectangle()
{
	Rectangle inventoryRectangle = {};

	inventoryRectangle.height = GetScreenHeight() * 0.9f;
	
	inventoryRectangle.width = GetScreenWidth() * 0.9f;
	//inventoryRectangle.width = inventoryRectangle.height * 3;

	inventoryRectangle = placeRectangleTopLeftCorner(inventoryRectangle, inventoryRectangle.width * 0.1f);

	inventoryRectangle.x += GetScreenWidth() * 0.05f;
	inventoryRectangle.y += GetScreenHeight() * 0.05f;
	return inventoryRectangle;
}


void InventorySlot::render(AssetManager& assetManager, Inventory& inventory, Rectangle rect)
{

	rect = shrinkRectanglePercentage(rect, 0.1f, 0.1f);


	if (CheckCollisionPointRec(GetMousePosition(), rect)) {
		DrawTexturePro(
			assetManager.frame,
			{ 0, 0, static_cast<float>(assetManager.frame.width), static_cast<float>(assetManager.frame.height) },
			rect,
			{ 0, 0 },
			0.0f,
			{ 220, 255, 220, 255 }
		);
	}
	else {
		DrawTexturePro(
			assetManager.frame,
			{ 0, 0, static_cast<float>(assetManager.frame.width), static_cast<float>(assetManager.frame.height) },
			rect,
			{ 0, 0 },
			0.0f,
			{ 180, 180, 200, 240 }
		);
	}
}

void InventorySlot::update()
{

}

void InventorySlot::addItem()
{

}

void InventorySlot::removeItem()
{
	// Remove An Item From the slot
}

void Inventory::render(AssetManager& assetManager)
{
	rectangle = getInventoryRectangle();

	DrawRectangle(rectangle.x, rectangle.y, rectangle.width, rectangle.height,
		{ 100, 100, 100, 100 });

	rectangle = shrinkRectanglePercentage(rectangle, 0.01f, 0.01f);

	Rectangle oneCellRectangle;
	oneCellRectangle.height = rectangle.width / 9;
	oneCellRectangle.width = oneCellRectangle.height;
	oneCellRectangle.x = rectangle.x;
	oneCellRectangle.y = rectangle.y;

	for (int i = 0; i < 9; i++)
		for (int j = 0; j < 3; j++)
		{

			Rectangle r = oneCellRectangle;
			r.x += i * oneCellRectangle.width;
			r.y += j * oneCellRectangle.height;

			r = shrinkRectanglePercentage(r, 0.1f, 0.1f);

			// Debug
			DrawRectangle(r.x, r.y, r.width, r.height, BLUE);

			if (CheckCollisionPointRec(GetMousePosition(), r))
			{
				DrawTexturePro(
					assetManager.frame,
					{ 0,0,static_cast<float>(assetManager.frame.width), static_cast<float>(assetManager.frame.height) }, //source
					r, //dest
					{ 0, 0 },// origin (top-left corner)
					0.0f, // rotation
					{ 220,250,220,250 } // tint
				);
			}
			else
			{
				DrawTexturePro(
					assetManager.frame,
					{ 0,0,static_cast<float>(assetManager.frame.width), static_cast<float>(assetManager.frame.height) }, //source
					r, //dest
					{ 0, 0 },// origin (top-left corner)
					0.0f, // rotation
					{ 180,180,200,240 } // tint
				);
			}

		}


}

void Inventory::addItem()
{

}

void Inventory::removeItem()
{

}

