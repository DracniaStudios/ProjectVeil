#include "scene.h"

Scene* Scene_new() {
	Scene* scene = new Scene;//(Scene*)malloc(sizeof(Scene));
	return scene;
}

void Scene_updateScene(void* manager_ptr, Scene* scene, float delta) {
	scene->update(manager_ptr, scene->object_ptr, delta);
}

void Scene_drawScene2D(void* manager_ptr, Scene* scene) {
	scene->draw2D(manager_ptr, scene->object_ptr);
}

void Scene_drawScene3D(void* manager_ptr, Scene* scene) {
	scene->draw3D(manager_ptr, scene->object_ptr);
}