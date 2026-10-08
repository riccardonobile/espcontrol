# Power Dashboard architecture plan

Status: architecture planning only; implementation requires review approval.
Baseline: `a406208906f900058da4c063d1e0c3eeeff9af6a` (merged upstream Theme
Support, verified 2026-10-08). Work branch: `feature/energy-dashboard`; retain
the existing document path and fork-only Draft PR #10 without renaming them.

## Scope

- Feature: **Power Dashboard**; card picker name: **Power**; default primary
  label: **Power Dashboard**; detail modal: **Power Flow**.
- Show instantaneous electrical power in W/kW, not accumulated Wh/kWh.
- Show Home consumption, the approved dynamic secondary label and a chevron.
- Open a Power Flow modal with Solar, Home, Grid and Battery.
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
was removed. Its state and head were rechecked for this refinement on 2026-10-08.
Reuse its generic label plumbing; do not adopt Climate-specific display options
or merge that feature branch into this one.

[Theme Support PR #2168](https://github.com/jtenniswood/espcontrol/pull/2168)
is merged as this plan's baseline. Its final implementation, including the
post-rebuild refresh fix, is authoritative for themes and lifecycle.

The pinned ESPHome version is `2026.9.1`; its installed LVGL integration selects
LVGL `9.5.0`. Use existing repository LVGL conventions and compile workflows,
not browser SVG APIs or assumptions about LVGL 8.

## Architecture and ownership

```text
Authored product card contract + per-card source
        -> existing generators -> firmware/web contracts
Web Power settings -> nine-field compact card configuration
        -> existing configuration service/store and backup path
        -> parsed card -> Power driver -> card-owned Power model
HA owner-scoped state/unit subscriptions -----------^
        -> Home value + Battery/Grid secondary label
        -> shared modal shell -> temporary Power Flow view
Display profile + current_theme() + separate user accent -> card/modal styles
```

Propose a dedicated Power card/driver, usable in the grid and in subpages.
Its click opens the flow modal; it does not send a Home Assistant service call.
Do not disguise it as a Sensor card: that driver's passive behavior removes
clickability. Existing card reset, allocation tracking and owner-scoped binding
paths should own the new context just as they own current drivers.

## Approved Power Card presentation

| Element | Specification |
| --- | --- |
| Main number | Latest valid instantaneous Home consumption, using the existing sensor value/unit widgets, precision formatting and display-profile sensor typography. |
| Primary label | Existing configurable `label`; when empty, localized **Power Dashboard**. Preserve user-entered labels verbatim. |
| Secondary label | Continuously derived Battery/Grid status from the same card-owned model; rules below. No independent subscription or Climate display-setting option. |
| Navigation | Existing right-chevron object and icon font; tap opens Power Flow even if readings are missing. This card has no HA service action. |
| Layout | Existing card surfaces, padding, wrapping, line clamps, responsive width compensation and span rules on both grid and subpages. No custom fonts, font files or parallel label layout. |

Use the normal sensor font; use existing large-value sizing only where the
current shared layout permits all labels and the chevron to fit. Reserve one
secondary-label line and the chevron gutter through the shared label helper;
respect the current primary-label line clamp. Keep the secondary line single
line with existing dot/ellipsis behavior. Long labels, long translations and
long formatted values must truncate/wrap within existing bounds, never overlap.
Missing Home data clears the value/unit using the existing numeric sensor
unavailable convention; it must not display a false zero or disable navigation.

## Dynamic secondary-label rules

Classify Battery and Grid independently as active, known idle or unavailable.
Unavailable includes absent/unconfigured, unknown, invalid-unit, malformed,
non-finite or disconnected data; it is never another spelling of zero.
The following partial-data fallback is recommended for approval:

| Battery | Grid | English rendering |
| --- | --- | --- |
| Charging | Exporting | `Charging · Exporting` |
| Discharging | Importing | `Discharging · Importing` |
| Charging | Known idle | `Charging` |
| Known idle | Importing | `Importing` |
| Known idle | Known idle | `Idle` |
| Active | Unavailable | Battery active label only, e.g. `Charging` |
| Unavailable | Active | Grid active label only, e.g. `Importing` |
| Known idle | Unavailable, or the reverse | `Unavailable` |
| Unavailable | Unavailable | `Unavailable` |

General rule: emit known active Battery first, then known active Grid, separated
by ` · ` only if both are active. If there are no active labels, emit Idle only
when both are valid and within the zero band; otherwise emit Unavailable.
Home/Solar/SOC validity does not
erase independently valid Battery/Grid status; the modal exposes each node's
availability so an omitted unknown status is not misrepresented as idle.

Use `espcontrol_i18n()` for default labels, node names, modal title and every
state word. Reuse existing `idle`/`unavailable` translation entries; Charging,
Discharging, Importing, Exporting and the Power names need authored keys in
`product/v2/translations/strings.*.txt`. `scripts/build.py i18n` owns generated
`components/espcontrol/i18n_generated.h`. The middle dot already exists in
`common/assets/text_glyphs.yaml`; no new font or font asset is needed.
Format from status keys without reparsing display strings. Recompute on model
updates/reconnects and write LVGL text only when it changes. If text visibility
or dimensions change, invoke the shared label-layout path, not a Power copy.

## PR #2162 reuse and integration gaps

PR #2162's reusable pieces are `BtnSlot::secondary_lbl`, the hidden label in
`common/device/button_widget.yaml`, static-child reset/cleanup, corresponding
subpage child creation, and `layout_button_label_with_secondary()` in
`button_grid_layout.h`. Climate-specific formatting remains Climate-owned.
Its secondary label inherits the existing primary-label font, uses 70% opacity,
100% width and dot mode. Dynamic subpage creation applies the existing label
font explicitly. The helper offsets the primary label using its actual line
height; reset hides/clears the static label rather than treating it as a dynamic
child. These are the shared typography/ownership rules to reuse.

Prefer consuming the merged infrastructure. If still pending, recommend the
smallest compatible extraction of those generic fields, creation/reset wiring
and the shared helper, coordinated with James. Include the slot generator's
initializer changes; never hand-edit generated device YAML. Do not import the
Climate feature, its options, or its web-only smaller-text rule into Power.

Concrete gaps to solve narrowly during approved implementation:

- In #2162, `refresh_climate_secondary_label_layout()` hides secondary labels
  for every non-Climate card. Make the shared visibility/layout dispatch admit
  Power's model-owned label; keep Climate's existing options unchanged.
- `set_subpage_chevron_visible()` in current `button_grid_layout.h` always hides
  the chevron and ignores its visibility/width parameters. Reuse the existing
  child/glyph, but add an explicit Power opt-in through shared layout handling.
  Do not globally unhide chevrons or change existing cards.
- The secondary helper resets width to 100%; reserve the Power chevron gutter
  and vertical label budget in that same helper/layout path. If an optional
  parameter is needed, preserve current defaults for other callers. Do not
  create another layout implementation or shrink a Power-specific font.

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

## Power data model and recommended allocation

Use instantaneous Home, Solar, Grid and Battery readings, plus optional Battery
SOC. Prefer the existing `entity` field for Home and typed card options for the
other IDs. Preserve the nine-field compact format; do not create device-wide
Power settings or another persistence service. Unconfigured inputs remain
unavailable with their nodes visible; do not invent zero-producing equipment.

Recommended explicit data rules, with numerical policy awaiting approval:

- **Units:** valid W maps directly to canonical watts; valid kW multiplies by
  1,000. Read unit attributes through existing subscriptions. Any explicit unit
  override must be a validated W/kW choice, never an automatic guess. Reject
  Wh/kWh, unknown units, empty/malformed numbers, NaN/Inf and normalization
  overflow. `parse_float_ref()` accepts numeric prefixes: add a narrow Power
  validation step that requires the complete trimmed numeric input. Check the
  existing formatter's representable range before calling it; do not alter
  other sensor parsers/formatters.
- **Display:** reuse existing `format_fixed_decimal()` and value/unit widgets
  with current precision/unit conventions. Convert for display separately from
  model watts; display rounding must not decide flow direction or Idle.
- **Polarity:** Grid positive = importing, negative = exporting; Battery
  positive = discharging, negative = charging, as in v0.32. Recommend explicit
  per-input inversion options for these two signals when integrations use the
  opposite sign. Do not infer polarity from current state or SOC. Home and Solar
  are nonnegative consumption/production; negative values beyond the zero band
  are invalid configuration/data, not reversed Home/Solar paths.
- **Zero band:** recommend `abs(power_w) <= 1 W` as known idle, inclusive of
  exact zero. Direction classification uses this band; retain the original
  normalized measurement for numeric display. The initial 1 W threshold is a
  proposal, not a new approved setting or promise of physical measurement
  accuracy. No hysteresis or configurable deadband is assumed in this phase.
- **Validity:** keep a validity flag per input. Optional SOC must be finite and
  within 0..100%; invalid/absent SOC displays unavailable but never gates valid
  power directions. Invalid Battery/Grid yields the secondary fallback above.
- **Partial data:** valid Home still renders when another input is missing;
  each modal node renders its own validity. Derived pairwise paths require a
  complete valid power snapshot; hide inferred paths when it is incomplete.
  Do not fill missing Home from a balance equation without separate approval.
- **Connection:** reuse `ha_api_state_connected()` and the existing HA pump path.
  Mark live readings unavailable on disconnect; retain cached samples as last-known
  data. On reconnect, replay/bootstrap through existing retained-value and
  owner-generation APIs, then resolve live validity from refreshed state/unit
  callbacks. Do not label stale retained data as current. Lack of periodic
  updates alone must not invalidate an unchanged, connected sensor: HA is
  event-driven. No per-card polling network connection or expiry timer.

Four aggregate readings do not uniquely identify the seven pairwise paths,
including simultaneous behind-the-meter flows. The mockup's checkbox values
are synthetic and not necessarily balanced. Measured edge sensors are the
strongest source if an installation exposes them; supporting them would need
an explicitly approved mapping/contract rather than seven extra mandatory IDs.

For a minimal first implementation, recommend **documented solar-first inferred
paths** from the four aggregate inputs, subject to architecture approval:

1. Split net Grid into import source/export sink and net Battery into discharge
   source/charge sink; only one direction of each is active within a snapshot.
2. Allocate Solar to Home, then Battery charging, then Grid export.
3. Allocate Battery discharge to remaining Home, then remaining Grid export.
4. Allocate Grid import to remaining Home, then remaining Battery charging.
5. Each allocation is the minimum of remaining source and sink. Never exceed
   a measured capacity, manufacture a negative flow or double-count a sample.

This priority is a display convention, not proof of actual routing. Keep node
numbers as measured readings; do not display inferred edge values as measured
data. Label the modal's path interpretation as estimated using existing title/
caption styling and explain the policy in configurator help/public docs. Ports
and geometry remain unchanged by that explanation.

Because HA samples arrive asynchronously and equipment has losses, check the
source/sink residual before exposing inferred paths. Recommend a provisional
balance tolerance of `max(10 W, 2% of the larger total)`; within it, leave any
residual unallocated, rather than adjusting measurements. Above it, show valid
node numbers/statuses and mark inferred paths unavailable. That tolerance and
the priority need approval and real-installation validation; neither guarantees
that temporally mismatched samples describe the same physical instant.

Use `ha_subscribe_state()` and unit attributes through `HaCallbackOwnerScope`,
`ha_read_retained_state()`/attribute access and existing subscription generations.
A complete five-input configuration needs five state channels and up to four
power-unit attribute channels (plus an SOC unit check if needed), deduplicated
by `ha_read_coordinator.h`. Opening/reopening the modal or updating its secondary
label adds no subscriptions. Reuse parsing/formatting from the sensor path,
not `subscribe_sensor_value()` alongside a second model-owned Home subscription.
Handle registration failure using existing diagnostics; release the card owner
on replacement, not whenever a modal closes.

## Theme Support #2168 lessons and reusable helpers

| Source | Existing mechanism to reuse |
| --- | --- |
| `components/espcontrol/theme_palette.h` | `current_theme()`, raw RGB palette roles, bounded `register_theme_refresh()`/`unregister_theme_refresh()` and `apply_current_theme()`. |
| `components/espcontrol/button_grid_style.h` | `CardPalette`, separate `current_button_primary_color()` and `readable_text_color_for_bg()`; keep its existing brightness calculation/output choices. |
| `components/espcontrol/display_color.h`, `button_grid_display.h`, `theme_runtime_ui.h` | Existing profile correction, sensor font/width tokens and grid surface helpers; apply correction at the established call site exactly once. |
| `components/espcontrol/button_grid_layout.h`, `button_grid_grid.h` | Sensor-number style, wrapped labels/line clamps, descendant foreground synchronization, span geometry and shared state handling. |
| `components/espcontrol/theme_runtime_tree.h` | Existing content-background/foreground/pressed-fill ownership markers. Protect relevant parts, not the entire modal's neutral chrome. |
| `components/espcontrol/button_grid_modal.h`, `button_grid_modal_layout.h`, `control_modal_service.h` | Shared shell, centering/content bounds, lifecycle, pressed/disabled helpers, `control_modal_register_theme()` and tracked target cleanup. |
| `components/espcontrol/theme_runtime_ui.h` | `refresh_theme_grid_after_rebuild()` after grid/subpage binding/reconstruction; same-mode restore must refresh new descendants too. |
| `components/espcontrol/button_grid_ha.h`, `ha_read_coordinator.h` | Shared state/unit subscriptions, retained values, owner scopes, generations and release; no new HA service. |
| `components/espcontrol/button_grid_config_parser.h`, `clock_bar.h`, `display_text.h` | Existing numeric parsing/formatting and `lv_label_set_display_text()` for normalized display text. |
| `src/webserver/state/preview_theme.ts`, existing card previews | `previewEffectiveTheme()`, `previewThemeCss()` and shared preview palette roles; Auto follows firmware Active Theme. |

Dark, Light and Auto must restyle an open Power Flow modal in place. Keep the
card's model and LVGL state intact; never rebuild the graph to change theme.
The shell already owns a refresh binding: use it for neutral modal chrome and
existing deletion cleanup. If additional Power style application is necessary,
extend the owner/driver refresh path narrowly, not one registry slot per card
or per path. New owners must handle/log failed registration just as #2168 does.

Use neutral card/modal surfaces and theme text roles at construction. Keep the
user accent and any approved functional path colors outside neutral palette
mapping, even if their RGB happens to equal a theme token. The default path
stroke can reuse the existing accent, matching the mockup's single-color arrows;
do not invent a new functional palette. Content ownership must not prevent
surrounding neutral labels/surfaces from refreshing.

**Concrete contrast gap:** shared `theme_apply_grid_button()` currently forces
white checked/pressed text before syncing descendants. Do not assume this is
readable on a bright selected accent. For accent/content-owned backgrounds,
Power-specific setup/state refresh must apply `readable_text_color_for_bg()`
to the actual display-ready fill for numeric text, unit, primary/secondary labels
and chevron. Neutral fills retain theme text roles and existing label opacity.
Invoke the Power refresh after shared styles and after the grid-owner theme
refresh when relevant; keep contrast outputs unchanged, avoid double color
correction and do not fix unrelated cards here.
Use the existing modal readable-content/pressed helpers where applicable. Do
not add style-change event loops or a second contrast algorithm. The exact
narrow Power dispatch hook is an integration gap to verify during implementation.
Power statuses themselves do not manufacture a checked/toggle state.

Construction while Light is already active must use the active palette directly.
After backup restore/rebuild, keep #2168's final grid refresh after objects and
bindings exist; do not move it earlier or fake Dark -> Light to force it. Test
legacy backups without theme fields and modern explicit-theme restores with
Power cards in both the main grid and a subpage.

The web preview must consume existing theme CSS variables rather than copy
prototype colors. Palette parity is already guarded by
`scripts/check_firmware_display_tokens.py`; preserve that boundary. Confirm
matching accent foreground behavior in preview without introducing a separate
Power contrast formula.

## Modal lifecycle, themes and performance

Add one Power Flow modal kind and use `control_modal_create_shell()` and the
existing close service. Keep an ephemeral view separate from card-owned data.
Closing destroys only the modal view; the card-owned model/subscriptions stay
available for the card summary. Card replacement, grid rebuild, subpage teardown
or restore must first detach any modal referencing that model, then release its
HA owner before destroying the model. Mere modal close does not unsubscribe the
card. Use existing modal/navigation cleanup and owner-generation protection;
never retain deleted label or point-buffer pointers. An offscreen card may keep
its normal existing binding lifetime; do not invent a second navigation policy.

Use native LVGL labels/icons and bounded line/polyline point arrays. Precompute
curve samples only on layout changes; allocate no points/strings every frame.
Check existing MDI glyphs in `product/v2/icons.json` before adding missing icons
through the authored font/icon sources. Do not require an SVG decoder, raster
canvas, downloaded font or full-screen image buffer for this graph.

Start with four node groups and at most seven bounded paths. Store point arrays
with the modal view so LVGL never references a temporary stack buffer. Set and
measure object, point-buffer, subscription and heap budgets on S3. Static paths
need no animation timer: update visibility and changed text on model updates,
and geometry only on content-size/profile changes. No per-edge timers, full-
screen redraw loop or geometry buffer of data-dependent size. If motion is
approved separately later, stop its one view-owned timer before view deletion.
Measure largest and smallest supported layouts, repeated open/close, rotations,
theme changes and HA reconnects; record heap/PSRAM high-water and frame cost.

## File-by-file implementation proposal

Paths in this table are future work, not changes made by this planning PR.

| File/source | Proposed responsibility |
| --- | --- |
| `product/v2/card_contract.json`, new `product/v2/cards/power.json`, `product/model_v2.json` | Add one Power entry and mapping; Home binding, default-label behavior, typed options, stable compact code and normalization. Keep aggregate/per-card sources equivalent. |
| `scripts/product_schema.py`, `scripts/build.py` | Extend established hook/driver handling only where the new contract requires it; no second generator. |
| `product/v2/product_compatibility.json` | Add Power round-trip fixtures without changing old contracts. |
| `components/espcontrol/button_grid_config_parser.h` | Typed parsing/normalization for approved Power options; strict sample validity remains Power-specific. |
| `components/espcontrol/button_grid_card_registry.h`, `button_grid_card_runtime.h` | Dedicated family/driver routing and modal-click behavior for main grid and subpages. |
| New `components/espcontrol/button_grid_power_driver.h` | Card visual/interaction/binding/cleanup entry points; model-owned Home value/status and Power-only contrast refresh. |
| New `components/espcontrol/power_flow_model.h`, `power_flow_layout.h` | Small pure Power data/status/allocation functions and bounded geometry; no generic graph engine or parallel number/contrast utilities. |
| New `components/espcontrol/button_grid_power.h` | Temporary Power Flow modal view and changed-value updates using the shared shell. |
| `components/espcontrol/button_grid_grid.h` | Dispatch/include/bind/cleanup Power through existing allocation paths; minimally admit Power's secondary-label layout after PR #2162. |
| `components/espcontrol/control_modal_service.h`, `button_grid_modal.h` | Add Power Flow kind/presentation/close path; preserve shell theme registration and existing dismissal policies. |
| `button_grid_config.h`, `button_grid_layout.h`, `button_grid_subpages.h` under `components/espcontrol/`; `common/device/button_widget.yaml`; `scripts/generate_device_slots.py` | Only PR #2162's shared label plumbing not yet present, plus Power-only chevron opt-in/gutter through the same layout helper. Do not duplicate creation/layout or change other cards. |
| `components/espcontrol/theme_runtime_ui.h` if required | Forward the existing grid/subpage owner refresh to Power's foreground/style refresh after shared styling; no per-card theme registry or global contrast rewrite. |
| New `src/webserver/cards/power.ts`; `src/webserver/entry.ts` | Register Power, settings fields and approved value/status preview using existing registry and theme helpers. |
| New `src/webserver/application/config_power_options.ts` only if existing adapters do not suffice | One typed options adapter shared by editor/normalization/preview; no new service or duplicate utilities. |
| `src/webserver/application/controls_fields.ts`, existing model/config/backup adapters | Extend only required field/normalization routing; use existing persistence/backup boundary. |
| `tests/firmware/CMakeLists.txt` and focused Power tests | Status truth table, strict samples, allocation/layout, driver lifecycle, theme/contrast and HA-owner coverage. |
| `tests/web/` plus existing contract/backup checks | Options, editing, preview, compact/subpage round trips, legacy backups and invalid values. |
| `product/v2/translations/strings.*.txt`, `product/v2/icons.json` only for missing entries | Add missing Power/node/status text and reuse existing glyphs through current generators; no additional font files. |
| `docs/features/` and card reference sources | Concise setup/sign/unit/limitations documentation after behavior is approved and implemented. |

Generated contracts (`button_grid_contract_generated.h`, web generated contract),
card capabilities, per-device slot initializer blocks, product snapshot and web
bundles must come from current generators. Use `scripts/build.py`,
`scripts/generate_device_slots.py`, `scripts/generate_device_manifest.py` when
applicable, and `scripts/check_product_snapshot.py --update`. Do not hand-edit
generated device sections or introduce device-specific Power YAML copies.
Helpers named in the reuse table need no changes merely because Power calls
them. No theme setting/palette, device profile, HA entity-name contract, camera,
artwork, clock, configuration service/store or storage-version change is proposed.

## Persistence and compatibility

Configuration service/store persists compact card strings already; keep runtime
samples and animation state out of storage. `PanelConfig` native format is
version 1 with a 2,048-byte record-body limit. Prove worst-case escaped entity
IDs/labels fit this and the existing legacy/subpage transport limits before
finalizing options. No storage/backup version bump is presumed necessary.
Inspect normalizers so new options survive web edits, export/import and subpage
serialization; unknown Power configuration on older firmware is a compatibility
question, not permission to rewrite existing card codes or silently drop fields.

## Implementation phases and acceptance gates

The card presentation is approved; implementation itself still requires explicit
approval. Start only after the remaining data/fallback decisions below are settled.
Prefer these small phases/commits, including focused tests and generator output
in the phase that owns them:

1. **`Add Power card model and contract`:** authored contract/mapping, bounded
   options, sample validation, polarity, secondary status function and allocation
   policy tests; prove escaped config/backup compatibility. Resolve the PR #2162
   shared-label dependency before UI integration.
2. **`Add Power card and static flow modal`:** shared sensor/secondary-label/
   chevron layout, driver-owned HA model, shell/native geometry and lifecycle;
   theme/pressed foreground integration. Test grid and subpage creation, partial
   data, reconnects, close/reopen, restore and style switching without rebuilding.
3. **`Add Power configurator and preview`:** current registry/settings controls,
   typed option routing and preview on existing theme state; backup/edit round
   trips, translation generation and required web bundles.
4. **`Document and validate Power Dashboard`:** concise public configuration/
   units/sign/inference guidance, end-to-end checks and actual hardware results.
   Add only genuinely needed regression fixes; do not split artificial commits
   or manufacture a separate generic library to match this outline.

Static geometry is the approved baseline. Motion is outside these phases until
separately approved; do not add dormant animation infrastructure now. Continue
all approved implementation on this branch and the existing fork Draft PR.

Dependency: PR #2162 may land during planning; re-read its final merged shape
before implementation. Neither its open status nor an HTML demo authorizes
application changes before this plan is reviewed.

## Risks and testing strategy

| Risk | Required protection |
| --- | --- |
| Secondary status lies about partial data | Exhaust all Battery/Grid combinations, all four approved active words/order/middle dot, one-active/one-unknown, idle/unknown and both-invalid. Idle requires two valid readings within the zero band; missing Home/SOC must not erase known states. |
| Labels/value/chevron overlap | Default/custom/wrapped primary labels, long translations, large W/kW values, secondary dot ellipsis and visible chevron on main/subpages across supported spans/profiles/rotations. Confirm existing fonts and middle-dot glyph, no new assets. |
| Aggregate sensors cannot identify pairwise flows | Golden cases for the approved priority, source/sink conservation and residual limits; charging/import, discharging/export, solar surplus, overnight and transition samples. No claimed measured edge values or double-counting. |
| Ports/curves move or reverse incorrectly | Three reference families plus actual modal bounds; fixed three-port coordinates per node across all edge-visibility combinations; exact reverse Grid/Battery centerline and tangent arrowhead direction; horizontal/vertical centering. |
| Unsafe units/signs/numbers | W/kW equivalence; Grid/Battery inversions; zero and both sides of deadband; Home/Solar negatives, unknown/kWh, whitespace, trailing junk, NaN/Inf, overflow, SOC bounds and escaped/long options. |
| Theme or accent foreground becomes stale | Dark -> Light -> Dark and effective Auto transitions with modal open/pressed; no value/navigation/subscription reset. Bright/dark accents, custom colors that equal neutral tokens and existing readable-helper outputs for every card descendant. |
| Rebuilt UI misses same-theme application | Construct/replace Power under active Light; restore legacy no-theme and modern explicit-theme backups, including subpages. No artificial mode change; summary, chevron, numeric text and modal chrome match immediately. |
| Dangling LVGL/model/geometry pointers | Repeated open/close, replacement while modal open, grid/subpage teardown, restore, display takeover and reconnect. Release owners once, unregister callbacks before deletion, keep point arrays alive and restore registry capacity after close. |
| Broken generated/persisted contract | Product/source equivalence, fixture coverage, native and legacy backup round trips; existing cards unchanged. |
| Duplicate subscriptions or memory growth | One shared card model, stable deduplicated channel counts across repeated modal opens; one shell theme owner, unchanged fixed registry bounds, bounded point arrays and unchanged-text suppression. Measure S3 heap/render cost. |
| Theme/preview divergence | Existing palette parity check; Dark/Light/manual preview and Auto Active Theme, matching Power label/validity logic and accent contrast. Preserve functional/content colors. |

During implementation run focused product/parser/card-runtime/modal/modal-layout/
display-token/HA-binding checks plus web/backup tests for each affected layer.
Run `npm run prepare:ci` before pushing, review regenerated diffs, and use the
then-current compile skill's factory targets/pinned ESPHome version before
requesting device tests. Compile success is not physical validation.

## Planning validation and review gate

This phase changes only this internal plan. It verifies repository alignment,
the actual detail SVG and current source paths; it does not validate Power
firmware, implement sensors/calculations or claim device testing.

### Remaining decisions requiring approval

1. Accept the partial-status fallback: show known active status alone; otherwise
   Unavailable unless both Battery and Grid are known idle.
2. Accept the solar-first inferred allocation and visible estimated-path
   explanation, or prefer a measured-edge mapping (which would require a
   separately specified contract). This choice determines what arrows mean.
3. Accept or revise the proposed 1 W zero band and residual tolerance of
   `max(10 W, 2%)`, and confirm explicit Grid/Battery inversion options. Validate
   this with the user's actual HA entities/units and inverter behavior before
   implementation; do not silently guess sensor polarity or missing units.
4. Coordinate the minimal generic-label integration with PR #2162 if it remains
   open; preserve its typography and existing-card behavior when adding the
   Power secondary-label/chevron layout and theme/contrast hooks.

Card naming, Home value, dynamic Battery/Grid summary, existing fonts, static
seven-path topology and main/subpage support are approved. They do not need
another design decision. Keep animations excluded unless separately approved.
Keep the fork-only PR Draft, leave `main` untouched and wait for explicit
implementation approval on `feature/energy-dashboard` and that same PR.
