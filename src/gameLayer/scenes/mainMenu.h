#ifndef SCENE_MAINMENU_H
#define SCENE_MAINMENU_H

#include <Scene.h>
#include <string>

struct MainMenu
{
	std::string name;
};

MainMenu* new_MainMenu();

void Scene_MainMenuUpdate(void* manager_ptr, void* object_ptr, float delta);
void Scene_MainMenuDraw(void* manager_ptr, void* object_ptr);

#endif
