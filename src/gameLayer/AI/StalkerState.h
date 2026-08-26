#pragma once
#ifndef STALKER_STATE_H
#define STALKER_STATE_H

/**
 * The stalker's four states, exactly as locked in the design.
 *
 * patrol -> investigate -> hunt -> search-last-known-position
 *
 * Changing this set is not a code decision. It is one of the four locked
 * decisions on the Vertical slice milestone and escalates before any work
 * starts.
 */
enum StalkerState
{
	STALKER_PATROL,
	STALKER_INVESTIGATE,
	STALKER_HUNT,
	STALKER_SEARCH_LAST_KNOWN,
	STALKER_STATE_COUNT
};

inline const char* stalkerStateToString(int state)
{
	switch (state)
	{
	case STALKER_PATROL:            return "Patrol";
	case STALKER_INVESTIGATE:       return "Investigate";
	case STALKER_HUNT:              return "Hunt";
	case STALKER_SEARCH_LAST_KNOWN: return "Search Last Known";
	default:                        return "Unknown";
	}
}

#endif
