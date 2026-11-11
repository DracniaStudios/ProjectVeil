#pragma once
#ifndef SIM_FIXED_TICK_H
#define SIM_FIXED_TICK_H

// The fixed simulation rate (DedicatedServerPlan.md Step 1.2, decision Q16).
// Rendering stays once per frame; the simulation runs zero or more fixed ticks
// per frame, so how far anything moves never depends on the frame rate.

namespace Sim {

	inline constexpr float kTickRate         = 60.0f;
	inline constexpr float kTickDelta        = 1.0f / kTickRate;
	inline constexpr float kMaxFrameTime     = 0.25f; // ignore longer stalls (breakpoints, window drags)
	inline constexpr int   kMaxTicksPerFrame = 8;     // catch-up limit; anything beyond is dropped, not queued

	/**
	 * Turns frame time into a whole number of ticks. Leftover time carries to the
	 * next frame. A frame that would need more than kMaxTicksPerFrame drops the
	 * rest, so a long stall never turns into a burst of catch-up ticks.
	 */
	struct FixedTickClock
	{
		float accumulator = 0.0f;

		// The ticks to run for a frame that took `frameTime` seconds
		int Advance(float frameTime)
		{
			if (!(frameTime > 0.0f)) { return 0; } // also rejects NaN
			accumulator += frameTime < kMaxFrameTime ? frameTime : kMaxFrameTime;

			int ticks = 0;
			while (accumulator >= kTickDelta && ticks < kMaxTicksPerFrame)
			{
				accumulator -= kTickDelta;
				++ticks;
			}
			if (ticks == kMaxTicksPerFrame && accumulator >= kTickDelta) { accumulator = 0.0f; }
			return ticks;
		}

		// How far the leftover time is into the next tick, 0..1, for drawing between ticks
		float Alpha() const { return accumulator / kTickDelta; }

		void Reset() { accumulator = 0.0f; }
	};
}

#endif
