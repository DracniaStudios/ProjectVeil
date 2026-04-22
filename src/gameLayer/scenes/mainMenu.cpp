#include "mainMenu.h"

MainMenu* new_MainMenu()
{
	MainMenu* menu = (MainMenu*)malloc(sizeof(MainMenu));
	menu->name = "Main Menu";

	return menu;
}

void Scene_MainMenuUpdate(void* manager_ptr, void* object_ptr, float delta)
{
	
}

void Scene_MainMenuDraw(void* manager_ptr, void* object_ptr)
{
	
}

Scene* Scene_MainMenuConstruct()
{
	Scene* scene = Scene_new();
	scene->update = Scene_MainMenuUpdate;
	scene->draw = Scene_MainMenuDraw;

	MainMenu* menu = new_MainMenu();

	scene->object_ptr = menu;

	return scene;
}
