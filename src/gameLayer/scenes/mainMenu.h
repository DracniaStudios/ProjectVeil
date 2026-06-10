#ifndef SCENE_MAINMENU_H
#define SCENE_MAINMENU_H

#include <SceneManager.h>

Scene& loadScene(Scene& scene);

struct MainMenu : Scene
{
	const char* name = "Main Menu";
};

void Scene_MainMenuUpdate(float delta);
void Scene_MainMenuDraw2D();
void Scene_MainMenuDraw3D();

#endif
