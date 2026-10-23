#ifndef SCENE_MAINMENU_H
#define SCENE_MAINMENU_H

#include <SceneManager.h>

struct MainMenu : SceneManagement::Scene
{
	
};

void Scene_MainMenuUpdate(SceneManagement::Scene& scene, float delta);
void Scene_MainMenuDraw2D(SceneManagement::Scene& scene);
void Scene_MainMenuDraw3D(SceneManagement::Scene& scene);

#endif
