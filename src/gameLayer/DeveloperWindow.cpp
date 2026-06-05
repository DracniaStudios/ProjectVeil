#include "DeveloperWindow.h"

#include <SceneManager.h>
#include <Player.h>
#include <AssetManager.h>

const char* boolString(bool boolean)
{
	if (boolean) { return "True"; }
	return "false";
}

void DeveloperWindow::update(Player* player)
{

	isActive =
		isCameraActive ||
		isInspectorActive ||
		isMiniGameActive ||
		isGameDataActive;

	if (IsKeyPressed(KEY_F1)) { isGameDataActive = !isGameDataActive; }
	if (IsKeyPressed(KEY_F2)) { isPlayerActive = !isPlayerActive; }
	if (IsKeyPressed(KEY_F3)) { isCameraActive = !isCameraActive; }
	if (IsKeyPressed(KEY_F4)) { isInspectorActive = !isInspectorActive; inspectObject = nullptr; }
	if (IsKeyPressed(KEY_F5)) { isMiniGameActive = !isMiniGameActive; }

	/// Update Game Data Windows
	if (isPlayerActive) showPlayerData(player);
	if (isCameraActive) showCameraData(player);
	if (isInspectorActive) showObjectInspector();
	if (isMiniGameActive) showMiniGameData(player);
	if (isGameDataActive) showGameData(player);

}

void DeveloperWindow::showGameData(Player* player)
{

	auto scene = SceneManager::getInstance().currentScene;
	ImGui::Begin("Game Data");

	/// Show Scene Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Scene Data");

	ImGui::Checkbox("Is 2D Active", &scene->is2DActive);
	ImGui::Checkbox("Is Mini Game Active", &scene->isMiniActive);
	ImGui::Separator();

	/// Show Game Map Data
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Game Map Data");

	ImGui::Text("Game Objects: %d", static_cast<int>(scene->gameMap.gameObjects.size()));
	ImGui::Text("Game Map Size: (%.2f, %.2f, %.2f)", &scene->gameMap.size.x);

	/// Entity Data
	ImGui::Text("Entity Count: %f", scene->entities.size());
	ImGui::Text("Last ID Used: %f", &scene->instanceHolder.idCounter);

	ImGui::End();
}

void DeveloperWindow::showPlayerData(Player* player)
{
	auto scene = SceneManager::getInstance().currentScene;
	
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

void DeveloperWindow::showCameraData(Player* player)
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
	return static_cast<ObjectType>(check->type);
}

Vector4 colorHolder = Vector4(0, 0, 0, 255);
static char inputName[128] = "New Object";

void DeveloperWindow::showObjectInspector()
{
	auto scene = SceneManager::getInstance().currentScene;
	auto rng = std::ranlux24_base(std::random_device{}());

	/// Show Object Inspector Window
	ImGui::Begin("Object Inspector");

	// Load Game Object Data To Inspector
	auto loadObjectData = [&](void* object_ptr)
	{
		if (object_ptr == nullptr) return;
		
		auto object = static_cast<GameObject*>(object_ptr);
		ImGui::PushID(object_ptr);

		const char* objectTypeString =
			object->type == OBJECT_PLAYER ? "Player" :
			object->type == OBJECT_ENTITY ? "Entity" :
			object->type == OBJECT_ITEM ? "Item" :
			object->type == OBJECT_PROJECTILE ? "Projectile" :
			object->type == OBJECT_ENVIRONMENT ? "Environment" :
			"Generic"
		;
		std::string dataString = objectTypeString;
		dataString += " Data";
		ImGui::TextColored(ImVec4(255, 255, 0, 255), dataString.c_str());
		//ImGui::Text("Name: %s", check->name);
		ImGui::Text("Type: %s", objectTypeString);
		ImGui::Text("ID: %d", static_cast<int>(object->id));
		ImGui::Spacing();

		// RigidBody Data
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Rigidbody");
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", object->getPosition().x, object->getPosition().y, object->getPosition().z);
		ImGui::Text("Scale: (%.2f, %.2f, %.2f)", object->rigidBody3D.scale.x, object->rigidBody3D.scale.y, object->rigidBody3D.scale.z);
		ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", object->rigidBody3D.velocity.x, object->rigidBody3D.velocity.y, object->rigidBody3D.velocity.z);
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Status");
		ImGui::Checkbox("Is Alive", &object->isAlive);
		ImGui::Text("Life Span: %f", object->lifeSpan);
		ImGui::Text("Life End: %f", object->endLife);

		// Object Flags 
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Flags");
		ImGui::Checkbox("isEnabled", &object->rigidBody3D.isEnabled);
		ImGui::Checkbox("isStatic", &object->rigidBody3D.isStatic);
		ImGui::Checkbox("isInteractable", &object->isInteractable);
		ImGui::Checkbox("isVisible", &object->display3DModel);
		ImGui::Checkbox("Show Collider", &object->displayCollider);
		ImGui::Checkbox("Up Touch", &object->rigidBody3D.upTouch);
		ImGui::Checkbox("Down Touch", &object->rigidBody3D.downTouch);
		ImGui::Checkbox("Left Touch", &object->rigidBody3D.leftTouch);
		ImGui::Checkbox("Right Touch", &object->rigidBody3D.rightTouch);
		ImGui::Checkbox("Front Touch", &object->rigidBody3D.frontTouch);
		ImGui::Checkbox("Back Touch", &object->rigidBody3D.backTouch);
		ImGui::Spacing();

		ImGui::PopID();
	};

	// Save Game Object Struct To Scene
	auto saveObjectData = [&](void* object_ptr)
	{
		if (object_ptr == nullptr) { object_ptr = new GameObject(); }
		
		auto object = static_cast<GameObject*>(object_ptr);
		ImGui::PushID(object_ptr);

		 /** Base Object Data **/
		ImGui::InputText("Name: ", inputName, 128);
		{
			object->name = inputName;
		}
		ImGui::InputInt("Object Type: ", &object->type, 1, 1);
		object->type = Clamp(object->type, 0, OBJECT_COUNT);
		const char* objectTypeString =
			object->type == OBJECT_PLAYER ? "Player" :
			object->type == OBJECT_ENTITY ? "Entity" :
			object->type == OBJECT_ITEM ? "Item" :
			object->type == OBJECT_PROJECTILE ? "Projectile" :
			object->type == OBJECT_ENVIRONMENT ? "Environment" :
			"Generic"
			;
		ImGui::Text(objectTypeString);

		// Color ( float to unsigned char conversion )
		{
			ImGui::InputFloat4("Color", &colorHolder.x);
			object->defaultColor = Color(
				static_cast<unsigned char>(Clamp(colorHolder.x, 0, 255)),
				static_cast<unsigned char>(Clamp(colorHolder.y, 0, 255)),
				static_cast<unsigned char>(Clamp(colorHolder.z, 0, 255)),
				static_cast<unsigned char>(Clamp(colorHolder.w, 0, 255))
			);
		}

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
		ImGui::Checkbox("isDestructible", &object->isDestructable);
		ImGui::Checkbox("Show Collider", &object->displayCollider);
		ImGui::Checkbox("Up Touch", &object->rigidBody3D.upTouch);
		ImGui::Checkbox("Down Touch", &object->rigidBody3D.downTouch);
		ImGui::Checkbox("Left Touch", &object->rigidBody3D.leftTouch);
		ImGui::Checkbox("Right Touch", &object->rigidBody3D.rightTouch);
		ImGui::Checkbox("Front Touch", &object->rigidBody3D.frontTouch);
		ImGui::Checkbox("Back Touch", &object->rigidBody3D.backTouch);
		ImGui::Spacing();

		ImGui::PopID();

		void* pointer = object;
		return pointer;
	};

	/// List Game Objects
	ImGui::BeginChild("Object List", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.2f));
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Game Objects");
	for (auto& object : scene->gameMap.gameObjects)
	{
		ImGui::PushID(&object);

		if (ImGui::Button(object.name))
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

		auto check = static_cast<GameObject*>(inspectObject);

		loadObjectData(inspectObject);

		if (check->type == OBJECT_ENTITY)
		{
			ImGui::TextColored(ImVec4(255, 255, 0, 255), "Additional Data");
			auto entity = static_cast<Entity*>(inspectObject);

			ImGui::Text("Health: (%.2f)", &entity->health);
			ImGui::Text("Max Health: (%.2f)", &entity->maxHealth);
			ImGui::Text("Stamina: (%.2f)", &entity->stamina);
			ImGui::Text("Max Stamina: (%.2f)", &entity->maxStamina);

			ImGui::Checkbox("Is Firing: ", &entity->isFiring);
			ImGui::Checkbox("Force Firing: ", &entity->forceFire);
			/*
			ImGui::Text("Is Firing: %s", boolString(&entity->isFiring));
			ImGui::Text("Force Firing: %s", boolString(&entity->forceFire));
			*/
		}

		if (ImGui::Button("Delete Object"))
		{
			scene->gameMap.removeObject(check);
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

		/** Base Object Data **/
		saveObjectData(object);

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


		auto newEntity = static_cast<Entity*>(saveObjectData(object));

		ImGui::InputInt("Object Type: ", &newEntity->type, 1, 1);
		const char* objectTypeString =
			newEntity->type == OBJECT_PLAYER ? "Player" :
			newEntity->type == OBJECT_ENTITY ? "Entity" :
			"Generic"
		;
		// Status Data
		ImGui::TextColored(ImVec4(0, 255, 255, 255), "Status");
		ImGui::InputFloat("Health", &newEntity->health);
		ImGui::InputFloat("Stamina", &newEntity->stamina);
		ImGui::InputFloat("Base Damage", &newEntity->baseDamage);
		ImGui::InputFloat("Base Speed", &newEntity->baseSpeed);
		ImGui::Spacing();
		
		// Object Flags
		ImGui::TextColored(ImVec4(100, 0, 0, 255), "Flags");

		// Combat Flags
		ImGui::Checkbox("Is Firing", &newEntity->isFiring);
		ImGui::Checkbox("Force Fire", &newEntity->forceFire);
		ImGui::Spacing();

		// Spawn Object Button
		if (ImGui::Button("Spawn Game Object"))
		{
			Entity clone = *newEntity;
			scene->gameMap.saveEntityAt(clone.getPosition(), clone);
			newObject = {};
			isCreatingEntity = false;
		}
		ImGui::End();
	}

	ImGui::EndChild();
	ImGui::End();
}

int currentGameID = 0;

void DeveloperWindow::showMiniGameData(Player* player)
{
	auto scene = SceneManager::getInstance().currentScene;
	
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