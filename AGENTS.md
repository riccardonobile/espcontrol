# AGENTS.md

## Communication

- The user is technical but not development-oriented. Explain changes in approachable terms and avoid unnecessary implementation detail.
- Keep final updates concise: what changed, how it was checked, and what the user needs to test.

## Development workflow

- Resolve the active repository root with `git rev-parse --show-toplevel` and
  use that worktree for repository files and commands. Do not assume the main
  checkout's path; this repository is often used through linked worktrees.
- Keep changes and checks in the worktree where the task was started unless a
  workflow explicitly needs another branch or worktree.

- Treat `main` as the stable branch.
- For normal code, firmware, configuration, UI, or documentation changes, create a short-lived branch from the latest `main`.
- Use a separate git worktree for feature or fix work so multiple issues can be developed and tested at the same time without changing `main`.
- Use short, descriptive branch names like `fix-display-timeout` or `update-pr-workflow`; do not include `codex` in branch names or PR titles.
- Infer the branch name from the requested outcome unless the task is ambiguous.
- Keep each branch focused on one bug fix, feature, device change, cleanup, or documentation change.
- If a request starts to include unrelated work, keep the extra work out of the branch unless the user explicitly asks to include it.
- Commit completed changes and push the branch.
- Open a pull request marked ready for review so automated checks and review systems run, instead of merging directly to `main`.
- Leave the pull request open until the user confirms they have tested it.
- Do not close related GitHub issues until the user confirms the fix works.
- Only work directly on `main` when the user explicitly asks for it, or for a tiny emergency/documentation-only change where a PR would add no value.
- After a pull request is merged, clean up its local worktree and branch when practical.

## Pull requests

- PR descriptions should explain the purpose of the change, the practical impact, and how it was checked.
- Include clear testing notes so the user can test the branch independently of `main`.
- If firmware needs flashing, name the affected display or device in the PR body.
- Distinguish automated checks from physical device testing. A compile/build pass is not the same as user-confirmed device testing.
- Use the automated PR testing guidance as the starting point for the PR body whenever it is available.

## Native EspControl UI Consistency

- Before adding UI, inspect the equivalent native EspControl cards, labels,
  measurements, modal surfaces and navigation. Reuse their helpers rather than
  approximating them with feature-specific styling.
- Match styling to semantics: passive information cards use non-interactive
  surfaces; action/navigation cards use native touch feedback; Back follows the
  existing pattern for its context. Do not add interactive states to passive UI.
- Support Dark, Light and Auto through existing semantic roles and theme helpers,
  including refresh while a view is open. New colors or palette roles need a
  clear reason; do not change the global theme to solve a feature-local issue.
- Use native font roles, measurement/unit alignment, label hierarchy and overflow
  behavior. Do not add fonts or arbitrary per-value size fallbacks to fit text.
- Reuse grid/profile margins, gaps and responsive layouts so views align with
  Home/subpages. Preserve unaffected profiles; avoid device-specific magic
  offsets and decorative parent containers.
- Account for LVGL draw-stack depth, internal heap/PSRAM, bounded allocations,
  object ownership, event/HA callback cleanup and theme refresh across repeated
  opening/closing. Prefer minimal changes that preserve approved behavior.
- Test affected profiles and representative native components when shared UI
  helpers change. Check resolved LVGL styles/bounds where relevant; passing host
  tests alone does not establish visual equivalence or physical acceptance.
