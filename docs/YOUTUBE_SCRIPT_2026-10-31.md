# Project Veil — Dev Update Script (target 10–12 min, ~1,500 words)

*[Spoken narration. Bracketed lines are on-screen directions.]*

## Intro (0:00–1:00)

[Footage: Stalker chasing the player in a dark corridor]

Hey everyone, and welcome back to the Project Veil dev update. This was a big week. You can now pause the game, you can finally get back to the main menu without closing it, the Stalker can actually catch you, and a lot of the code under the hood got rebuilt so we can add things faster from here on.

I'll walk through the three changes you'll notice most, then cover everything else more quickly. Let's get into it.

## Top change 1: the Pause menu (1:00–4:00)

[Footage: pressing the Pause key mid-game]

Until now, if you were playing Project Veil and wanted to stop, you had a problem. There was no way back to the main menu. You had to close the game.

That's fixed. Press the new Pause input in gameplay and the game dims behind a Pause menu. You get three options. Return takes you back to playing. Settings opens the settings panel on top of the pause screen. Exit To Main Menu does what it says.

[Footage: open Settings from Pause, close it, back to Pause]

This works because of a bigger change underneath. The game used to run on one "is the 2D screen active" flag that meant pause, minigame and menu all at once. That caused a nasty bug. You could open Settings, press TAB, open Settings again, and end up with a dimmed screen and no menu. Now every screen is a layer in a stack. Title, Inventory, Settings, Multiplayer, MiniGame and Pause can each be open once, and the game always knows which one is on top.

[Footage: TAB toggling Inventory, backing out of overlays]

TAB opens your Inventory during gameplay. If a menu is open, it backs out one layer. If you're in a minigame, it quits. The cursor also locks when you're playing and frees up for any menu. And behind the main menu you no longer walk around or look around. The crosshair, which was accidentally never showing, is back too.

## Top change 2: the Stalker gets you (4:00–6:30)

[Footage: Stalker patrols, spots player, chases]

The Stalker already patrolled, spotted you, chased you and searched where it last saw you. This week it learned to finish the job.

When it reaches you, the game registers contact. Right now that sends you to the tutorial scene and shuts down both you and the Stalker. It's a placeholder. A real damage and death flow is coming.

There's a good bug story here. The first version of the contact check was in the Chase state. But the game switches the Stalker into its Kill state once you're within attack range, and that range sits inside its sight range. So by the time it was close enough to hit you, it was no longer running the code that checks for a hit. We moved the check into the Kill state, and we added a safety check so the Stalker doesn't crash if the player doesn't exist yet.

## Top change 3: minigames and the world editor (6:30–8:30)

[Footage: playing Maze, Crane, Simon Says]

All six minigames got rebuilt around a session system. Before, a minigame ended by freeing itself while it was still running, which was a recipe for crashes. Now a game just reports "running," "won," or "lost," and the session handles the rest. Lose and you restart at the same station. Win and you get the buff, and the task station completes. If you start a practice run from the Artifact, you only get the buff and it no longer opens the last door you touched.

[Footage: World Editor, picking a target]

For level designers, task stations now have a MiniGame Target. Click Pick Target, click the thing you want to toggle, and it's outlined in its own colour. There are also Launch, Win, Retry and Quit buttons in the minigame inspector so you can test without playing through.

## Everything else, quickly (8:30–10:30)

[Footage: settings screen]

- **Frame rate.** There's a new Frame Rate option in Display settings, from 30 up to 360. The game targets 60 FPS by default.
- **Window size.** You can still resize the window, but it now stays between 1280x720 and 3840x2160.
- **Saved worlds.** Loading is more reliable. Doors saved as disabled stay disabled, and a damaged Stalker comes back damaged, not at full health.
- **Interaction.** A tiny typo, a stray semicolon, meant the interact check didn't skip objects it should have. Fixed.
- **FlappyBird.** Its out-of-bounds check works again.
- **Code cleanup.** The engine folder is now organised into clear sections, about 400 unused sample files were removed, and a pile of compiler warnings are gone.
- **Community.** We added a Code of Conduct to the repo.

## Heads-up on compatibility (10:30–11:00)

If you have older saves, they still load. The game just stops saving some old 2D flags and adds a target ID for stations. If you're modding or working on a branch, file paths in the engine folder moved, so you'll need to update your includes.

## What's still rough (11:00–11:30)

Clicking in-game while a menu is open isn't finished, and the Stalker's catch response is a placeholder. Those are next.

## Call to action (11:30–12:00)

That's the update. If you want to follow along, subscribe so you catch the next one, and the next milestone is due November 6th. Tell us in the comments what you want from the Stalker's catch: a jump scare, a game over screen, or something nastier. If you want to try the game or check out the code, the link's in the description. Thanks for watching, and see you next week.
