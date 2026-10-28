#include "OverlayStack.h"

#include <algorithm>

const char* OverlayName(Overlay overlay)
{
	switch (overlay)
	{
	case Overlay::Title:       return "Title";
	case Overlay::Settings:    return "Settings";
	case Overlay::Multiplayer: return "Multiplayer";
	case Overlay::MiniGame:    return "MiniGame";
	}
	return "Unknown";
}

void OverlayStack::Open(Overlay overlay)
{
	// Each overlay is open at most once, so a button pressed twice can't stack
	// a second copy that one Return would then leave behind.
	if (IsOpen(overlay)) { return; }

	stack.push_back(overlay);
}

void OverlayStack::Close(Overlay overlay)
{
	// The Title belongs to the Main Menu scene and goes when the scene does
	if (overlay == Overlay::Title) { return; }

	std::erase(stack, overlay);
}

void OverlayStack::CloseAll()
{
	std::erase_if(stack, [](Overlay overlay) { return overlay != Overlay::Title; });
}

std::optional<Overlay> OverlayStack::BackPressed()
{
	if (stack.empty()) { return std::nullopt; }

	const Overlay top = stack.back();
	if (top == Overlay::Title) { return std::nullopt; }

	stack.pop_back();
	return top;
}

std::optional<Overlay> OverlayStack::Top() const
{
	if (stack.empty()) { return std::nullopt; }
	return stack.back();
}

bool OverlayStack::IsOpen(Overlay overlay) const
{
	return std::find(stack.begin(), stack.end(), overlay) != stack.end();
}

bool OverlayStack::GameplayHasInput() const
{
	return stack.empty();
}

bool OverlayStack::DimsWorld() const
{
	return !stack.empty() && stack.back() != Overlay::Title;
}
