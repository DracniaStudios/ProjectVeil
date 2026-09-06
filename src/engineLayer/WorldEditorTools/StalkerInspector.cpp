#include "WorldEditor.h"

#include <AI/Stalker.h>
#include <Perception/SoundField.h>

/**
 * Stalker AI inspector.
 *
 * Exists because the slice's acceptance criteria are behavioural and cannot be
 * checked from the outside: "driven only by sound events, with no position
 * feed" and "Director hints alter stalker behaviour without granting ground
 * truth — verify by logging what the Director passes". Both are claims about
 * *why* the stalker moved, which is invisible while watching it move.
 */
void WorldEditor::ShowStalkerData()
{
	auto scene = SceneManager::getInstance().currentScene;

	ImGui::Begin("Stalker AI");

	if (scene == nullptr) { ImGui::Text("No active scene."); ImGui::End(); return; }

	// --- Sound field ------------------------------------------------------
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Sound Field");
	ImGui::Text("Clock: %.2fs", scene->soundField.Now());
	ImGui::Text("Live events: %d / %d",
		static_cast<int>(scene->soundField.Events().size()),
		static_cast<int>(SoundField::kCapacity));

	if (ImGui::CollapsingHeader("Recent noises"))
	{
		const float now = scene->soundField.Now();
		for (const auto& event : scene->soundField.Events())
		{
			ImGui::Text("%-9s L%.2f  age %.1fs  (%.1f, %.1f)  src %d",
				soundKindToString(event.kind), event.loudness, now - event.timestamp,
				event.position.x, event.position.z, static_cast<int>(event.sourceId));
		}
	}

	ImGui::Separator();

	// --- Stalkers ---------------------------------------------------------
	int found = 0;
	scene->gameMap.ForEachEntity([&](Entity& entity)
	{
		if (entity.kind != ENTITYKIND_STALKER) { return; }
		auto* stalker = static_cast<Stalker*>(&entity);
		++found;

		ImGui::PushID(static_cast<int>(entity.id));
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s (id %d)", stalker->name.c_str(), static_cast<int>(entity.id));

		ImGui::Text("State: %s", stalkerStateToString(stalker->state));
		ImGui::Text("Time in state: %.1fs", stalker->timeInState);
		ImGui::Text("Position: (%.1f, %.1f)", stalker->getPosition().x, stalker->getPosition().z);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "Belief");
		if (stalker->hasLastKnown)
		{
			// The whole of what the stalker knows. If this ever tracks the
			// player continuously, the sound-only decision has been broken.
			ImGui::Text("Last known: (%.1f, %.1f)",
				stalker->lastKnownPosition.x, stalker->lastKnownPosition.z);
			ImGui::Text("Heard: %s at L%.2f, %.1fs ago",
				soundKindToString(stalker->lastHeardKind),
				stalker->lastHeardLoudness,
				scene->soundField.Now() - stalker->lastStimulusTime);
		}
		else
		{
			ImGui::Text("Last known: none - the stalker has no idea where you are");
		}

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "Tuning");
		ImGui::DragFloat("Hearing threshold", &stalker->hearingThreshold, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Hunt threshold", &stalker->huntThreshold, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Hunt patience", &stalker->huntPatience, 0.1f, 0.0f, 30.0f);
		ImGui::DragFloat("Search duration", &stalker->searchDuration, 0.1f, 0.0f, 60.0f);
		ImGui::DragFloat("Search radius", &stalker->searchRadius, 0.1f, 0.0f, 50.0f);
		ImGui::DragFloat("Base speed", &stalker->baseSpeed, 0.1f, 0.0f, 20.0f);
		// How far ahead it looks for walls. Too short and it turns after it is
		// already against one; too long and it swerves around geometry it was
		// never going to touch.
		ImGui::DragFloat("Avoid probe", &stalker->avoidProbeDistance, 0.1f, 0.0f, 20.0f);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "Patrol Route");
		ImGui::Checkbox("Draw route in world", &stalker->displayRoute);
		ImGui::Text("Waypoints: %d (heading for %d)",
			static_cast<int>(stalker->waypoints.size()), stalker->currentWaypoint);

		if (stalker->waypoints.empty())
		{
			// Worth stating outright: an empty route is not an error, but it does
			// mean the stalker never moves until something makes a noise, which
			// looks identical to the AI being broken.
			ImGui::TextColored(ImVec4(1, 1, 0, 1),
				"No route: the stalker holds position until it hears something.");
		}

		// Authoring is "walk there, then press the button". Typing coordinates
		// for a patrol route is unusable, and the player is the only cursor the
		// editor has that is already grounded in the world.
		if (ImGui::Button("Add Waypoint At Player") && scene->player)
		{
			stalker->waypoints.push_back(scene->player->getPosition());
			stalker->displayRoute = true;
			statusMessage = "Added waypoint " + std::to_string(stalker->waypoints.size());
		}
		if (ImGui::Button("Add Waypoint At Stalker"))
		{
			stalker->waypoints.push_back(stalker->getPosition());
			stalker->displayRoute = true;
		}
		if (ImGui::Button("Add Waypoint At Camera"))
		{
			stalker->waypoints.push_back(SceneManager::getInstance().camera3D.position);
			stalker->displayRoute = true;
		}

		if (!stalker->waypoints.empty())
		{
			if (ImGui::Button("Remove Last"))
			{
				stalker->waypoints.pop_back();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Route"))
			{
				stalker->waypoints.clear();
			}

			int removeIndex = -1;
			for (std::size_t w = 0; w < stalker->waypoints.size(); ++w)
			{
				ImGui::PushID(static_cast<int>(w));
				char label[32] = {};
				std::snprintf(label, sizeof(label), "wp %d", static_cast<int>(w));
				ImGui::InputFloat3(label, &stalker->waypoints[w].x);
				ImGui::SameLine();
				if (ImGui::Button("X")) { removeIndex = static_cast<int>(w); }
				ImGui::PopID();
			}
			// Deferred so the vector is not resized while it is being walked.
			if (removeIndex >= 0)
			{
				stalker->waypoints.erase(stalker->waypoints.begin() + removeIndex);
			}

			// currentWaypoint indexes the route and is clamped in update(), but
			// only on the patrol path — leaving it past the end here would show a
			// nonsense "heading for" until the stalker next patrols.
			if (stalker->currentWaypoint >= static_cast<int>(stalker->waypoints.size()))
			{
				stalker->currentWaypoint = 0;
			}
		}

		ImGui::PopID();
		ImGui::Separator();
	});

	if (found == 0) { ImGui::Text("No stalker in the scene."); }

	// --- Director ---------------------------------------------------------
	ImGui::TextColored(ImVec4(255, 0, 255, 255), "Director");
	ImGui::Text("Clock: %.1fs   Cooldown: %.1fs",
		scene->director.Now(), scene->director.CooldownRemaining());

	const auto& hints = scene->director.Log();
	if (hints.empty())
	{
		ImGui::Text("No hints issued yet.");
	}
	else
	{
		// Each row names the world fact behind the hint. A hint whose reason
		// mentions the player's position rather than a station or a timer is
		// the failure this panel is here to catch.
		for (const auto& hint : hints)
		{
			ImGui::Text("%.1fs  (%.1f, %.1f) c%.2f  %s",
				hint.timestamp, hint.region.x, hint.region.z, hint.confidence, hint.reason);
		}
	}

	ImGui::End();
}
