#include "DeveloperWindow.h"

#include <SceneManager.h>
#include <Player.h>


void DeveloperWindow::render(SceneManager* manager, Player* player)
{
	if (isPlayerActive) showPlayerData(manager, player);
	if (isCameraActive) showCameraData(manager, player);
	if (isInspectorActive) showObjectInspector(manager);
	if (isMiniGameActive) showMiniGameData(manager, player);
}

void DeveloperWindow::update()
{
	ImGui::Begin("Game Data");

	ImGui::Checkbox("Player Data Window", &isPlayerActive);
	ImGui::Checkbox("Camera Data Window", &isCameraActive);
	ImGui::Checkbox("Object Data Window", &isInspectorActive);
	ImGui::Checkbox("Mini Game Data Window", &isMiniGameActive);

	ImGui::End();
}

void DeveloperWindow::showPlayerData(SceneManager* manager, Player* player)
{
	auto scene = manager->currentScene;
	ImGui::Begin("Player Data");

	if (ImGui::Button("Hurt Player")) { player->health -= 1; }
	ImGui::InputInt("Player Health: ", &player->health, 1, 1);
	if (scene->is2DActive)
	{
		ImGui::Checkbox("RigidBody is Enabled: ", &player->rigidBody2D.isEnabled);
		ImGui::Text("Player Position 2D: (%.2f, %.2f)", player->rigidBody2D.translation.x, player->rigidBody2D.translation.y);
		ImGui::Text("Player Velocity 2D: (%.2f, %.2f)", player->rigidBody2D.velocity.x, player->rigidBody2D.velocity.y);
		ImGui::Text("Player Scale 2D: (%.2f, %.2f)", player->rigidBody2D.scale.x, player->rigidBody2D.scale.y);

	}
	else
	{
		ImGui::Checkbox("RigidBody is Enabled: ", &player->rigidBody3D.isEnabled);
		ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", player->rigidBody3D.translation.x, player->rigidBody3D.translation.y, player->rigidBody3D.translation.z);
		ImGui::Text("Player Velocity 3D: (%.2f, %.2f, %.2f)", player->rigidBody3D.velocity.x, player->rigidBody3D.velocity.y, player->rigidBody3D.velocity.z);
		ImGui::Text("Player Scale 3D: (%.2f, %.2f, %.2f)", player->rigidBody3D.scale.x, player->rigidBody3D.scale.y, player->rigidBody3D.scale.z);

	}
	ImGui::Text("Player Forward: (%.2f, %.2f, %.2f)", player->rigidBody3D.front.x, player->rigidBody3D.front.y, player->rigidBody3D.front.z);

	ImGui::Separator();
	ImGui::End();

}

void DeveloperWindow::showCameraData(SceneManager* manager, Player* player)
{
	ImGui::Begin("Camera Data");

	ImGui::Text("Camera Forward: (%.2f, %.2f, %.2f)", player->camera.forward.x, player->camera.forward.y, player->camera.forward.z);
	
	ImGui::Separator();

	ImGui::End();
}

void DeveloperWindow::showObjectInspector(SceneManager* manager)
{
	auto scene = manager->currentScene;

	ImGui::Begin("Object Inspector");

	for (auto& object : scene->gameMap.gameObjects)
	{
		ImGui::PushID(object.id);
		ImGui::LabelText("GameObject: ", object.name);
		ImGui::Checkbox("GameObject Is Enabled", &object.isEnabled);
		ImGui::Separator();
		ImGui::PopID();
	}
	ImGui::End();
}

void DeveloperWindow::showMiniGameData(SceneManager* manager, Player* player)
{
	auto scene = manager->currentScene;
	auto miniGame = manager->currentMiniGame;

	ImGui::Begin("Mini Game Data");

	ImGui::Text("Display Mini Game Data");

	ImGui::End();
}