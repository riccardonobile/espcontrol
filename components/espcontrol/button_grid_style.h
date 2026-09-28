#pragma once

// Semantic colors for the current dark device UI. Keep RGB values in parity
// with authored common/theme/colors.yaml; no runtime theme selection exists.
// The default accent is user configuration, not part of the neutral palette.

constexpr uint32_t DEFAULT_ACCENT_COLOR_RAW = 0xFF8C00;
constexpr uint32_t THEME_BACKGROUND = 0x000000;
constexpr uint32_t THEME_SURFACE_PRIMARY_RAW = 0x313131;
constexpr uint32_t THEME_SURFACE_SECONDARY_RAW = 0x212121;

constexpr uint32_t DEFAULT_ACCENT_COLOR = correct_display_color(DEFAULT_ACCENT_COLOR_RAW);
constexpr uint32_t THEME_SURFACE_PRIMARY = correct_display_color(THEME_SURFACE_PRIMARY_RAW);
constexpr uint32_t THEME_SURFACE_SECONDARY = correct_display_color(THEME_SURFACE_SECONDARY_RAW);
constexpr uint32_t THEME_TEXT_PRIMARY = 0xFFFFFF;
constexpr uint32_t THEME_TEXT_INVERTED = 0x000000;
constexpr uint32_t THEME_TEXT_MUTED = 0xB0B0B0;
constexpr uint32_t THEME_TEXT_DISABLED = 0x707070;
constexpr uint32_t THEME_BORDER = THEME_SURFACE_PRIMARY;
constexpr uint32_t THEME_CONTROL_NEUTRAL = THEME_SURFACE_PRIMARY;
constexpr uint32_t THEME_TRACK_BACKGROUND = THEME_SURFACE_PRIMARY;
constexpr uint32_t THEME_OVERLAY = 0x000000;

constexpr uint32_t readable_text_color_for_bg(uint32_t bg_color) {
  uint32_t red = (bg_color >> 16) & 0xFF;
  uint32_t green = (bg_color >> 8) & 0xFF;
  uint32_t blue = bg_color & 0xFF;
  uint32_t brightness = (red * 299 + green * 587 + blue * 114) / 1000;
  return brightness > 186 ? THEME_SURFACE_SECONDARY : THEME_TEXT_PRIMARY;
}

static_assert(readable_text_color_for_bg(0xFFFFFF) == THEME_SURFACE_SECONDARY,
              "light backgrounds need dark text");
static_assert(readable_text_color_for_bg(0x000000) == THEME_TEXT_PRIMARY,
              "dark backgrounds need light text");

inline uint32_t &current_button_primary_color_ref() {
  static uint32_t color = DEFAULT_ACCENT_COLOR;
  return color;
}

inline void set_current_button_primary_color(uint32_t color) {
  current_button_primary_color_ref() = color;
}

inline uint32_t current_button_primary_color() {
  return current_button_primary_color_ref();
}
