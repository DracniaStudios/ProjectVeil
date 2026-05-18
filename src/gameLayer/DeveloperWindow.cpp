#include "DeveloperWindow.h"

#include <SceneManager.h>
#include <Player.h>


void DeveloperWindow::render(SceneManager* manager)
{

	ImGui::Begin("Game Data");

	ImGui::Checkbox("Player Data Window", &isPlayerActive);
	ImGui::Checkbox("Camera Data Window", &isCameraActive);
	ImGui::Checkbox("Object Data Window", &isInspectorActive);
	ImGui::Checkbox("Mini Game Data Window", &isMiniGameActive);

	ImGui::Separator();
	
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Scene Data");
	auto scene = static_cast<Scene*>(manager->currentScene);
	
	
	ImGui::End();

}

void DeveloperWindow::update(SceneManager* manager, Player* player)
{
	if (isPlayerActive) showPlayerData(manager, player);
	if (isCameraActive) showCameraData(manager, player);
	if (isInspectorActive) showObjectInspector(manager);
	if (isMiniGameActive) showMiniGameData(manager, player);
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

GameObject* inspectObject = {};
GameObject* newObject = {};
bool isCreatingObject = false;

void DeveloperWindow::showObjectInspector(SceneManager* manager)
{
	auto scene = manager->currentScene;

	ImGui::Begin("Object Inspector");

	//ImGui::BeginChild("Game Object List");
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Game Objects");
	for (auto& object : scene->gameMap.gameObjects)
	{
		ImGui::PushID(object.id);
		if (ImGui::Button(object.name)){ inspectObject = inspectObject == &object ? nullptr : &object; }
		ImGui::PopID();
	}
	
	ImGui::Separator();

	if (inspectObject != nullptr) {
		ImGui::Text("ID: %d", inspectObject->id);
		ImGui::Text("Name: %s", &inspectObject->name);
		ImGui::InputFloat3("Position: ", &inspectObject->rigidBody3D.translation.x);
		ImGui::InputFloat3("Scale: ", &inspectObject->rigidBody3D.scale.x);
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", inspectObject->rigidBody3D.velocity.x, inspectObject->rigidBody3D.velocity.y, inspectObject->rigidBody3D.velocity.z);
		ImGui::Checkbox("isEnabled", &inspectObject->rigidBody3D.isEnabled);
		ImGui::Checkbox("isStatic", &inspectObject->rigidBody3D.isStatic);
		ImGui::Checkbox("isVisible", &inspectObject->display3DModel);
		ImGui::Checkbox("Show Collider", &inspectObject->displayCollider);
		ImGui::Checkbox("Up Touch", &inspectObject->rigidBody3D.upTouch);
		ImGui::Checkbox("Down Touch", &inspectObject->rigidBody3D.downTouch);
		ImGui::Checkbox("Left Touch", &inspectObject->rigidBody3D.leftTouch);
		ImGui::Checkbox("Right Touch", &inspectObject->rigidBody3D.rightTouch);
		ImGui::Checkbox("Front Touch", &inspectObject->rigidBody3D.frontTouch);
		ImGui::Checkbox("Back Touch", &inspectObject->rigidBody3D.backTouch);
	}

	ImGui::Separator();

	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Create Object");
	if (ImGui::Button("Create New Object")) { isCreatingObject = true; }
	if (isCreatingObject)
	{
		if (newObject == nullptr){ newObject = new GameObject(); }
		
		ImGui::Text("Object Data:");
		ImGui::InputFloat3("Position: ", &newObject->rigidBody3D.translation.x);
		ImGui::InputFloat3("Scale: ", &newObject->rigidBody3D.scale.x);
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Rigidbody");
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", newObject->rigidBody3D.velocity.x, newObject->rigidBody3D.velocity.y, newObject->rigidBody3D.velocity.z);
		ImGui::Spacing();
		
		ImGui::TextColored(ImVec4(100, 0, 0, 255), "Flags");
		ImGui::Checkbox("isEnabled", &newObject->rigidBody3D.isEnabled);
		ImGui::Checkbox("isStatic", &newObject->rigidBody3D.isStatic);
		ImGui::Checkbox("isVisible", &newObject->display3DModel);
		ImGui::Checkbox("Show Collider", &newObject->displayCollider);
		ImGui::Checkbox("Up Touch", &newObject->rigidBody3D.upTouch);
		ImGui::Checkbox("Down Touch", &newObject->rigidBody3D.downTouch);
		ImGui::Checkbox("Left Touch", &newObject->rigidBody3D.leftTouch);
		ImGui::Checkbox("Right Touch", &newObject->rigidBody3D.rightTouch);
		ImGui::Checkbox("Front Touch", &newObject->rigidBody3D.frontTouch);
		ImGui::Checkbox("Back Touch", &newObject->rigidBody3D.backTouch);
		ImGui::Spacing();

		if (ImGui::Button("Spawn Game Object"))
		{
			GameObject clone = *newObject;
			//clone.mesh = getMeshVariant(clone.meshVariant, meshVector);
			scene->gameMap.saveObjectAt(newObject->getPosition(), clone);
			newObject = {};
			isCreatingObject = false;
		}
	}

	ImGui::End();
}

int currentGameID = 0;

void DeveloperWindow::showMiniGameData(SceneManager* manager, Player* player)
{
	auto scene = manager->currentScene;
	auto miniGame = manager->currentMiniGame;

	ImGui::Begin("Mini Game Data");

	ImGui::Text("Display Mini Game Data");
	ImGui::InputInt("Mini Game ID", &currentGameID, 1, 1);
	currentGameID = Clamp(currentGameID, 0, 2);

	if (ImGui::Button("Launch Mini Game"))
	{
		switch (MINI_GAME_ID)
		{
		case 0:
			scene->miniGame = MiniGame_flappyBird();
			break;
		case 1:
			scene->miniGame = MiniGame_crane();
			break;
		default:
			scene->miniGame = nullptr;
			scene->isMiniActive = false;
			return;
		}

		scene->isMiniActive = true;
	}

	ImGui::End();
}