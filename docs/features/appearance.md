---
title: EspControl Appearance Settings
description:
  Choose the display theme and customise the primary colour on your EspControl panel.
---

# Appearance

These settings control the theme and active colour used across your panel. You'll find them in the **Settings** tab on the [Setup](/features/setup) page, under the **Appearance** section.

## Theme

- **Dark** — keeps EspControl's original dark appearance and is the default for existing and new configurations.
- **Light** — uses a light background and surfaces while preserving the same layout and accent colours.
- **Auto** — selects the active Dark or Light theme from either fixed times or the panel's calculated sunrise and sunset.

With **Auto > Time**, set **Light Theme Start** and **Dark Theme Start** using `HH:MM` local times. The range may cross midnight. If both times are equal or a value is invalid, the panel safely uses Dark.

With **Auto > Sunrise / Sunset**, the panel uses the same on-device solar calculation as automatic backlight control. Location comes from the timezone selected in [Time Settings](/features/clock); no Home Assistant sun entity or extra dependency is required. Dark is used until valid clock and solar data are available.

Theme changes apply while the panel is running without closing the current page or open controls. **Active Theme** is a read-only configurator diagnostic showing the theme currently in use.

## Active Colour

- **Primary** — the colour cards show when an entity is active. Use the colour picker or type a colour code (for example, `FF8C00` for orange).
- Secondary inactive cards and tertiary information cards use fixed panel colours so setup stays simpler and modal styling remains consistent.
