#include "DeveloperWindow.h"

int textureId = 0; // Global variable to hold the texture ID
const char* decoyName;

void DisplayObject(GameObject& object) 
{
	ImGui::PushID(&object);

	// Generic Data
	//ImGui::InputText(object.name.c_str(), &object.name.c_str());
	ImGui::Text(object.name.c_str());
	ImGui::TextColored(ImVec4(255, 0, 255, 255), std::to_string(object.id).c_str());
	ImGui::Checkbox("Enabled", &object.isEnabled);
	ImGui::Checkbox("Selected", &object.canBeSelected);
	ImGui::Checkbox("Destroyable", &object.isDestructible);

	ImGui::InputInt("Object Type: ", &object.type, 1, 1);
	const char* objectTypeString =
		object.type == OBJECT_PLAYER ? "Player" :
		object.type == OBJECT_ENTITY ? "Entity" :
		object.type == OBJECT_ITEM ? "Item" :
		object.type == OBJECT_PROJECTILE ? "Projectile" :
		object.type == OBJECT_ENVIRONMENT ? "Environment" :
		"Generic"
		;
	ImGui::TextColored(ImVec4(255, 0, 255, 255), objectTypeString);

	// Debug Display
	ImGui::Checkbox("Show Model", &object.display3DModel);
	ImGui::Checkbox("Show Direction", &object.displayDirection);
	ImGui::Checkbox("Show Collider", &object.displayCollider);

	// Status Data
	ImGui::Checkbox("Alive", &object.isAlive);
	ImGui::Checkbox("Pending Destroy", &object.pendingDestroy);
	ImGui::InputFloat("Life Time", &object.lifeTime);
	ImGui::InputFloat("Death Time", &object.deathTime);
	ImGui::InputFloat("Decay Time", &object.decayTime);

	// Physics

	// Renderer
	// Model Data
	// Mesh Data
	ImGui::Text("Texture: ", &object.texture->name);
	ImGui::Text("Texture ID: ", static_cast<int>(object.texture->texture.id));

	
	


	ImGui::PopID();

}

void DeveloperWindow::ShowObjectInspector()
{
	auto scene = SceneManager::getInstance().currentScene;

	/// Show Object Inspector Window
	ImGui::Begin("Object Inspector");

	// Load Game Object Data To Inspector
	auto loadObjectData = [&](GameObject* object)
		{
			if (object == nullptr) return;

			ImGui::PushID(object);

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
			ImGui::Text("Life Span: %f", object->lifeTime);
			ImGui::Text("Life End: %f", object->deathTime);

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

			ImGui::TextColored(ImVec4(255, 255, 0, 255), "Texture");
			ImGui::InputInt("Texture ID: ", &textureId);
			object->texture = &AssetManager::getInstance().assets[textureId];

			ImGui::Image((ImTextureRef)(intptr_t)&AssetManager::getInstance().assets[textureId].texture.id, ImVec2(64, 64));
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
			}
			inspectObject = &object;
		}
		ImGui::PopID();
	}

	ImGui::EndChild();
	ImGui::Separator();

	ImGui::BeginChild("Selector", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.8f));
	/// Show Selected Object Data
	if (inspectObject != nullptr) {

		auto check = static_cast<GameObject*>(inspectObject);
		//loadObjectData(inspectObject);
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
		newObject = new GameObject;
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
			inspectObject = scene->gameMap.saveObject(*newObject);
			newObject = nullptr;
			isCreatingObject = false;
		}
		ImGui::End();
	}

	ImGui::EndChild();
	ImGui::End();
}
