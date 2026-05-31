#include "DeveloperWindow.h"

#include <SceneManager.h>
#include <Player.h>
#include <AssetManager.h>

void DeveloperWindow::render(SceneManager* manager)
{
	/// Render Main developer Window

	// Get Scene From Manager
	auto scene = static_cast<Scene*>(manager->currentScene);

	// Begin Game Data Window
	ImGui::Begin("Game Data");

	// Show Enable Window Flags
	ImGui::Checkbox("Player Data Window", &isPlayerActive);
	ImGui::Checkbox("Camera Data Window", &isCameraActive);
	ImGui::Checkbox("Object Data Window", &isInspectorActive);
	ImGui::Checkbox("Mini Game Data Window", &isMiniGameActive);

	ImGui::Separator();
	
	/// Show Scene Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Scene Data");
	
	ImGui::Checkbox("Is 2D Active", &scene->is2DActive);
	ImGui::Checkbox("Is Mini Game Active", &scene->isMiniActive);
	
	ImGui::Separator();
	
	/// Show Game Map Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Game Map Data");
	
	ImGui::Text("Game Object ID: %d", static_cast<int>(scene->gameMap.objectID));
	ImGui::Text("Game Objects: %d", static_cast<int>(scene->gameMap.gameObjects.size()));
	ImGui::InputFloat3("Game Map Size", &scene->gameMap.size.x);
	
	ImGui::End();

}

void DeveloperWindow::update(SceneManager* manager, AssetManager* assetManager, Player* player)
{
	/// Update Game Data Windows
	if (isPlayerActive) showPlayerData(manager, player);
	if (isCameraActive) showCameraData(manager, player);
	if (isInspectorActive) showObjectInspector(manager, assetManager);
	if (isMiniGameActive) showMiniGameData(manager, player);

}

void DeveloperWindow::showPlayerData(SceneManager* manager, Player* player)
{
	auto scene = manager->currentScene;
	
	/// Begin Player Window
	ImGui::Begin("Player Data");

	if (ImGui::Button("Hurt Player")) { player->health -= 1; }
	ImGui::InputFloat("Player Health: ", &player->health, 1, 1);
	ImGui::Checkbox("Is Firing", player->IsFiring());
	
	/// Show Rigidbody Data based on space
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

	/// Show Player Directional Data and Flags
	ImGui::Text("Player Forward: (%.2f, %.2f, %.2f)", player->rigidBody3D.forward.x, player->rigidBody3D.forward.y, player->rigidBody3D.forward.z);
	ImGui::Checkbox("Up Touch", &player->rigidBody3D.upTouch);
	ImGui::Checkbox("Down Touch", &player->rigidBody3D.downTouch);
	ImGui::Checkbox("Left Touch", &player->rigidBody3D.leftTouch);
	ImGui::Checkbox("Right Touch", &player->rigidBody3D.rightTouch);
	ImGui::Checkbox("Front Touch", &player->rigidBody3D.frontTouch);
	ImGui::Checkbox("Back Touch", &player->rigidBody3D.backTouch);

	ImGui::Separator();
	ImGui::End();

}

void DeveloperWindow::showCameraData(SceneManager* manager, Player* player)
{
	/// Show Camera Data Window
	ImGui::Begin("Camera Data");

	ImGui::Text("Camera Forward: (%.2f, %.2f, %.2f)", player->camera.forward.x, player->camera.forward.y, player->camera.forward.z);
	ImGui::Text("Camera Offset: (%.2f, %.2f, %.2f)", player->camera.offset.x, player->camera.offset.y, player->camera.offset.z);
	ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", player->camera.position.x, player->camera.position.y, player->camera.position.z);
	ImGui::Text("Camera Sensitivity: (%.2f, %.2f)", player->camera.sensitivity.x, player->camera.sensitivity.y);
	ImGui::Text("Camera Look Rotation: (%.2f, %.2f)", player->camera.lookRotation.x, player->camera.lookRotation	.y);
	ImGui::Text("Camera Lean: (%.2f, %.2f)", player->camera.lean.x, player->camera.lean	.y);

	ImGui::Separator();

	ImGui::End();
}

GameObject* inspectObject;
GameObject* newObject = {};
bool isCreatingObject = false;

void DeveloperWindow::showObjectInspector(SceneManager* manager, AssetManager* assetManager)
{
	auto scene = manager->currentScene;
	auto rng = std::ranlux24_base(std::random_device{}());


	/// Show Object Inspector Window
	ImGui::Begin("Object Inspector");

	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Game Objects");
	
	/// List Game Objects
	for (auto& object : scene->gameMap.gameObjects)
	{
		ImGui::PushID(object.id);

		std::string displayName =  object.name;
		displayName += " (ID: " + std::to_string(object.id) + ")";
		if (ImGui::Button(displayName.c_str()))
		{
			inspectObject = inspectObject == &object ? nullptr : &object;
		}
		ImGui::PopID();
	}
	
	ImGui::Separator();

	/// Show Selected Object Data
	if (inspectObject != nullptr) {
		ImGui::Text("ID: %d", inspectObject->id);
		ImGui::Text("Name: %s", &inspectObject->name);
		ImGui::Spacing();
		
		// RigidBody Data
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Rigidbody");
		ImGui::InputFloat3("Position: ", &inspectObject->rigidBody3D.translation.x);
		ImGui::InputFloat3("Scale: ", &inspectObject->rigidBody3D.scale.x);
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", inspectObject->rigidBody3D.velocity.x, inspectObject->rigidBody3D.velocity.y, inspectObject->rigidBody3D.velocity.z);
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Status");
		ImGui::InputFloat("Health", &inspectObject->health);
		ImGui::Spacing();

		// Object Flags 
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Flags");
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
		ImGui::Spacing();
	}

	ImGui::Separator();

	/// Show Create Object Window
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Create Object");
	if (ImGui::Button("Create New Object")) { isCreatingObject = !isCreatingObject; }
	if (isCreatingObject)
	{
		/// Create Game Object Window 
		ImGui::Begin("Create Object");
		if (newObject == nullptr)
		{
			// Default Object Data
			newObject = new GameObject();
			newObject->rigidBody3D.translation = Vector3One();
			newObject->rigidBody3D.scale = Vector3One();
			newObject->meshData = Vector4One();
			newObject->defaultColor = Color(
				getRandomInt(rng, 0, 255),
				getRandomInt(rng, 0, 255),
				getRandomInt(rng, 0, 255),
				255
			);
		}
		
		// Altered Object Data
		ImGui::Text("Object Data:");
		ImGui::InputFloat3("Position: ", &newObject->rigidBody3D.translation.x);
		ImGui::InputFloat3("Scale: ", &newObject->rigidBody3D.scale.x);
		ImGui::InputInt("Mesh Variant", &newObject->meshVariant, 1, 1);
		newObject->meshVariant = Clamp(newObject->meshVariant, 0, MESH_COUNT);
		ImGui::Spacing();

		// Texture Data
		ImGui::InputInt("Block ID: ", &newObject->blockID);
		
		// Status Data
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Status");
		ImGui::InputFloat("Health: %.2f", &newObject->health);
		ImGui::Spacing();
		
		// Object Flags
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

		// Spawn Object Button
		if (ImGui::Button("Spawn Game Object"))
		{
			GameObject clone = *newObject;
			scene->gameMap.saveObjectAt(newObject->getPosition(), clone);
			newObject = {};
			isCreatingObject = false;

		}
		ImGui::End();
	}

	ImGui::End();
}

int currentGameID = 0;

void DeveloperWindow::showMiniGameData(SceneManager* manager, Player* player)
{
	auto scene = manager->currentScene;
	
	/// Begin Mini Game Data Window
	ImGui::Begin("Mini Game Data");

	ImGui::Text("Display Mini Game Data");
	ImGui::InputInt("Mini Game ID", &currentGameID, 1, 1);
	currentGameID = Clamp(currentGameID, 0, 2);

	ImGui::Checkbox("Is Mini Game Active", &scene->isMiniActive);

	/// Launch Mini Game Button
	if (ImGui::Button("Launch Mini Game"))
	{
		switch (currentGameID)
		{
			// Select Mini Game
		case 1:
			scene->miniGame = MiniGame_crane();
			scene->isMiniActive = true;
			player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.2f));
			break;
		default:
			scene->miniGame = MiniGame_flappyBird();
			scene->isMiniActive = true;
			player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f));

			break;
		}
	}
	ImGui::Separator();

	/// Show Mini Game Data
	ImGui::TextColored(ImVec4(0, 0, 255, 255), "Mini Game Data");

	if (scene->miniGame != nullptr)
	{
		ImGui::Text("Score: %d", scene->miniGame->data->score);
		ImGui::Text("Score Goal: %d", scene->miniGame->data->scoreGoal);
		ImGui::Text("Is Left Goal Active: %s", scene->miniGame->data->isLeftGoalActive ? "True" : "False");
		ImGui::Text("Is Right Goal Active: %s", scene->miniGame->data->isRightGoalActive ? "True" : "False");
		ImGui::InputFloat4("Screen Size: %d", &scene->miniGame->data->screen.x);
		ImGui::InputFloat4("Goal Size: %d", &scene->miniGame->data->goal.x);

		ImGui::Text("Obstacles: %d", scene->miniGame->data->obstacles.size());
		for (auto& obj : scene->miniGame->data->obstacles)
		{
			if (ImGui::Button(std::to_string(obj.x).c_str()))
			{
				ImGui::Text("Object: X %f", obj.x);
				ImGui::Text("Object: Y %f", obj.y);
				ImGui::Text("Object: Width %f", obj.width);
				ImGui::Text("Object: Height %f", obj.height);

			}
		}
	}

	ImGui::End();
}