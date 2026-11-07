# YouTube Script — Subject Veil Dev Update (Nov 7, 2026)

**Target length:** 10–12 minutes (~1,600 words at a relaxed pace)
**Working title:** "I Rebuilt My Horror Game's Foundation So You Can Play It Together"

---

## [0:00] Cold open

*(B-roll: Stalker chase, then the jumpscare overlay.)*

That scream you just heard? That's new. And if you stick around, I'll show you how a jumpscare led me down a week-long
rabbit hole that ended with my horror game running a server.

## [0:30] Intro

Hey everyone, welcome back to the Subject Veil dev update. This week I made 28 commits, and most were about one thing:
making this game something you can eventually play with friends. I'll cover the jumpscare, then the big one, multiplayer
foundations, then the quieter changes that make it all hold together. And at the end I'll ask for your help.

## [1:30] Top change 1 — The Jumpscare and the new Stalker

*(Screen capture: Stalker catches player.)*

Until now, being caught by the Stalker wasn't dramatic enough. This week there's a proper Jumpscare overlay when it
attacks. The Pause overlay and the jumpscare both run through the overlay stack, so they don't fight each other.

The Stalker itself got smarter. It now moves into Hunt after a Chase, so if you break line of sight, it doesn't just give
up. It searches. The AI is now four clear states, and there's a fallback for when it can't find a path to you.

I also fixed a bug where the sound you make followed your velocity rather than the direction you were moving. The Stalker
hears what you *intend*, not what physics happened to do that frame. It feels fairer and easier to learn.

And there's audio now: warnings as the Stalker changes state, and a sound when you activate a station. In a horror game,
what you hear is half the fear.

## [4:00] Top change 2 — Multiplayer: Host and Join

*(Screen capture: dev network window, two capsules moving.)*

Here's the headline. Subject Veil now has a Host and Join flow. One player starts a server inside their own game and
plays on it. Others connect by address. Right now the other players show up as capsules in a dev window. It's not pretty,
but it's real data over a real network.

The more important part is how I got there. Rather than bolt multiplayer on at the end, I made single-player use the same
path. When you play alone, the game runs a server and a client in one process and talks to itself through a "loopback."
So there's only one version of the rules. If it works alone, it works together.

Under the hood: the game ticks at a fixed 60 times a second regardless of your frame rate. Your input becomes a small
command. Players are known by a Slot number, so the game never assumes "the" player. I can fake bad network conditions to
test lag. It's all behind a transport interface, so if I switch networking libraries later, the game doesn't care.

## [6:30] Top change 3 — Rounds that remember

*(Screen capture: station activation, fail screen.)*

Multiplayer needs rules about winning and losing. Rounds have fail and win states, and catches are tracked per player. If
you're caught, there's a full wipe: everyone back to spawn, stations reset, the Stalker's memory cleared, back to Patrol.

And the game can remember. A Host stores which stations are completed in a SpacetimeDB database, so a world's progress
persists between sessions. Single-player still uses plain save files. The database is there for hosting.

## [8:00] Under the hood, quickly

A few smaller things deserve a mention.

The map now stores its data in an ECS library called EnTT. I tried Flecs too, and removed it. Simpler is better.

There's a new resource path system, so the game finds its files and saves reliably no matter where you launch it from.

Big assets like audio banks and textures now live in Git LFS, which keeps the repo lean. If you clone it, install Git LFS
first or you'll get tiny placeholder files. That's the one change that can bite contributors.

I also fixed a regression where Stalker catches could stop registering, plus a crash when rendering with no scene loaded,
and cleared out a pile of compiler warnings. And there are new automated tests covering the network protocol, the
transport, the world store and the Stalker's brain.

## [9:30] What's next

Next up is making multiplayer feel like a game rather than a debug window: real player models instead of capsules,
a proper lobby, and stress-testing the Stalker with more than one target. Protocol v1 isn't frozen yet, so expect
changes.

## [10:30] Call to action

If you want to follow along, subscribe and hit the bell, because next time I'll show what two players being hunted
together looks like.

Tell me in the comments: would you rather play this with friends, or alone in the dark? I read every answer.

If you're a developer, tell me how you'd handle the Stalker's search behavior. And if you enjoyed this, a like helps
more than you'd think.

Thanks for watching. See you next week. Stay out of the dark.

*(End card.)*
