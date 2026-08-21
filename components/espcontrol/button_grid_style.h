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

// Functional text drawn over user/state accent colors is intentionally stable
// across themes. It is not a semantic foreground token.
constexpr uint32_t FUNCTIONAL_ACTIVE_TEXT = 0xFFFFFF;
constexpr uint32_t FUNCTIONAL_DARK_TEXT = DEFAULT_TERTIARY_COLOR;

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

inline uint32_t resolve_semantic_theme_color(uint32_t stored,
                                             ThemeColorRole role) {
  const uint32_t dark = espcontrol::theme::palette_for(ActiveTheme::DARK).color(role);
  const uint32_t light = espcontrol::theme::palette_for(ActiveTheme::LIGHT).color(role);
  if (stored == dark || stored == light ||
      stored == correct_display_color(dark) ||
      stored == correct_display_color(light)) {
    return theme_color(role);
  }
  return stored;
}

class ThemeLvglStyles {
 public:
  static constexpr size_t ROLE_COUNT =
      static_cast<size_t>(ThemeColorRole::TRACK) + 1;

  void ensure_initialized() {
    initialized_ = true;
  }

  void update(ActiveTheme theme, bool report = true) {
    applied_theme_ref() = theme;
    if (!initialized_) return;
    for (size_t index = 0; index < ROLE_COUNT; ++index) {
      const auto role = static_cast<ThemeColorRole>(index);
      const lv_color_t color = lv_color_hex(theme_color(role));
      if (background_initialized_[index]) {
        lv_style_set_bg_color(&background_[index], color);
        if (report) lv_obj_report_style_change(&background_[index]);
      }
      if (text_initialized_[index]) {
        lv_style_set_text_color(&text_[index], color);
        if (report) lv_obj_report_style_change(&text_[index]);
      }
      if (border_initialized_[index]) {
        lv_style_set_border_color(&border_[index], color);
        if (report) lv_obj_report_style_change(&border_[index]);
      }
      if (arc_initialized_[index]) {
        lv_style_set_arc_color(&arc_[index], color);
        if (report) lv_obj_report_style_change(&arc_[index]);
      }
    }
  }

  lv_style_t *background(ThemeColorRole role) {
    ensure_initialized();
    const size_t index = static_cast<size_t>(role);
    if (!background_initialized_[index]) {
      lv_style_init(&background_[index]);
      background_initialized_[index] = true;
      lv_style_set_bg_color(&background_[index],
                            lv_color_hex(theme_color(role)));
    }
    return &background_[index];
  }
  lv_style_t *text(ThemeColorRole role) {
    ensure_initialized();
    const size_t index = static_cast<size_t>(role);
    if (!text_initialized_[index]) {
      lv_style_init(&text_[index]);
      text_initialized_[index] = true;
      lv_style_set_text_color(&text_[index], lv_color_hex(theme_color(role)));
    }
    return &text_[index];
  }
  lv_style_t *border(ThemeColorRole role) {
    ensure_initialized();
    const size_t index = static_cast<size_t>(role);
    if (!border_initialized_[index]) {
      lv_style_init(&border_[index]);
      border_initialized_[index] = true;
      lv_style_set_border_color(&border_[index],
                                lv_color_hex(theme_color(role)));
    }
    return &border_[index];
  }
  lv_style_t *arc(ThemeColorRole role) {
    ensure_initialized();
    const size_t index = static_cast<size_t>(role);
    if (!arc_initialized_[index]) {
      lv_style_init(&arc_[index]);
      arc_initialized_[index] = true;
      lv_style_set_arc_color(&arc_[index], lv_color_hex(theme_color(role)));
    }
    return &arc_[index];
  }

  void remove_background_styles(lv_obj_t *obj, lv_style_selector_t selector) {
    remove_initialized_styles(obj, background_, background_initialized_, selector);
  }
  void remove_text_styles(lv_obj_t *obj, lv_style_selector_t selector) {
    remove_initialized_styles(obj, text_, text_initialized_, selector);
  }
  void remove_border_styles(lv_obj_t *obj, lv_style_selector_t selector) {
    remove_initialized_styles(obj, border_, border_initialized_, selector);
  }
  void remove_arc_styles(lv_obj_t *obj, lv_style_selector_t selector) {
    remove_initialized_styles(obj, arc_, arc_initialized_, selector);
  }

 private:
  static void remove_initialized_styles(
      lv_obj_t *obj, std::array<lv_style_t, ROLE_COUNT> &styles,
      const std::array<bool, ROLE_COUNT> &initialized,
      lv_style_selector_t selector) {
    if (!obj) return;
    for (size_t index = 0; index < ROLE_COUNT; ++index) {
      if (initialized[index]) lv_obj_remove_style(obj, &styles[index], selector);
    }
  }

  bool initialized_{false};
  std::array<lv_style_t, ROLE_COUNT> background_{};
  std::array<lv_style_t, ROLE_COUNT> text_{};
  std::array<lv_style_t, ROLE_COUNT> border_{};
  std::array<lv_style_t, ROLE_COUNT> arc_{};
  std::array<bool, ROLE_COUNT> background_initialized_{};
  std::array<bool, ROLE_COUNT> text_initialized_{};
  std::array<bool, ROLE_COUNT> border_initialized_{};
  std::array<bool, ROLE_COUNT> arc_initialized_{};
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

inline bool theme_palette_background_matches(lv_color_t color,
                                             const espcontrol::theme::ThemePalette &palette) {
  return theme_color_matches(color, correct_display_color(palette.background)) ||
         theme_color_matches(color, correct_display_color(palette.surface)) ||
         theme_color_matches(color, correct_display_color(palette.surface_secondary)) ||
         theme_color_matches(color, correct_display_color(palette.control_background)) ||
         theme_color_matches(color, correct_display_color(palette.modal_background)) ||
         theme_color_matches(color, correct_display_color(palette.overlay)) ||
         theme_color_matches(color, correct_display_color(palette.track));
}

inline bool theme_text_has_functional_background(lv_obj_t *obj,
                                                 const espcontrol::theme::ThemePalette &from,
                                                 const espcontrol::theme::ThemePalette &to) {
  for (lv_obj_t *current = obj; current != nullptr; current = lv_obj_get_parent(current)) {
    if (lv_obj_has_state(current, LV_STATE_CHECKED)) return true;
    if (lv_obj_get_style_bg_opa(current, LV_PART_MAIN) != LV_OPA_COVER) continue;
    const lv_color_t color = lv_obj_get_style_bg_color(current, LV_PART_MAIN);
    if (!theme_palette_background_matches(color, from) &&
        !theme_palette_background_matches(color, to)) {
      return true;
    }
  }
  return false;
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
  const bool functional_text = theme_text_has_functional_background(obj, from, to);
  if (!functional_text && theme_color_matches(text, from.text_primary)) {
    lv_obj_set_style_text_color(obj, lv_color_hex(to.text_primary), LV_PART_MAIN);
  } else if (!functional_text && theme_color_matches(text, from.text_secondary)) {
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

inline void theme_style_background(lv_obj_t *obj, ThemeColorRole role,
                                   lv_style_selector_t selector = LV_PART_MAIN) {
  if (!obj) return;
  theme_lvgl_styles().remove_background_styles(obj, selector);
  lv_obj_remove_local_style_prop(obj, LV_STYLE_BG_COLOR, selector);
  lv_obj_add_style(obj, theme_lvgl_styles().background(role), selector);
}

inline void theme_style_text(lv_obj_t *obj, ThemeColorRole role,
                             lv_style_selector_t selector = LV_PART_MAIN) {
  if (!obj) return;
  theme_lvgl_styles().remove_text_styles(obj, selector);
  lv_obj_remove_local_style_prop(obj, LV_STYLE_TEXT_COLOR, selector);
  lv_obj_add_style(obj, theme_lvgl_styles().text(role), selector);
}

inline void theme_style_border(lv_obj_t *obj, ThemeColorRole role,
                               lv_style_selector_t selector = LV_PART_MAIN) {
  if (!obj) return;
  theme_lvgl_styles().remove_border_styles(obj, selector);
  lv_obj_remove_local_style_prop(obj, LV_STYLE_BORDER_COLOR, selector);
  lv_obj_add_style(obj, theme_lvgl_styles().border(role), selector);
}

inline void theme_style_arc(lv_obj_t *obj, ThemeColorRole role,
                            lv_style_selector_t selector = LV_PART_MAIN) {
  if (!obj) return;
  theme_lvgl_styles().remove_arc_styles(obj, selector);
  lv_obj_remove_local_style_prop(obj, LV_STYLE_ARC_COLOR, selector);
  lv_obj_add_style(obj, theme_lvgl_styles().arc(role), selector);
}

constexpr uint32_t readable_text_color_for_bg(uint32_t bg_color) {
  uint32_t red = (bg_color >> 16) & 0xFF;
  uint32_t green = (bg_color >> 8) & 0xFF;
  uint32_t blue = bg_color & 0xFF;
  uint32_t brightness = (red * 299 + green * 587 + blue * 114) / 1000;
  return brightness > 186 ? FUNCTIONAL_DARK_TEXT : FUNCTIONAL_ACTIVE_TEXT;
}

static_assert(readable_text_color_for_bg(0xFFFFFF) == FUNCTIONAL_DARK_TEXT,
              "light backgrounds need dark text");
static_assert(readable_text_color_for_bg(0x000000) == FUNCTIONAL_ACTIVE_TEXT,
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
