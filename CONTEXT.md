# Project Veil

A first-person horror game with an in-engine World Editor. This glossary gives each game concept one name, so code, docs and agents use the same word for it.

## Language

### Screen and input

**Overlay**:
A 2D layer drawn over the world that takes player input away from gameplay while it is open, such as a menu or a running MiniGame. Overlays stack: closing one returns to the one beneath it.
_Avoid_: 2D mode, 2D active, menu mode

**Main Menu**:
The scene the game starts in. It opens the Title overlay when it is built, and its world keeps running behind it.
_Avoid_: title, title screen

**Title**:
The Overlay holding Play, Multiplayer, Settings and Exit, opened by the Main Menu scene. It closes only when the scene changes.
_Avoid_: main menu (that is the scene)

**Inventory**:
The Overlay the player opens during gameplay to look at what they carry. The world keeps running while it is open, so it is never a safe place to hide.
_Avoid_: pause, pause menu

**Pause**:
The Overlay that stops the game. In single-player it freezes the world; in multiplayer the shared world keeps running.
_Avoid_: inventory, menu

### MiniGames

**MiniGame**:
A short 2D game the player plays inside an Overlay. Each round ends Won or Lost; a Lost round restarts fresh, so only the player can walk away from one.
_Avoid_: mini game, minigame overlay

**Task station**:
An interactable in the world that starts a MiniGame. Winning there completes the station and toggles its Target.
_Avoid_: terminal, machine

**Target**:
The door or other object a Task station's MiniGame enables or disables when won. A station with no Target toggles itself.
_Avoid_: activator

**Practice run**:
A MiniGame started from the Artifact instead of a Task station. Winning grants the game's buff but completes nothing and toggles nothing.
_Avoid_: replay

### World

**Spawn**:
Adding a new object to the world, made from a prototype. It gets a new id and starts enabled, with full health; the prototype is left as it was.
_Avoid_: create, add, save (Save writes to disk)

**Restore**:
Putting an object back into the world exactly as it was saved, under its saved id. Nothing is reset: a door saved shut stays shut, and a hurt Stalker stays hurt.
_Avoid_: load (that is the whole save file), respawn

**Destroy**:
Taking an object out of the world. It leaves at the end of the frame, never partway through one.
_Avoid_: delete, erase, remove

**Clear**:
Emptying the whole world at once, before a new world is built or a save is loaded. Ids start over, so the player's inventory empties with it.
_Avoid_: reset, wipe
