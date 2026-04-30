#include "mainMenu.h"


struct Player
{
	Vector3 position3D;
	Vector2 position2D;
} player;

// Clamp 2D position to the game map boundaries (pos.x / 1920) ( pos.y / 1080) to get percentage of screen, then multiply by map size to get world position 

/*
MainMenu* new_MainMenu()
{
	MainMenu* menu = new MainMenu();
	menu->name = "Main Menu";
	menu->gameMap.create(1920, 5, 1080);

	return menu;
}
*/
bool lockOn = false;
bool isImGuiEnabled = false;
Vector2 cubePosition = Vector2{0, 0};

void Scene_MainMenuUpdate(void* manager_ptr, void* object_ptr, float delta)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(manager->currentScene->object_ptr);
	UpdateCamera(&manager->camera3D, CAMERA_FREE);

	if (IsKeyDown(KEY_W)) player.position3D.z -= 5.0f * delta;
	if (IsKeyDown(KEY_S)) player.position3D.z += 5.0f * delta;
	if (IsKeyDown(KEY_A)) player.position3D.x -= 5.0f * delta;
	if (IsKeyDown(KEY_D)) player.position3D.x += 5.0f * delta;

	// Clamp 2D to position from game map size to screen size
	{
		int screenX = GetScreenWidth();
		int screenY = GetScreenHeight();

		player.position2D.x = screenX / 2 + player.position3D.x;
		player.position2D.y = screenY / 2 + player.position3D.z;

		player.position3D.x = Clamp(player.position3D.x, -(screenX / 2), screenX / 2);
		player.position3D.z = Clamp(player.position3D.z, -(screenY / 2), screenY / 2);

		player.position2D.x = Clamp(player.position2D.x, 0, screenX);
		player.position2D.y = Clamp(player.position2D.y, 0, screenY);

	}


#pragma region ImGui
	
	if (IsKeyPressed(KEY_F10)) { isImGuiEnabled = !isImGuiEnabled; }
	
	if (isImGuiEnabled) {
		ImGui::Begin("Game Data");
		ImGui::Checkbox("Lock On Camera", &lockOn);
		ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", player.position3D.x, player.position3D.y, player.position3D.z);
		ImGui::Text("Player Position 2D: (%.2f, %.2f)", player.position2D.x, player.position2D.y);
		ImGui::SliderFloat2("Cube Position", &cubePosition.x, -1920, 1920);
		ImGui::Text("Mouse Position: (%.2f, %.2f)", GetMousePosition().x, GetMousePosition().y);

		ImGui::End();
	}
#pragma endregion
}

void Scene_MainMenuDraw2D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	DrawRectangle(player.position2D.x, player.position2D.y, 32, 32, BLUE);
}
void Scene_MainMenuDraw3D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);

	DrawGrid(10.0f, 1.0f);

	DrawSphere(player.position3D, 0.5f, BLUE);

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