# Project Veil Release Notes — Oct 10 – Oct 17, 2026

This week the UI Engine landed on main and the first real menus were built on top of it. The main menu now has working Settings and Multiplayer screens, and a batch of scheduled code-scan fixes closed several crash risks around missing scenes. There are no breaking changes for players.

## New

### Multiplayer menu
The main menu has a new "Multiplayer" button that opens a full-screen Multiplayer panel with "Start Server" and "Join Server" buttons and a Return button. The panel covers most of the screen (80% in each direction), matching the new Settings screen. Be aware that Start Server and Join Server are placeholders and do nothing yet. They mark where the networking work (branch 86) will connect.

### Settings menu lists every setting
The Settings menu now loads every entry from the settings list and shows each one with its current value, for example "VSync: 1". Clicking an entry toggles it between on and off. Before, only a single hard-coded VSync button existed. New settings now appear in the menu automatically, with no UI work needed.

### UI Engine merged into main
The UI Engine refactor (#87) is merged. It adds panels, a layout stack, and widgets positioned as fractions of the screen, replacing the old stack of buttons. Buttons also gained `textWidth` and `textHeight` fields for finer control over label size. Players will mostly see this as more consistent menus at any window size.

## Improved

### Bigger, clearer menus
The Settings menu moved from a small box on the right of the screen to a large centered panel with a Return button at the top. The main menu panel was made taller to fit the new Multiplayer button. Opening or closing a menu also pauses and resumes the 2D scene, so the game does not keep running behind a menu.

### Cleaner UI code
The Return button logic was pulled into one shared helper used by both Settings and Multiplayer. Old unused button-placement code was deleted, and the duplicated draw code is gone. This keeps future menus small, since a new one is one panel plus a few button calls.

## Fixed

### Hidden buttons no longer react to the mouse
A widget's visibility setting was not being applied when drawing. Buttons that were hidden or scrolled out of their panel could still be hovered and clicked. Hover and click checks now run only for visible buttons, and visibility is judged against the parent panel rather than the whole screen.

### Crash guards in menus
Scheduled code scans added checks for a missing scene in the Return button, the Settings screen, the Multiplayer screen, and the main menu draw. Previously, opening one of these menus without an active scene could crash. Closing a menu now also only closes the menu that owns the button.

### Safer scene loading and minigames
An unknown minigame ID now falls through to a safe default case instead of undefined behavior. The scene load message is printed only for valid scene IDs, and stray whitespace and unused local variables were removed from the Maze minigame, the tutorial scene, and the placeholder scene callbacks.

## Breaking

None this week. Developers should note that Widget Text's `fontSize` was renamed to `textSize`.

---

*Source commits: #87, #90, #110, #111, #112, "Updated Settings Menu", "Added Multiplayer Menu", and the scheduled code-scan fixes (Oct 10 – Oct 16).*
