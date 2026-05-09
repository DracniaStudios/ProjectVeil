#include "mainMenu.h"

bool isImGuiEnabled = false;

Player player;
AssetManager assetManager;
GameObject* selectedObject;

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

					std::cout << "Ray hit Object ID: " << object.id << "\n";
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

	player.update(manager, deltaTime);
	player.camera.UpdateCameraFPS(&cam, &player);

	if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
	{
		selectedObject = selectObject(scene, cam);
	}

#pragma region ImGui
	if (IsKeyPressed(KEY_F10)) { isImGuiEnabled = !isImGuiEnabled; }

	if (isImGuiEnabled) {
		ImGui::Begin("Game Data");

		ImGui::BeginChild("Player Data");

		ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", player.rigidBody3D.translation.x, player.rigidBody3D.translation.y, player.rigidBody3D.translation.z);
		ImGui::Text("Player Velocity 3D: (%.2f, %.2f, %.2f)", player.rigidBody3D.velocity.x, player.rigidBody3D.velocity.y, player.rigidBody3D.velocity.z);
		ImGui::Text("Camera Forward: (%.2f, %.2f, %.2f)", player.camera.forward.x, player.camera.forward.y, player.camera.forward.z);

		ImGui::Separator();

		ImGui::Text("Player Forward: (%.2f, %.2f, %.2f)", player.rigidBody3D.front.x, player.rigidBody3D.front.y, player.rigidBody3D.front.z);
		ImGui::Text("Player Backward: (%.2f, %.2f, %.2f)", player.rigidBody3D.back.x, player.rigidBody3D.back.y, player.rigidBody3D.back.z);


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

	return scene;
}