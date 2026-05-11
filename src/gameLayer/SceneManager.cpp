#include "SceneManager.h"

SceneManager* SceneManager_new() {
	SceneManager* manager = (SceneManager*)malloc(sizeof(SceneManager));
	return manager;
}


void SceneManager_init(SceneManager* manager) {
	manager->currentScene = nullptr;
	manager->nextScene = nullptr;

	// ... Initialize other scenes as needed
	manager->scenes[0] = Scene_MainMenuConstruct();

	// ... Initialize mini games
	manager->miniGames[0] = MiniGame_flappyBird();
	/*
	manager->miniGames[1] = MiniGame_crane();
	manager->miniGames[2] = MiniGame_doctor();
	manager->miniGames[3] = MiniGame_saysMe();
	manager->miniGames[4] = MiniGame_timedSaysMe();
	manager->miniGames[5] = MiniGame_maze();
	manager->miniGames[6] = MiniGame_roShamBoo();
	*/
	manager->transition = Transition_new();

	SceneManager_push(manager, SCENE_MAIN_MENU);
}

void SceneManager_update(SceneManager* manager, float delta) {
	// Update Transition
	if (manager->transition->direction != NONE) {
		if (manager->transition->direction == OUT) {
			manager->transition->opacity += 5;
			if (manager->transition->opacity >= 255) SceneManager_transition(manager, IN);

		}
		else {
			manager->transition->opacity -= 5;
			if (manager->transition->opacity <= 0) SceneManager_transition(manager, NONE);
		}
	}

	// Update Scene
	if (manager->currentScene) Scene_updateScene(manager, manager->currentScene, delta);
}

void SceneManager_draw(SceneManager* manager) {

	// Draw Scene 3D
	BeginMode3D(manager->camera3D);
	if (manager->currentScene) Scene_drawScene3D(manager, manager->currentScene);
	EndMode3D();
	
	// Draw Scene 2D
	BeginMode2D(manager->camera2D);
	if (manager->currentScene) Scene_drawScene2D(manager, manager->currentScene);
	EndMode2D();

	// Draw Transition
	if (manager->transition->direction != NONE)
		DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, (unsigned char)manager->transition->opacity });
}

void SceneManager_push(SceneManager* manager, int sceneID) {
	if (sceneID >= 0 && sceneID < SCENE_COUNT) {
		manager->nextScene = manager->scenes[sceneID];
		SceneManager_transition(manager, manager->currentScene ? OUT : IN);
	}
}

void SceneManager_transition(SceneManager* manager, TransitionDirection direction) {
	if (direction == IN) {
		manager->currentScene = manager->nextScene;
		manager->nextScene = nullptr;
	}

	manager->transition->direction = direction;

	if (direction == OUT) manager->transition->opacity = 0;
	else if (direction == IN) manager->transition->opacity = 255;
	else manager->transition->opacity = -1;
}