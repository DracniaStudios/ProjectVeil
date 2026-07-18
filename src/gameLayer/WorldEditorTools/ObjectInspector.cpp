#include "WorldEditor.h"

int textureId = 0; // Global variable to hold the texture ID

void WorldEditor::ShowObjectInspector()
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Show Object Inspector Window
	ImGui::Begin("Object Inspector");

	// Load Game Object Data To Inspector
	auto loadObjectData = [&](GameObject* object)
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
		
			ImGui::TextColored(ImVec4(255, 255, 0, 255), "Texture");
			ImGui::Image((ImTextureRef)(intptr_t)object->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture.id, ImVec2(64, 64));

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
			ImGui::Text("Life End: %f", object->deathSpan);

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

			ImGui::PopID();
		};

	// Save Game Object Struct To Scene
	auto saveObjectData = [&](GameObject* object)
		{
			if (object == nullptr) { object = new GameObject(); }

			ImGui::PushID(object);

			/** Base Object Data **/
			ImGui::InputText("Name: ", createName, 128);
			{
				object->name = createName;
			}
			ImGui::InputInt("Object Type: ", &object->type, 1, 1);
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

			// Altered Object Data
			ImGui::Text("Object Data:");
			ImGui::InputFloat3("Position: ", &object->rigidBody3D.translation.x);
			ImGui::InputFloat3("Scale: ", &object->rigidBody3D.scale.x);

			ImGui::TextColored(ImVec4(255, 255, 0, 255), "Texture");
			auto& assets = AssetManager::getInstance().assets;
			if (!assets.empty())
			{
				ImGui::InputInt("Texture ID: ", &textureId);
				textureId = Clamp(textureId, 0, static_cast<int>(assets.size()) - 1);

				// setTexture records the asset name so the choice survives save/load
				if (assets[textureId].type == ASSET_TEXTURE) { object->setTexture(assets[textureId].name); }

				ImGui::Image((ImTextureRef)(intptr_t)assets[textureId].texture.id, ImVec2(64, 64));
			}
			// Load Texture based on texture ID
			
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

			ImGui::PopID();

			void* pointer = object;
			return pointer;
		};

	/// List Game Objects
	ImGui::BeginChild("Object List", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.2f));
	ImGui::TextColored(ImVec4(0, 255, 0, 255), "Game Objects");
	for (auto& object : scene->gameMap.gameObjects)
	{
		if (getType(&object) == OBJECT_PROJECTILE) continue;
		ImGui::PushID(&object);
		if (ImGui::Button(object.name.c_str()))
		{
			if (getType(&object) == OBJECT_ENTITY)
			{
				isEntityActive = true;
				isInspectorActive = false;
				inspectEntityId = object.id; // Entities share their id with the gameObjects shadow
			}
			inspectObjectId = object.id;
		}
		ImGui::PopID();
	}

	ImGui::EndChild();
	ImGui::Separator();

	ImGui::BeginChild("Selector", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.8f));
	/// Show Selected Object Data (resolved by id each frame — spawns/destroys move objects around)
	if (GameObject* check = findGameObject(inspectObjectId)) {

		//loadObjectData(check);
		if (ImGui::Button("Delete Object"))
		{
			scene->gameMap.removeObject(check);
			inspectObjectId = 0;
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
		delete newObject; // Reclaim any previous staging object
		newObject = isCreatingObject ? new GameObject : nullptr;
	}

	if (isCreatingObject)
	{
		/// Create Game Object Window 
		ImGui::Begin("Create Object");

		/** Base Object Data **/
		saveObjectData(newObject);

		// Spawn Object Button
		if (ImGui::Button("Spawn Game Object"))
		{
			newObject->rigidBody3D.Teleport(newObject->getPosition());
			inspectObjectId = scene->gameMap.saveObject(*newObject)->id;
			delete newObject; // saveObject stored a copy; the staging object is done
			newObject = nullptr;
			isCreatingObject = false;
		}
		ImGui::End();
	}

	ImGui::EndChild();
	ImGui::End();
}
