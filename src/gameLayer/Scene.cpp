#include "scene.h"

Scene* Scene_new() {
	Scene* scene = new Scene;//(Scene*)malloc(sizeof(Scene));
	return scene;
}

void Scene_updateScene(void* manager_ptr, Scene* scene, float delta) {
	scene->update(manager_ptr, scene->object_ptr, delta);

	for (auto& object : scene->gameMap.gameObjects) {
		object.update(delta);
	}

	for (auto& body : scene->gameMap.rigidBodies3D) {
		body.update(delta);
		body.resolveConstrains(scene->gameMap.rigidBodies3D.data(), static_cast<int>(scene->gameMap.rigidBodies3D.size()));
	}


}

void Scene_drawScene2D(void* manager_ptr, Scene* scene) {
	scene->draw2D(manager_ptr, scene->object_ptr);
	for (auto& object : scene->gameMap.gameObjects) {
		object.render2D();
	}
}

void Scene_drawScene3D(void* manager_ptr, Scene* scene) {
	scene->draw3D(manager_ptr, scene->object_ptr);
	for (auto& object : scene->gameMap.gameObjects) {
		object.render3D();
	}
}