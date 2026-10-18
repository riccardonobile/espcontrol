# Power Dashboard architecture plan

Status: Phases 1 and 2 complete; Phase 3 web configurator/preview implemented.
Phase 4A includes representative compiles and separate actual-device factory/OTA
builds; see the corrected physical-device record below.
Phase 4B upstream reconciliation, UI refinements and automated/build checks are
complete. Physical validation of the new integrated firmware remains pending.
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

## Phase 3 implementation boundary

Power is available in the existing main/subpage picker. The shared editor owns
label, four required sensor selectors, optional SOC, precision and two explicit
inversion toggles. All selectors use the existing sensor autocomplete; no new
service or storage schema. Power does not draw a card icon, so no ineffective
icon setting is exposed. Its pressed accent remains the existing global Primary.

The grid preview uses existing sensor-value/label fonts, layout/profile variables,
neutral sensor surfaces and chevron glyph. It shows `-- W` and `Unavailable`;
an explanatory editor note identifies these as placeholders. The browser has
no live HA power channels, so it does not reimplement calculation/status logic
or infer Idle from missing data. No detail-preview mechanism exists to reuse;
Phase 2 native Power Flow remains unchanged. Dark/Light/manual and Auto Active
Theme use the existing preview theme infrastructure. Preview is neutral, not
an accent-filled active-state simulation, so it needs no second contrast policy.

Web editor labels are English like existing cards. Option labels come from the
authored product contract and generated metadata. The current translation
catalogue/generator applies only to physical firmware; existing translated
Power status remains unchanged. Do not create an independent web localization
system in this phase.

### Storage investigation and compatibility decision

- `common/config/button_template.yaml` and `_4chunk.yaml` persist each main
  card in an ESPHome text entity with max_length 255. Native PanelConfig records
  can hold 2,048 bytes, but reconciliation still writes the bounded text entity;
  a larger native record/backup is not a supported larger main card.
- The parser uses strings and does not independently impose 255. Every main
  card type shares the text-entity bound. Existing semicolon/escaped nine-field
  formats, option keys, firmware normalization and backup version are unchanged.
- Subpages use existing 4/8 text chunks (device-dependent), 255 bytes each,
  shared by the complete subpage. A larger Power card can fit there if the full
  serialized/escaped subpage fits. Moving/pasting back to main rechecks 255.
- Save and main-grid paste now count UTF-8 bytes using the existing subpage
  helper, rather than characters. The Power editor shows the exact serialized
  count; unsupported main-grid saves are rejected with actionable feedback.
  Restore validates the authoritative Power main/subpage payload before writes;
  old partial Power configurations can still restore missing readings.
- Existing encoding already omits unused tails/default fields where supported.
  Renaming option keys, abbreviating sensor domains or adding aliases would need
  matching firmware/parser changes and would break old-firmware compatibility;
  no such change is approved. Omitting the default stored label could save
  space but would not reliably make long IDs fit and changes canonical output.
- Concrete raw nine-field fixture: five short IDs/default label 188 bytes; the
  realistic five inverter/house IDs 288 bytes (before inversions). Removing SOC
  from that fixture gives 229 bytes; both inversions give 256 bytes. A longer
  label or longer IDs can exceed it again. A subpage is the existing alternative.
- If long main-grid IDs are a requirement, choices needing separate approval
  are shorter entity IDs upstream in HA (renaming automation references), a new
  compatible alias encoding (new firmware support, older firmware cannot use
  it), or increasing storage (migration/device-memory/backup review). Validation
  does not solve that installation constraint; physical setup must confirm fit.

Latest upstream reviewed for this phase: `9987b6c3e` (11 commits after Phase 2's
`fb904df1b`). Overlap includes card colours/Global naming, soft corners, Device
tab/settings, preview resizing and dependency updates. No unrelated changes were
rebased/merged; reconcile these before the eventual upstream contribution.

## Phase 1 implementation boundary

`components/espcontrol/power_flow_model.h` now owns strict power/SOC validation,
canonical polarity, balance validity, inferred allocations and translated status
text independently of LVGL. The initial constants are 1 W idle, 10 W minimum
balance tolerance and 2% relative tolerance. Values above the existing numeric
formatter's safe precision-3 range (2,147,482 W) are rejected. Small negative
Solar/Home noise within the idle band is retained without creating capacity.

The authored `power` contract keeps all nine saved fields: Home uses `entity`;
`options` holds `solar_entity`, `grid_entity`, `battery_entity`, optional
`battery_soc_entity`, then bare `invert_grid`/`invert_battery` flags (absent means
false). Default label is `Power Dashboard`; explicit labels are preserved.
Unused `sensor`/`unit` fields are cleared; display precision allows empty/0/1/2.
The new subpage code is `PW`; existing codes and storage versions are unchanged.
Generated normalizers and typed config views preserve these options in firmware,
browser serialization, native backups and card transfers.

## Phase 2 implementation boundary

`button_grid_power_driver.h` installs the native sensor-value card, shared
secondary label and existing chevron on main grids and subpages. Phase 2 kept
Power hidden; Phase 3 now enables it with the editor controls described above.
Its runtime contract now declares state/attribute subscriptions, modal navigation
and a card-owned allocation. The nine-field config and Phase 1 calculation model
are unchanged.

`power_card_data.h` accepts separate state/unit callbacks into one bounded
card-owned snapshot. `button_grid_power.h` shares that snapshot with one temporary
Power Flow view. Subscriptions are owner-scoped to the context, retained/deduplicated
by the existing HA coordinator and released before context disposal. The existing
HA disconnect event clears live snapshots, including units; reconnect callbacks
restore valid readings. Stale cached values are not displayed as current. Opening
or closing the view creates no subscription, timer or extra network request.

Power Flow uses the existing modal shell and its `set_clock_bar_modal_label()`
mechanism. Its localized **Power Dashboard** title replaces the temperature;
close/navigation/owner teardown restores the current page title or temperature.
There is no duplicate modal header. Valid node watts remain visible with
independent status and optional Battery SOC. The existing label style shows
**Inferred flows**, **Idle**, **Unavailable** or **Inconsistent readings**;
unavailable/inconsistent calculations hide all paths without moving ports.

`power_flow_layout.h` preserves v0.32 reference families, node centers, all fixed
ports, quadratic bends and arrow tangents. Fixed LVGL polylines sample the SVG
quadratic segments. Grid/Battery share identical points in reverse order. Geometry
scales uniformly and centers inside native panel bounds below the back button
and above one explanation line. The temporary view owns the bounded point arrays
until deletion. Existing compiled icon/label fonts are reused; no SVG dependency,
canvas, framebuffer, animation or polling timer exists.
The display profile carries the already compiled `font_text_small` as an optional
compact text role. Power Flow uses it only when three body-font rows would reach
the fixed ports; the card primary/secondary/value fonts remain unchanged.

Neutral modal chrome follows the shell's existing theme owner. Power's narrow
foreground refresh runs after shared grid restyling and uses
`readable_text_color_for_bg()` for current pressed/custom backgrounds. It changes
no other card's contrast policy. Theme changes preserve the model, graph objects,
points, navigation and subscriptions. Flow-line colors remain accent-owned.

Host coverage includes shared label/gutter bounds, native driver callbacks,
partial/inconsistent/recovered data, disconnect/reconnect, all reference families,
fixed ports/reversed geometry, theme/accent foreground, repeated close/reopen,
subpage deletion and context disposal. Existing clock-bar title tests exercise
the actual title restoration helper. Firmware compiles and physical testing are
reported separately in the Draft PR; a host test does not prove visual legibility.

**Transport constraint for the editor phase:** native record bodies allow 2,048
bytes, but live main-grid card text and its existing save validation still cap
the serialized card at 255 bytes. Five representative short IDs plus the default label serialize to 188 bytes;
five realistic inverter/house IDs serialize to 288 bytes before inversion flags.
Long IDs therefore can exceed this. The bound is preserved and tested, with no truncation or schema
bump. Phase 2/3 must keep this validation visible and assess realistic installation
IDs before exposing the card; native backup capacity alone does not prove a
configuration can be applied to live text entities. Subpages retain their existing
device-dependent chunk limits.

Tests cover all 35 planned allocation cases in W/kW, conservation (including
hidden allocations), parsing/limits, SOC, status combinations/i18n, invalidation,
polarity and valid-invalid-valid transitions. Shared serialization fixtures cover
defaults, custom labels, optional entities, malformed flags, canonical ordering,
compact escaping and firmware/browser parity. No firmware size or physical UI
validation is claimed for this model-only phase.

## Verified inputs and boundaries

The supplied `espcontrol-energy-prototype-v32.html` is 23,972 bytes, SHA-256
`df46109520e294c3659315b7928343ad6639272797a24eee0f1ba7c2e3e30931`.
Use its `flowDiagram(s, family)` detail SVG as the visual reference. The page's
toolbar, simulator, overview cards and extra metric tiles are not requirements
for firmware. Do not copy the complete page, CSS palette or synthetic sensor
values into the product.

[James's PR #2162](https://github.com/jtenniswood/espcontrol/pull/2162) was
closed without merging. Its final head is
`b0983005c0cced5820f85849e97e598041ea4415` (verified 2026-10-13).
Phase 2 extracts only its generic secondary-label field, hidden static/dynamic
label creation/reset, slot generator wiring and shared label-layout helper.
Power extends that shared helper's optional numeric/chevron bounds. Climate
options, formatting and subscriptions were not imported.

Latest upstream inspected for Phase 2: `fb904df1b33eae0ed2d1349099436b8360302af8`.
The existing feature branch remains based on the approved merged-theme baseline;
no main update/rebase or unrelated upstream changes are included in this phase.
Upstream's later per-card color, modal/card and artwork changes need reconciliation
before an eventual upstream PR; they are not a reason to merge the closed Climate PR.

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
The approved partial-data fallback is:

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

## Finalized Power Flow calculation proposal

Use instantaneous Home, Solar, Grid and Battery readings, plus optional Battery
SOC. Prefer the existing `entity` field for Home and typed card options for the
other IDs. Preserve the nine-field compact format; do not create device-wide
Power settings or another persistence service. Unconfigured inputs remain
unavailable with their nodes visible; do not invent zero-producing equipment.
All four power entity IDs are configurable; SOC is an optional separate percent
entity and never participates in the power balance.

The user's actual HA conventions are confirmed, matching v0.32:

| Input | Default canonical meaning after W/kW normalization |
| --- | --- |
| Solar (`S`) | Positive production, zero when idle. |
| Home (`H`) | Positive consumption, zero when idle. |
| Grid (`G`) | Positive importing, negative exporting. |
| Battery (`B`) | Positive discharging, negative charging. |
| Battery SOC | Independent percentage; optional, not a power input. |

Explicit data rules, retaining the existing validity/formatting proposal:

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
- **Polarity:** default to the confirmed signs above. Keep optional Grid and
  Battery inversion flags, both defaulting to false, for other installations.
  Apply each chosen inversion once after unit normalization, before status,
  balance and allocation; preserve the original HA value separately. Do not
  infer or auto-flip signs from a balance error, current state or SOC. Home and Solar
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

### Signed balance and visibility gate

For normalized, polarity-adjusted watts, the expected relationship is:

```text
H = S + G + B
R = S + G + B - H
source_total = max(S, 0) + max(G, 0) + max(B, 0)
sink_total   = max(H, 0) + max(-G, 0) + max(-B, 0)
T = max(BALANCE_ABSOLUTE_W, BALANCE_RELATIVE * max(source_total, sink_total))
```

Use full-precision normalized readings for `R` and `T`, before display rounding
or idle-band suppression. `R > 0` means excess measured supply; `R < 0` means
excess measured demand. Small noise within the Home/Solar zero band still has
nonnegative working capacities; negative values beyond it remain invalid.

Keep tolerance centrally tunable through named model design constants, initially
`MIN_BALANCE_TOLERANCE_W = 10 W` and `RELATIVE_BALANCE_TOLERANCE = 0.02`.
Phase 1 uses these approved initial values, subject to real-sensor validation,
not new persisted fields or web/HA controls.
The same constants must drive calculation and tests; do not scatter thresholds
among the card, modal and preview. The previously proposed 1 W idle band remains
subject to validation as well.

Inferred arrows are eligible only when all four power readings/units are valid,
live and `abs(R) <= T` (boundary inclusive). If any input is missing/invalid or
the residual exceeds `T`, hide **all inferred arrows**, including allocations
from an earlier valid snapshot. Keep independently valid node values and
Battery/Grid status visible; neither residual failure nor missing Solar/Home
turns known Battery/Grid states into Unavailable. Invalid SOC does not gate arrows.
Recompute on each relevant model update; no delay, synthetic replacement value
or extra subscription is required to wait for samples to become consistent.

This tolerates measurement differences, inverter losses and asynchronous HA
updates without asserting an atomic sample set. Within tolerance, leave residual
capacity unallocated rather than editing readings to force the equation. Above
tolerance, the valid readings remain useful while the allocation is withheld.
Do not infer a missing Home/Solar value merely because the other terms balance.

### Deterministic solar-first allocation

Four aggregate readings cannot uniquely identify actual pairwise routes. The
first implementation proposal is therefore **inferred visual allocation**, not
measured edge telemetry. Measured flow sensors could be a stronger future input
but would require an explicitly approved separate mapping; they are not required
by, or added to, this four-entity proposal.

After the gate passes, construct nonnegative working capacities. Readings within
the idle band contribute zero; retain their original values for numeric display:

```text
Sources: Solar production, Grid import, Battery discharge
Sinks:   Home consumption, Battery charge, Grid export
```

Grid import/export and Battery charge/discharge are mutually exclusive for each
signed input. Allocate in exactly this order:

1. Solar -> Home
2. Solar -> Battery
3. Solar -> Grid
4. Battery -> Home
5. Battery -> Grid
6. Grid -> Home
7. Grid -> Battery

For each edge, assign `min(remaining_source, remaining_sink)` and immediately
subtract it from **both** capacities. Each edge is nonnegative; the sum of all
outgoing edges never exceeds a source, and the sum of incoming edges never
exceeds a sink. No negative remainder, double counting, fabricated balancing
edge or simultaneous reciprocal Grid/Battery flow is allowed.

Recommend the same 1 W band for arrow visibility: a path is visible only when
its allocated value is greater than the band. An allocated sub-band remainder
is still deducted from capacity bookkeeping; hiding it must not release it for
another edge. This visibility threshold is part of the numerical proposal to
validate, not a separate animation or measurement claim.

This priority is a display convention, not proof of actual routing. Keep node
numbers as measured readings; do not display inferred edge values as measured
data. Label the modal's path interpretation as estimated using existing title/
caption styling and explain the policy in configurator help/public docs. Ports
and geometry remain unchanged by that explanation.

Keep all seven v0.32 paths and fixed ports regardless of active edges. Allocation
changes values/visibility only; it never moves ports or recomputes their positions.
Grid -> Battery traverses the exact Battery -> Grid centerline in reverse, with
the arrowhead reversed. Geometry is recalculated only for layout/profile changes.

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

Phases 1–3 are approved and implemented. Phase 4 requires explicit approval;
retain the remaining physical-validation/compatibility decisions below.
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

### Table-driven calculation tests to implement

These are specifications for future host/model tests, not tests already added
or an implemented Power feature. S/H/G/B mean Solar/Home/Grid/Battery. Readings
are watts after unit normalization but **before** selected sign inversions.
An em dash means a missing or invalid input, not zero. Invert G/B means the
corresponding compatibility flag is true; otherwise both flags are false.
Expected paths are visible allocations under the proposed 1 W band and
10 W/2% balance constants; all unlisted paths must be hidden. English secondary
text denotes the existing i18n-rendered status keys, Battery first.

| Case | Readings W `(S, H, G, B)` | Invert | Expected visible inferred paths, W | Secondary label |
| --- | --- | --- | --- | --- |
| Solar supplies Home | `(1000, 1000, 0, 0)` | None | S -> H: 1000 | `Idle` |
| Solar charges Battery | `(1500, 1000, 0, -500)` | None | S -> H: 1000; S -> B: 500 | `Charging` |
| Solar exports to Grid | `(1500, 1000, -500, 0)` | None | S -> H: 1000; S -> G: 500 | `Exporting` |
| Battery supplies Home | `(0, 1000, 0, 1000)` | None | B -> H: 1000 | `Discharging` |
| Battery exports to Grid | `(0, 0, -500, 500)` | None | B -> G: 500 | `Discharging · Exporting` |
| Grid supplies Home | `(0, 1000, 1000, 0)` | None | G -> H: 1000 | `Importing` |
| Grid charges Battery | `(0, 0, 500, -500)` | None | G -> B: 500 | `Charging · Importing` |
| Import and charging | `(300, 1000, 1000, -300)` | None | S -> H: 300; G -> H: 700; G -> B: 300 | `Charging · Importing` |
| Discharge and export | `(300, 700, -400, 800)` | None | S -> H: 300; B -> H: 400; B -> G: 400 | `Discharging · Exporting` |
| Solar charges and exports | `(1800, 1000, -300, -500)` | None | S -> H: 1000; S -> B: 500; S -> G: 300 | `Charging · Exporting` |
| All zero | `(0, 0, 0, 0)` | None | None | `Idle` |
| Near zero | `(0.5, 0.5, 0.5, -0.5)` | None | None | `Idle` |
| At 1 W idle boundary | `(0, 1, 1, 0)` | None | None | `Idle` |
| Above idle boundary | `(0, 1.1, 1.1, 0)` | None | G -> H: 1.1 | `Importing` |
| Small accepted imbalance | `(300, 295, 0, 0)` | None | S -> H: 295 | `Idle` |
| At absolute tolerance | `(100, 90, 0, 0)` | None | S -> H: 90 | `Idle` |
| Beyond absolute tolerance | `(100, 89, 0, 0)` | None | None | `Idle` |
| At negative absolute tolerance | `(90, 100, 0, 0)` | None | S -> H: 90 | `Idle` |
| Beyond negative absolute tolerance | `(89, 100, 0, 0)` | None | None | `Idle` |
| At relative tolerance | `(1200, 1176, 0, 0)` | None | S -> H: 1176 | `Idle` |
| Beyond relative tolerance | `(1200, 1175, 0, 0)` | None | None | `Idle` |
| At negative relative tolerance | `(1176, 1200, 0, 0)` | None | S -> H: 1176 | `Idle` |
| Beyond negative relative tolerance | `(1175, 1200, 0, 0)` | None | None | `Idle` |
| Large negative residual | `(500, 1000, 200, -100)` | None | None | `Charging · Importing` |
| Missing Solar | `(—, 1000, 700, 300)` | None | None | `Discharging · Importing` |
| Missing Home | `(500, —, -200, -300)` | None | None | `Charging · Exporting` |
| Missing Grid | `(500, 500, —, 0)` | None | None | `Unavailable` |
| Missing Battery | `(500, 1000, 500, —)` | None | None | `Importing` |
| Invalid Grid, active Battery | `(0, 1000, —, 1000)` | None | None | `Discharging` |
| Both status inputs missing | `(500, 500, —, —)` | None | None | `Unavailable` |
| Grid inversion | `(0, 1000, -1000, 0)` | G | G -> H: 1000 | `Importing` |
| Battery inversion | `(0, 1000, 0, -1000)` | B | B -> H: 1000 | `Discharging` |
| Both inversions | `(300, 1000, -1000, 300)` | G + B | S -> H: 300; G -> H: 700; G -> B: 300 | `Charging · Importing` |
| Opposite signs without inversion | `(300, 1000, -1000, 300)` | None | None | `Discharging · Exporting` |
| Sub-band allocation | `(1000, 999.5, 50, -50)` | None | S -> H: 999.5; G -> B: 49.5 | `Charging · Importing` |

Additional assertions/variants for this matrix:

- Run each valid numeric fixture with W input and equivalent kW input; canonical
  allocations, residual, validity and secondary state must be identical.
- Parameterize missing/invalid inputs with absent entities, `unknown`,
  `unavailable`, NaN/Inf, trailing numeric junk, unsupported Wh/kWh and overflow.
  Reject each without inventing zero or flipping polarity. Missing Home clears
  the card number; valid other nodes and independent statuses remain visible.
- Run valid cases with SOC omitted, 0%, 78%, 100%, unknown, -1% and 101%.
  SOC rendering changes as appropriate; power paths and summary do not.
- Absolute-boundary fixtures have `abs(R) = 10 W` versus `11 W`, with
  `T = 10 W`; relative-boundary fixtures have `24 W` versus `25 W`, with
  `T = 24 W`. Both positive and negative residual directions need coverage.
- The sub-band allocation fixture also assigns 0.5 W to Solar -> Battery,
  then 49.5 W to Grid -> Battery. The first arrow is hidden but its capacity
  deduction remains: the model must not allocate 50 W from Grid as well.
  Grid retains 0.5 W unallocated; Battery's complete incoming sum is 50 W.
- Assert every allocation is finite/nonnegative and incoming/outgoing sums are
  bounded by measured capacities. Check leftover capacities, not only flags;
  no residual is silently added to another path to force exact balance.
- Test valid -> invalid/unbalanced -> valid transitions: clear previously active
  arrows immediately, retain valid readings/status, then restore the correct
  paths on recovery without moving ports, restarting subscriptions or reopening.
- Check all seven paths separately and together as geometry-only fixtures,
  including reciprocal Grid/Battery traversal. This inferred model never
  enables both reciprocal directions at once; the prototype's "Show all" is
  only a topology stress test. Port positions must be invariant in every case.

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

1. Physically validate the initial 1 W idle/arrow band and centrally tunable
   tolerance constants of 10 W and 2% against actual sensor update cadence,
   measurement differences and inverter losses. Exact W/kW attributes/entity IDs
   still belong to the eventual card configuration, not guessed defaults.
2. Confirm the concise estimated-path explanation and imbalance/unavailable
   indication using existing modal styles, without disturbing v0.32 geometry.
3. Reconcile later upstream card/modal/color changes before opening an upstream
   PR. The minimal generic-label extraction from closed PR #2162 is complete;
   its Climate feature was not imported.

Card naming, Home value, dynamic Battery/Grid summary, existing fonts, static
seven-path topology and main/subpage support are approved. Sensor signs are now
confirmed, both optional inversion flags default to false, and the calculation
proposal is the seven-step solar-first inferred allocation specified above.
Do not reopen those input conventions or auto-correct them from a residual.
Measured-edge mapping and animations remain outside this initial proposal
unless separately approved.
Keep the fork-only PR Draft, leave `main` untouched and wait for explicit
Phase 4 approval on `feature/energy-dashboard` and that same PR.

## Phase 4A representative compile record (2026-10-14)

These are compile-skill representatives, not the user's physical devices.
Do not install their binaries on JC8012P4A1 or ESP32-P4-86.
All three factory configuration checks and fresh sequential compiles passed from
`c2b855863ed7c81c09081ccdc1652a314db98b8f`. No application/configuration changes,
upstream integration or hardware access were needed. A later notes-only commit
records these results; the binaries intentionally identify this verified source.

Repository pin: ESPHome 2026.9.1; resolved ESP-IDF v5.5.5; GNU 14.2.0
(esp-14.2.0_20260121, RISC-V P4 / Xtensa S3). Native image generation used esptool
4.12.0; independent image inspection used esptool 5.3.1. Each build used its
existing `builds/<target>.factory.yaml`, with the exact component ref above.
A fresh application build volume was used, not older application artifacts.

| Target | Application/OTA bytes | OTA slot bytes | Free bytes | Free |
| --- | ---: | ---: | ---: | ---: |
| `guition-esp32-p4-jc1060p470` | 5,364,544 | 7,274,496 | 1,909,952 | 26.26% |
| `guition-esp32-p4-jc4880p443` | 5,325,552 | 7,274,496 | 1,948,944 | 26.79% |
| `guition-esp32-s3-4848s040` | 4,954,032 | 7,274,496 | 2,320,464 | 31.90% |

Artifacts are local, ignored outputs (not Git/release assets). Actual Windows root:
`D:\Projects\espcontrol\.worktrees\energy-dashboard\build\power-dashboard-phase4a\c2b855863`.
Under each device-named subdirectory, the exact artifact names/checksums are:

| Filename (inside matching target directory) | Bytes | SHA-256 |
| --- | ---: | --- |
| `guition-esp32-p4-jc1060p470-pr10-c2b855863-ota.bin` | 5,364,544 | `855793d9e5dde54a15717cfa30e28f35ad1a573958d27633ffa54e4d29d5c4b5` |
| `guition-esp32-p4-jc1060p470-pr10-c2b855863-factory.bin` | 5,495,616 | `e045661343f5274740094f90e56384200408c1e8da7f6b76cca6ecd1e49b48a2` |
| `guition-esp32-p4-jc4880p443-pr10-c2b855863-ota.bin` | 5,325,552 | `a5f6c7ac2ebbe201e266b09ca93eb2502370dae27472e96d87aa0cc82e09f47a` |
| `guition-esp32-p4-jc4880p443-pr10-c2b855863-factory.bin` | 5,456,624 | `8b3becc09a399b9eed4a53f4f415951f7a169c76498a35fcc4222e476c4b1e31` |
| `guition-esp32-s3-4848s040-pr10-c2b855863-ota.bin` | 4,954,032 | `27e8e93d9d31933239b05c516a639a16ad94923976cf36b47f36d81f36b60e51` |
| `guition-esp32-s3-4848s040-pr10-c2b855863-factory.bin` | 5,085,104 | `ef15078a2a7f9847acc87e53dcd88f22b7a0ec82048f2e75af969e2f948ca5f0` |

`*-ota.bin` is the application-only OTA image; `*-factory.bin` is the merged
USB/UART image at flash base 0x0. Factory is not an OTA payload. All use 16 MB
flash, app0 at 0x20000, app1 at 0x710000, each 0x6f0000 bytes. Partition table,
factory application contents and OTA size were checked; factory and OTA contain
the same application. ESPTool image checksum/hash and Windows SHA-256 checks
passed. Critical compiled Power headers and embedded web asset matched the exact
Git source. Do not use these original P4 targets for the alternate V2/V3 models.

Each device directory contains configuration/compile logs, `manifest.json`,
`SHA256SUMS.txt`, partition/flash metadata and `ota-image-info.log`. Root manifest,
validation logs, `HARDWARE-TEST-CHECKLIST.md` and the adjacent
`power-dashboard-pr10-c2b855863.zip` support copying/retrieval with Explorer.
No public download URL is claimed.

Warnings match Phase 2 logs: P4 experimental-IDF warning; S3 GPIO19/GPIO20
USB-Serial-JTAG pin warnings. P4 bootloader has 6% free (separate from the ample
application OTA headroom); S3 bootloader 36%. No new Power-specific compile
warnings or firmware failures remain. Existing factory version is 0.0.0;
use artifact filename/checksum/source manifest to identify this test build.

Automated validation at c2b855863: `npm run prepare:ci` passed (85 firmware host
checks, 154 web unit tests, 113 web smoke checks, 260 serialization parity cases,
product/generated checks, docs and mocked installer). Focused CTest model/layout/
UI/tree/title tests: 5/5; focused Power web test passed; generated-browser Power
journeys passed all nine layouts, including main/subpage and restore-size guards.
`git diff --check` passed. These results do not establish physical correctness.

### Shared manual validation before Phase 4B

- Export a backup; use the correct OTA/factory file and hardware revision. Keep
  the branch build installed while testing (avoid official update actions).
- Card: Home W/kW, default/custom label, Battery-first/Grid-second status, Idle
  only for two known idle readings, fonts/spans and chevron. Preview placeholders
  must be distinguished from device measurements.
- Detail: Solar top/Home right/Grid left/Battery bottom; centering/proportions,
  SOC, all seven directed paths, fixed ports and exact reversed Grid/Battery
  centerline. No moving animation is expected.
- Navigation/theme: main and subpage; top-bar Power Dashboard replaces temperature
  and restores current content on close; repeat opens/closes; Dark/Light/effective
  Auto with view open/pressed; bright/dark Primary and readable foreground; no
  state/navigation reset.
- HA: confirm signs/units, real charging/discharging/import/export; reconnect and
  unknown inputs; keep valid readings/status and hide all inferred arrows for
  incomplete/inconsistent data. Recover without reopening.
- Assess the 1 W idle/arrow threshold and max(10 W, 2% of larger source/sink total)
  tolerance with real losses/asynchronous updates; record flicker/readings/timing.
- Save/reload and backup/restore under Light, including subpages. Confirm the
  size limit rejects unsupported main-grid configurations without truncation.
- Controlled disposable HA sensor-state fixtures and the fuller optional
  rotation/long-label/inversion/near-zero checklist are in the local test guide.
  Use these to exercise rare flow directions without actuator commands.

Physical testing is pending. Report hardware revision, artifact/checksum,
sensor conventions/readings, theme/page, screenshots and device logs. Do not
start Phase 4B upstream reconciliation until hardware feedback and approval.

## Phase 4A corrected physical-device record (2026-10-15)

The actual test devices are ESP32-P4-86 and the 10.1-inch JC8012P4A1 V2.
The user confirmed **non-v3 P4 silicon and rear-case marking >=2628**. This
selects `guition-esp32-p4-jc8012p4a1-v2`; no similar model is substituted.
Revision selection follows the current screen documentation:

| Evidence | Exact supported target |
| --- | --- |
| P4 v3.x production silicon, regardless of case date | `guition-esp32-p4-jc8012p4a1-v3` |
| Non-v3 silicon, case marking <=2627 | `guition-esp32-p4-jc8012p4a1` (V1/original; no suffix) |
| Non-v3 silicon, case marking >=2628 | `guition-esp32-p4-jc8012p4a1-v2` |

Use an existing ESPHome/esptool chip-information log and rear-case marking/photo;
chip revision takes priority over case date. These builds do not query hardware.
V3 also has different installation restrictions (USB initially, native ESPHome
OTA afterward; browser firmware upload disabled), so V2 artifacts must never
be treated as interchangeable. See `docs/screens/jc8012p4a1.md` and its V1/V2/V3
pages for authoritative revision guidance.

### Device-specific mandatory layout checks

- **JC8012P4A1 V2:** 10.1-inch, 1280x800 landscape, 5x4 / 20 cards. Check default
  and supported wider/taller Power spans, long primary/secondary labels, Home
  value and chevron without overlap. Check the wide modal's centering and
  proportions, Solar top/Home right/Grid left/Battery bottom, SOC, seven paths,
  three fixed ports per node and exact reversed Grid/Battery centerline.
- **ESP32-P4-86:** 4-inch, 720x720 square, 3x3 / 9 cards. Check compact wrapping/
  ellipsis, numeric/SOC clipping, chevron spacing, square-modal fit and touch
  targets. Target is the repository-supported ETH-2RO panel; normal factory
  profile uses Wi-Fi, not the optional Ethernet-only manual variant.
- On **both**, repeat main/subpage navigation and modal open/close; verify HUD
  title restoration, open/pressed Dark/Light/Auto switching, bright/dark accent,
  no value/subscription/navigation reset, reconnect, and Light-mode backup
  restore. Compare the same settled HA snapshot, validity recovery and reversible
  geometry. No animation is expected. The shared controlled-flow checklist above
  remains applicable; physical success is still pending.

Fresh outputs and the expanded local `HARDWARE-TEST-CHECKLIST.md` belong under
`build/power-dashboard-user-builds/4e49ca05e/`, separate from representatives.
Application source: `4e49ca05e07667c48be50336f81d239aca222c58`; a later notes-only
commit records results without changing the compiled Power code.

Both existing `builds/<target>.factory.yaml` configuration checks and fresh
sequential compiles passed with ESPHome 2026.9.1, ESP-IDF v5.5.5 and GCC 14.2.0.
No device/partition/application source changes were necessary.

| Actual target | OTA/app bytes | Factory bytes | OTA slot bytes | Free bytes | Free |
| --- | ---: | ---: | ---: | ---: | ---: |
| `esp32-p4-86` | 6,869,344 | 7,000,416 | 15,663,104 | 8,793,760 | 56.14% |
| `guition-esp32-p4-jc8012p4a1-v2` | 5,697,968 | 5,829,040 | 7,274,496 | 1,576,528 | 21.67% |

P4-86 uses 32 MB flash, app0 0x20000 / app1 0xf10000, each 0xef0000 bytes.
JC8012 V2 uses 16 MB, app0 0x20000 / app1 0x710000, each 0x6f0000 bytes.
The compiled partition tables match each target's authored CSV. OTA requires
that the installed layout already matches; it does not update partitions.
Factory files are merged USB/UART images at base 0x0, not OTA payloads.

All four files have independently verified Windows SHA-256 checksums. Both OTA
image checksum/hash checks passed, and each factory contains the identical
application and compiled partition table. Critical compiled Power headers and
embedded web assets match the exact source commit. Each target directory holds
`manifest.json`, `SHA256SUMS.txt`, configuration/compile logs, partition/flash
metadata and `ota-image-info.log`. Root manifest, checksum/report/checklist and
validation logs are included in `power-dashboard-user-pr10-4e49ca05e.zip` in the
parent output directory. No binaries/logs/build scripts are committed.

Existing P4 experimental-IDF warnings remain. P4-86's fresh configuration also
emitted CMake warnings (including missing kconfig); automatic reconfiguration
recovered, with no source change/build failure. These messages were not present
in the earlier representative logs, so they are recorded rather than labelled
pre-existing. Bootloader headroom is 4% for P4-86 and 6% for JC8012 V2, separate
from application headroom. No Power-specific compiler warning/failure was found.

`npm run prepare:ci` passed at 4e49ca05e: 85 host tests, 154 web unit tests,
113 smoke checks, 260 serialization parity cases, generated checks, docs and
mocked installer. No generated-source churn; `git diff --check` passed.
The local guide covers the two actual layouts, controlled seven-path cases,
theme/accent, validity/reconnect, navigation/lifecycle and save/restore.
Physical testing is pending; no flash, rebase or Phase 4B work was performed.

## Hardware refinement implementation (2026-10-15)

- Power reuses the native navigable card surface (`surface_card`), geometry,
  padding/radius, pressed accent, standard chevron object/font/profile offsets
  and shared secondary-label gutter. Borrowing the numeric widget no longer
  opts it into the passive sensor surface, including the theme refresh registry.
  It explicitly remains clickable, non-checkable and navigation-only; no HA
  service calls or duplicate data subscriptions are introduced.
- Full Power content requires two columns. Editor creation expands only into
  genuinely free cells, size menus exclude narrow spans, and save/restore
  validation rejects unsupported footprints before writes. Existing narrow
  firmware cards retain label-only navigation until safely resized, with no
  neighbor movement/deletion or runtime auto-expansion. Main/subpage geometry
  uses the actual profile column count. The preview mirrors this fallback.
- The subpage-entry chevron helper previously ignored all visibility/profile
  arguments and always hid the glyph. This was already present in upstream
  base a4062089, rather than introduced by the Power label helper. Restoring the
  setting-controlled helper exposes only intended navigation arrows; Power
  uses the same bottom-right profile alignment instead of a custom center arrow.
- Flow retains v0.32 topology, seven directions and exact reverse Grid/Battery
  centerline. Node centers now fit outward to the actual content rectangle and
  fixed measured text envelope. Ports depend on geometry/configured fonts,
  never active paths/readings/status/SOC availability. No animation/timer added.
- Heading icon/name sizing uses existing fonts and centered native layout.
  Single-line value/status rows and a separate SOC row reserve a stable four-row
  envelope. Activity and idle captions use `text_muted`; values/SOC/names remain
  primary. The permanent inferred-flow footer is removed. Existing exceptional
  and idle indicators use the back-control row; inferred provenance remains
  explicit in configurator help and public documentation.
- All Power numbers are W, including values >=1000 W; saved precision continues
  as decimal places in watts. Grid/Battery display absolute magnitude while
  signed model readings/allocation remain unchanged. The model's existing
  numeric bounds still protect the shared formatter.
- No palette, display correction, schema, compact fields, configuration version,
  device profile, font asset or allocation algorithm changed. Modal ownership,
  title restoration and card-owned subscriptions are retained. Physical recheck
  is required on ESP32-P4-86 and user-confirmed JC8012P4A1 V2 (non-v3, case >=2628).


### Refinement verification and physical retest builds

Final firmware source: `f6af57923e0c77435021b45a6d9086c2ef691982`.
The initial device compile exposed two `std::max(int, int32_t)` deductions
that host `int` doubles did not exercise. Explicit bounded pixel conversion
and host font/layout doubles using a distinct integer type reproduce the
failure before the fix and pass afterwards. No palette/geometry value changed.

`npm run prepare:ci` passed cleanly at this source: all 48 tasks, 85 firmware
host tests, 154 web unit tests, 113 smoke checks, 260 parity cases, generated
checks, docs and mocked installer. Four focused geometry/UI/theme-tree/HUD
tests passed again. Expanded Power browser journeys passed all nine layouts;
blocked expansion/paste and narrow save/restore reject before writes.
Only two reviewed Power preview fingerprints changed in the runtime fixture;
existing generators own the refreshed embedded/immutable web assets. Complete
contribution and working-tree `git diff --check` passed.

Fresh sequential factory config/compile checks passed with pinned ESPHome
2026.9.1, ESP-IDF v5.5.5 and GCC 14.2.0 for the actual devices:

| Device | OTA/app bytes | OTA slot bytes | Free bytes | Free |
| --- | ---: | ---: | ---: | ---: |
| ESP32-P4-86 | 6,871,920 | 15,663,104 | 8,791,184 | 56.13% |
| JC8012P4A1 V2 | 5,700,672 | 7,274,496 | 1,573,824 | 21.63% |

Factory sizes are 7,002,992 and 5,831,744 bytes respectively. User confirmation
remains non-v3 silicon/case >=2628 for V2; no device substitution. Existing
32 MB/16 MB partitions and device profiles are unchanged. Compiled Power/shared
headers and embedded web match the exact source; factory app/partition bytes,
OTA image checksum/hash, Windows SHA-256 and ZIP contents are verified.
Warning sets match the previous actual-device builds: experimental IDF,
RISC-V header sign conversions and recovered P4-86 CMake/kconfig warnings.
Bootloader headroom is 4%/6%, separate from the application figures.

Local artifacts: `build/power-dashboard-hardware-refinement/f6af57923/` with
target folders, OTA/factory binaries, manifests, SHA256SUMS, logs, validation
summary and `HARDWARE-TEST-CHECKLIST.md`. Adjacent archive:
`power-dashboard-refined-pr10-f6af57923.zip`. Previous physical-test binaries
are superseded for this refinement retest. OTA requires the existing compatible
partition layout; factory is the merged USB/UART image at offset 0x0.

Physical validation of the refined presentation remains pending on both
devices, especially native touch/pressed contrast, wide/legacy card layout,
parent subpage chevron, four-digit W/long labels, outward flow/SOC/status fit,
open-modal themes, HUD restoration and reconnect/rebuild cleanup. No device
was flashed, no rebase/main changes occurred, and Phase 4B has not started.


## Phase 4B: upstream reconciliation (2026-10-18)

The original base was `a406208906f900058da4c063d1e0c3eeeff9af6a`.
The verified upstream integration target is
`7c92df26cc8b95cd89780b366ae812bb8af169de` (93 commits later).
Merge upstream into the published feature branch; do not rebase or force-push.
Remote recovery tag `archive/energy-dashboard-pre-phase4b-20261018` preserves
`f09cf4dd6b0abaeffd106a3ef316059bf61a9818`. Fork/main remains unchanged.

| Overlap | Resolution |
| --- | --- |
| Card colors/contrast/corners | Retain upstream palettes, color options, contrast events and continuous corners; adapt Power consumers. |
| Parser/subpages | Strip/preserve upstream shared colors around Power normalization; no early return losing custom colors. |
| Weather/action dispatch/modal enum | Keep new forecast driver, lifecycle and modal alongside Power; neither replaces the other. |
| Theme refresh | Preserve upstream image/card behavior and Power ownership until shared contrast adaptation. |
| Web persistence/backup | Keep upstream native APIs, reset/session/PIN behavior and advertised byte capacity; layer Power placement/entity checks. |
| Resize handles | Reuse upstream handles; filter unsupported Power targets and retain commit-time neighbor/width checks. |
| Editor/preview CSS | Keep upstream foreground variables, color controls, naming and scaling, plus narrow Power content/layout extension. |
| Fonts/device generation | Reuse upstream small-text fields/helper and conditional catalog assignment; remove duplicate Power definitions. |
| Translations/contract/runtime inventory | Retain both upstream additions and Power definitions; regenerate with existing tools. |
| Native/browser tests | Keep upstream P4, weather, upload, speaker, PIN and lifecycle coverage together with Power cases. |
| Generated output | Rebuild contracts, i18n, device profiles, runtime fixtures, web assets and snapshot; do not copy stale feature output. |

Upstream #2237 deliberately increased card/subpage capacity to 4 KiB. Power
must use advertised capacity, preserving the 255-byte fallback for older firmware.
This supersedes earlier Phase 1-4A storage notes; the compact format stays nine
fields and oversized input is still rejected, never truncated. Public docs and
Power editor help now describe the current contract rather than imposing 255
bytes on new firmware. Added tests cover both advertised and legacy limits.

The generic secondary-label extension remains necessary: current upstream has
no equivalent shared helper. The setting-controlled subpage-chevron correction
is retained; no global unconditional-chevron rule is introduced. New upstream
forecast, media, unavailable sensor, display timing, web upload and connection
fixes are preserved. Windows CRLF import affected the captive-portal hash check;
normalizing the local HTML line endings and regenerating its existing headers
restored parity without changing provisioning behavior or weakening the test.


### Phase 4B measurement and geometry refinements

- Native measurement grouping: the previous Power-only reservation stretched
  the value label, detaching W. Keep value and container at `LV_SIZE_CONTENT`,
  limit the value's maximum width, and retain the shared sensor constructor,
  fonts, zero column padding, bottom alignment and existing unit padding.
  Host tests compile the actual constructor and upstream contrast functions;
  browser cases cover adjacent units/baselines at 9/923/1042/3500/35000 W.
- Reuse upstream `apply_button_colors`, contrast events and descendant sync.
  Remove Power's extra foreground-refresh callback/algorithm; Power registers
  only its navigation click. The theme runtime UI is now identical to upstream.
- Center reference is the actual measured LVGL panel content bounds after the
  native shell applies its HUD/screen-safe layout. Previously only the top Back
  row reduced the diagram, shifting its center by half that asymmetric inset.
  Keep one full-panel center and reserve measured Back/status clearance equally
  at top and bottom. This distinguishes the visual reference from drawable bounds;
  changing Back size cannot move the center. The shared modal shell is unchanged.
- Elbows retain v0.32 quadratic geometry with 16 bounded segments instead of 8,
  plus straight endpoints (19 points/path). Integer LVGL coordinates round to
  nearest pixel instead of truncating; supported float builds retain subpixels.
  The live path array grows by 448 bytes, geometry temporary by 896 bytes; no
  timers, LVGL objects, per-frame allocations or new rendering dependencies.
  Midpoint chord error at the maximum 40px radius is below 0.06px. Pixel-grid
  quantization and software rasterizer limitations still need physical review;
  this is not a claim of perfect antialiasing.
- Tests cover full-panel centering with multiple Back sizes, symmetric nodes,
  bounds on square/landscape/portrait profiles, bounded storage, fixed endpoints,
  exact reversible Grid/Battery buffers including integer rounding, HUD restore,
  themes/custom colors, subscriptions and repeated close/rebuild lifecycles.


The representative S3 compile exposed a silent merge duplication of upstream's
new small-text role. Removed Power's duplicate fields/helper/assignment; the
display-profile header is now byte-identical to upstream. The UI host test now
compiles actual production font-role and GridConfig definitions, reproducing the
duplicate failure before correction. Parser doubles include the native max-width
API, and browser resize checks use upstream handles rather than the retired menu.
Added explicit main/subpage custom-color round trips: 293 browser/firmware inputs.


### Phase 4B validation and physical-test delivery

Production source is frozen at `193c89b5f5aa22c477de16181c617eff56a7324b`;
only this validation record follows it. Upstream base remains
`7c92df26cc8b95cd89780b366ae812bb8af169de`. Integration uses a merge,
preserving the previous feature head and all 93 upstream commits. No force push,
main/history rewrite, upstream PR or device flashing was performed.

A clean locked-dependency `npm run prepare:ci` passed at that source: 49 CI
tasks, 131 host tests, 181 web unit tests, 113 web smoke checks, 293
firmware/browser serialization cases, generated checks, docs and mocked installer.
Focused Power native/layout/contrast/theme/HUD tests passed. The Power browser
journey passed all nine layouts; native/legacy quick colors, active colors,
subpage Back, inline card naming and camera dimming journeys also passed.

The full original browser suite still fails the weather height-anchor assertion.
An isolated pristine upstream web tree reproduces the identical geometry; extra
layout frames do not change either result. No unrelated weather production code
or assertion was changed, weakened or suppressed. This remains a reported
upstream browser-validation limitation, separate from the passing Power tests.

All factory entry points below passed configuration and sequential fresh-source
compiles with pinned ESPHome 2026.9.1, ESP-IDF v5.5.5 and GCC 14.2.0.

| Target | App/OTA bytes | OTA slot bytes | Free | Purpose |
| --- | ---: | ---: | ---: | --- |
| `guition-esp32-s3-4848s040` | 5,073,504 | 7,274,496 | 30.26% | representative compilation only |
| `esp32-p4-86` | 6,999,552 | 15,663,104 | 55.31% | actual device physical retest |
| `guition-esp32-p4-jc8012p4a1-v2` | 5,826,528 | 7,274,496 | 19.9% | actual device physical retest |
| `guition-esp32-p4-jc1060p470` | 5,494,176 | 7,274,496 | 24.47% | representative compilation only |
| `guition-esp32-p4-jc4880p443` | 5,456,832 | 7,274,496 | 24.99% | representative compilation only |

Actual-device artifacts are under
`build/power-dashboard-phase4b/193c89b5f/<device-slug>/`.
The adjacent `power-dashboard-phase4b-pr10-193c89b5f.zip` includes both actual
devices, OTA/factory images, manifests, SHA-256 sums, all build/configuration
logs, the compile matrix, validation evidence and a focused hardware checklist.
No binaries are tracked. Existing physical-test images are superseded.

OTA/app equality, factory application and partition content, authored partition
parity, compiled source/web hashes, offline esptool checksum/hash and independent
Windows SHA-256 were verified. Actual-device warning lines introduce none beyond
the preceding hardware builds; full warnings and bootloader/application headroom
are recorded in the local manifests. Working-tree and Power-delta whitespace
checks against the new upstream baseline passed; existing upstream vendor EOF
whitespace is not rewritten.

Retest P4-86 and confirmed non-v3-silicon JC8012P4A1 V2 before merging: native
adjacent W including five digits, two-column/main/subpage layouts, full-panel
centering and Solar/Battery SOC visibility, static elbows/arrowheads and exact
reversed Grid/Battery paths, current colors/pressed contrast, open-modal theme
changes, chevrons, HUD restoration, reconnect and backup/restore. Integer
software rendering still requires hardware inspection. Draft PR #10 stays open
and Draft; wait for hardware feedback before further implementation.
