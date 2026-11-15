You are the lead on a three-part engineering review of Project Veil and its engine, BlueJay, in this repo. You orchestrate. Opus 5.5 agents do the reviews and own the workstreams. Sonnet agents write the code. I'm the solo developer, and I review every diff before it merges.

## Context
- Veil is a C++23 horror game currently built on raylib. BlueJay is its engine. Targets: Windows and Linux, 1080p at 60fps.
- Near-term milestone: a vertical slice (one room, one task station, one stalker enemy with sound-only perception). Its definition of done must not change.
- These decisions are already made. Treat them as fixed. If the code gives you evidence one is wrong, report it and stop; don't work around it.
  1. raylib is replaced by SDL3 plus an OpenGL renderer. The swap is time-boxed, and raylib must keep building as the fallback. No Vulkan yet.
  2. The ECS migration comes after the SDL3 swap: GameObjects and Components first, then engine-wide. EnTT sits behind the existing Entity API, and callers never see EnTT types. A deliberate hybrid is an acceptable stopping point.
  3. A simulation seam separates game simulation from rendering, input, and audio, so a headless dedicated server can run the same simulation later. Networking will use GameNetworkingSockets. Don't build networking in this pass.
  4. C++ only. No new third-party dependencies beyond SDL3 and EnTT.

## Phase 0: Map the repo (you, no delegation)
Read the codebase yourself before delegating anything. Write docs/review/00-map.md containing: the module inventory and dependencies; every raylib touchpoint grouped by category (window, input, rendering, audio, math, timing, file I/O); the Entity API surface and its callers; the build system and the test and smoke-test commands. Every later agent receives this file.

## Phase 1: Three reviews in parallel (Opus 5.5, read-only)
Spawn three agents with model "opus". Confirm that resolves to Opus 5.5. If it doesn't, pass the full model ID or tell me. Each agent is read-only on source and writes only under docs/review/.
- A. Architecture: module boundaries, ownership and lifetimes, where simulation and presentation are tangled, what blocks a headless build, fixed-timestep and determinism risks.
- B. SDL3 swap: the platform and renderer interface that replaces raylib, the OpenGL renderer scope the slice actually needs, the migration order, how raylib stays buildable behind a compile option, and what could blow the time box.
- C. ECS: how the current GameObject/Component model maps onto EnTT behind the Entity API, the migration order, safe hybrid stopping points, and memory and performance effects.

Each agent delivers three things: findings ranked by severity with file and line evidence; one decision record; and a list of work orders. A work order names the goal, the files it touches, the interfaces or signatures to end up with, the commands that prove it is done, and what is out of scope. Size each work order for one reviewable diff, roughly under 400 changed lines.

## Phase 2: One plan, then stop (you)
Check the agents' key claims against the code; don't relay them unverified. Resolve conflicts between the three reviews, especially where B and C touch the same rendering components. Merge everything into docs/review/PLAN.md: ordered work orders, their dependencies, which can run in parallel, and a cut line separating what the vertical slice needs from what can wait.

Then stop and show me the plan. Write no code until I approve it, and execute only the work orders I approve.

## Phase 3: Implementation (Opus 5.5 owns, Sonnet writes)
For each approved workstream, spawn one Opus 5.5 agent as owner. The owner spawns Sonnet agents (model "sonnet"), one work order each, each in its own git worktree on a branch named review/<stream>/<order-id>.
- Each Sonnet agent gets its work order, 00-map.md, and the relevant decision record. It implements only that order, builds, runs the tests and the smoke test, and returns the diff with the command output.
- The owner reviews each diff against the work order and the decision record. It sends back anything off-spec, up to two rounds, then escalates to you.
- Work orders that touch the same files run in sequence, not in parallel.
- If an Opus agent can't spawn its own agents in this environment, have it return its work orders, spawn the Sonnet agents yourself, and send each diff back to the owner for review.

## Phase 4: Verify and report (you)
On each branch, confirm: the Linux and Windows builds pass, the headless smoke test passes, the raylib fallback still builds, and gameplay behavior is unchanged. Report per branch: what changed, what you verified and how, what failed or was skipped, and the order I should review and merge in.

## Rules for every agent
- Never commit to main, merge, push, or force anything. Branches only. Merging is mine.
- These are structural changes. Don't change gameplay behavior.
- Don't write player-facing content (text, audio, art).
- If something is ambiguous and reversible, take the conservative reading and note it. If it contradicts a fixed decision or can't be undone, stop and ask me.