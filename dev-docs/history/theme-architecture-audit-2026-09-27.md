<!-- DEV-DOC-STATUS: historical -->

# Theme architecture audit — 2026-09-27

Investigation baseline: `upstream/main` at `4ae9685ad` (also fast-forwarded to
`origin/main`). This records the shipped dark presentation; it is not a theme
design or current operational guidance. Read the live firmware, source-of-truth
contract, and UI playbook before changing it.

## Executive Summary

The device has no single theme owner. ESPHome YAML creates pages and persistent
grid buttons. `common/theme/button.yaml` gives buttons an LVGL style and global
button theme. Device `lvgl.yaml` files make the main page black and the top bar
white. Shared setup/saver YAML repeats black, white, and grey. C++ then sets
local LVGL styles for card backgrounds, descendants, sliders, modals and state
changes. Local styles and callback updates can supersede the YAML base.

`button_grid_style.h` already names some dark values, but its accent and neutrals
are mixed with `CardPalette`, which carries the user-selected accent and two
fixed neutrals through the card drivers. Equivalent raw values occur independently
in YAML, C++, browser preview tokens, device files, and artwork presentation.
The blockers for multiple themes are these split sources, hardcoded standalone
screens, explicit descendant colors, and state callbacks that reapply colors
after construction. A palette declaration alone would not recolor a live UI.

## Architecture Map

```text
product/v2/device_catalog.json --generate_device_manifest.py--> devices/manifest.json
  --generate_device_slots.py--> generated sections of devices/*/packages.yaml
                                         and devices/*/device/sensors.yaml
authored devices/*/packages.yaml
  -> common/theme/button.yaml -> ESPHome LVGL `control` + button theme
  -> common/config/colors.yaml -> restored `button_on_color` text entity
  -> common/device/*.yaml + authored devices/*/device/lvgl.yaml
  -> ESPHome-created pages, persistent grid buttons, labels, setup screens
                                            |
saved Button N Config -> parse_cfg -> card_runtime_context -> grid_phase1
                                            | apply_button_colors + driver visual
                                            v
                                     actual LVGL objects
                                            ^
                              grid_phase2 / HA subscription callbacks
                              + interaction, availability, modal updates
```

`grid_phase1` and `grid_phase2` are in `button_grid_grid.h`. The former parses
the restored global on color, corrects it for the display, constructs a
`CardPalette`, and applies local default/checked/pressed button colors before
calling the selected visual driver. The latter builds it again, binds HA state,
and creates subpage objects. `common/config/colors.yaml` debounces live accent
changes and calls `apply_button_grid`. In contrast, YAML startup/setup/saver
pages do not pass through `CardPalette`.

```text
main page YAML black -> subpage copies main-page background
                        -> network/status page copies current page background
main-page first-card font/color/padding -> dynamic subpage/status cards
shared modal shell -> C++ tertiary panel + local control styles
                 -> card-specific modal updates + nested black 50% scrim
```

The browser's `src/webserver/application/styles.ts` CSS variables style the
configurator itself. `src/webserver/state/ui_tokens.ts` and preview code imitate
the device's orange/grey palette; they do not feed the firmware LVGL theme.

## Current Dark Palette

These are source RGB values before any per-profile color correction. A blank
semantic category is not evidence of an intentional palette value.

| Semantic role | Current value | Definition/source | Main consumers | Classification |
| --- | ---: | --- | --- | --- |
| Application/page background | `#000000` | authored device `device/lvgl.yaml` (`Black`); shared loading/setup/saver YAML | main, subpage copy, setup, clock, camera fallback | Theme-owned, except screen-off black |
| Primary card/control surface | `#313131` | `common/theme/button.yaml` `button_control_color`; `button_grid_style.h` `DEFAULT_SECONDARY_COLOR_RAW` | grid default, modal buttons, neutral controls | Theme-owned |
| Secondary modal surface | `#212121` | `button_grid_style.h` `DEFAULT_TERTIARY_COLOR_RAW` | shared modal/nested panels, information card background | Theme-owned |
| Additional setup/action surface | `#333333` | `common/device/screen_ha_actions.yaml` | HA action panel | Theme-owned; differs from `#313131` |
| Primary text | `#FFFFFF` | button YAML; `DARK_TEXT_PRIMARY`; device bar and setup YAML | card descendants, bar, screens, modals | Theme-owned |
| Muted text | `#B0B0B0` | `DARK_TEXT_MUTED`; setup/loading YAML | setup hints, modal metadata | Theme-owned |
| Camera fallback text | `#777777` | `screen_camera_screensaver.yaml` | camera fallback | Theme-owned candidate; differs from muted text |
| Disabled text | `#707070` | `card_availability.h` `DARK_TEXT_DISABLED` | disabled card descendants | Theme-owned state treatment |
| Inverted text | `#000000` | `DARK_TEXT_INVERTED` | some slider foreground states | Theme-owned, context dependent |
| Border/divider | `#000000` (button YAML, width `0`); `#313131` (`DARK_BORDER`) | two independent definitions | grid border (normally invisible), modal chrome | Theme-owned; semantic mismatch |
| Neutral control/track | `#313131` | `DARK_CONTROL_NEUTRAL`, `DARK_TRACK_BACKGROUND`, `SECONDARY_GREY` | sliders, arcs, inactive controls | Theme-owned; same RGB serves several jobs |
| Overlay/scrim | `#000000`, nested at 50% opacity | `DARK_OVERLAY` + `LV_OPA_50` in modal helpers | nested menus, alarm/image takeovers | Theme-owned with opacity/context |
| Selected/on/accent | `#FF8C00` default; restored six-character user value may replace it | button YAML, `colors.yaml`, `DEFAULT_PRIMARY_COLOR_RAW` | checked/pressed cards, slider fill, modal accent | User-configurable, not a neutral theme token |
| Checked/on foreground | `#FFFFFF` | `button_on_text_color` in YAML | checked grid/card descendants | Theme-owned foreground with accent contrast risk |
| Alarm triggered/error | `#C62828` | `ALARM_TRIGGERED_COLOR` | triggered alarm card, warning toast | Functional |
| Media error | `#FF6B6B` | `button_grid_media.h` | speaker error label | Functional |
| Cooling | `#2979FF` | `CLIMATE_COOLING_COLOR` | climate cooling controls | Functional |
| Cover-art text | `#FFFFFF` or `#FFF5E0`, by profile | authored `devices/*/packages.yaml` substitution, duplicated in `product/v2/device_catalog.json` | cover-art title/artist/progress | Content presentation/profile choice |

There is no verified global success or warning color in the device palette;
do not invent one in normalization. `#FFFFFF` is also used for QR modules,
artwork/clock overlays, and several selected controls with different reasons.
The cover-art fallback control uses `#313131`, while its pressed fallback is
`#585858`; these belong to that presentation, not the grid neutral by default.

## Color Ownership Inventory

| File/source | Responsibility | Authored/generated | Generator if applicable | Notes |
| --- | --- | --- | --- | --- |
| `common/theme/button.yaml` | LVGL `control` and global button default/pressed/checked styles | Authored | none | `$button_*` substitutions are compile-time YAML defaults |
| `common/config/colors.yaml` | Restored global `Button On Color` text entity and refresh script | Authored runtime config | none | `FF8C00` initial value; six-character persisted user value |
| `components/espcontrol/button_grid_style.h`, `card_availability.h` | Named dark constants, readable foreground heuristic, disabled labels | Authored C++ | none | `readable_text_color_for_bg` returns tertiary grey for bright backgrounds, white otherwise; threshold 186 |
| `components/espcontrol/button_grid_grid.h`, `button_grid_layout.h`, drivers and card/modal headers | Correct and carry colors; write local LVGL state/part styles | Authored C++ | none | `CardPalette` is built per grid pass, not a global semantic theme |
| `common/device/screen_*.yaml` | Loading, setup, clock, camera, cover-art and other standalone objects | Authored YAML | none | Color literals reach LVGL directly; some lambda C++ updates later |
| `devices/*/device/lvgl.yaml` | Main page, top bar and persistent button includes | Authored per-device YAML | none | Repeated black page and white HUD; geometry varies by profile |
| `devices/*/packages.yaml` | Includes shared YAML and profile substitutions | Authored file with generated blocks | `scripts/generate_device_slots.py` for marked blocks | Cover-art text substitution is authored outside generated blocks and duplicates catalog data |
| `devices/*/device/sensors.yaml` | Grid slots/config wiring | Authored file with generated blocks | `scripts/generate_device_slots.py` | Required check: `python3 scripts/generate_device_slots.py --check` |
| `product/v2/device_catalog.json` | Profile cover-art text and hardware/font/slot facts | Authored product source | `scripts/generate_device_manifest.py` produces `devices/manifest.json`; `scripts/generate_device_slots.py` produces marked slot blocks from manifest | Check manifest/slots with their `--check` commands and `npm run check:product`; package cover-art substitution is independently authored |
| `src/webserver/state/ui_tokens.ts`, `application/preview_render.ts` | Browser preview approximation | Authored web source | `python3 scripts/build.py www` produces bundles | Separate from device theme; `npm run check:web-smoke`/`check:web-asset-manifest` if changed |
| `product/v2/card_contract.json` | Card options such as `active_color`, not RGB palette | Authored | `python3 scripts/build.py contract` | Generated C++/TS contract and capability docs; check contract outputs/product |
| `product/v2/entity_names.json` | Name of `button_on_color` entity | Authored | `python3 scripts/build.py entities` | `common/config/entity_names.yaml` and web catalog are generated; check entities/product |
| `scripts/*` | Generators and color-related verification assertions | Authored tooling | none | No palette generator links YAML, C++ and preview values |

The exact generator/check ownership is in `dev-docs/source-of-truth.md`.
`docs/public/webserver/**` and generated C++/TS contract outputs are not edit
targets for a theme refactor. Numeric hex literals in storage, Unicode parsers,
CRC code, and test probes are not UI colors.

## Hardcoded Color Audit

- **Future theme candidates:** black main/setup/loading pages; white HUD and
  setup text; `#B0B0B0` hints; `#333333` HA action panel; C++ `DARK_TEXT_*`,
  `DARK_BORDER`, `DARK_CONTROL_NEUTRAL`, `DARK_TRACK_BACKGROUND`; tertiary modal
  panels; camera fallback `#777777`; disabled `#707070`; nested overlay black
  with its opacity. Verify contrast and inherited/local selectors separately.
- **Intentionally fixed:** screen-off page black is a display blanking behavior;
  QR dark/light modules are explicitly black/white in `wifi_qr_open_modal` and
  require their quiet zone. Clock/photo/image text shadows use translucent black
  against variable images. These need a contrast rule, not blanket remapping.
- **State/functional:** alarm red `#C62828`, media speaker error `#FF6B6B`,
  cooling blue `#2979FF`, checked/pressed/disabled states and brightness/cover
  states. Keep their meaning; review contrast on any future light surface.
- **Accent/user-configurable:** `#FF8C00` is an initial on color, not a fixed
  theme neutral. The saved `Button On Color` overrides it in grid passes;
  `on_pattern` creates a highlight from that color. `active_color` is a card
  option choosing an active-state fill, not a per-card RGB picker. The clock
  text is another persisted user color (`schedule_clock_text_color`, initially
  white). Light temperature and color controls derive fills from HA values.
- **Content-specific:** RGB565 artwork, image/camera pixels, cover-art sampled
  accents, artwork control colors and profile text choices stay with content.
  `cover_art.h::playback_icon_color` chooses black/white for minimum contrast
  across normal and pressed states.
- **Uncertain/design review:** fixed white text on arbitrary user on colors;
  `readable_text_color_for_bg` applies only at selected call sites and uses
  dark `TERTIARY_GREY` as its bright-background foreground. Modal selected
  labels sometimes force white. The `#313131` versus `#333333` and `#777777`
  versus `#B0B0B0` differences need visual intent confirmed before collapsing.

## Basic Toggle/Action Walkthrough

```text
Button N Config text entity (or native PanelConfig card string)
 -> button_grid_config_parser.h::parse_cfg
 -> button_grid_card_runtime.h::card_runtime_context
 -> button_grid_grid.h::grid_phase1 / grid_phase2
 -> button_grid_basic_action_driver.h::basic_action_driver_matches
 -> basic_action_driver_setup_visual -> button_grid_cards.h::setup_toggle_visual
 -> basic_action_driver_bind_main -> basic_action_driver_bind_toggle
 -> button_grid_subscriptions.h::subscribe_toggle_state
 -> button_grid_layout.h::set_card_checked_state
```

The saved card string owns entity, label, icon, optional on icon and sensor
behavior, not a local RGB on/off pair. `basic_action_driver_matches` accepts
toggle, action, alarm action, internal, push, screen lock and webhook drivers,
plus fan switch; option-select actions take another driver. The setup function
dispatches specialized visuals for some variants. For a plain toggle,
`setup_toggle_visual` chooses label/icon and optional sensor/unit text, but does
not set the button colors. An action card uses `setup_action_card`, local-action
or another specialized path and may add a push transition.

The persistent `button_${num}` comes from `common/device/button_widget.yaml`:
`styles: control` adds the YAML neutral fill; the global LVGL button theme
sets white text and orange checked/pressed fill. In phase 1,
`setup_card_visual` calls `apply_button_colors` first, writing local
`LV_PART_MAIN | DEFAULT/CHECKED/PRESSED` backgrounds from `CardPalette`.
The palette's on value is the parsed, profile-corrected `Button On Color` or
the default; off is profile-corrected `#313131`; sensor is profile-corrected
`#212121`. Optional stripes are derived from on color. The local state styles
can therefore override the YAML button theme. The YAML checked text remains
white. The labels inside the button are separate objects; explicit sensor
label color also starts white.

Phase 2 binds the entity via `basic_action_driver_bind_toggle` and
`subscribe_toggle_state`. The HA callback calls `set_card_checked_state`,
which changes `LV_STATE_CHECKED` and copies the button's resolved text color
into every descendant, then swaps the icon/sensor overlay. Friendly-name and
sensor subscriptions change text. Press/click handlers in `button_widget.yaml`
and `basic_action_driver_handle_main_click` send actions; a tap does not define
the durable on state. Unavailability applies `DARK_TEXT_DISABLED` to labels in
the disabled state (`card_availability.h`). Main-grid allocations and subpage
owner lifetimes differ; a future live theme update must include both.

There is no universal adaptive card foreground here. The existing
`readable_text_color_for_bg` is used for selected modal/choice controls and
confirmation, not for the basic toggle's arbitrary accent background.

## Styling Surface Matrix

| Surface | Base style source | Runtime overrides | Dark assumptions | Theme risk |
| --- | --- | --- | --- | --- |
| Main grid / normal card | Device `device/lvgl.yaml` black page; `button_widget.yaml` + YAML `control`/button theme | `grid_phase1` palette; driver; phase 2 HA callbacks | black page, grey tile, white descendants | High: YAML/C++ and live state split |
| Basic toggle/action | Same persistent button; `setup_toggle_visual` or action visual | `apply_button_colors`, checked-state/descendant sync, availability, optional stripes | white on arbitrary on color | High: contrast and selector precedence |
| Slider | Grid button then `setup_slider_visual`/`setup_slider_widget` in `button_grid_sliders.h`; fill child and transparent LVGL slider | HA percentage resizes fill; light temperature/color and media variants change fill; modal tracks/knobs | neutral `#313131`, white labels/handle | High: child styles and state updates |
| Climate card | Grid base; `climate_control_driver_setup_visual` and `setup_climate_control_button` | HA context, `climate_modal_arc_color`, mode/selection updates; shared modal shell | dark neutral track/panel, white and muted text | High: many local arc/selector styles |
| Media card | Grid base; `media_driver_setup_visual`/`setup_media_card`; shared modal | playback/artwork/subscriptions, progress/volume controls, speaker error | dark track/panel, white labels, muted artist | High: several modes and content exception |
| Modal | `control_modal_open_shell` creates top-layer overlay/panel; `control_modal_style_panel` uses tertiary | card-specific controls; nested scrim at black 50%; toast color | tertiary panel, grey buttons, white labels | High: panel and children have separate styles |
| Alarm UI | Grid + `alarm_driver_setup_visual`; alarm modal/page C++ | `alarm_apply_home_state` uses accent or triggered red; mode/PIN/arming updates | black takeover, white text, grey inactive | High: functional red plus dark chrome |
| Loading page | `screen_loading.yaml` or ethernet variant creates page | status text/visibility; no shared palette pass | black, white, muted grey | Medium: standalone YAML |
| Wi-Fi setup | `screen_wifi_setup.yaml` page/labels | network text and QR details; QR modules fixed black/white | black, white, muted grey | Medium; preserve QR contrast |
| Home Assistant setup | `screen_ha_setup.yaml`; action page in `screen_ha_actions.yaml` | connection/action status updates | black/white; action panel `#333333` | Medium |
| Ethernet setup | `screen_ethernet_setup.yaml` and `screen_loading_ethernet.yaml` in supported packages | Ethernet status text | black, white, muted grey | Medium: conditional device surface |
| Subpage/navigation | `button_grid_grid.h` creates screen and copies main page color; `create_grid_card_button`/`create_dynamic_card_slot` | palette and card drivers; HA callbacks; back button; clock bar | borrowed white text and grey tile | High: dynamic objects need updates |
| Network/status | Device HUD YAML; `network_status_open_modal` creates grid-like cards on copied page background | `network_status_refresh_page` updates quality/IP; icon/state helper changes glyphs | white HUD; fallback white, grey cards | Medium: color is partly borrowed |
| Clock/screensaver | `screen_clock.yaml` black clock and transparent image overlay | schedule text entity applies local color; shadow stays translucent black | black clock, white default text | Medium: user text color and image exception |
| Cover-art mode | `screen_cover_art.yaml` + `cover_art.h` + artwork decoder; catalog profile substitution | sampled accent/background, playback pressed color and adaptive icon | black fallback, profile white/warm text | High: content-driven colors should remain independent |

The ESPHome/LVGL button theme contributes only where an LVGL button is used;
page/label YAML and dynamically created plain `lv_obj_t` containers rely on
their own styles. A theme switch would need an explicit refresh contract for
persistent grid objects, dynamically created subpages/modals and visible
standalone YAML pages. This audit does not prescribe that contract.

## Existing Abstractions Worth Reusing

- `button_grid_style.h` centralizes several dark names and the foreground
  heuristic. It is a useful list of existing meanings, though the values and
  caller assumptions must be checked individually.
- `button_grid_layout.h::apply_button_colors` and
  `set_card_checked_state` are common entry points for grid state styles.
- `button_grid_grid.h::CardPalette` already carries corrected accent/off/sensor
  values through main, subpage and driver paths. Its propagation pattern is
  useful; its current fields are not a complete semantic theme.
- `control_modal_style_panel`, nested overlay helpers, and driver environments
  provide focused places to audit modal surfaces and state refresh.
- `display_correct_color` and the profile's channel percentages provide an
  existing hardware conversion boundary. Keep raw semantic RGB and corrected
  LVGL values distinct.

## Existing Abstractions That Should Not Become the Global Theme System

- `CardPalette` is per grid pass and card-oriented; it cannot reach setup,
  loading, HUD, clock or cover-art YAML. It also deliberately includes the
  user accent and a sensor-card neutral. Reusing it as a universal palette
  would conflate configuration with theme choice.
- `current_button_primary_color()` is a mutable latest-accent holder used by
  confirmation controls. It is not a mode or whole UI palette.
- `readable_text_color_for_bg()` is a two-outcome contrast heuristic, with a
  dark-theme tertiary fallback for bright fills. It does not style objects or
  guarantee contrast for all user-selected colors.
- Browser CSS variables and `WEB_UI_COLORS` govern browser UI/preview. They
  are an approximation and cannot own device LVGL appearance.
- Cover-art sampled accent, QR module colors, and alarm state colors have
  content/protocol/functional ownership, not global neutral-theme ownership.

## YAML/C++ Duplication

| Concept | Independent copies and effect | Synchronization |
| --- | --- | --- |
| Default accent `#FF8C00` | `button.yaml` theme, `colors.yaml` restored-entity initial value, `button_grid_style.h`, `WEB_UI_COLORS` browser preview | No generator links the values. Live grid uses the restored entity; YAML startup theme can differ if defaults drift. |
| Neutral card `#313131` | `button.yaml`, C++ `DEFAULT_SECONDARY_COLOR_RAW`/aliases, `WEB_UI_COLORS`; also artwork fallback | No shared generation; artwork use is a separate presentation decision. |
| Tertiary `#212121` | C++ `DEFAULT_TERTIARY_COLOR_RAW`, browser preview; setup action uses `#333333` instead | No automatic synchronization; difference may be intentional. |
| White/black | YAML button checked/text/border, per-device HUD/page, setup/savers; C++ `DARK_TEXT_PRIMARY`, `DARK_OVERLAY`, direct literals | No generator; button border is width zero while C++ `DARK_BORDER` is grey. |
| Cover-art text | Catalog profile `#FFFFFF`/`#FFF5E0` -> generated manifest; independently authored package substitution -> cover-art YAML | No generator copies the value into the package. Keep both authored sources in sync and run product and device-profile checks; only slot blocks are generated. |
| Browser preview | `WEB_UI_COLORS` and CSS `--screen-*` versus firmware values | Build generates web bundles from TS, but does not derive TS from firmware theme YAML/C++. |

The package includes are authored, while slot/sensor sections are generated.
`scripts/build.py --check` checks current outputs, but does not establish a
single source for the independently authored dark colors.

## Theme Exceptions

QR modules remain explicit black on white in `wifi_qr_open_modal`; changing
their colors risks scanner readability. Images, camera frames, and RGB565
artwork are content. User-selected global on color and schedule clock text
color survive mode changes unless a later design explicitly changes that
contract. HA light hue/temperature and artwork accents are derived from state
or content. Alarm triggered red, cooling blue, and media error pink need
contrast review against future surfaces; they are not dark neutrals.

Adaptive text exists only at specific controls (`readable_text_color_for_bg`,
`playback_icon_color`). The first currently uses a threshold and dark tertiary
grey; the second chooses black/white across normal and pressed artwork control
colors. Neither should be generalized without contrast tests.

`correct_display_color` in `display_color.h` multiplies RGB channels; the
profile variant in `button_grid_display.h` uses percentages from `GridConfig`.
`grid_phase1`/`grid_phase2` apply it to accent/off/sensor before making
`CardPalette`. The no-profile C++ constants currently use 100% multipliers.
The P4 example declares RGB display order; color order and RGB565 decoding
are hardware/content boundaries, not evidence for a device-specific theme.
The 4-inch S3 has a 16 MB flash, PSRAM and a 12% LVGL buffer with documented
internal-RAM pressure; the 7-inch P4 also uses 16 MB flash and PSRAM. Future
styles/object allocations and live recoloring must be budgeted and compiled on
the constrained S3 as well as a P4. No hardware-specific theme logic exists.

## Proposed Semantic Token Vocabulary

This is vocabulary for a **later** refactor, mapping to today's dark values,
not a new palette or implementation. Some roles intentionally share RGB.

| Proposed token | Current dark value | Evidence/boundary |
| --- | ---: | --- |
| `BACKGROUND` | `#000000` | main/setup/loading page |
| `SURFACE_PRIMARY` | `#313131` | default grid card/control |
| `SURFACE_SECONDARY` | `#212121` | modal panel/tertiary card |
| `TEXT_PRIMARY` | `#FFFFFF` | normal cards/setup/HUD |
| `TEXT_MUTED` | `#B0B0B0` | helper/status text |
| `TEXT_INVERTED` | `#000000` | certain filled controls |
| `TEXT_DISABLED` | `#707070` | disabled card labels |
| `BORDER` | `#313131` | visible C++ modal chrome; YAML button border is `#000000` at width 0 and needs separate review |
| `CONTROL_NEUTRAL` | `#313131` | inactive modal controls |
| `TRACK_BACKGROUND` | `#313131` | slider/arc tracks |
| `OVERLAY` | `#000000` with context opacity | nested scrim/takeover |

Keep accent (`#FF8C00` default or user value), functional colors, QR modules,
artwork, shadows, and schedule clock text outside this neutral vocabulary.
`#777777` camera fallback and `#333333` HA action panel need design review;
no token should silently replace them with a nearby existing value.

## Recommended Scope: Dark Theme Token Normalization

Objective: replace dark-specific/hardcoded theme concepts with semantic names
while preserving the current dark appearance and behavior. First record exact
rendered values and selectors for representative S3 and P4 surfaces. Then
normalize the shared YAML dark defaults and C++ theme-owned neutrals/text,
including standalone setup/loading/HUD and modal/slider paths. Keep the
accent value, functional states, artwork, QR, clock text user value and all
behavior unchanged. Do not collapse unresolved `#333333`/`#777777` differences
without a reviewed visual decision.

Likely authored files: `common/theme/button.yaml`, shared `common/device/screen_*.yaml`,
`components/espcontrol/button_grid_style.h`, `card_availability.h`, grid/layout,
modal, slider/climate/media/alarm/status helpers, and device `device/lvgl.yaml`.
Preview parity may later involve `src/webserver/state/ui_tokens.ts` and
`application/preview_render.ts`, but browser chrome is a separate theme.
`product/v2/device_catalog.json` only changes if a genuine profile fact changes.

Do not change compact card strings, `PanelConfig`, configuration service/store,
backup schema, `product_compatibility.json`, generated device blocks,
generated contract/i18n files or published web bundles directly. For any
authored inputs that do change, follow `dev-docs/source-of-truth.md` to run
their generators and checks; device blocks come from manifest/slot generators,
web bundles from `scripts/build.py www`. Follow `change-firmware-ui.md`:
firmware parser, modals, modal layouts, card runtime, display tokens and HA
bindings checks, plus generated-output and focused device/profile checks.
Compile affected S3/P4 firmware before publishing a code change. Test physical
contrast and state transitions separately from compilation.

Visual invariants: same raw RGB and effective display-corrected colors; same
normal/pressed/checked/disabled precedence; no changed opacity, gradients,
shadow, text wrapping, card geometry or modal lifetime; restored user on
color and schedule clock text still work; QR scans; artwork and HA light colors
remain content-driven. Risks are duplicated YAML/C++ values, descendant
foreground sync, existing foreground contrast on arbitrary accents, dynamic
subpages/modals and constrained S3 memory.

## Future Architecture Notes

The likely owner of a manual preference needs a decision. A restored ESPHome
select/text setting alongside `common/config/colors.yaml` is simple and is
already exposed through device entities, but backup/restore and native saves
would need explicit handling. `PanelConfig` V1 has bounded generic setting
records (`panel_config_document.h`), but firmware
`panel_config_text_bindings.cpp` currently projects only `button_order` and
`button_on_color`; adding a key is not automatically a working persistent
setting. A core configuration-service-owned setting is another possibility,
with codec, runtime adapter, migration, capacity and downgrade implications.
Do not pick a schema from this audit.

Any persistent selection would require coordinated changes in device settings,
`src/webserver/application/settings_page.ts`/appearance state, typed web state,
API/event handling, preview, backup import/export and native PanelConfig
handling as appropriate. `product/v2/product_compatibility.json` and
`dev-docs/compatibility-contract.md` protect saved strings and backups; a new
preference must have explicit default/import/downgrade behavior. The current
browser CSS theme is independent of this future device setting.

Time support already exists: `common/addon/time.yaml` configures SNTP and HA
time, restored timezone selection, and clock callbacks. `core_infra.yaml` has
authenticated HA connection/recovery and intervals; the card code has HA
state/attribute subscriptions. `backlight_schedule.yaml`, `backlight.h` and
`sun_calc.h` already compute on-device sunrise/sunset from timezone-derived
coordinates and schedule brightness. No `sun.sun` subscription or theme
scheduler/selector was found. Scheduled and sunrise/sunset theme switching
should be separate later tasks; reuse of backlight timing needs ownership and
failure-mode review rather than assuming brightness mode equals theme mode.

## Verification for this documentation baseline

| Check | Result |
| --- | --- |
| `python3 scripts/build.py --check` | Passed: all outputs up to date, with `PYTHONUTF8=1`, locked npm dependencies and network access for pinned MDI CSS. |
| `npm run check:dev-docs` | Passed after adding the required historical status marker. |
| `npm run docs:build` | Passed: 86 canonical pages, 59 FAQ answers, 20 redirects, links and anchors checked. |
| `npm run check:fast` | Attempted; stopped at `firmware-tests` because no C++ compiler is installed on this Windows host. Remaining fast tasks did not run. |

The initial generator attempt failed under Windows cp1252, then reached a
network restriction under UTF-8. The final run passed with the required
environment. The documentation check's validator passed on its first rerun,
but its runner could not write a check cache under `.git` in the sandbox;
the authorized rerun completed successfully. No production file changed, so
physical-device testing is not applicable.
