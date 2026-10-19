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
