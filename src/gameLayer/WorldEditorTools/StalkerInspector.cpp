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
	for (auto& [id, entity] : scene->entities)
	{
		if (!entity || entity->kind != ENTITYKIND_STALKER) { continue; }
		auto* stalker = static_cast<Stalker*>(entity.get());
		++found;

		ImGui::PushID(static_cast<int>(id));
		ImGui::TextColored(ImVec4(255, 0, 0, 255), "%s (id %d)", stalker->name.c_str(), static_cast<int>(id));

		ImGui::Text("State: %s", stalkerStateToString(stalker->state));
		ImGui::Text("Time in state: %.1fs", stalker->timeInState);
		ImGui::Text("Position: (%.1f, %.1f)", stalker->getPosition().x, stalker->getPosition().z);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(255, 255, 0, 255), "Belief");
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
		ImGui::TextColored(ImVec4(255, 255, 0, 255), "Tuning");
		ImGui::DragFloat("Hearing threshold", &stalker->hearingThreshold, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Hunt threshold", &stalker->huntThreshold, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Hunt patience", &stalker->huntPatience, 0.1f, 0.0f, 30.0f);
		ImGui::DragFloat("Search duration", &stalker->searchDuration, 0.1f, 0.0f, 60.0f);
		ImGui::DragFloat("Search radius", &stalker->searchRadius, 0.1f, 0.0f, 50.0f);
		ImGui::DragFloat("Base speed", &stalker->baseSpeed, 0.1f, 0.0f, 20.0f);

		ImGui::Text("Waypoints: %d (current %d)",
			static_cast<int>(stalker->waypoints.size()), stalker->currentWaypoint);

		ImGui::PopID();
		ImGui::Separator();
	}

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
