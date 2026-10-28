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

**Pause**:
The Overlay that stops the game. In single-player it freezes the world; in multiplayer the shared world keeps running.
_Avoid_: menu

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
Emptying the whole world at once, before a new world is built or a save is loaded. Ids start over.
_Avoid_: reset, wipe

**World lighting**:
The lights, atmosphere, exposure and shadows a world is authored with, kept in its World save, so each scene is lit its own way. A Setting can change how it looks on screen, never what the world holds.
_Avoid_: lighting settings, graphics settings

### Saves

**Game save**:
A play session as it stood when the player left: the world, anything still in flight, and the player. Loading one Clears the world and Restores everything in it.
_Avoid_: save file, savegame

**World save**:
A level as authored in the World Editor: its objects and lighting, with no player and nothing in flight. Loading one replaces the world and leaves the player as they were.
_Avoid_: scene, map file, level file

### Settings

**Setting**:
One of the player's own choices about how the game plays, looks and sounds, shared by every scene and kept apart from any save. It adjusts how a world is shown or heard, never the world itself.
_Avoid_: option, preference, config
