#include "WorldEditor.h"

void WorldEditor::ShowEntityInspector()
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Show Object Inspector Window
	ImGui::Begin("Entity Inspector");

	// Load Game Object Data To Inspector
	auto loadObjectData = [&](Entity* object)
		{
			if (object == nullptr) return;
			ImGui::PushID(object);

			const char* objectTypeString = objectTypeToString(object->type);
			std::string dataString = objectTypeString;
			dataString += " Data";
			ImGui::TextColored(ImVec4(255, 255, 0, 255), "%s", dataString.c_str());

			ImGui::Text("Name: %s", object->name.c_str());
			ImGui::Text("Type: %s", objectTypeString);
			ImGui::Text("ID: %d", static_cast<int>(object->id));
			ImGui::Spacing();

			// RigidBody Data
			ImGui::TextColored(ImVec4(0, 255, 255, 255), "Rigidbody");
			ImGui::Text("Position: (%.2f, %.2f, %.2f)", object->getPosition().x, object->getPosition().y, object->getPosition().z);
			ImGui::Text("Scale: (%.2f, %.2f, %.2f)", object->rigidBody3D.scale.x, object->rigidBody3D.scale.y, object->rigidBody3D.scale.z);
			ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", object->rigidBody3D.GetVelocity().x, object->rigidBody3D.GetVelocity().y, object->rigidBody3D.GetVelocity().z);
			ImGui::Spacing();

			ImGui::TextColored(ImVec4(0, 255, 255, 255), "Status");
			ImGui::Checkbox("Is Alive", &object->isAlive);
			ImGui::Text("Life Span: %f", object->lifeSpan);
			ImGui::Text("Death Span: %f", object->deathSpan);

			// Object Flags 
			ImGui::TextColored(ImVec4(0, 255, 255, 255), "Flags");
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

			// Entity Data
			ImGui::Text("Health: (%.2f)", object->health);
			ImGui::Text("Max Health: (%.2f)", object->maxHealth);
			ImGui::Text("Stamina: (%.2f)", object->stamina);
			ImGui::Text("Max Stamina: (%.2f)", object->maxStamina);

			ImGui::Checkbox("Is Firing: ", &object->isFiring);
			ImGui::Checkbox("Force Firing: ", &object->forceFire);

			ImGui::PopID();
		};

	// Save Game Object Struct To Scene
	auto saveObjectData = [&](Entity* object)
		{
			if (object == nullptr) { object = new Entity(); }

			ImGui::PushID(object);

			/** Base Object Data **/
			ImGui::InputText("Name: ", createName, 128);
			{
				object->name = createName;
			}
			ImGui::InputInt("Entity Type: ", &object->type, 1, 1);
			object->type = Clamp(object->type, 0, OBJECT_COUNT - 1);
			ImGui::Text("%s", objectTypeToString(object->type));

			// Color ( float to unsigned char conversion )
			{
				ImGui::InputFloat4("Color", &createColorHolder.x);
				object->defaultColor = Color(
					static_cast<unsigned char>(Clamp(createColorHolder.x, 0, 255)),
					static_cast<unsigned char>(Clamp(createColorHolder.y, 0, 255)),
					static_cast<unsigned char>(Clamp(createColorHolder.z, 0, 255)),
					static_cast<unsigned char>(Clamp(createColorHolder.w, 0, 255))
				);
			}

			// Altered Entity Data
			ImGui::Text("Entity Data:");
			ImGui::InputFloat3("Position: ", &object->rigidBody3D.translation.x);
			ImGui::InputFloat3("Scale: ", &object->rigidBody3D.scale.x);
			ImGui::Spacing();

			// Object Flags
			ImGui::TextColored(ImVec4(100, 0, 0, 255), "Flags");
			ImGui::Checkbox("isEnabled", &object->rigidBody3D.isEnabled);
			ImGui::Checkbox("isStatic", &object->rigidBody3D.isStatic);
			ImGui::Checkbox("isVisible", &object->display3DModel);
			ImGui::Checkbox("isDestructible", &object->isDestructible);
			ImGui::Checkbox("Show Collider", &object->displayCollider);
			ImGui::Checkbox("Up Touch", &object->rigidBody3D.upTouch);
			ImGui::Checkbox("Down Touch", &object->rigidBody3D.downTouch);
			ImGui::Checkbox("Left Touch", &object->rigidBody3D.leftTouch);
			ImGui::Checkbox("Right Touch", &object->rigidBody3D.rightTouch);
			ImGui::Checkbox("Front Touch", &object->rigidBody3D.frontTouch);
			ImGui::Checkbox("Back Touch", &object->rigidBody3D.backTouch);
			ImGui::Spacing();

			ImGui::InputFloat("Health: ", &object->health);
			ImGui::InputFloat("Max Health: ", &object->maxHealth);
			ImGui::InputFloat("Stamina: ", &object->stamina);
			ImGui::InputFloat("Max Stamina: ", &object->maxStamina);
			ImGui::Spacing();

			ImGui::Checkbox("Is Firing: ", &object->isFiring);
			ImGui::Checkbox("Force Firing: ", &object->forceFire);
			ImGui::Spacing();

			ImGui::PopID();

			void* pointer = object;
			return pointer;
		};

	/// List Game Objects
	ImGui::BeginChild("Entity List", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.2f));
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Entites");
	for (auto& entity : scene->entities)
	{
		ImGui::PushID(&entity);
		if (ImGui::Button(entity.second->name.c_str())) { inspectEntityId = entity.first; }
		ImGui::PopID();
	}
	ImGui::EndChild();
	ImGui::Separator();

	ImGui::BeginChild("Selector", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.8f));
	/// Show Selected Object Data (resolved by id each frame — the entity may have died or been cleared by Load Game)
	if (Entity* inspectEntity = findEntity(inspectEntityId)) {

		loadObjectData(inspectEntity);
		if (ImGui::Button("Delete Object"))
		{
			scene->gameMap.removeEntity(inspectEntity);
			inspectEntityId = 0;
		}
	}

	ImGui::EndChild();
	ImGui::Separator();


	/// Show Create Object Window
	ImGui::BeginChild("Create Windows");

	/** Object Window **/
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Create Entity");
	if (ImGui::Button("Create New Entity"))
	{
		isCreatingEntity = !isCreatingEntity;
		delete newEntity; // Reclaim any previous staging entity
		newEntity = isCreatingEntity ? new Entity : nullptr; // A null member would crash Spawn — the lambda only patches its local copy
	}

	if (isCreatingEntity)
	{
		/// Create Game Object Window 
		ImGui::Begin("Create Entity");

		/** Base Object Data **/
		saveObjectData(newEntity);

		// Spawn Object Button
		if (ImGui::Button("Spawn Entity"))
		{
			newEntity->rigidBody3D.Teleport(newEntity->getPosition());
			inspectEntityId = scene->gameMap.saveEntity(*newEntity)->id;
			delete newEntity; // saveEntity stored a copy; the staging entity is done
			newEntity = nullptr;
			isCreatingEntity = false;
		}
		ImGui::End();
	}

	ImGui::EndChild();
	ImGui::End();
}
