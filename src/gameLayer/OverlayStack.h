#pragma once
#ifndef OVERLAYSTACK_H
#define OVERLAYSTACK_H

#include <optional>
#include <vector>

/**
 * A 2D layer drawn over the world that takes player input away from gameplay
 * while it is open. See CONTEXT.md.
 */
enum class Overlay
{
	Title,
	Inventory,
	Settings,
	Multiplayer,
	MiniGame,
};

// Display name, for the World Editor
const char* OverlayName(Overlay overlay);

/**
 * Which Overlays are open, in the order they were opened, and the rules for
 * what each one does to the player's input.
 *
 * Holds state only: it draws nothing and reads no input device, so the scene
 * reports key presses to it and asks it what to draw.
 */
class OverlayStack
{
public:
	/** Commands **/
	// Opens an overlay on top of the others. Ignored if it is already open.
	void Open(Overlay overlay);

	// Closes an overlay wherever it is, so the one beneath it is on top again.
	// Ignored for the Title, which closes only with its scene.
	void Close(Overlay overlay);

	// Closes every overlay but the Title. For the World Editor.
	void CloseAll();

	// What the Inventory action (TAB) does. Returns the overlay it closed, if any.
	// The Title is never closed by it, and nothing opens over the Title.
	std::optional<Overlay> InventoryPressed();

	/** Queries **/
	// The overlay that was opened last and is still open, or none
	std::optional<Overlay> Top() const;

	bool IsOpen(Overlay overlay) const;

	// True only when no overlay is open
	bool GameplayHasInput() const;

	// True when an overlay other than a lone Title is open
	bool DimsWorld() const;

	// Every open overlay, bottom first. For the World Editor to show.
	const std::vector<Overlay>& OpenOverlays() const { return stack; }

private:
	std::vector<Overlay> stack; // Bottom first
};

#endif // OVERLAYSTACK_H
