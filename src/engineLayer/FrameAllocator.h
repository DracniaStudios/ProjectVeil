#pragma once
#ifndef FRAME_ALLOCATOR_H
#define FRAME_ALLOCATOR_H

#include <ArenaAllocator.h>

#include <cstddef>
#include <cstdint>
#include <utility>

/**
 * Two arenas, swapped every frame, so this frame can still read what last
 * frame built.
 *
 * A single per-frame arena -- reset at the top of the frame, filled during it,
 * reset again -- is the right shape for scratch that is born and dies inside
 * one frame, and Scene.cpp already runs one for the collision broad phase. It
 * breaks the moment one frame's output is another frame's input: interpolation
 * between the previous and current transform, a "what did the stalker hear last
 * tick" query, a render command list the next frame diffs against. Reset the
 * one arena and last frame's answer is gone; keep it and the arena never
 * resets.
 *
 * Double-buffering resolves that without a copy. nextFrame() flips which arena
 * is current and clears the new one, so the arena holding last frame's data is
 * left completely alone for the whole of this frame, then becomes the scratch
 * for the frame after. Memory is never copied, only re-labelled.
 *
 *     FrameAllocator frames(256 * 1024);
 *
 *     // once per frame, before anything allocates
 *     frames.nextFrame();
 *
 *     auto* poses = frames.createArray<Pose>(boneCount);   // this frame
 *     const Arena& lastFrame = frames.previous();          // still intact
 *
 * The lifetime rule is exactly two frames and not one more: anything from
 * previous() is gone after the next nextFrame(). Data that must outlive that
 * belongs in a level arena, a pool or a slot map, not here.
 *
 * Both arenas are sized the same, because either one can be the busy one.
 * Size them from highWaterMark() after a representative session rather than by
 * guessing, the same way Scene.cpp's solver arena was sized.
 */
class FrameAllocator
{
public:
	explicit FrameAllocator(std::size_t bytesPerFrame)
		// Non-copyable, non-movable elements in an array are fine here: each
		// element is copy-initialised from a prvalue, which C++17 guarantees
		// constructs in place with no temporary to move from.
		: arenas{ Arena(bytesPerFrame), Arena(bytesPerFrame) }
	{
	}

	FrameAllocator(const FrameAllocator&) = delete;
	FrameAllocator& operator=(const FrameAllocator&) = delete;

	/**
	 * Advances to the next frame: what was current becomes previous, and the
	 * arena that held the older generation is cleared and becomes current.
	 *
	 * Call this once, at a fixed point in the frame, before anything has
	 * allocated. Calling it twice in a frame silently discards a generation --
	 * previous() would then return this frame's own partial data rather than
	 * last frame's.
	 */
	void nextFrame() noexcept
	{
		current_ = 1 - current_;
		arenas[current_].reset();
		++frames_;
	}

	Arena& current() noexcept { return arenas[current_]; }
	const Arena& current() const noexcept { return arenas[current_]; }

	// Last frame's arena, untouched for the duration of this frame.
	Arena& previous() noexcept { return arenas[1 - current_]; }
	const Arena& previous() const noexcept { return arenas[1 - current_]; }

	// Forwards to the current arena, so the common case reads as one object
	// rather than frames.current().try_allocate(...).
	void* try_allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) noexcept
	{
		return current().try_allocate(bytes, alignment);
	}

	template <class T, class... Args>
	T* create(Args&&... args)
	{
		return current().create<T>(std::forward<Args>(args)...);
	}

	template <class T>
	T* createArray(std::size_t count)
	{
		return current().createArray<T>(count);
	}

	// How many times nextFrame() has been called. Useful as the stamp on
	// cached data that must be rebuilt when the frame turns over.
	std::uint64_t frameIndex() const noexcept { return frames_; }

	std::size_t capacityPerFrame() const noexcept { return arenas[0].capacity(); }

	// The busiest either arena has ever been -- the number to size the pair
	// from, since either one can be the busy one.
	std::size_t highWaterMark() const noexcept
	{
		const std::size_t a = arenas[0].highWaterMark();
		const std::size_t b = arenas[1].highWaterMark();
		return a > b ? a : b;
	}

private:
	Arena arenas[2];
	int current_ = 0;
	std::uint64_t frames_ = 0;
};

#endif
