#pragma once

#include <array>

#include "espcontrol_app_core.h"

// Shared UI colour tokens for device-side LVGL rendering.
// Primary is user configurable; secondary and tertiary are fixed defaults.

constexpr uint32_t DEFAULT_PRIMARY_COLOR_RAW = 0xFF8C00;
constexpr uint32_t DEFAULT_SECONDARY_COLOR_RAW = 0x313131;
constexpr uint32_t DEFAULT_TERTIARY_COLOR_RAW = 0x212121;

constexpr uint32_t DEFAULT_SLIDER_COLOR = correct_display_color(DEFAULT_PRIMARY_COLOR_RAW);
constexpr uint32_t DEFAULT_OFF_COLOR = correct_display_color(DEFAULT_SECONDARY_COLOR_RAW);
constexpr uint32_t DEFAULT_TERTIARY_COLOR = correct_display_color(DEFAULT_TERTIARY_COLOR_RAW);

constexpr uint32_t SECONDARY_GREY = DEFAULT_OFF_COLOR;
constexpr uint32_t TERTIARY_GREY = DEFAULT_TERTIARY_COLOR;
constexpr uint32_t DARK_TEXT_PRIMARY = 0xFFFFFF;
constexpr uint32_t DARK_TEXT_INVERTED = 0x000000;
constexpr uint32_t DARK_TEXT_MUTED = 0xB0B0B0;
constexpr uint32_t DARK_TEXT_SOFT = DARK_TEXT_PRIMARY;
constexpr uint32_t DARK_BORDER = SECONDARY_GREY;
constexpr uint32_t DARK_CONTROL_NEUTRAL = SECONDARY_GREY;
constexpr uint32_t DARK_OVERLAY = 0x000000;
constexpr uint32_t DARK_TRACK_BACKGROUND = SECONDARY_GREY;

using ThemeColorRole = espcontrol::theme::ColorRole;
using ActiveTheme = espcontrol::theme::ActiveTheme;

inline ActiveTheme &applied_theme_ref() {
  static ActiveTheme theme = ActiveTheme::DARK;
  return theme;
}

inline uint32_t theme_color(ThemeColorRole role) {
  const uint32_t raw = espcontrol::theme::palette_for(applied_theme_ref()).color(role);
  switch (role) {
    case ThemeColorRole::TEXT_PRIMARY:
    case ThemeColorRole::TEXT_SECONDARY:
      return raw;
    default:
      return correct_display_color(raw);
  }
}

class ThemeLvglStyles {
 public:
  static constexpr size_t ROLE_COUNT =
      static_cast<size_t>(ThemeColorRole::TRACK) + 1;

  void ensure_initialized() {
    if (initialized_) return;
    for (size_t index = 0; index < ROLE_COUNT; ++index) {
      lv_style_init(&background_[index]);
      lv_style_init(&text_[index]);
      lv_style_init(&border_[index]);
      lv_style_init(&arc_[index]);
    }
    initialized_ = true;
    update(applied_theme_ref(), false);
  }

  void update(ActiveTheme theme, bool report = true) {
    applied_theme_ref() = theme;
    if (!initialized_) return;
    for (size_t index = 0; index < ROLE_COUNT; ++index) {
      const auto role = static_cast<ThemeColorRole>(index);
      const lv_color_t color = lv_color_hex(theme_color(role));
      lv_style_set_bg_color(&background_[index], color);
      lv_style_set_text_color(&text_[index], color);
      lv_style_set_border_color(&border_[index], color);
      lv_style_set_arc_color(&arc_[index], color);
      if (report) {
        lv_obj_report_style_change(&background_[index]);
        lv_obj_report_style_change(&text_[index]);
        lv_obj_report_style_change(&border_[index]);
        lv_obj_report_style_change(&arc_[index]);
      }
    }
  }

  lv_style_t *background(ThemeColorRole role) {
    ensure_initialized();
    return &background_[static_cast<size_t>(role)];
  }
  lv_style_t *text(ThemeColorRole role) {
    ensure_initialized();
    return &text_[static_cast<size_t>(role)];
  }
  lv_style_t *border(ThemeColorRole role) {
    ensure_initialized();
    return &border_[static_cast<size_t>(role)];
  }
  lv_style_t *arc(ThemeColorRole role) {
    ensure_initialized();
    return &arc_[static_cast<size_t>(role)];
  }

 private:
  bool initialized_{false};
  std::array<lv_style_t, ROLE_COUNT> background_{};
  std::array<lv_style_t, ROLE_COUNT> text_{};
  std::array<lv_style_t, ROLE_COUNT> border_{};
  std::array<lv_style_t, ROLE_COUNT> arc_{};
};

inline ThemeLvglStyles &theme_lvgl_styles() {
  static ThemeLvglStyles styles;
  return styles;
}

inline void apply_active_theme(ActiveTheme theme) {
  theme_lvgl_styles().ensure_initialized();
  theme_lvgl_styles().update(theme);
}

inline bool theme_color_matches(lv_color_t color, uint32_t expected) {
  return lv_color_eq(color, lv_color_hex(expected));
}

inline void theme_transition_tree(lv_obj_t *obj, ActiveTheme previous,
                                  ActiveTheme next) {
  if (!obj || previous == next || lv_obj_has_flag(obj, LV_OBJ_FLAG_USER_1)) return;
  const auto &from = espcontrol::theme::palette_for(previous);
  const auto &to = espcontrol::theme::palette_for(next);

  const lv_color_t background = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
  const lv_opa_t background_opa = lv_obj_get_style_bg_opa(obj, LV_PART_MAIN);
  if (background_opa == LV_OPA_COVER &&
      theme_color_matches(background, correct_display_color(from.background))) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(correct_display_color(to.background)),
                              LV_PART_MAIN);
  } else if (theme_color_matches(background,
                                 correct_display_color(from.surface_secondary))) {
    lv_obj_set_style_bg_color(
        obj, lv_color_hex(correct_display_color(to.surface_secondary)), LV_PART_MAIN);
  } else if (theme_color_matches(background,
                                 correct_display_color(from.surface))) {
    lv_obj_set_style_bg_color(obj, lv_color_hex(correct_display_color(to.surface)),
                              LV_PART_MAIN);
  }

  const lv_color_t text = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
  if (theme_color_matches(text, from.text_primary)) {
    lv_obj_set_style_text_color(obj, lv_color_hex(to.text_primary), LV_PART_MAIN);
  } else if (theme_color_matches(text, from.text_secondary)) {
    lv_obj_set_style_text_color(obj, lv_color_hex(to.text_secondary), LV_PART_MAIN);
  }

  const lv_color_t border = lv_obj_get_style_border_color(obj, LV_PART_MAIN);
  if (theme_color_matches(border, correct_display_color(from.border))) {
    lv_obj_set_style_border_color(obj, lv_color_hex(correct_display_color(to.border)),
                                  LV_PART_MAIN);
  }

  const lv_color_t arc = lv_obj_get_style_arc_color(obj, LV_PART_MAIN);
  if (theme_color_matches(arc, correct_display_color(from.track))) {
    lv_obj_set_style_arc_color(obj, lv_color_hex(correct_display_color(to.track)),
                               LV_PART_MAIN);
  }

  const int32_t child_count = static_cast<int32_t>(lv_obj_get_child_cnt(obj));
  for (int32_t index = 0; index < child_count; ++index) {
    theme_transition_tree(lv_obj_get_child(obj, index), previous, next);
  }
}

inline void theme_remove_role_styles(lv_obj_t *obj, lv_style_t *(ThemeLvglStyles::*style_getter)(ThemeColorRole),
                                     lv_style_selector_t selector) {
  if (!obj) return;
  auto &styles = theme_lvgl_styles();
  for (size_t index = 0; index < ThemeLvglStyles::ROLE_COUNT; ++index) {
    lv_obj_remove_style(obj, (styles.*style_getter)(static_cast<ThemeColorRole>(index)), selector);
  }
}

inline void theme_style_background(lv_obj_t *obj, ThemeColorRole role,
                                   lv_style_selector_t selector = LV_PART_MAIN) {
  if (!obj) return;
  theme_remove_role_styles(obj, &ThemeLvglStyles::background, selector);
  lv_obj_remove_local_style_prop(obj, LV_STYLE_BG_COLOR, selector);
  lv_obj_add_style(obj, theme_lvgl_styles().background(role), selector);
}

inline void theme_style_text(lv_obj_t *obj, ThemeColorRole role,
                             lv_style_selector_t selector = LV_PART_MAIN) {
  if (!obj) return;
  theme_remove_role_styles(obj, &ThemeLvglStyles::text, selector);
  lv_obj_remove_local_style_prop(obj, LV_STYLE_TEXT_COLOR, selector);
  lv_obj_add_style(obj, theme_lvgl_styles().text(role), selector);
}

inline void theme_style_border(lv_obj_t *obj, ThemeColorRole role,
                               lv_style_selector_t selector = LV_PART_MAIN) {
  if (!obj) return;
  theme_remove_role_styles(obj, &ThemeLvglStyles::border, selector);
  lv_obj_remove_local_style_prop(obj, LV_STYLE_BORDER_COLOR, selector);
  lv_obj_add_style(obj, theme_lvgl_styles().border(role), selector);
}

inline void theme_style_arc(lv_obj_t *obj, ThemeColorRole role,
                            lv_style_selector_t selector = LV_PART_MAIN) {
  if (!obj) return;
  theme_remove_role_styles(obj, &ThemeLvglStyles::arc, selector);
  lv_obj_remove_local_style_prop(obj, LV_STYLE_ARC_COLOR, selector);
  lv_obj_add_style(obj, theme_lvgl_styles().arc(role), selector);
}

constexpr uint32_t readable_text_color_for_bg(uint32_t bg_color) {
  uint32_t red = (bg_color >> 16) & 0xFF;
  uint32_t green = (bg_color >> 8) & 0xFF;
  uint32_t blue = bg_color & 0xFF;
  uint32_t brightness = (red * 299 + green * 587 + blue * 114) / 1000;
  return brightness > 186 ? TERTIARY_GREY : DARK_TEXT_PRIMARY;
}

static_assert(readable_text_color_for_bg(0xFFFFFF) == TERTIARY_GREY,
              "light backgrounds need dark text");
static_assert(readable_text_color_for_bg(0x000000) == DARK_TEXT_PRIMARY,
              "dark backgrounds need light text");

inline uint32_t &current_button_primary_color_ref() {
  static uint32_t color = DEFAULT_SLIDER_COLOR;
  return color;
}

inline void set_current_button_primary_color(uint32_t color) {
  current_button_primary_color_ref() = color;
}

inline uint32_t current_button_primary_color() {
  return current_button_primary_color_ref();
}
