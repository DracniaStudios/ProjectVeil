#pragma once
#ifndef SIM_PLAYER_COMMAND_H
#define SIM_PLAYER_COMMAND_H

// One player's input for one simulation tick. The only thing a client ever
// sends about its player: the server turns commands into state, never the
// other way round (DedicatedServerPlan.md rule R4, MultiplayerSplitPlan.md E2).

#include <cstdint>

namespace Sim {

	// Held buttons are set every tick they are down. Pressed (edge) buttons are
	// latched by the client until a tick consumes them, so a press is never lost
	// when a frame runs zero ticks, and never doubled when it runs two.
	enum CommandButton : std::uint16_t {
		BUTTON_NONE        = 0,
		BUTTON_CROUCH      = 1 << 0, // held   — ACTION_MOVE_CROUCH
		BUTTON_SPRINT      = 1 << 1, // held   — ACTION_MOVE_SPRINT
		BUTTON_INTERACT    = 1 << 2, // edge   — ACTION_MOVE_INTERACT
		BUTTON_JUMP        = 1 << 3, // edge   — ACTION_MOVE_JUMP
		BUTTON_USE_ITEM    = 1 << 4, // edge   — ACTION_USE_ITEM
		BUTTON_USE_ITEM2   = 1 << 5, // held   — ACTION_USE_ITEM2
		BUTTON_FLASHLIGHT  = 1 << 6, // edge   — ACTION_USE_FLASHLIGHT
		BUTTON_ARTIFACT    = 1 << 7, // held   — ACTION_USE_ARTIFACT
		BUTTON_ARTIFACT_R  = 1 << 8, // edge   — ACTION_USE_ARTIFACT_RIGHT
		BUTTON_ARTIFACT_L  = 1 << 9, // edge   — ACTION_USE_ARTIFACT_LEFT
	};

	// The buttons that fire once per press. A repeated command (the server's
	// stand-in for a lost one, rule P-6) keeps the held buttons and drops these.
	inline constexpr std::uint16_t kEdgeButtons =
		BUTTON_INTERACT | BUTTON_JUMP | BUTTON_USE_ITEM | BUTTON_FLASHLIGHT | BUTTON_ARTIFACT_R | BUTTON_ARTIFACT_L;

	// Every bit a command may carry. The server masks anything else off.
	inline constexpr std::uint16_t kAllButtons = (1 << 10) - 1;

	struct PlayerCommand {
		std::uint32_t sequence   = 0;    // +1 per tick, per client; the server acks the last one it applied
		std::uint32_t clientTick = 0;    // client's tick when sampled (debugging, later prediction)
		std::int8_t   moveX      = 0;    // -1 left, 0, +1 right
		std::int8_t   moveY      = 0;    // -1 back, 0, +1 forward
		std::uint16_t buttons    = 0;    // CommandButton bits
		float         yawDelta   = 0.0f; // radians this tick, after the client's sensitivity
		float         pitchDelta = 0.0f;
	};

	static_assert(sizeof(PlayerCommand) <= 24, "PlayerCommand is sent ~60 times a second; keep it small");

	inline bool HasButton(const PlayerCommand& cmd, CommandButton button)
	{
		return (cmd.buttons & button) != 0;
	}

	// What the server applies when a Slot's queue is empty: the last command's
	// movement and held buttons, with no presses and no look (rule P-6).
	inline PlayerCommand RepeatOf(const PlayerCommand& last)
	{
		PlayerCommand repeat = last;
		repeat.buttons = static_cast<std::uint16_t>(last.buttons & ~kEdgeButtons);
		repeat.yawDelta = 0.0f;
		repeat.pitchDelta = 0.0f;
		return repeat;
	}
}

#endif
