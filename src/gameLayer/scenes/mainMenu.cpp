#include "mainMenu.h"


bool isImGuiEnabled = false;

Player player = {};
AssetManager assetManager = {};
GameObject* selectedObject = {};

MiniGame* currentMiniGame = {};
int miniGameID = 0;

Vector3 cubePosition = { 0, 0, 0 };

GameObject* selectObject(Scene* scene, Camera& cam)
{
	Ray selectRay;
	selectRay.position = cam.position; // Adjust the ray's origin to be at the player's head height
	selectRay.direction = player.camera.forward;

	if (scene->gameMap.gameObjects.size() > 0)
	{
		for (auto& object : scene->gameMap.gameObjects)
		{
			if (object.isEnabled)
			{
				BoundingBox objectBox = {
					object.getPosition() - object.getSize() / 2,
					object.getPosition() + object.getSize() / 2
				};
				if (GetRayCollisionBox(selectRay, objectBox).hit)
				{
					if (!object.canBeSelected) { continue; }

					return &object;
				}
			}
		}
	}

	return nullptr;

}

void Scene_MainMenuUpdate(void* manager_ptr, void* object_ptr, float deltaTime)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(object_ptr);
	auto& cam = manager->camera3D;

	scene->gameMap.gameObjects[0].rigidBody3D.scale = scene->gameMap.getMapSize();

	/// Update Scene Data
	if (scene->is2DActive)
	{
		player.update2D(manager, deltaTime);
	}
	else
	{
		player.update3D(manager, deltaTime);
		player.camera.UpdateCameraFPS(&cam, &player);
	}

	if (scene->isMiniActive)
	{
		if (currentMiniGame != nullptr)
		{
			currentMiniGame->update(manager_ptr, &player, deltaTime);
		}
	}

	/// Player Select Objects
	if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
	{
		selectedObject = selectObject(scene, cam);
	}

	/// Switch to 2D Mode
	if (IsKeyPressed(KEY_TAB)) scene->is2DActive = !scene->is2DActive;

#pragma region ImGui
	if (IsKeyPressed(KEY_F10)) { isImGuiEnabled = !isImGuiEnabled; }

	if (isImGuiEnabled) {
		ImGui::Begin("Game Data");

		ImGui::Checkbox("Scene is 2D: ", &scene->is2DActive);

		ImGui::BeginChild("Player Data");

		if (scene->is2DActive)
		{
			ImGui::Checkbox("RigidBody is Enabled: ", &player.rigidBody2D.isEnabled);
			ImGui::Text("Player Position 2D: (%.2f, %.2f)", player.rigidBody2D.translation.x, player.rigidBody2D.translation.y);
			ImGui::Text("Player Velocity 2D: (%.2f, %.2f)", player.rigidBody2D.velocity.x, player.rigidBody2D.velocity.y);
			ImGui::Text("Player Scale 2D: (%.2f, %.2f)", player.rigidBody2D.scale.x, player.rigidBody2D.scale.y);
			
		}
		else
		{
			ImGui::Checkbox("RigidBody is Enabled: ", &player.rigidBody3D.isEnabled);
			ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", player.rigidBody3D.translation.x, player.rigidBody3D.translation.y, player.rigidBody3D.translation.z);
			ImGui::Text("Player Velocity 3D: (%.2f, %.2f, %.2f)", player.rigidBody3D.velocity.x, player.rigidBody3D.velocity.y, player.rigidBody3D.velocity.z);
			ImGui::Text("Player Scale 3D: (%.2f, %.2f, %.2f)", player.rigidBody3D.scale.x, player.rigidBody3D.scale.y, player.rigidBody3D.scale.z);
			
		}
		ImGui::Text("Player Forward: (%.2f, %.2f, %.2f)", player.rigidBody3D.front.x, player.rigidBody3D.front.y, player.rigidBody3D.front.z);

		ImGui::Text("Camera Forward: (%.2f, %.2f, %.2f)", player.camera.forward.x, player.camera.forward.y, player.camera.forward.z);
		ImGui::Separator();

		ImGui::Separator();
		ImGui::InputFloat3("Cube Position", &cubePosition.x);
		if (ImGui::Button("Add Game Object"))
		{
			GameObject newObject;
			newObject.rigidBody3D.translation = cubePosition;
			newObject.rigidBody3D.scale = Vector3(1, 1, 1);
			scene->gameMap.saveObjectAt(cubePosition, newObject);
		}
		
		ImGui::Separator();
		// Show Current Game Object Data
		if (selectedObject != nullptr)
		{
			DrawCubeWires(selectedObject->rigidBody3D.translation, selectedObject->rigidBody3D.scale.x, selectedObject->rigidBody3D.scale.y, selectedObject->rigidBody3D.scale.z, WHITE);
			ImGui::Text("Selected Object ID: %d", selectedObject->id);
			//ImGui::Text("Selected Object Name: %s", selectedObject->name);
			ImGui::Text("Selected Object Position: (%.2f, %.2f, %.2f)", selectedObject->rigidBody3D.translation.x, selectedObject->rigidBody3D.translation.y, selectedObject->rigidBody3D.translation.z);
			ImGui::Text("Selected Object Scale: (%.2f, %.2f, %.2f)", selectedObject->rigidBody3D.scale.x, selectedObject->rigidBody3D.scale.y, selectedObject->rigidBody3D.scale.z);
			ImGui::Text("Selected Object Velocity: (%.2f, %.2f, %.2f)", selectedObject->rigidBody3D.velocity.x, selectedObject->rigidBody3D.velocity.y, selectedObject->rigidBody3D.velocity.z);
			ImGui::Checkbox("Selected Object Visible", &selectedObject->display3DModel);
			ImGui::Checkbox("Selected Object Collider", &selectedObject->displayCollider);
			ImGui::Checkbox("Selected Object Up Touch", &selectedObject->rigidBody3D.upTouch);
			ImGui::Checkbox("Selected Object Down Touch", &selectedObject->rigidBody3D.downTouch);
			ImGui::Checkbox("Selected Object Left Touch", &selectedObject->rigidBody3D.leftTouch);
			ImGui::Checkbox("Selected Object Right Touch", &selectedObject->rigidBody3D.rightTouch);
			ImGui::Checkbox("Selected Object Front Touch", &selectedObject->rigidBody3D.frontTouch);
			ImGui::Checkbox("Selected Object Back Touch", &selectedObject->rigidBody3D.backTouch);
		}

		ImGui::Separator();

		ImGui::Checkbox("Activate Mini Game", &scene->isMiniActive);
		ImGui::InputInt("Set MiniGame ID", &miniGameID);
		miniGameID = Clamp(miniGameID, 0, 1);

		ImGui::EndChild();

		ImGui::End();
	}
#pragma endregion
}

void Scene_MainMenuDraw2D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(object_ptr);

	if (scene->is2DActive)
	{
		DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{20, 20, 20, 200});
		if (scene->isMiniActive)
		{
			currentMiniGame = MiniGame_flappyBird();
			currentMiniGame->draw(manager_ptr, object_ptr);
		}
	}
	/// Always Render Player Last (On Top)
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
	scene->gameMap.create(Vector3(100, 1, 100));
	scene->gameMap.gameObjects.push_back(player);
	player.onEnable();

	return scene;
}