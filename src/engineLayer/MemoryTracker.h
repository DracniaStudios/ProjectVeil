#pragma once
#ifndef MEMORY_TRACKER_H
#define MEMORY_TRACKER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

/**
 * Answers "what is using my memory?" by subsystem, with budgets.
 *
 * Every allocator in this folder reports its own occupancy -- Arena::used(),
 * PoolAllocator::liveCount(), SlotMap::size() -- but each only knows about
 * itself. The question that actually comes up during a long session is the
 * aggregate one: which subsystem grew, and is it over what was budgeted for
 * it? That needs somewhere for the individual allocators to report to.
 *
 * Granularity is deliberately coarse. This records blocks -- one arena, one
 * pool, one slot map's storage -- not individual allocations. Recording a bump
 * allocation would cost more than the bump. Register the block when it is
 * created and the tracker stays free on the frame path:
 *
 *     Arena levelArena(8 * 1024 * 1024);
 *     MemoryTracker::Registration reg(GlobalMemoryTracker(),
 *                                     MemoryTag::Level, levelArena.capacity());
 *     // ... reg's destructor removes it when the arena goes
 *
 * What this is NOT: a complete account of the process. Unreal's LLM can claim
 * its categories sum to the total because it owns the global allocator; this
 * only ever sees what is handed to it. Anything going through plain new/malloc
 * is invisible, and so is everything raylib allocates for textures, models and
 * sounds -- raylib routes those through its own RL_MALLOC, and GLFW and the
 * graphics driver do not route through anything you control. Read the numbers
 * as "what the engine's own allocators hold", not as RSS.
 *
 * Counters are atomic because registration can happen off the main thread
 * (asset loading, a job system later). They are relaxed: each counter is
 * independent and nothing here orders other memory, so a report taken while
 * another thread is registering may catch a tag mid-update. That is the right
 * trade for a debug facility -- correct totals, no synchronisation on the
 * paths being measured.
 */
enum class MemoryTag : std::uint8_t
{
	Unknown = 0,
	Level,        // world/room data, freed on unload
	Frame,        // per-frame scratch
	Entities,     // entity and component storage
	Particles,
	Physics,      // solver scratch, broad-phase views, contacts
	AI,           // pathfinding, perception, director state
	Audio,        // voices, streaming buffers
	Rendering,    // command lists, sort keys, transient geometry
	Assets,       // CPU-side asset data (not raylib's GPU handles)
	SaveLoad,     // serialisation scratch
	UI,
	Scratch,      // short-lived general temporaries

	Count
};

inline const char* ToString(MemoryTag tag) noexcept
{
	switch (tag)
	{
	case MemoryTag::Unknown:   return "Unknown";
	case MemoryTag::Level:     return "Level";
	case MemoryTag::Frame:     return "Frame";
	case MemoryTag::Entities:  return "Entities";
	case MemoryTag::Particles: return "Particles";
	case MemoryTag::Physics:   return "Physics";
	case MemoryTag::AI:        return "AI";
	case MemoryTag::Audio:     return "Audio";
	case MemoryTag::Rendering: return "Rendering";
	case MemoryTag::Assets:    return "Assets";
	case MemoryTag::SaveLoad:  return "SaveLoad";
	case MemoryTag::UI:        return "UI";
	case MemoryTag::Scratch:   return "Scratch";
	case MemoryTag::Count:     break;
	}
	return "Invalid";
}

class MemoryTracker
{
public:
	static constexpr std::size_t TagCount = static_cast<std::size_t>(MemoryTag::Count);

	MemoryTracker() = default;

	MemoryTracker(const MemoryTracker&) = delete;
	MemoryTracker& operator=(const MemoryTracker&) = delete;

	// Records a block of `bytes` against a tag.
	void add(MemoryTag tag, std::size_t bytes) noexcept
	{
		Entry& entry = entryFor(tag);

		const std::size_t now = entry.live.fetch_add(bytes, std::memory_order_relaxed) + bytes;
		entry.blocks.fetch_add(1, std::memory_order_relaxed);
		entry.totalAdded.fetch_add(bytes, std::memory_order_relaxed);

		// Raise the peak only if this reading beats it. The loop re-reads on
		// contention rather than assuming the compare succeeded, and stops as
		// soon as another thread has already recorded a higher peak.
		std::size_t seen = entry.peak.load(std::memory_order_relaxed);
		while (now > seen &&
			!entry.peak.compare_exchange_weak(seen, now, std::memory_order_relaxed))
		{
		}
	}

	// Gives a block back. Removing more than is live clamps at zero rather
	// than wrapping to an enormous number that would make every later reading
	// nonsense.
	void remove(MemoryTag tag, std::size_t bytes) noexcept
	{
		Entry& entry = entryFor(tag);

		std::size_t current = entry.live.load(std::memory_order_relaxed);
		std::size_t next;
		do
		{
			next = bytes > current ? 0 : current - bytes;
		} while (!entry.live.compare_exchange_weak(current, next, std::memory_order_relaxed));

		std::size_t blocks = entry.blocks.load(std::memory_order_relaxed);
		while (blocks > 0 &&
			!entry.blocks.compare_exchange_weak(blocks, blocks - 1, std::memory_order_relaxed))
		{
		}
	}

	// A budget of 0 means unbudgeted, which is the default and never reports
	// as over.
	void setBudget(MemoryTag tag, std::size_t bytes) noexcept
	{
		entryFor(tag).budget.store(bytes, std::memory_order_relaxed);
	}

	std::size_t live(MemoryTag tag) const noexcept { return entryFor(tag).live.load(std::memory_order_relaxed); }
	std::size_t peak(MemoryTag tag) const noexcept { return entryFor(tag).peak.load(std::memory_order_relaxed); }
	std::size_t blocks(MemoryTag tag) const noexcept { return entryFor(tag).blocks.load(std::memory_order_relaxed); }
	std::size_t budget(MemoryTag tag) const noexcept { return entryFor(tag).budget.load(std::memory_order_relaxed); }

	bool overBudget(MemoryTag tag) const noexcept
	{
		const std::size_t limit = budget(tag);
		return limit != 0 && live(tag) > limit;
	}

	std::size_t liveTotal() const noexcept
	{
		std::size_t total = 0;
		for (const Entry& entry : entries) { total += entry.live.load(std::memory_order_relaxed); }
		return total;
	}

	std::size_t peakTotal() const noexcept
	{
		std::size_t total = 0;
		for (const Entry& entry : entries) { total += entry.peak.load(std::memory_order_relaxed); }
		return total;
	}

	// Clears counters and budgets alike -- for tests, and for starting a fresh
	// measurement mid-session.
	void reset() noexcept
	{
		for (Entry& entry : entries)
		{
			entry.live.store(0, std::memory_order_relaxed);
			entry.peak.store(0, std::memory_order_relaxed);
			entry.blocks.store(0, std::memory_order_relaxed);
			entry.totalAdded.store(0, std::memory_order_relaxed);
			entry.budget.store(0, std::memory_order_relaxed);
		}
	}

	// One line per tag that has ever held anything, for the developer overlay
	// or a log at shutdown. Tags that were never used are left out so the
	// interesting ones are not buried.
	std::string report() const
	{
		std::string out = "tag          live       peak     blocks  budget\n";

		for (std::size_t i = 0; i < TagCount; ++i)
		{
			const MemoryTag tag = static_cast<MemoryTag>(i);
			const Entry& entry = entries[i];

			const std::size_t peakBytes = entry.peak.load(std::memory_order_relaxed);
			const std::size_t budgetBytes = entry.budget.load(std::memory_order_relaxed);
			if (peakBytes == 0 && budgetBytes == 0) { continue; }

			char line[160];
			std::snprintf(line, sizeof(line), "%-10s %9zu %10zu %10zu %7zu%s\n",
				ToString(tag),
				entry.live.load(std::memory_order_relaxed),
				peakBytes,
				entry.blocks.load(std::memory_order_relaxed),
				budgetBytes,
				overBudget(tag) ? "  OVER" : "");

			out += line;
		}

		return out;
	}

	/**
	 * RAII registration: reports a block on construction and gives it back on
	 * destruction.
	 *
	 * Keeping the pair together is the point -- a manual add()/remove() pair
	 * leaks a tag's worth of bytes down every early return, and a tracker that
	 * drifts is worse than none, because the number still looks authoritative.
	 */
	class Registration
	{
	public:
		Registration() = default;

		Registration(MemoryTracker& target, MemoryTag tag, std::size_t bytes) noexcept
			: tracker(&target), taggedAs(tag), size(bytes)
		{
			tracker->add(taggedAs, size);
		}

		~Registration() { release(); }

		Registration(const Registration&) = delete;
		Registration& operator=(const Registration&) = delete;

		Registration(Registration&& other) noexcept
			: tracker(other.tracker), taggedAs(other.taggedAs), size(other.size)
		{
			other.tracker = nullptr;
		}

		Registration& operator=(Registration&& other) noexcept
		{
			if (this != &other)
			{
				release();
				tracker = other.tracker;
				taggedAs = other.taggedAs;
				size = other.size;
				other.tracker = nullptr;
			}
			return *this;
		}

		// Reports the block back early. Safe to call more than once.
		void release() noexcept
		{
			if (tracker)
			{
				tracker->remove(taggedAs, size);
				tracker = nullptr;
			}
		}

		bool active() const noexcept { return tracker != nullptr; }
		std::size_t bytes() const noexcept { return size; }
		MemoryTag tag() const noexcept { return taggedAs; }

	private:
		MemoryTracker* tracker = nullptr;
		MemoryTag taggedAs = MemoryTag::Unknown;
		std::size_t size = 0;
	};

private:
	struct Entry
	{
		std::atomic<std::size_t> live{ 0 };
		std::atomic<std::size_t> peak{ 0 };
		std::atomic<std::size_t> blocks{ 0 };
		std::atomic<std::size_t> totalAdded{ 0 };
		std::atomic<std::size_t> budget{ 0 };
	};

	// An out-of-range tag lands on Unknown rather than reading past the array.
	Entry& entryFor(MemoryTag tag) noexcept
	{
		const std::size_t index = static_cast<std::size_t>(tag);
		return entries[index < TagCount ? index : 0];
	}

	const Entry& entryFor(MemoryTag tag) const noexcept
	{
		const std::size_t index = static_cast<std::size_t>(tag);
		return entries[index < TagCount ? index : 0];
	}

	Entry entries[TagCount];
};

// The process-wide tracker. A function-local static rather than a namespace
// one so it is constructed on first use, matching how Scene.cpp holds the
// collision solver's arena.
inline MemoryTracker& GlobalMemoryTracker() noexcept
{
	static MemoryTracker tracker;
	return tracker;
}

#endif
