# Subject Veil — Weekly Update Video Script
**Target length:** ~10-12 minutes narration
**Covers:** commits from 2026-09-19 through 2026-09-26

---

## INTRO (0:00–0:45)

[FOOTAGE: quick montage — Tutorial Scene opening, Stalker chasing the player down a hallway, a clean launch into the main menu]

Hey, welcome back. It's been a bug-hunting week on Subject Veil, and I want to walk you through what changed, because a couple of these fixes actually matter a lot more than "bug fix" makes them sound.

Quick shape of the week: one new scene got added, the game's main threat got its teeth back, and a bunch of crashes got squashed — including one that could kill either of the minigames outright. Let's get into it.

---

## CHANGE 1 — Tutorial Scene (0:45–3:00)

[FOOTAGE: player spawning into the new Tutorial Scene, walking through the intro beats, then transitioning into the main game]

First up, the new stuff. Subject Veil now has an actual Tutorial Scene. Up until now, if you started a new game, you got dropped straight into the world with zero onboarding — which is fine for me, because I already know how everything works, but it's not fine for anyone picking this up for the first time.

So there's now a dedicated scene that runs first, walks you through the core interactions and movement before you're on your own in the real level.

Funny story — this one almost didn't make it out the door this week. The build that added the Tutorial Scene shipped with a broken include that kept it from compiling, and on top of that there was a bug where the scene manager would just never actually route you into it — so functionally, the tutorial existed in the codebase but was completely unreachable. Both of those got caught and fixed within a couple of days, so as of right now it's in, it builds, and it plays start to finish.

[FOOTAGE: side-by-side or before/after showing the old cold-open into the world vs. the new tutorial intro]

---

## CHANGE 2 — The Stalker Deals Damage Again (3:00–6:30)

[FOOTAGE: Stalker catching the player, contact happening, health bar dropping]

Okay, this is the one I actually want you to pay attention to, because it's a big deal and it's a little embarrassing.

Somewhere in a previous refactor — when I moved collision handling over to a new component-based system — the code that actually applies the Stalker's damage when it touches you got silently dropped. No compile error, no test failure, nothing. It just... stopped happening. Which means for a stretch of time, the thing that's supposed to be actively hunting you and ending your run if it catches you was completely harmless on contact. You could just stand there and get bumped into all day.

[FOOTAGE: if possible, a clip from before the fix showing the Stalker making contact with no health change]

That's fixed now — contact damage is back, and it's back with actual regression tests behind it this time, so if a future refactor tries to quietly break this again, the test suite will catch it before it ships. I don't want to find out about this kind of thing from a comment that says "wait, is the Stalker supposed to do nothing?"

The lesson here, if you're building anything with a component-based architecture: moving logic between systems is exactly the kind of change that needs a test locking in the behavior, not just a visual check that it "looks right." It looked right. It just didn't do anything.

---

## CHANGE 3 — Crashes and Feel Fixes (6:30–9:30)

[FOOTAGE: Flappy Bird minigame crashing on an obstacle hit, then the same moment after the fix, surviving cleanly]

Rounding it out — a pass of crash fixes and feel fixes that stack up to a noticeably more stable build.

Both minigames, Flappy Bird and the Crane game, had a crash where hitting an obstacle or going out of bounds could bring the whole game down. Both are fixed now, so those minigames should just work.

While I was in there, I also found out Flappy Bird had a leftover debug call stacking extra gravity on top of the real gravity every single frame — so it's been playing heavier than it was actually tuned to. That's gone now, so the minigame should feel the way it was originally designed to feel.

[FOOTAGE: main menu loading cleanly on launch, no fade]

There's also a fix for something you'd have seen every single time you launched the game: a leftover pointer was making the game think it needed to fade in from the tutorial scene on startup, even on a completely fresh launch. Small thing, but it was on literally every boot, so it's nice to have gone.

[FOOTAGE: gamepad in hand, pressing interact/move without triggering an artifact action]

And if you play on a controller — the Interact and Move buttons were quietly double-booked with a separate set of Artifact actions, so pressing one could accidentally trigger the other. That's been reassigned, matching a fix I already made on keyboard a while back.

Plus a crash on interacting with artifact-unlock stations, some quietly-wrong colors in my own level editor's UI, and — under the hood, nothing you'll see directly — a chunk of new engine infrastructure around memory allocation that's going to make this exact class of "silent use-after-free" bug harder to write going forward. Some of it already caught a real bug before it shipped, which is exactly the point of building it.

---

## PERFORMANCE NOTE (9:30–10:15)

[FOOTAGE: FPS counter overlay in a busy scene, before/after if captured]

One more thing worth mentioning: the collision solver got a rewrite this week that measured close to 5x faster on a representative level — down from about 1.3 milliseconds to about a quarter of a millisecond per frame. You probably won't consciously notice it today, but it's headroom for busier levels down the line, and it's the kind of change that pays for itself the more stuff I put in a scene.

---

## CLOSE / CALL TO ACTION (10:15–11:00)

[FOOTAGE: title card, wishlist / follow links on screen]

So that's the week — one new scene, one very overdue damage fix, and a stack of crashes closed out. Nothing here is a breaking change, nothing changes your saves, this is all just the game getting more solid under you.

If you want to follow along as Subject Veil comes together, hit subscribe, and drop a comment if you've run into any of these bugs yourself — I want to hear about it either way. See you in the next one.

---

## SHOT LIST

- [FOOTAGE] Quick montage: Tutorial Scene opening, Stalker chase, clean main menu launch (intro)
- [FOOTAGE] Player spawning into new Tutorial Scene, walking through intro beats, transition to main game
- [FOOTAGE] Before/after: old cold-open into world vs. new tutorial intro
- [FOOTAGE] Stalker catching player, contact, health bar dropping
- [FOOTAGE] (if available) Pre-fix clip: Stalker contact with no health change
- [FOOTAGE] Flappy Bird crash on obstacle hit vs. post-fix surviving the same hit
- [FOOTAGE] Main menu loading cleanly on launch, no spurious fade
- [FOOTAGE] Gamepad in hand: Interact/Move not triggering Artifact actions
- [FOOTAGE] FPS counter overlay in a busy scene, before/after if capturable
- [FOOTAGE] Title card with subscribe / wishlist / follow links
