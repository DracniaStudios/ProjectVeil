# Project Veil — Release Notes (Oct 24 – Oct 31, 2026)

Build version in Changelog.txt: 0.4.24 (next milestone due 2026-11-06).

## New

- **Pause menu.** Pressing the new Pause input during gameplay opens a Pause overlay. It offers a Return button, a Settings button that opens the settings panel on top of the pause screen, and an "Exit To Main Menu" button. This closes the gap noted in the previous build, where there was no in-game way back to the Main Menu.
- **Frame Rate setting.** Display settings now include a Frame Rate choice (30, 60, 120, 240, 360). The game also sets its target frame rate at startup rather than before the window opens, and the delta-time cap now follows the 60 FPS target instead of a hard-coded 30 FPS value.
- **Window size limits.** The game window stays resizable but now has a minimum size of 1280x720 and a maximum of 3840x2160.
- **Stalker catches the player.** When the Stalker reaches the player it now registers contact. It logs the hit, switches to the tutorial scene, and disables both the player and itself. This is placeholder behaviour until a proper death or damage flow exists.
- **Community docs.** A Code of Conduct was added to the repository.

## Improved

- **Overlay stack replaces the single 2D flag.** Title, Inventory, Settings, Multiplayer, MiniGame and Pause are tracked as overlays, each open at most once. TAB opens the Inventory in gameplay, backs out of the top overlay otherwise, and quits a running minigame. The Main Menu now locks camera and movement, so the player no longer looks around behind it. The cursor locks during gameplay and frees for any overlay or the World Editor.
- **MiniGames run through a session.** All six minigames now report Running, Won or Lost to a MiniGameSession instead of freeing themselves mid-update. A lost round restarts at the same station, and a win applies its buff (Doctor also heals) and completes the task station. A Practice run from the Artifact grants only the buff and no longer toggles the last station's door.
- **World Editor.** Task stations get a "MiniGame Target" field with Pick Target and Clear Target. The selected target is outlined in its own colour. The MiniGame Inspector gains Launch, Win, Retry and Quit buttons and a live cursor readout. Editor panels show the overlay stack and a Close All Overlays button.
- **Consistent world loading.** One lifecycle (Spawn, Restore, Destroy, Clear) now covers every object kind. Saved objects come back in their saved state and a damaged Stalker no longer returns at full health. Destruction is deferred to the end of the frame. Loading a game or world and creating a new world all clear the world the same way.
- **Codebase tidy-up.** Engine-layer source was reorganised into folders (Assets, Audio, Editor, InputSystem, Lighting, Memory, ParticleSystem, Physics3D, Save, SceneManagement). About 400 files of unused raylib sample projects were removed from the repo, and the README project structure was refreshed.

## Fixed

- **Settings and overlay desync.** Opening Settings, pressing TAB, then opening Settings again used to leave a dimmed screen with no menu. The overlay stack removes this.
- **Reticle.** The crosshair draws again. It was previously only drawn in cases where it could not be seen.
- **Interact raycast.** A stray semicolon meant disabled or non-interactable objects were not actually skipped. They are now skipped, so the crosshair only targets valid objects.
- **Stalker attack logic.** The contact check originally sat in the Chase state, but the Kill state takes over once the Stalker is in attack range, so contact could never register. The check now lives in the Kill state. The Stalker also null-checks the player to avoid crashes.
- **Doors and saved state.** A door saved as disabled no longer comes back enabled after loading.
- **FlappyBird.** The "left the green area" check now fires correctly.
- **Compiler warnings.** Fixed a nested comment opener in Settings.cpp, missing initializers, unused functions and variables, and seed narrowing in minigames.

## Breaking

- **Save files.** Saves no longer write Is2DActive or IsMiniActive, and the player's 2D entry is removed. Older saves still load. Task stations now save a "TargetId", and stations from older saves take their target from "Activator".
- **Settings file.** `saves/settings.json` gained the FrameRate entry, and a default save file was added to `saves/`.
- **Project layout.** Engine-layer headers moved to subfolders, so any local branches or tools that include old paths (e.g. `engineLayer/AssetManager.h`) need updating.
- **Removed APIs.** `Scene::is2DActive`, `player.interactObjectId`, `Scene::ResetID` and `ResetGameObjectIds` are gone.

## Known issues

- Clicking inside the game while an overlay is open has not been finished.
- The Stalker contact response is a placeholder.
