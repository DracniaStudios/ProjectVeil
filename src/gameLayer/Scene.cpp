#include "scene.h"

Scene* Scene_new() {
	Scene* scene = (Scene*)malloc(sizeof(Scene));
	return scene;
}

void Scene_updateScene(void* manager_ptr, Scene* scene, float delta) {
	scene->update(manager_ptr, scene->object_ptr, delta);
}

void Scene_drawScene(void* manager_ptr, Scene* scene) {
	scene->draw(manager_ptr, scene->object_ptr);
}