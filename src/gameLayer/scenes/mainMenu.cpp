#include "mainMenu.h"


bool lockOn = false;
bool isImGuiEnabled = false;
int cameraMode = CAMERA_FIRST_PERSON;

int sensitivityX = 0.5f;
int sensitivityY = 0.5f;

Player player;
struct CameraData
{
	Vector3 position = {};
	Vector3 rotation = {};
	float zoom = 0.0f;
}cameraData;

void Scene_MainMenuUpdate(void* manager_ptr, void* object_ptr, float delta)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(manager->currentScene->object_ptr);

	// Camera Logic
	{
		auto& cam = manager->camera3D;
		cam.position = player.rigidBody3D.translation + Vector3(0, 2, -5);
		cam.target = player.rigidBody3D.front + player.rigidBody3D.translation;
		UpdateCamera(&manager->camera3D, cameraMode);
		//UpdateCameraPro(&manager->camera3D, cameraData.position, cameraData.rotation, cameraData.zoom);
	}

	player.update(delta);

#pragma region ImGui
	
	if (IsKeyPressed(KEY_F10)) { isImGuiEnabled = !isImGuiEnabled; }

	if (isImGuiEnabled) {
		ImGui::Begin("Game Data");
		
		ImGui::BeginChild("Player Data");
		//ImGui::Text("Mouse Position: (%.2f, %.2f)", GetMousePosition().x, GetMousePosition().y);
		
		ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", player.rigidBody3D.translation.x, player.rigidBody3D.translation.y, player.rigidBody3D.translation.z);
		ImGui::Text("Player Scale: (%.2f, %.2f, %.2f)", player.rigidBody3D.scale.x, player.rigidBody3D.scale.y, player.rigidBody3D.scale.z);
		ImGui::Text("Player Position 2D: (%.2f, %.2f)", player.position2D.x, player.position2D.y);

		ImGui::Text("Player Forward: (%.2f, %.2f, %.2f)", player.rigidBody3D.front.x, player.rigidBody3D.front.y, player.rigidBody3D.front.z);
		ImGui::Text("Player Backward: (%.2f, %.2f, %.2f)", player.rigidBody3D.back.x, player.rigidBody3D.back.y, player.rigidBody3D.back.z);
		
		ImGui::EndChild();
		/*
		ImGui::BeginChild("Camera Data");
		ImGui::InputInt("Camera Mode", &cameraMode);
		ImGui::Checkbox("Lock On Camera", &lockOn);
		ImGui::EndChild();
		*/
		
		
		ImGui::End();
	}
#pragma endregion
}

void Scene_MainMenuDraw2D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);

	//player.render2D();
	
}
void Scene_MainMenuDraw3D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);

	DrawGrid(10.0f, 1.0f);

	player.render3D();

}

Scene* Scene_MainMenuConstruct()
{
	Scene* scene = Scene_new();
	scene->update = Scene_MainMenuUpdate;
	scene->draw2D = Scene_MainMenuDraw2D;
	scene->draw3D = Scene_MainMenuDraw3D;
	scene->object_ptr = scene;

	scene->gameMap.create(1920, 5, 1080);

	return scene;
}