#include "DeveloperWindow.h"

void DeveloperWindow::ShowPlayerData(Player* player)
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Begin Player Window
	ImGui::Begin("Player Data");

	std::string dataString = "Player";
	dataString += " Data";
	ImGui::TextColored(ImVec4(255, 255, 0, 255), dataString.c_str());
	ImGui::Text("Name: %s", &player->name);
	ImGui::Text("ID: %d", static_cast<int>(player->id));
	ImGui::Spacing();


	/// Show Rigidbody Data based on space
	if (scene->is2DActive)
	{
		//ImGui::Checkbox("RigidBody is Enabled: ", &player->rigidBody2D.isEnabled);
		ImGui::Text("Player Position 2D: (%.2f, %.2f)", player->rigidBody2D.translation.x, player->rigidBody2D.translation.y);
		ImGui::Text("Player Velocity 2D: (%.2f, %.2f)", player->rigidBody2D.velocity.x, player->rigidBody2D.velocity.y);
		ImGui::Text("Player Scale 2D: (%.2f, %.2f)", player->rigidBody2D.scale.x, player->rigidBody2D.scale.y);

	}
	else
	{
		//ImGui::Checkbox("RigidBody is Enabled: ", &player->rigidBody3D.SetActive());
		ImGui::Text("Player Position 3D: (%.2f, %.2f, %.2f)", player->rigidBody3D.translation.x, player->rigidBody3D.translation.y, player->rigidBody3D.translation.z);
		ImGui::Text("Player Velocity 3D: (%.2f, %.2f, %.2f)", player->rigidBody3D.GetVelocity().x, player->rigidBody3D.GetVelocity().y, player->rigidBody3D.GetVelocity().z);
		ImGui::Text("Player Scale 3D: (%.2f, %.2f, %.2f)", player->rigidBody3D.scale.x, player->rigidBody3D.scale.y, player->rigidBody3D.scale.z);

	}
	ImGui::Spacing();

	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Status");
	ImGui::Checkbox("Is Alive", &player->isAlive);
	ImGui::Text("Life Span: %f", player->lifeSpan);
	ImGui::Text("Life End: %f", player->endLife);
	ImGui::Spacing();

	/// Show Player Directional Data and Flags
	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Flags");
	ImGui::Text("Player Forward: (%.2f, %.2f, %.2f)", player->rigidBody3D.forward.x, player->rigidBody3D.forward.y, player->rigidBody3D.forward.z);
	ImGui::Checkbox("isEnabled", &player->rigidBody3D.isEnabled);
	ImGui::Checkbox("isStatic", &player->rigidBody3D.isStatic);
	ImGui::Checkbox("isVisible", &player->display3DModel);
	ImGui::Checkbox("Show Collider", &player->displayCollider);
	ImGui::Checkbox("Up Touch", &player->rigidBody3D.upTouch);
	ImGui::Checkbox("Down Touch", &player->rigidBody3D.downTouch);
	ImGui::Checkbox("Left Touch", &player->rigidBody3D.leftTouch);
	ImGui::Checkbox("Right Touch", &player->rigidBody3D.rightTouch);
	ImGui::Checkbox("Front Touch", &player->rigidBody3D.frontTouch);
	ImGui::Checkbox("Back Touch", &player->rigidBody3D.backTouch);
	ImGui::Spacing();

	ImGui::Text("Health: (%.2f)", &player->health);
	ImGui::Text("Max Health: (%.2f)", &player->maxHealth);
	ImGui::Text("Stamina: (%.2f)", &player->stamina);
	ImGui::Text("Max Stamina: (%.2f)", &player->maxStamina);
	ImGui::Spacing();

	ImGui::Checkbox("Is Firing: ", &player->isFiring);
	ImGui::Checkbox("Force Firing: ", &player->forceFire);
	ImGui::Spacing();

	ImGui::Separator();

	ImGui::TextColored(ImVec4(0, 255, 255, 255), "Change Data");
	if (ImGui::Button("Hurt Player")) { player->health -= 1; }
	ImGui::InputFloat("Player Health: ", &player->health, 1, 1);
	ImGui::Checkbox("Is Firing", &player->isFiring);
	ImGui::End();

}
