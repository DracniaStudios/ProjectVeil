# veil-sync setup

State and canonical definition for the `veil-sync` skill, which reconciles the
Notion **Tasks** board against this repo's git history.

## Files here

| File | Purpose |
|---|---|
| `LAST_SYNC` | Commit SHA the last sync processed. The skill reads this to build its `git log <sha>..HEAD` range. Advanced automatically at the end of an approved sync. |
| `SKILL.md` | Canonical skill definition, kept in version control. See "Source of truth" below. |

## One-time local setup

`.gitmessage` ships in the repo, but `commit.template` is per-clone git config and
cannot be committed. Run this once per machine, from the repo root:

```bash
git config commit.template .gitmessage
```

Every commit then carries the task slug it serves:

```
feat(perception): propagate emitter events through material attenuation

Veil-Task: perception-emitters
```

Slugs: `perception-emitters`, `stalker-fsm`, `task-station`, `room-sliceready`,
`slice-integration`.

The trailer is authoritative. Path heuristics are the fallback, and they are
guesses — the roadmap's expected paths (`src/ai/`, `src/director/`,
`src/perception/`) do not match this repo's `src/gameLayer/` layout, so the
trailer matters more here than the Production Plan assumed.

## Source of truth

`veil-sync` is a **custom account-level skill** (synced from the Claude account,
`source: custom`), not a project skill. The copy Claude loads at runtime lives
outside this repo and is overwritten on every sync, so editing it locally does
not stick. `.claude/` is gitignored here, so a project-scoped skill is not an
option either.

`SKILL.md` in this directory is therefore the version-controlled record. **After
changing it, paste its contents into the account skill** — otherwise the running
skill and this file drift apart, which is the exact failure this directory was
created to fix.

## What was wrong before

The skill pointed at `collection://374c2671-520b-4f1f-8ef5-ff1a20dae0ca`
(`Entity Implementation – Task Tracker`), which no longer resolves —
`data_source_not_found`. It also documented a schema
(Entity / Phase / Priority / Due / Effort / Notes / Summary) that the live board
does not have. The real tracker is `Tasks`,
`collection://ce12dd35-4540-45bd-a0c2-84c3c14a58dc`, with
Name / Status / System / Estimate / Displaces / Milestone / Scheduled.
