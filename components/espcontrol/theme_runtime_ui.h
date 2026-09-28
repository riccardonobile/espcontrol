#pragma once

// Runtime LVGL targets for the palette refresh boundary. The ESPHome YAML
// creates these persistent objects; the C++ theme updates them in place when
// the active palette changes. Dark -> Dark leaves their existing styles alone.

struct ThemeGridTargets {
  lv_obj_t *main_page = nullptr;
  lv_obj_t *buttons[MAX_GRID_SLOTS]{};
  bool neutral_buttons[MAX_GRID_SLOTS]{};
  int count = 0;
  int red_percent = 100;
  int green_percent = 100;
  int blue_percent = 100;
};

inline ThemeGridTargets &theme_grid_targets() {
  static ThemeGridTargets targets;
  return targets;
}

inline uint32_t theme_grid_correct_color(uint32_t raw_rgb,
                                         const ThemeGridTargets &targets) {
  return correct_display_color(raw_rgb, targets.red_percent,
                               targets.green_percent, targets.blue_percent);
}

inline void theme_apply_grid_button(lv_obj_t *button, uint32_t neutral,
                                    const ThemePalette &theme) {
  if (!button) return;
  // Change only the neutral/default fill. Checked and pressed backgrounds are
  // owned by the persisted user accent and the card's state callbacks.
  lv_obj_set_style_bg_color(button, lv_color_hex(neutral), LV_PART_MAIN);
  lv_obj_set_style_text_color(button, lv_color_hex(theme.text_primary), LV_PART_MAIN);
  lv_obj_set_style_text_color(button, lv_color_hex(theme.text_primary),
                              static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(button, lv_color_hex(theme.text_primary),
                              static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_PRESSED);
  sync_card_checked_text_color(button);
  set_card_content_disabled(button, lv_obj_has_state(button, LV_STATE_DISABLED));
}

inline void theme_apply_grid(void *context, const ThemePalette &theme) {
  auto &targets = *static_cast<ThemeGridTargets *>(context);
  if (!targets.main_page) return;
  lv_obj_set_style_bg_color(targets.main_page, lv_color_hex(theme.background), LV_PART_MAIN);
  const uint32_t neutral = theme_grid_correct_color(theme.surface_primary, targets);
  for (int i = 0; i < targets.count; ++i) {
    if (targets.neutral_buttons[i])
      theme_apply_grid_button(targets.buttons[i], neutral, theme);
  }
  for (auto &entry : navigation_subpages()) {
    if (!entry.screen) continue;
    lv_obj_set_style_bg_color(entry.screen, lv_color_hex(theme.background), LV_PART_MAIN);
    theme_apply_grid_button(entry.back_button, neutral, theme);
    for (auto &card : entry.cards) {
      if (card.neutral_background)
        theme_apply_grid_button(card.button, neutral, theme);
    }
  }
}

inline void register_theme_grid(lv_obj_t *main_page, BtnSlot *slots,
                                const bool *neutral_buttons, int count,
                                int red_percent, int green_percent,
                                int blue_percent) {
  if (!main_page || !slots || !neutral_buttons) return;
  auto &targets = theme_grid_targets();
  const bool new_owner = targets.main_page != main_page;
  targets.main_page = main_page;
  targets.count = bounded_grid_slots(count);
  targets.red_percent = red_percent;
  targets.green_percent = green_percent;
  targets.blue_percent = blue_percent;
  for (int i = 0; i < targets.count; ++i) {
    targets.buttons[i] = slots[i].btn;
    targets.neutral_buttons[i] = neutral_buttons[i];
  }
  for (int i = targets.count; i < MAX_GRID_SLOTS; ++i) targets.buttons[i] = nullptr;
  register_theme_refresh(main_page, theme_apply_grid, &targets);
  if (new_owner) {
    lv_obj_add_event_cb(main_page, [](lv_event_t *event) {
      unregister_theme_refresh(lv_event_get_target(event));
      theme_grid_targets().main_page = nullptr;
    }, LV_EVENT_DELETE, nullptr);
  }
  apply_current_theme();
}

struct ThemeHudTargets {
  lv_obj_t *temperature = nullptr;
  lv_obj_t *time = nullptr;
  lv_obj_t *network = nullptr;
  lv_obj_t *night = nullptr;
};

inline ThemeHudTargets &theme_hud_targets() {
  static ThemeHudTargets targets;
  return targets;
}

inline void theme_apply_hud(void *context, const ThemePalette &theme) {
  const auto &targets = *static_cast<ThemeHudTargets *>(context);
  lv_obj_t *labels[] = {targets.temperature, targets.time, targets.network, targets.night};
  for (lv_obj_t *label : labels) {
    if (label) lv_obj_set_style_text_color(label, lv_color_hex(theme.text_primary), LV_PART_MAIN);
  }
}

inline void register_theme_hud(lv_obj_t *temperature, lv_obj_t *time,
                               lv_obj_t *network, lv_obj_t *night) {
  auto &targets = theme_hud_targets();
  const bool new_owner = targets.time != time;
  targets = {temperature, time, network, night};
  if (!time) return;
  register_theme_refresh(time, theme_apply_hud, &targets);
  if (new_owner) {
    lv_obj_add_event_cb(time, [](lv_event_t *event) {
      unregister_theme_refresh(lv_event_get_target(event));
      theme_hud_targets() = {};
    }, LV_EVENT_DELETE, nullptr);
  }
  apply_current_theme();
}
