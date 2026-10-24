#include "ContactRules.h"

#include <AudioManager.h>
#include <Scene.h>

namespace ContactRules
{
	namespace
	{
		const char* groundImpactSound = "Jump_Landing_With_Small_Coins";

		// The object a body id belongs to: the player lives outside the GameMap
		GameObject* Resolve(SceneManagement::Scene& scene, std::uint64_t id)
		{
			if (scene.player != nullptr && scene.player->id == id) { return scene.player; }
			return scene.gameMap.FindWorldObject(id);
		}

		// Physics impacts as a perception event (plan's emitter table: scaled by
		// impact velocity).
		//
		// Loudness comes from closing speed along the contact normal, which is what
		// separates a thrown crate from a body resting on the floor. A resting or
		// sliding contact closes at ~0 and never reaches kQuietImpact, so walking a
		// room does not light up the sound field — footsteps are their own emitter
		// with their own gait rules, and this must not quietly duplicate them.
		void EmitImpactNoise(SceneManagement::Scene& scene, const Physics3D::ContactEvent& contact,
			GameObject& a, GameObject& b)
		{
			constexpr float kQuietImpact = 2.0f;   // below this there is no audible event
			constexpr float kLoudImpact = 12.0f;   // at or above this, full loudness

			if (contact.closingSpeed <= kQuietImpact) { return; }

			const float loudness = Clamp(
				(contact.closingSpeed - kQuietImpact) / (kLoudImpact - kQuietImpact), 0.0f, 1.0f);

			// Credited to the body that moved, not to the static geometry it hit
			const GameObject& striker = contact.faster == b.id ? b : a;

			// SoundField only records the AI-perception event; playing it back for
			// the player is a separate, explicit step here (SoundField itself has
			// no AudioManager/FMOD dependency by design).
			AudioManager::AudioManager::getInstance().Play3D(groundImpactSound, striker.getPosition(), AudioManager::AUDIO_GAMEPLAY_SFX, loudness);
			scene.soundField.Emit(striker.getPosition(), loudness, SOUND_IMPACT, striker.id);
		}

		// Contact damage from the Stalker catching the player
		void ApplyStalkerContactDamage(GameObject& a, GameObject& b)
		{
			GameObject* player = nullptr;
			GameObject* stalker = nullptr;
			if (a.type == OBJECT_PLAYER) { player = &a; stalker = &b; }
			else if (b.type == OBJECT_PLAYER) { player = &b; stalker = &a; }
			else { return; }

			if (stalker->type != OBJECT_ENTITY
				|| static_cast<Entity*>(stalker)->kind != ENTITYKIND_STALKER) {
				return;
			}

			static_cast<Entity*>(player)->applyHealthValue(stalker->baseDamage, true);
		}

		// Entity::Attack() spawns a physical OBJECT_PROJECTILE that flies, collides,
		// and decays after 3s; this is what makes it hurt
		void ApplyProjectileDamage(GameObject& a, GameObject& b)
		{
			GameObject* projectile = nullptr;
			GameObject* target = nullptr;
			if (a.type == OBJECT_PROJECTILE) { projectile = &a; target = &b; }
			else if (b.type == OBJECT_PROJECTILE) { projectile = &b; target = &a; }
			else { return; }

			if (target->type != OBJECT_PLAYER && target->type != OBJECT_ENTITY) { return; }

			static_cast<Entity*>(target)->applyHealthValue(projectile->baseDamage, true);
			projectile->Destroy();
		}
	}

	void Apply(SceneManagement::Scene& scene, std::span<const Physics3D::ContactEvent> events)
	{
		for (const Physics3D::ContactEvent& contact : events)
		{
			if (contact.phase != Physics3D::ContactPhase::Enter) { continue; }

			GameObject* a = Resolve(scene, contact.a);
			GameObject* b = Resolve(scene, contact.b);
			if (a == nullptr || b == nullptr) { continue; }

			if (!contact.isTrigger) { EmitImpactNoise(scene, contact, *a, *b); }
			ApplyStalkerContactDamage(*a, *b);
			ApplyProjectileDamage(*a, *b);
		}
	}
}
