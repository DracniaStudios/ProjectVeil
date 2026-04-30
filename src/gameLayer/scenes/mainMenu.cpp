#include "mainMenu.h"


bool lockOn = false;
bool isImGuiEnabled = false;

int cameraMode = CAMERA_THIRD_PERSON;

Player player;

void Scene_MainMenuUpdate(void* manager_ptr, void* object_ptr, float delta)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(manager->currentScene->object_ptr);

	// Camera Logic
	{
		auto& cam = manager->camera3D;
		cam.target = player.getPosition();
		//cam.target.x += 1; // Right
		//cam.target.z += 1; // Back
		cam.target.z -= 1; // Forward

		// Clamp Camera Close To Player
		int camMinDistance = 0;
		int camMaxDistance = 5;
		cam.position.x = Clamp(cam.position.x, player.getPosition().x + camMinDistance, player.getPosition().x + camMaxDistance);
		cam.position.y = Clamp(cam.position.y, player.getPosition().y + camMinDistance, player.getPosition().y + camMaxDistance);
		cam.position.z = Clamp(cam.position.z, player.getPosition().z + camMinDistance, player.getPosition().z + camMaxDistance);

		cameraMode = Clamp(cameraMode, 0, 4);
	}

	UpdateCamera(&manager->camera3D, cameraMode);
	player.update(delta);

#pragma region ImGui
	
	if (IsKeyPressed(KEY_F10)) { isImGuiEnabled = !isImGuiEnabled; }

	if (isImGuiEnabled) {
		ImGui::Begin("Game Data");
		
		ImGui::BeginChild("Player Data");
		ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", player.rigidBody3D.translation.x, player.rigidBody3D.translation.y, player.rigidBody3D.translation.z);
		ImGui::Text("Player Position 2D: (%.2f, %.2f)", player.position2D.x, player.position2D.y);
		ImGui::Text("Mouse Position: (%.2f, %.2f)", GetMousePosition().x, GetMousePosition().y);
		ImGui::EndChild();

		ImGui::BeginChild("Camera Data");
		ImGui::InputInt("Camera Mode", &cameraMode);
		ImGui::Checkbox("Lock On Camera", &lockOn);
		ImGui::EndChild();
		
		
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