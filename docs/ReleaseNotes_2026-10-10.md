# Project Veil Release Notes — Oct 3 – Oct 10, 2026

This week was a polish-and-stability week. The big theme is the new UI Engine, which got its first round of fixes, plus a handful of crash-prevention changes under the hood. There are no breaking changes.

## New

### Positionable buttons in the UI Engine
Buttons can now be placed anywhere on screen by passing a rectangle expressed as fractions of the screen (0 to 1), instead of being stacked automatically down the middle. Because the position is a percentage, a button lands in the same relative spot at any resolution or window size. This is the groundwork for the real main menu and the player HUD, where we need buttons in specific places rather than one long column.

### UI Engine roadmap
The Changelog now spells out the UI Engine plan: a central engine that owns every UI element, a PlayerUI for in-game elements, several widget types, and percent-based layout. This is a planning update, not a shipped feature, but it shows where the menu and HUD work is heading.

## Improved

### Main menu title placement
The "Main Menu" title was being drawn in the wrong spot, low and to the left of where it belonged. It now sits near the top of the screen, above the Play button. The text-drawing code also now handles pixel and percentage units separately, so text sizes consistently wherever it is placed.

### Clearer failure messages when a world won't load
If the main menu or tutorial world fails to load, the game now prints a clear message naming the world instead of silently continuing with an empty scene. This should make missing or corrupted save files far easier to spot. Players will mostly benefit indirectly, through faster diagnosis of "why is my level empty?" reports.

## Fixed

### Buttons and titles no longer risk being drawn twice
The UI Engine's render pass used to replay its widget list, which re-created widgets while looping over that same list. That could crash and could draw every widget twice. Widgets are now drawn once, when the scene draws them, so menus are both safer and cheaper to render.

### Player visible outside of minigames
The player's 2D rendering was accidentally tied to the minigame check, so the player was only drawn while a minigame was active. The player now renders every frame as intended.

### Safer start-up and shutdown
Several memory-safety issues found by the scheduled code scan were fixed. The scene manager now stops with a clear error if it cannot allocate its transition object, rather than crashing later on a null pointer. Game objects now have a virtual destructor, so entities such as the player and enemies are cleaned up correctly when released through a base pointer. Missing includes and an undefined-behavior overload in the UI code were also corrected.

## Breaking

None this week.

---

*Source commits: #78, #83, #85, #88, #89 and the UIEngine planning commit (Oct 3 – Oct 9).*
