#include "mainMenu.h"

bool lockOn = false;
bool isImGuiEnabled = false;
int cameraMode = CAMERA_FIRST_PERSON;

Player player;
PlayerCamera playerCamera;

Vector3 cubePosition = { 0, 0, 0 };

void Scene_MainMenuUpdate(void* manager_ptr, void* object_ptr, float deltaTime)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(object_ptr);

	auto& cam = manager->camera3D;
	playerCamera.UpdateCameraFPS(&cam, &player);
	player.update(&cam, deltaTime);

#pragma region ImGui

	if (IsKeyPressed(KEY_F10)) { isImGuiEnabled = !isImGuiEnabled; }

	if (isImGuiEnabled) {
		ImGui::Begin("Game Data");

		ImGui::BeginChild("Player Data");

		ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", player.rigidBody3D.translation.x, player.rigidBody3D.translation.y, player.rigidBody3D.translation.z);
		ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", cam.target.x, cam.target.y, cam.target.z);

		ImGui::Separator();

		ImGui::Text("Player Forward: (%.2f, %.2f, %.2f)", player.rigidBody3D.front.x, player.rigidBody3D.front.y, player.rigidBody3D.front.z);
		ImGui::Text("Player Backward: (%.2f, %.2f, %.2f)", player.rigidBody3D.back.x, player.rigidBody3D.back.y, player.rigidBody3D.back.z);

		ImGui::InputFloat3("Cube Position", &cubePosition.x);

		if (ImGui::Button("Add Game Object"))
		{
			GameObject newObject;
			newObject.rigidBody3D.translation = cubePosition;
			scene->gameMap.saveObjectAt(cubePosition, newObject);
		}

		ImGui::EndChild();

		ImGui::End();
	}
#pragma endregion
}

void Scene_MainMenuDraw2D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);

	player.render2D();
	
};

void Scene_MainMenuDraw3D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(object_ptr);
	DrawGrid(100.0f, 1.0f);
	player.render3D();

}

Scene* Scene_MainMenuConstruct()
{
	Scene* scene = Scene_new();
	scene->update = Scene_MainMenuUpdate;
	scene->draw2D = Scene_MainMenuDraw2D;
	scene->draw3D = Scene_MainMenuDraw3D;
	scene->object_ptr = scene;

	//scene->gameMap.create(1920, 5, 1080);
	scene->gameMap.create(10, 1, 5);
	return scene;
}