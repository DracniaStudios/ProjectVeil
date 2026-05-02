#include "mainMenu.h"


bool lockOn = false;
bool isImGuiEnabled = false;
int cameraMode = CAMERA_FIRST_PERSON;

Player player;

void Scene_MainMenuUpdate(void* manager_ptr, void* object_ptr, float delta)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);
	//auto scene = static_cast<Scene*>(manager->currentScene->object_ptr);

	// Camera Logic
	{
		auto& cam = manager->camera3D;

		cam.position = Vector3(
			player.getPosition().x,
			player.getPosition().y + (player.rigidBody3D.scale.y / 2),
			player.getPosition().z
		);

		UpdateCamera(&cam, cameraMode);

		// FPS-style camera look
		static float yaw = 0.0f;
		static float pitch = 0.0f;
		const float sensitivity = -0.003f;
		Vector2 mouseDelta = GetMouseDelta();
		yaw   += mouseDelta.x * sensitivity;
		pitch += mouseDelta.y * sensitivity;
		if (pitch > 1.5f) pitch = 1.5f;
		if (pitch < -1.5f) pitch = -1.5f;

		Vector3 camForward = {
			cosf(pitch) * sinf(yaw),
			sinf(pitch),
			cosf(pitch) * cosf(yaw)
		};
		camForward = Vector3Normalize(camForward);
		cam.target = Vector3Add(cam.position, Vector3Scale(camForward, 10.0f));

		// Update player direction vectors to match camera look
		Vector3 flatForward = camForward; flatForward.y = 0; flatForward = Vector3Normalize(flatForward);
		if (Vector3Length(flatForward) > 0.001f) {
			player.rigidBody3D.front = flatForward;
			player.rigidBody3D.back = Vector3Scale(flatForward, -1);
			player.rigidBody3D.right = Vector3{ -flatForward.z, 0, flatForward.x };
			player.rigidBody3D.left = Vector3{ flatForward.z, 0, -flatForward.x };
			player.rigidBody3D.up = Vector3{ 0, 1, 0 };
			player.rigidBody3D.down = Vector3{ 0, -1, 0 };
		}

		player.update(&cam, delta);

#pragma region ImGui

		if (IsKeyPressed(KEY_F10)) { isImGuiEnabled = !isImGuiEnabled; }

		if (isImGuiEnabled) {
			ImGui::Begin("Game Data");

			ImGui::BeginChild("Player Data");

			ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", player.rigidBody3D.translation.x, player.rigidBody3D.translation.y, player.rigidBody3D.translation.z);
			ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", cam.target.x, cam.target.y, cam.target.z);
			
			ImGui::Separator();

			ImGui::Text("Player Forward: (%.2f, %.2f, %.2f)", player.rigidBody3D.front.x, player.rigidBody3D.front.y, player.rigidBody3D.front.z);
			ImGui::Text("Player Backward: (%.2f, %.2f, %.2f)", player.rigidBody3D.back.x, player.rigidBody3D.back.y, player.rigidBody3D.back.z);

			ImGui::EndChild();


			ImGui::End();
		}
#pragma endregion
	}
}
void Scene_MainMenuDraw2D(void* manager_ptr, void* object_ptr)
{
	auto manager = static_cast<SceneManager*>(manager_ptr);

	//player.render2D();
	
};
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