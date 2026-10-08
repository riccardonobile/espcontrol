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

## Verified inputs and boundaries

The supplied `espcontrol-energy-prototype-v32.html` is 23,972 bytes, SHA-256
`df46109520e294c3659315b7928343ad6639272797a24eee0f1ba7c2e3e30931`.
Use its `flowDiagram(s, family)` detail SVG as the visual reference. The page's
toolbar, simulator, overview cards and extra metric tiles are not requirements
for firmware. Do not copy the complete page, CSS palette or synthetic sensor
values into the product.

[James's PR #2162](https://github.com/jtenniswood/espcontrol/pull/2162) is still
open at `b6e9f8c866bdc54e06124f7cf4d12fac7b23603a`. Its final current diff adds
Climate secondary labels using the same climate entity, with status/actual/target
display choices and an optional prefix. Earlier generic secondary-entity work
was removed. Reuse the current neutral label plumbing if it lands; do not
depend on its obsolete intermediate options or merge that branch into this one.

The pinned ESPHome version is `2026.9.1`; its installed LVGL integration selects
LVGL `9.5.0`. Use existing repository LVGL conventions and compile workflows,
not browser SVG APIs or assumptions about LVGL 8.

## Architecture and ownership

```text
Authored product card contract + per-card source
        -> existing generators -> firmware/web contracts
Web Energy settings -> nine-field compact card configuration
        -> existing configuration service/store and backup path
        -> parsed card -> Energy driver -> card-owned Energy model
HA owner-scoped state/unit subscriptions -----------^
        -> card label/value update
        -> shared modal shell -> temporary Energy Flow view
Display profile + current_theme() + separate user accent -> card/modal styles
```

Propose a dedicated Energy card/driver, usable in the grid and in subpages.
Its click opens the flow modal; it does not send a Home Assistant service call.
Do not disguise it as a Sensor card: that driver's passive behavior removes
clickability. Existing card reset, allocation tracking and owner-scoped binding
paths should own the new context just as they own current drivers.

Proposed card presentation is a primary name, optional secondary caption and
the existing navigation chevron. A live Home-consumption value can reuse the
sensor row if approved. Exact secondary-caption/value semantics are a review
decision; the detail SVG does not define this new card's complete layout.
Keep existing card spans, grid geometry, wrapping and default interaction states.

PR #2162's reusable pieces are `BtnSlot::secondary_lbl`, the hidden label in
`common/device/button_widget.yaml`, static-child reset/cleanup, corresponding
subpage child creation, and `layout_button_label_with_secondary()` in
`button_grid_layout.h`. Climate-specific formatting remains Climate-owned.
Recheck that PR before implementation; if it has not landed, agree a minimal
shared label extraction rather than duplicating the Climate feature.

## Detail SVG geometry and flow invariants

Use the modal's available content rectangle, not the physical screen size.
The SVG uniformly scales and centers its reference geometry; use the existing
display/modal layout families to select a reference shape and apply the same
uniform fit. Account for shell/back-button space before fitting.

| Prototype family | Reference size | Horizontal offset | Vertical offset |
| --- | --- | ---: | ---: |
| Landscape | 620 x 430 | 192 | 143 |
| Square | 520 x 430 | 158 | 132 |
| Portrait | 520 x 620 | 158 | 174 |

Relative to center `(cx, cy)`, Solar is above, Home right, Grid left and Battery
below. Solar/Home/Grid show kW; Battery shows SOC percent. Captions are
Production, Consumption, Importing/Exporting/Idle, and Charging/Discharging/Idle.
Home's numeric text is slightly larger. Preserve readable node spacing and
arrow topology with existing compiled font sizes rather than scaling fonts
arbitrarily or adding the mockup's browser fonts.

Fixed connection ports in reference coordinates:

- Solar L/C/R: x = `cx - 18`, `cx`, `cx + 18`; y = `solar.y + 54`.
- Grid T/C/B: x = `grid.x + 69`; y = `cy - 18`, `cy`, `cy + 18`.
- Home T/C/B: x = `home.x - 69`; y = `cy - 18`, `cy`, `cy + 18`.
- Battery L/C/R: x = `cx - 18`, `cx`, `cx + 18`; y = `battery.y - 54`.

| Directed edge | Fixed ports | Geometry |
| --- | --- | --- |
| Solar -> Grid | Solar L -> Grid T | Rounded upper-left elbow |
| Solar -> Home | Solar R -> Home T | Rounded upper-right elbow |
| Solar -> Battery | Solar C -> Battery C | Straight vertical |
| Grid -> Home | Grid C -> Home C | Straight horizontal |
| Grid -> Battery | Grid B -> Battery L | Battery/Grid centerline reversed |
| Battery -> Home | Battery R -> Home B | Rounded lower-right elbow |
| Battery -> Grid | Battery L -> Grid B | Rounded lower-left elbow |

Keep ports invariant when power, labels or edge visibility change. Compute
Grid/Battery geometry once and reverse traversal/arrowhead direction, rather
than generating two slightly different curves. The prototype uses quadratic
corners, open V arrowheads, a 3.2-unit shaft and rounded caps/joins; paint paths
behind nodes. Tests should verify final-tangent arrow orientation and endpoints.
Its normal controls make the reciprocal Grid/Battery pair exclusive, while
"Show all" deliberately displays both as a geometry stress case.

**There is no animated flow in v0.32:** no SVG animation, animation loop or
periodic timer. Implement static geometry first. Moving indicators, their speed
and reduced-motion behavior require a separate review decision, not a claim
that they are already validated by this prototype.

## Home Assistant data and power-flow model

Proposed inputs: instantaneous Solar, Home, Grid and Battery power entities,
plus optional Battery SOC. Prefer the existing entity field for Home power and
typed options for the other IDs, labels and any explicitly approved polarity
settings. This preserves the existing nine compact fields. Final option names,
required/optional nodes and card-value semantics need architecture review.

- Normalize finite numeric W/kW samples to watts; cumulative kWh sensors are
  not suitable power inputs. Reuse existing numeric display formatting.
- Prototype signs are Grid positive = import, negative = export; Battery
  positive = discharge, negative = charge. HA integrations vary: document
  accepted conventions and decide whether explicit polarity settings are needed.
- Missing/unknown/unavailable/non-finite data must remain unavailable, not zero.
  Distinguish zero-power idle from missing data and do not invent flow arrows.
- SOC is independent of power direction; validate percent values and display
  an unavailable value safely rather than interpreting it as battery power.
- Updates arrive asynchronously. Keep validity and freshness alongside samples;
  decide the deadband/staleness policy before calculating directions.

Four aggregate power readings cannot uniquely determine all seven pairwise
flows. The prototype's checkbox-driven numbers are synthetic and do not even
guarantee power balance. Review must choose between explicitly measured edge
inputs and a clearly documented inferred-allocation policy. If inference is
chosen, test conservation, losses/residuals and mutually exclusive net flows;
do not present an arbitrary allocation as measured data. This is the main
functional dependency before implementing the graph.

Use `ha_subscribe_state()` and unit attributes through `HaCallbackOwnerScope`,
existing retained-state APIs and subscription generations. A full five-input
configuration needs five state channels and up to four unit-attribute channels,
shared/deduplicated by the coordinator. Modal opening should read the card's
model rather than start duplicate wire subscriptions. Handle registration
failure using existing diagnostics; release the owner on card replacement.

## Modal lifecycle, themes and performance

Add one Energy modal kind and use `control_modal_create_shell()` and the
existing close service. Keep an ephemeral view separate from card-owned data.
Close/replacement/grid restore must unregister view/theme callbacks and stop
any timer before deleting LVGL objects. Guard callbacks using the existing
owner/generation mechanisms; never retain deleted label or line pointers.

Use native LVGL labels/icons and bounded line/polyline point arrays. Precompute
curve samples only on layout changes; allocate no points/strings every frame.
Check existing MDI glyphs in `product/v2/icons.json` before adding missing icons
through the authored font/icon sources. Do not require an SVG decoder, raster
canvas, downloaded font or full-screen image buffer for this graph.

Theme-neutral surfaces/text come from `current_theme()` and display correction
follows existing ownership. Keep Primary/accent and any approved flow-state
colors separate. Do not change global palette values to match prototype CSS.
Register the modal with existing refresh ownership; theme switching updates
live objects in place without losing graph state or subscriptions. Keep content
markers/boundaries explicit rather than relying on coincidental RGB matching.

Start with four node groups and at most seven bounded paths. Set and measure
object, point-buffer, subscription and heap budgets on S3 before adding motion.
If approved later, one modal-owned timer should update only visible moving
markers at a bounded cadence, pause when hidden/display-taken-over or data is
invalid, and stop on close. No per-edge timers or whole-screen redraw loop.
Measure largest and smallest supported layouts, repeated open/close, rotations,
theme changes and HA reconnects; record heap/PSRAM high-water and frame cost.

## File-by-file implementation proposal

Paths in this table are future work, not changes made by this planning PR.

| File/source | Proposed responsibility |
| --- | --- |
| `product/v2/card_contract.json`, new `product/v2/cards/energy.json`, `product/model_v2.json` | Add one complete card entry and mapping; stable compact code, typed options, defaults and migration/normalization policy; keep aggregate and per-card sources equivalent. |
| `scripts/product_schema.py`, `scripts/build.py` | Extend established hook/driver handling only where the new contract requires it; no second generator. |
| `product/v2/product_compatibility.json` | Add Energy round-trip fixtures without changing old contracts. |
| `components/espcontrol/button_grid_config_parser.h` | Typed parsing/normalization for approved Energy options, bounded values and safe invalid input. |
| `components/espcontrol/button_grid_card_registry.h`, `button_grid_card_runtime.h` | Dedicated family/driver routing and modal-click behavior for main grid and subpages. |
| New `components/espcontrol/button_grid_energy_driver.h` | Card visual/interaction/binding/cleanup entry points; uses shared model and existing formatting. |
| New `components/espcontrol/energy_flow_model.h`, `energy_flow_layout.h` | Pure input/direction/allocation and geometry calculations, independent of LVGL and HA for host testing. |
| New `components/espcontrol/button_grid_energy.h` | Modal view, LVGL object updates and theme refresh integration using the shared shell. |
| `components/espcontrol/button_grid_grid.h` | Dispatch/include/register/cleanup the Energy driver through existing allocation paths. |
| `components/espcontrol/control_modal_service.h`, `button_grid_modal.h` | Add Energy kind/shell presentation and close path; preserve current modal policies. |
| `button_grid_config.h`, `button_grid_layout.h`, `button_grid_subpages.h` under `components/espcontrol/`; `common/device/button_widget.yaml` | Reuse PR #2162 secondary-label plumbing and existing chevron in both object-creation paths; avoid duplicate child ownership. |
| New `src/webserver/cards/energy.ts`; `src/webserver/entry.ts` | Card registration, settings fields and theme-aware card preview using existing registry/helpers. |
| New `src/webserver/application/config_energy_options.ts` if needed | One typed options adapter shared by editor, normalization and preview; avoid ad hoc copies. |
| `src/webserver/application/controls_fields.ts`, existing model/config/backup adapters | Extend only required field/normalization routing; use existing persistence/backup boundary. |
| `tests/firmware/CMakeLists.txt` and focused Energy tests | Model/layout, driver lifecycle, modal/theme and HA-owner regression coverage. |
| `tests/web/` plus existing contract/backup checks | Options, editing, preview, compact/subpage round trips, legacy backups and invalid values. |
| `product/v2/translations/strings.*.txt`, `product/v2/icons.json` when required | Reuse existing firmware text/icon keys; add genuinely missing node/status labels or glyphs through authored sources and their normal generation path. |
| `docs/features/` and card reference sources | Concise setup/sign/unit/limitations documentation after behavior is approved and implemented. |

Generated contracts (`button_grid_contract_generated.h`, web generated contract),
card capabilities, per-device slot initializer blocks, product snapshot and web
bundles must come from current generators. Use `scripts/build.py`,
`scripts/generate_device_slots.py`, `scripts/generate_device_manifest.py` when
applicable, and `scripts/check_product_snapshot.py --update`. Do not hand-edit
generated device sections or introduce device-specific Energy YAML copies.

## Persistence and compatibility

Configuration service/store persists compact card strings already; keep runtime
samples and animation state out of storage. `PanelConfig` native format is
version 1 with a 2,048-byte record-body limit. Prove worst-case escaped entity
IDs/labels fit this and the existing legacy/subpage transport limits before
finalizing options. No storage/backup version bump is presumed necessary.
Inspect normalizers so new options survive web edits, export/import and subpage
serialization; unknown Energy configuration on older firmware is a compatibility
question, not permission to rewrite existing card codes or silently drop fields.

## Implementation phases and acceptance gates

1. **Architecture review:** settle card labels/value, required inputs, sign/unit
   conventions, pairwise-flow semantics, optional nodes and static versus moving
   arrows. Confirm PR #2162 reuse and layout acceptance on compact displays.
2. **Contracts and pure logic:** add authored card/options sources, generator
   routing and fixtures; implement/test unit parsing, directions and fixed-port
   geometry with no LVGL dependencies. Review compatibility and size limits.
3. **Static card/modal:** integrate grid/subpage driver, shared shell, fonts,
   labels, chevron and seven native paths. Verify theme ownership, small-screen
   fit and reciprocal geometry before introducing live subscriptions.
4. **Live HA and lifecycle:** bind model with owner-scoped subscriptions, update
   only changed text/paths, handle missing/reconnecting data and release safely.
   Add motion only if separately approved and the S3 budget permits it.
5. **Configurator and end-to-end validation:** settings/preview/backup support,
   generated assets, concise public docs, full checks, representative firmware
   compiles and physical display testing. Continue on this same branch/Draft PR.

Dependency: PR #2162 may land during planning; re-read its final merged shape
before implementation. Neither its open status nor an HTML demo authorizes
application changes before this plan is reviewed.

## Risks and testing strategy

| Risk | Required protection |
| --- | --- |
| Aggregate sensors cannot identify pairwise flows | Review the policy; table-driven conservation/direction tests with import, export, charge, discharge, zero, residual and missing inputs. |
| Compact labels/ports overlap | Pure geometry tests for three reference families and rotations; physical smallest-S3 and P4-86 inspection; no port drift when labels grow. |
| Stale callbacks/timer pointers | Repeated open/close, card replacement, subpage teardown, restore, reconnect, theme switch while open and shutdown tests using existing host stubs. |
| Unsupported units/signs or unsafe parsing | W/kW, unknown/kWh, NaN/Inf, signed values, malformed options, long IDs and percent-escaped delimiter tests. |
| Broken generated/persisted contract | Product/source equivalence, fixture coverage, native and legacy backup round trips; existing cards unchanged. |
| Render/memory cost on constrained S3 | Bounded objects/points, no idle animation, no additional full-screen buffer; measure heap and render cost on hardware. |
| Theme/preview divergence | Use existing firmware and web theme access paths, test Dark/Light/Auto preview, preserve accent/content colors and contrast. |

During implementation run focused product/parser/card-runtime/modal/modal-layout/
display-token/HA-binding checks plus web/backup tests for each affected layer.
Run `npm run prepare:ci` before pushing, review regenerated diffs, and use the
then-current compile skill's factory targets/pinned ESPHome version before
requesting device tests. Compile success is not physical validation.

## Planning validation and review gate

This phase changes only this internal plan. It verifies repository alignment,
the actual detail SVG and current source paths; it does not validate Energy
firmware, implement sensors/calculations or claim device testing.

Architecture review must resolve the data/flow and card-presentation questions
above before implementation. Keep the fork-only PR Draft, leave `main` untouched
and continue approved future work on `feature/energy-dashboard` and that same PR.
