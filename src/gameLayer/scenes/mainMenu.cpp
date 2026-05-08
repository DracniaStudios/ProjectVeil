#include "mainMenu.h"

bool isImGuiEnabled = false;

Player player;
PlayerCamera playerCamera;
AssetManager assetManager;

Vector3 cubePosition = { 0, 0, 0 };
int currentObjectID = 0;

GameObject selectedObject;

void Scene_MainMenuUpdate(void* manager_ptr, void* object_ptr, float deltaTime)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(object_ptr);

	auto& cam = manager->camera3D;
	playerCamera.UpdateCameraFPS(&cam, &player);
	player.update(manager, deltaTime);

	if (&scene->gameMap.gameObjects[0] != &player)
	{
		scene->gameMap.gameObjects[0] = player;
	}

	if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
	{
		Ray selectRay;
		selectRay.position = cam.position + Vector3(0, 1, 0); // Adjust the ray's origin to be at the player's head height
		selectRay.direction = playerCamera.forward;
		
		// Place Object At Mouse Positon
		static GameObject newObject;
		
		
		/*
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
						selectedObject = object;
						std::cout << "Selected Object ID: " << selectedObject.id << "\n";
						std::cout << "Selected Object Position: (" << selectedObject.getPosition().x << ", " << selectedObject.getPosition().y << ", " << selectedObject.getPosition().z << ")\n";
						break; // Stop checking after the first hit
					}
				}
			}
		}
		*/
	}

#pragma region ImGui
	if (IsKeyPressed(KEY_F10)) { isImGuiEnabled = !isImGuiEnabled; }

	if (isImGuiEnabled) {
		ImGui::Begin("Game Data");

		ImGui::BeginChild("Player Data");

		ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", player.rigidBody3D.translation.x, player.rigidBody3D.translation.y, player.rigidBody3D.translation.z);
		ImGui::Text("Player Velocity 3D: (%.2f, %.2f, %.2f)", player.rigidBody3D.velocity.x, player.rigidBody3D.velocity.y, player.rigidBody3D.velocity.z);
		ImGui::Text("Camera Target: (%.2f, %.2f, %.2f)", cam.target.x, cam.target.y, cam.target.z);

		ImGui::Separator();

		ImGui::Text("Player Forward: (%.2f, %.2f, %.2f)", player.rigidBody3D.front.x, player.rigidBody3D.front.y, player.rigidBody3D.front.z);
		ImGui::Text("Player Backward: (%.2f, %.2f, %.2f)", player.rigidBody3D.back.x, player.rigidBody3D.back.y, player.rigidBody3D.back.z);


		ImGui::Separator();
		if (ImGui::Button("Add Game Object"))
		{
			GameObject newObject;
			newObject.rigidBody3D.translation = cubePosition;
			newObject.rigidBody3D.scale = Vector3(1, 1, 1);
			scene->gameMap.saveObjectAt(cubePosition, newObject);
		}
		ImGui::InputFloat3("Cube Position", &cubePosition.x);
		
		ImGui::Separator();
		// Show Current Game Object Data
		ImGui::InputInt("Current Object ID: ", &currentObjectID, 1, 1);
		currentObjectID = Clamp(currentObjectID, 0, scene->gameMap.gameObjects.size());
		if (&scene->gameMap.gameObjects[currentObjectID] != nullptr)
		{
			auto& currentObject = scene->gameMap.gameObjects[currentObjectID];
			ImGui::Checkbox("Selected Object Enabled", &currentObject.isEnabled);
			if(currentObject.isEnabled) {
				DrawCubeWires(currentObject.rigidBody3D.translation, currentObject.rigidBody3D.scale.x, currentObject.rigidBody3D.scale.y, currentObject.rigidBody3D.scale.z, WHITE);
				ImGui::Text("Selected Object Position: (%.2f, %.2f, %.2f)", currentObject.rigidBody3D.translation.x, currentObject.rigidBody3D.translation.y, currentObject.rigidBody3D.translation.z);
				ImGui::Text("Selected Object Scale: (%.2f, %.2f, %.2f)", currentObject.rigidBody3D.scale.x, currentObject.rigidBody3D.scale.y, currentObject.rigidBody3D.scale.z);
				ImGui::Text("Selected Object Velocity: (%.2f, %.2f, %.2f)", currentObject.rigidBody3D.velocity.x, currentObject.rigidBody3D.velocity.y, currentObject.rigidBody3D.velocity.z);
				ImGui::Checkbox("Selected Object Visible", &currentObject.display3DModel);
				ImGui::Checkbox("Selected Object Collider", &currentObject.displayCollider);
				ImGui::Checkbox("Selected Object Up Touch", &currentObject.rigidBody3D.upTouch);
				ImGui::Checkbox("Selected Object Down Touch", &currentObject.rigidBody3D.downTouch);
				ImGui::Checkbox("Selected Object Left Touch", &currentObject.rigidBody3D.leftTouch);
				ImGui::Checkbox("Selected Object Right Touch", &currentObject.rigidBody3D.rightTouch);
				ImGui::Checkbox("Selected Object Front Touch", &currentObject.rigidBody3D.frontTouch);
				ImGui::Checkbox("Selected Object Back Touch", &currentObject.rigidBody3D.backTouch);
			}
		}

		ImGui::EndChild();

		ImGui::End();
	}
#pragma endregion
}

void Scene_MainMenuDraw2D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	//player.render2D();
	DrawTexturePro(assetManager.magicSphere,
		Rectangle{ 0, 0, static_cast<float>(assetManager.magicSphere.width), static_cast<float>(assetManager.magicSphere.height) },
		Rectangle{static_cast<float>(GetScreenWidth() * 0.5f), static_cast<float>(GetScreenHeight() * 0.5f), static_cast<float>(GetScreenWidth() * 0.5f), static_cast<float>(GetScreenHeight() * 0.5f) },
		Vector2{ 0, 0 },
		0,
		WHITE);
};

void Scene_MainMenuDraw3D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	auto scene = static_cast<Scene*>(object_ptr);
	DrawGrid(100.0f, 1.0f);
	player.render3D();
	DrawRay(Ray{manager->camera3D.position, manager->camera3D.target}, RED);
}

Scene* Scene_MainMenuConstruct()
{
	Scene* scene = Scene_new();
	scene->update = Scene_MainMenuUpdate;
	scene->draw2D = Scene_MainMenuDraw2D;
	scene->draw3D = Scene_MainMenuDraw3D;
	scene->object_ptr = scene;

	//scene->gameMap.create(1920, 5, 1080);
	scene->gameMap.create(100, 1, 100);
	scene->gameMap.gameObjects.push_back(player);

	return scene;
}