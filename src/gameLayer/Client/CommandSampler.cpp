#include "CommandSampler.h"

#include <InputSystem.h>

#include <raylib.h>

#include <algorithm>

namespace Client {

	namespace {
		std::uint16_t Bit(bool on, Sim::CommandButton button) { return on ? static_cast<std::uint16_t>(button) : 0; }
	}

	void CommandSampler::SampleFrame(bool gameplayHasInput)
	{
		if (!gameplayHasInput)
		{
			// Nothing held, nothing latched: a press made just before an overlay
			// opened would otherwise fire the moment it closes.
			moveX = 0;
			moveY = 0;
			held = 0;
			latched = 0;
			pendingYaw = 0.0f;
			pendingPitch = 0.0f;
			return;
		}

		auto& input = InputSystem::InputSystem::getInstance();
		using namespace InputSystem;

		const std::int8_t x = input.IsActionDown(ACTION_MOVE_LEFT) ? -1 : input.IsActionDown(ACTION_MOVE_RIGHT) ? 1 : 0;
		const std::int8_t y = input.IsActionDown(ACTION_MOVE_FORWARD) ? 1 : input.IsActionDown(ACTION_MOVE_BACKWARD) ? -1 : 0;

		const std::uint16_t down = static_cast<std::uint16_t>(
			Bit(input.IsActionDown(ACTION_MOVE_CROUCH), Sim::BUTTON_CROUCH)
			| Bit(input.IsActionDown(ACTION_MOVE_SPRINT), Sim::BUTTON_SPRINT)
			| Bit(input.IsActionDown(ACTION_USE_ITEM2), Sim::BUTTON_USE_ITEM2)
			| Bit(input.IsActionDown(ACTION_USE_ARTIFACT), Sim::BUTTON_ARTIFACT));

		const std::uint16_t pressed = static_cast<std::uint16_t>(
			Bit(input.IsActionPressed(ACTION_MOVE_INTERACT), Sim::BUTTON_INTERACT)
			| Bit(input.IsActionPressed(ACTION_MOVE_JUMP), Sim::BUTTON_JUMP)
			| Bit(input.IsActionPressed(ACTION_USE_ITEM), Sim::BUTTON_USE_ITEM)
			| Bit(input.IsActionPressed(ACTION_USE_FLASHLIGHT), Sim::BUTTON_FLASHLIGHT)
			| Bit(input.IsActionPressed(ACTION_USE_ARTIFACT_RIGHT), Sim::BUTTON_ARTIFACT_R)
			| Bit(input.IsActionPressed(ACTION_USE_ARTIFACT_LEFT), Sim::BUTTON_ARTIFACT_L));

		const Vector2 mouse = GetMouseDelta();
		Record(x, y, down, pressed, mouse.x, mouse.y);
	}

	void CommandSampler::Record(std::int8_t x, std::int8_t y, std::uint16_t down, std::uint16_t pressed, float mouseDeltaX, float mouseDeltaY)
	{
		moveX = x;
		moveY = y;
		held = static_cast<std::uint16_t>(down & ~Sim::kEdgeButtons);
		latched = static_cast<std::uint16_t>(latched | (pressed & Sim::kEdgeButtons));

		pendingYaw += mouseDeltaX * kLookSensitivity;

		// Only the pitch the view can still turn before the limit is kept, so the
		// server, which clamps the same way, never looks somewhere the camera doesn't
		const float viewPitch = std::clamp(basePitch + pendingPitch + mouseDeltaY * kLookSensitivity, -kPitchLimit, kPitchLimit);
		pendingPitch = viewPitch - basePitch;
	}

	Sim::PlayerCommand CommandSampler::Consume(std::uint32_t clientTick)
	{
		Sim::PlayerCommand command = {};
		command.sequence = ++sequence;
		command.clientTick = clientTick;
		command.moveX = moveX;
		command.moveY = moveY;
		command.buttons = static_cast<std::uint16_t>(held | latched);
		command.yawDelta = pendingYaw;
		command.pitchDelta = pendingPitch;

		latched = 0;
		pendingYaw = 0.0f;
		pendingPitch = 0.0f;
		return command;
	}

	void CommandSampler::Clear()
	{
		moveX = 0;
		moveY = 0;
		held = 0;
		latched = 0;
		pendingYaw = 0.0f;
		pendingPitch = 0.0f;
	}
}
