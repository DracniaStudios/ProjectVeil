# YouTube Script — Project Veil Weekly Update (Oct 17, 2026)

**Target length:** ~10–12 minutes (about 1,600 spoken words at a relaxed pace). Bracketed notes are directions for the editor/presenter.
**Honest note for the presenter:** the Multiplayer menu is a shell, because its buttons do nothing yet. Say so on camera. The script frames it as the doorway to networking.

---

## COLD OPEN (0:00–0:30)
[B-roll: click Multiplayer on the main menu, the big panel opens, click Start Server, nothing happens]

Okay, I clicked Start Server and nothing happened. Perfect. That's exactly what's supposed to happen this week, and I'll explain why in a minute. Stick around, because this is the week Project Veil's menus finally started to feel like a real game.

## INTRO (0:30–1:30)
[On camera or voiceover with logo]

Hey everyone, welcome back to the Project Veil weekly update. Every week I walk you through what changed, so you can see how the game is coming together.

Last week I told you the UI Engine was getting its first round of testing. This week it's merged into main, and we've built two real screens on top of it: a Settings menu that knows about every setting in the game, and a brand new Multiplayer menu.

The plan is simple. Top three changes first, then the smaller fixes, then what's next. Let's go.

## TOP CHANGE #1: THE UI ENGINE LANDS (1:30–4:30)
[Screen capture: main menu at several window sizes]

Let's start with the foundation. The UI Engine is now merged into main.

Quick recap of why it exists. Every game needs menus, buttons, titles and health bars. You could place each one by hand, with numbers like "draw this button at pixel 400, 300." That works until someone plays on a different monitor, and then everything is in the wrong place.

So the UI Engine uses panels, and everything inside a panel is positioned as a fraction of the screen. A panel that is 80 percent wide is 80 percent wide on a laptop and on an ultrawide. Inside a panel, rows stack automatically, so adding a button is literally one more line of code.

[Screen capture: resize the window, menu scales]

This week buttons also gained text width and height controls, so we can size labels precisely. And we renamed a text property from "fontSize" to "textSize", so the names match what they do. That's a developer detail, but it will save us confusion later.

Why do you care? Because every menu, the HUD, the pause screen, will be built on this. Today's work is why the next menus will be quick to make.

## TOP CHANGE #2: THE SETTINGS MENU, FOR REAL (4:30–7:00)
[Screen capture: open Settings, toggle VSync]

Change number two is the Settings menu.

Before this week, Settings was a tiny box on the right side of the screen with one button: VSync, on or off. It worked, but it was a one-off.

Now it's a big centered panel with a Return button at the top. And here's the clever part. The menu doesn't have a hard-coded list of buttons anymore. It reads the game's settings list and builds a button for every setting, showing the name and the current value. Click one, and it toggles.

[Screen capture: show a setting changing from 1 to 0]

That means when we add a new setting, say a volume option or a fullscreen toggle, it appears in the menu automatically. Nobody has to touch the UI code. For a small team, that's the kind of design that pays off for months.

One honest caveat: right now every setting toggles between on and off. Sliders for things like volume are still to come.

Also, when a menu opens, the 2D scene behind it pauses, and it resumes when you hit Return. So the game doesn't run behind your back while you're in Settings.

## TOP CHANGE #3: THE MULTIPLAYER MENU (7:00–8:45)
[Screen capture: main menu, new Multiplayer button, then the panel]

And change number three, the one from the cold open: the Multiplayer menu.

The main menu has a new Multiplayer button. Click it, and you get a large panel with Start Server, Join Server, and Return. Return works. The other two are placeholders.

I want to be upfront about that. We are not shipping multiplayer this week. What we did is build the doorway. The menu exists, it opens and closes cleanly, and it shares the same Return logic as Settings. When the networking work lands, those two buttons are where it plugs in.

[On camera]

Why do it in this order? Because it's much easier to build networking when there is already a place in the game to start and join a session. Menu first, plumbing second.

## THE SMALLER STUFF: BUGS AND SAFETY (8:45–10:30)
[Screen capture: code scan commit list, or simple text slides]

Now the quick round of fixes. This is stuff you hopefully never notice, which is the point.

First: hidden buttons. There was a bug where a button that was hidden, or scrolled outside its panel, could still react to your mouse. Now the engine only checks hover and clicks on buttons you can actually see, and "visible" is judged against the panel, not the whole screen.

Second: crash guards. We run a scheduled code scan, and this week it found several places where a menu assumed a scene was loaded. The Return button, the Settings screen, the Multiplayer screen and the main menu draw all now check first. If there's no scene, they skip quietly instead of crashing.

Third: unknown minigame IDs. If the game is handed a minigame ID it doesn't recognize, it now lands in a safe default case instead of doing something undefined.

And a few cleanups: unused variables removed from the Maze minigame and the tutorial, and the scene-loaded message now only prints for valid scenes, so the console is easier to read.

None of that is flashy, but together it makes the game steadier.

## WHAT'S NEXT (10:30–11:15)
[On camera]

Next week, the plan is to start wiring up the Multiplayer buttons, since the menu is ready for them. We also want to expand Settings beyond on/off toggles, and begin the player HUD on the same UI Engine.

## CALL TO ACTION (11:15–11:45)
[On camera, end card]

If you enjoyed this, hit like, because it helps more people find the project. Subscribe and turn on notifications, so you catch the weekly update. And tell me in the comments: what setting would you want in the menu first? Volume, controls, graphics? I read every comment.

Thanks for watching. See you next week.

[End card: subscribe button, previous episode]
