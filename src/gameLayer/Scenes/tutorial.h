#ifndef SCENE_TUTORIAL_H
#define SCENE_TUTORIAL_H

#include <SceneManager.h>

struct Tutorial : SceneManagement::Scene
{

};

void Scene_TutorialUpdate(SceneManagement::Scene& scene, float delta);
void Scene_TutorialDraw2D(SceneManagement::Scene& scene);
void Scene_TutorialDraw3D(SceneManagement::Scene& scene);

#endif // SCENE_TUTORIAL_H