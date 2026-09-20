#ifndef SCENE_TUTORIAL_H
#define SCENE_TUTORIAL_H

#include <SceneManager.h>

struct Tutorial : Scene
{

};

void Scene_TutorialUpdate(float delta);
void Scene_TutorialDraw2D();
void Scene_TutorialDraw3D();

#endif SCENE_TUTORIAL_H