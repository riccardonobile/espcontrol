# Energy Dashboard architecture plan

Status: architecture planning only; implementation requires review approval.
Baseline: `a406208906f900058da4c063d1e0c3eeeff9af6a` (fork and upstream `main`,
verified 2026-10-08). Work branch: `feature/energy-dashboard`.

## Scope

- Add an Energy Card with primary/secondary labels and a navigation chevron.
- Open an Energy Flow modal with Solar, Home, Grid and Battery.
- Preserve HTML mockup v0.32's fixed arrow connection points and reversible
  Grid/Battery paths; avoid changing existing cards or modal behavior.
- Reuse existing card drivers, modal lifecycle, theme palette, fonts,
  display profiles, Home Assistant subscriptions and configuration contracts.
- Keep authored/generated boundaries and existing backups compatible.

## Inputs and dependencies

- HTML mockup v0.32 has not been supplied or located in this checkout. Exact
  layout, labels, flow rules and animation details need verification against
  that file before implementation. Only the requirements stated above are
  currently verified design constraints.
- [James's PR #2162](https://github.com/jtenniswood/espcontrol/pull/2162)
  is open at `b6e9f8c866bdc54e06124f7cf4d12fac7b23603a`. Its current scope is
  Climate secondary labels, not a general Energy card. Review its reusable
  label/layout plumbing without merging the feature branch into this work.

## Review gate

The next planning update will map actual source files, lifecycle and data
ownership; propose implementation phases, dependencies, risks and tests.
No application, firmware, configuration, generated output or schema changes
are authorized in this phase. Continue future implementation on this same
branch and fork-only Draft PR after architecture review.
