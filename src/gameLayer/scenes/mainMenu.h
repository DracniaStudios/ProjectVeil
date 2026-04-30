#ifndef SCENE_MAINMENU_H
#define SCENE_MAINMENU_H

#include <SceneManager.h>
#include <Scene.h>
#include <string>

Scene& loadScene(Scene& scene);

struct MainMenu : Scene
{
	const char* name;
};

//MainMenu* new_MainMenu();

void Scene_MainMenuUpdate(void* manager_ptr, void* object_ptr, float delta);
void Scene_MainMenuDraw2D(void* manager_ptr, void* object_ptr);
void Scene_MainMenuDraw3D(void* manager_ptr, void* object_ptr);

#endif
