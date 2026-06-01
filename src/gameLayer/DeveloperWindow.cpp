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

bool isCreatingObject = false;
bool isCreatingEntity = false;

ObjectType getType(void* object)
{
	auto check = static_cast<GameObject*>(object);
	return check->type;
}

void DeveloperWindow::showObjectInspector(SceneManager* manager, AssetManager* assetManager)
{
	auto scene = manager->currentScene;
	auto rng = std::ranlux24_base(std::random_device{}());


	/// Show Object Inspector Window
	ImGui::Begin("Object Inspector");

	
	/// List Game Objects
	ImGui::BeginChild("Object List", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.2f));
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Game Objects");
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
	
	ImGui::EndChild();
	ImGui::Separator();

	ImGui::BeginChild("Selector", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.8f));
	/// Show Selected Object Data
	if (inspectObject != nullptr) {

		auto check = getType(inspectObject) == OBJECT_GENERIC ? static_cast<GameObject*>(inspectObject): static_cast<Entity*>(inspectObject);

		if (check->type == OBJECT_ENTITY)
		{
			ImGui::TextColored(ImVec4(255, 255, 0, 255), "Entity Data");
			check = static_cast<Entity*>(inspectObject);
		}
		else
		{
			ImGui::TextColored(ImVec4(255, 255, 0, 255), "Object Data");
			check = {};
			check = static_cast<GameObject*>(inspectObject);
		}


		ImGui::Text("ID: %d", check->id);
		ImGui::Text("Name: %s", check->name);
		ImGui::Text("Type: %d", check->type);
		ImGui::Spacing();

		// RigidBody Data
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Rigidbody");
		ImGui::InputFloat3("Position: ", &check->rigidBody3D.translation.x);
		ImGui::InputFloat3("Scale: ", &check->rigidBody3D.scale.x);
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", check->rigidBody3D.velocity.x, check->rigidBody3D.velocity.y, check->rigidBody3D.velocity.z);
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Status");
		ImGui::Checkbox("Is Alive", &check->isAlive);
		ImGui::Text("Life Span: %f", check->lifeSpan);
		ImGui::Text("Life End: %f", check->endLife);
		ImGui::Text("Health: %f", check->health);
		if (check->type == OBJECT_ENTITY)
		{
			auto entity = static_cast<Entity*>(check);

			ImGui::Text("Stamina: %f", entity->stamina);
			ImGui::Text("Base Damage: %f", entity->baseDamage);
			ImGui::Text("Base Speed: %f", entity->baseSpeed);
		}
		ImGui::Spacing();

		// Object Flags 
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Flags");
		ImGui::Checkbox("isEnabled", &check->rigidBody3D.isEnabled);
		ImGui::Checkbox("isStatic", &check->rigidBody3D.isStatic);
		ImGui::Checkbox("isInteractable", &check->isInteractable);
		ImGui::Checkbox("isVisible", &check->display3DModel);
		ImGui::Checkbox("Show Collider", &check->displayCollider);
		ImGui::Checkbox("Up Touch", &check->rigidBody3D.upTouch);
		ImGui::Checkbox("Down Touch", &check->rigidBody3D.downTouch);
		ImGui::Checkbox("Left Touch", &check->rigidBody3D.leftTouch);
		ImGui::Checkbox("Right Touch", &check->rigidBody3D.rightTouch);
		ImGui::Checkbox("Front Touch", &check->rigidBody3D.frontTouch);
		ImGui::Checkbox("Back Touch", &check->rigidBody3D.backTouch);
		ImGui::Spacing();

		if (ImGui::Button("Delete Object"))
		{
			scene->gameMap.removeObject(check->id);
			inspectObject = nullptr;
		}
	}

	ImGui::EndChild();
	ImGui::Separator();


	/// Show Create Object Window
	ImGui::BeginChild("Create Windows");

	/** Object Window **/
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Create Object");
	if (ImGui::Button("Create New Object"))
	{
		isCreatingObject = !isCreatingObject;
		newObject = new GameObject();
	}

	if (isCreatingObject)
	{
		/// Create Game Object Window 
		ImGui::Begin("Create Object");
		
		auto object = static_cast<GameObject*>(newObject);
		if (object == nullptr) { object = new GameObject(); }
		//permaAssertComment(object == nullptr, "No Object Data @ DeveloperWindow.cpp");
		/** Base Object Data **/
		object->type = OBJECT_GENERIC;
		object->rigidBody3D.isStatic = true;
		object->rigidBody3D.translation = Vector3One();
		object->rigidBody3D.scale = Vector3One();
		object->meshData = Vector4One();
		object->defaultColor = Color(
			getRandomInt(rng, 0, 255),
			getRandomInt(rng, 0, 255),
			getRandomInt(rng, 0, 255),
			255
		);
		
		// Altered Object Data
		ImGui::Text("Object Data:");
		ImGui::InputFloat3("Position: ", &object->rigidBody3D.translation.x);
		ImGui::InputFloat3("Scale: ", &object->rigidBody3D.scale.x);
		ImGui::InputInt("Mesh Variant", &object->meshVariant, 1, 1);
		object->meshVariant = Clamp(object->meshVariant, 0, MESH_COUNT);
		ImGui::Spacing();

		// Texture Data
		ImGui::InputInt("Block ID: ", &object->blockID);
		
		// Object Flags
		ImGui::TextColored(ImVec4(100, 0, 0, 255), "Flags");
		ImGui::Checkbox("isEnabled", &object->rigidBody3D.isEnabled);
		ImGui::Checkbox("isStatic", &object->rigidBody3D.isStatic);
		ImGui::Checkbox("isVisible", &object->display3DModel);
		ImGui::Checkbox("Show Collider", &object->displayCollider);
		ImGui::Checkbox("Up Touch", &object->rigidBody3D.upTouch);
		ImGui::Checkbox("Down Touch", &object->rigidBody3D.downTouch);
		ImGui::Checkbox("Left Touch", &object->rigidBody3D.leftTouch);
		ImGui::Checkbox("Right Touch", &object->rigidBody3D.rightTouch);
		ImGui::Checkbox("Front Touch", &object->rigidBody3D.frontTouch);
		ImGui::Checkbox("Back Touch", &object->rigidBody3D.backTouch);
		ImGui::Spacing();

		// Spawn Object Button
		if (ImGui::Button("Spawn Game Object"))
		{
			GameObject clone = *object;
			scene->gameMap.saveObjectAt(object->getPosition(), clone);
			newObject = {};
			isCreatingObject = false;
		}
		ImGui::End();
	}

	/** Entity Window **/
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Create Entity");
	if (ImGui::Button("Create Entity"))
	{
		isCreatingEntity = !isCreatingEntity;
		newObject = new Entity();
	}
	if (isCreatingEntity)
	{
		/// Create Game Object Window 
		ImGui::Begin("Create Entity");
		auto object = static_cast<Entity*>(newObject);
		
		if (object == nullptr) { object = new Entity(); }
		//permaAssertComment(object == nullptr, "No Entity Data @ DeveloperWindow.cpp");

		object->type = OBJECT_ENTITY;
		object->rigidBody3D.isStatic = false;
		object->rigidBody3D.translation = Vector3One();
		object->rigidBody3D.scale = Vector3One();
		object->meshData = Vector4One();
		object->defaultColor = Color(255, 0, 0, 255);

		// Altered Object Data
		ImGui::Text("Entity Data:");
		ImGui::InputFloat3("Position: ", &object->rigidBody3D.translation.x);
		ImGui::InputFloat3("Scale: ", &object->rigidBody3D.scale.x);
		ImGui::InputInt("Mesh Variant", &object->meshVariant, 1, 1);
		object->meshVariant = Clamp(object->meshVariant, 0, MESH_COUNT);
		ImGui::Spacing();

		// Texture Data
		ImGui::InputInt("Block ID: ", &object->blockID);
		
		// Status Data
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Status");
		ImGui::InputFloat("Health", &object->health);
		ImGui::InputFloat("Stamina", &object->stamina);
		ImGui::InputFloat("Base Damage", &object->baseDamage);
		ImGui::InputFloat("Base Speed", &object->baseSpeed);
		ImGui::Spacing();
		
		// Object Flags
		ImGui::TextColored(ImVec4(100, 0, 0, 255), "Flags");
		ImGui::Checkbox("isEnabled", &object->rigidBody3D.isEnabled);
		ImGui::Checkbox("isStatic", &object->rigidBody3D.isStatic);
		ImGui::Checkbox("isVisible", &object->display3DModel);
		ImGui::Checkbox("Show Collider", &object->displayCollider);
		ImGui::Checkbox("Up Touch", &object->rigidBody3D.upTouch);
		ImGui::Checkbox("Down Touch", &object->rigidBody3D.downTouch);
		ImGui::Checkbox("Left Touch", &object->rigidBody3D.leftTouch);
		ImGui::Checkbox("Right Touch", &object->rigidBody3D.rightTouch);
		ImGui::Checkbox("Front Touch", &object->rigidBody3D.frontTouch);
		ImGui::Checkbox("Back Touch", &object->rigidBody3D.backTouch);
		ImGui::Spacing();

		// Spawn Object Button
		if (ImGui::Button("Spawn Game Object"))
		{
			GameObject clone = *object;
			scene->gameMap.saveObjectAt(object->getPosition(), clone);
			newObject = {};
			isCreatingEntity = false;
		}
		ImGui::End();
	}

	ImGui::EndChild();
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