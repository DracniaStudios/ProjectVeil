---
name: veil-sync
description: Reconciles the Project Veil Tasks tracker in Notion against the repo's git history. Use when I ask to sync the tracker, update task status from commits, check what's actually done, reconcile the board, or ask what's changed since the last sync — and at the start of every Monday Plan and Friday Retro.
---

# Tracker sync — commits to the Tasks board

Produces a **proposed** diff against the Notion tracker. It does not write. I approve, then it writes.

Tracker: `Tasks`, in the **Projects** studio hub
Data source: `collection://ce12dd35-4540-45bd-a0c2-84c3c14a58dc`
Milestones (relation target): `collection://f6cf4104-883b-4a6d-a4d2-c65cda8549d9`

Schema — all seven properties, exactly as they exist:

| Property | Type | Values / notes |
|---|---|---|
| `Name` | title | The task |
| `Status` | status | `Not started`, `Blocked`, `Needs sorting`, `In progress`, `Done` |
| `System` | select | `Room`, `Task station`, `Perception`, `Stalker AI`, `Director`, `Build/Tooling`, `Devlog` |
| `Estimate` | number | Hours |
| `Displaces` | text | What this pushes out of the milestone. Required for anything added mid-milestone. |
| `Milestone` | relation | → Milestones data source |
| `Scheduled` | date | Range; query as `date:Scheduled:start` / `date:Scheduled:end` |

There is no `Entity`, `Phase`, `Priority`, `Due`, `Effort`, `Notes`, or `Summary` property. Do not invent them.

## Step 1 — Find the range

In this order:

1. `.veil-sync/LAST_SYNC` containing a commit SHA — use `git log <sha>..HEAD`
2. Most recent tag matching `sync-*` — use `git log <tag>..HEAD`
3. Fall back to `git log --since="7 days ago"` and say that you fell back

Pull bodies and file stats, not subjects: `git log --stat --format=full <range>`

Read the diffs for anything the commit messages undersold. Also run `git log --diff-filter=A --name-only <range>` to catch new subsystems that have no tracker row yet.

## Step 2 — Map commits to tasks

Prefer the `Veil-Task:` commit trailer over path heuristics. The repo ships a template at `.gitmessage`; a commit carrying `Veil-Task: stalker-fsm` is authoritative and needs no guessing.

When the trailer is absent, fall back to paths. **These are the repo's real directories** — the roadmap's `src/ai/`, `src/director/`, `src/perception/` do not exist:

| System | Slug | Paths |
|---|---|---|
| Perception | `perception-emitters` | `src/gameLayer/Perception/`, footstep emission in `src/gameLayer/Objects/Entity/Player.cpp`, `src/gameLayer/AudioManager.*` |
| Stalker AI | `stalker-fsm` | `src/gameLayer/AI/Stalker*`, `src/gameLayer/Entity.*` |
| Director | `stalker-fsm` | `src/gameLayer/AI/Director*` |
| Task station | `task-station` | `src/gameLayer/MiniGames/`, `src/gameLayer/Objects/Interactable/`, `src/gameLayer/MiniGame.h` |
| Room | `room-sliceready` | `src/gameLayer/gameMap.*`, `src/gameLayer/WorldEditor*`, `resources/`, `saves/` |
| Build/Tooling | `slice-integration` | `CMakeLists.txt`, `CMakePresets.json`, `.github/`, `src/gameLayer/SaveSystem.*`, `src/gameLayer/*Allocator.h` |
| Devlog | — | No code paths. Never inferred from commits. |

Record the path evidence — you will show it.

A commit is evidence of *activity*, not completion. Assign status by tier:

| Evidence | Proposed status |
|---|---|
| No commits touching this area in range | unchanged |
| Commits touching the area | **In progress** |
| Commit message explicitly closes it, **or** tests added covering it, **or** the row's stated acceptance criteria are visibly met in the diff | **Done — flagged for confirmation** |

**Never propose Done on path evidence alone.** If the only signal is "files in this area changed," it is In progress. When torn, pick the lower tier and say why.

`Blocked` and `Needs sorting` are mine to set, not yours. Never propose moving a row *into* either. A row sitting in `Blocked` that shows commit activity is a **contradiction** — surface it under Step 3 rather than silently advancing it.

## Step 3 — Catch the other three cases

- **Orphan work** — significant commits matching no tracker row. Propose new rows with `Name`, `System`, and the milestone relation. Leave `Estimate` and `Scheduled` blank for me. Fill `Displaces` only if you can name what it pushes out; otherwise leave it and flag that it needs one.
- **Stale rows** — Status is `In progress` but zero commits touched it in 14+ days. Flag as stale. Do not change it; ask whether it's blocked, abandoned, or just quiet.
- **Contradictions** — row says `Done` but commits show active churn, or a `Blocked` row shows activity. Surface loudly. This usually means the row closed early or the block cleared without being recorded.

## Step 4 — Output the diff, then stop

One table, nothing else:

| Task | Current | Proposed | Evidence | Confidence |
|---|---|---|---|---|

Evidence cites commit SHAs and paths. Confidence is high/medium/low, and low-confidence rows get a one-line reason.

Then a short list of orphans, a short list of stale rows, and a one-line count: `N rows changed, M new, K stale`.

**Stop here.** Do not touch Notion. Wait for approval.

## Step 5 — Write, on approval only

Apply only the approved rows. Then write the current HEAD SHA to `.veil-sync/LAST_SYNC` so the next run picks up cleanly. If I approved only some rows, still advance LAST_SYNC — rejected rows were judgments, not unprocessed work.

## Standing rules

- Propose, never write, until approved. This board is Producer's; the sync serves it, it doesn't own it.
- Path heuristics are guesses. Show them so I can correct them.
- Refactors, formatting passes, build fixes, and dependency bumps do not move task status. They are noise here even when they were most of the week.
- If the range is empty, say "no commits since last sync" and stop. Do not pad.
