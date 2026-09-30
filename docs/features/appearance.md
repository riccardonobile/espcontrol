---
title: "Appearance Settings"
description:
  Choose the panel theme and customise its active colour.
---

# Appearance

Find these controls in **Settings → Appearance** on the [Setup](/features/setup) page. Your selection is saved on the panel and applies without a reboot. Existing panels default to Dark.

## Theme

- **Dark** and **Light** select a fixed appearance.
- **Schedule** uses the panel's local clock. Set **Light starts** and **Dark starts** in 24-hour `HH:MM` format. The Light period can cross midnight. If both times are the same, Dark remains active. If time is temporarily unavailable, the panel keeps its last appearance; a fresh boot falls back to Dark until time is known.
- **Sun** switches to Light at sunrise and Dark at sunset. It uses the panel's existing on-device sunrise/sunset calculation based on the configured timezone and its approximate location. Set the correct timezone under **Settings → Time**. Sun does not require a Home Assistant `sun.sun` entity. If time or solar data is unavailable, the panel keeps its last appearance; a fresh boot falls back to Dark.

The theme changes the panel's neutral backgrounds, controls and text. Artwork, camera images, cover art, QR codes, user-selected clock text and status colours keep their own colours.

## Active colour

- **Primary** — the colour cards show when an entity is active. Use the colour picker or type a colour code (for example, `FF8C00` for orange).
- Secondary inactive cards and tertiary information cards use fixed panel colours so setup stays simpler and modal styling remains consistent.

Disabled Home Assistant cards keep their background colour and show muted labels and icons. Their normal text and icon colours return when the card becomes available again.

Colour changes apply to the panel automatically after a brief pause (about 200 ms), plus the time needed to redraw the cards, including cards on subpages. You do not need to restart the panel. **Reset colours** applies the default colour in the same way.
