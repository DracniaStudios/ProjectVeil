#pragma once
#ifndef CLIENT_COMMAND_SAMPLER_H
#define CLIENT_COMMAND_SAMPLER_H

#include <sim/PlayerCommand.h>

#include <cstdint>

namespace Client {

	/**
	 * The local player's input, sampled once per rendered frame and handed to the
	 * simulation one PlayerCommand per tick (DedicatedServerPlan.md Step 1.3).
	 *
	 *   - Held buttons and movement are whatever is down at the last sample.
	 *   - Pressed buttons are latched until a tick consumes them, so a frame that
	 *     runs no tick keeps the press and a frame that runs two fires it once.
	 *   - Mouse look accumulates until a tick consumes it.
	 *
	 * The camera looks along the player's simulated angles plus whatever look no
	 * tick has consumed yet (ViewYaw, ViewPitch), so it turns the moment the mouse
	 * does and still agrees with the simulation once the tick runs.
	 */
	class CommandSampler
	{
	public:
		static CommandSampler& getInstance()
		{
			static CommandSampler instance;
			return instance;
		}

		// Radians per mouse pixel. Negative so moving the mouse right turns right.
		static constexpr float kLookSensitivity = -0.003f;

		// The pitch limit, either way, in radians. Keeps the view off the poles.
		static constexpr float kPitchLimit = 1.5f;

		/**
		 * The local player's look as the simulation last left it. Set every frame
		 * before sampling: the pitch limit is applied against it.
		 */
		void SetLookBase(float yaw, float pitch) { baseYaw = yaw; basePitch = pitch; }

		/**
		 * Reads the InputSystem and the mouse for this frame. While gameplay
		 * doesn't have the player's input (an overlay, the World Editor), nothing
		 * is held, nothing is latched and the view doesn't turn.
		 */
		void SampleFrame(bool gameplayHasInput);

		/**
		 * Records one frame's input directly. SampleFrame calls it with what the
		 * devices report; tests call it with what they want pressed.
		 * @param held  Every CommandButton bit currently down
		 * @param pressed Every CommandButton bit pressed this frame
		 * @param mouseDelta Pixels the mouse moved this frame
		 */
		void Record(std::int8_t moveX, std::int8_t moveY, std::uint16_t held, std::uint16_t pressed, float mouseDeltaX, float mouseDeltaY);

		// The next tick's command: clears the latched presses and the look it carries
		Sim::PlayerCommand Consume(std::uint32_t clientTick);

		// Forgets everything pending, as if no frame had been sampled
		void Clear();

		// Where the camera looks: the look base plus the look not yet consumed
		float ViewYaw() const { return baseYaw + pendingYaw; }
		float ViewPitch() const { return basePitch + pendingPitch; }

		// The sequence the next Consume will carry
		std::uint32_t NextSequence() const { return sequence + 1; }

	private:
		CommandSampler() = default;

		std::int8_t moveX = 0;
		std::int8_t moveY = 0;
		std::uint16_t held = 0;
		std::uint16_t latched = 0;
		float pendingYaw = 0.0f;
		float pendingPitch = 0.0f;

		float baseYaw = 0.0f;
		float basePitch = 0.0f;

		std::uint32_t sequence = 0;
	};
}

#endif
