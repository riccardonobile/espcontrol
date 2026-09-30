#pragma once

#include "theme_runtime_tree.h"

// ESPHome YAML creates these pages with the Dark compile-time substitutions.
// A bounded set of object references lets the normal registry refresh them
// without rebuilding their content or changing screen navigation.
struct ThemeStaticPageTargets {
  lv_obj_t *page = nullptr;
  lv_obj_t *labels[12]{};
  uint8_t roles[12]{};  // 1 = primary, 2 = muted
  uint8_t label_count = 0;
  lv_obj_t *actions[2]{};
  uint8_t action_count = 0;
};

inline ThemeStaticPageTargets (&theme_static_pages())[8] {
  static ThemeStaticPageTargets pages[8]{};
  return pages;
}

inline void theme_collect_static_labels(lv_obj_t *obj, ThemeStaticPageTargets &targets) {
  if (lv_obj_check_type(obj, &lv_button_class) && targets.action_count < 2 &&
      theme_color_matches(lv_obj_get_style_bg_color(obj, LV_PART_MAIN), DARK_THEME.setup_action))
    targets.actions[targets.action_count++] = obj;
  if (lv_obj_check_type(obj, &lv_label_class) && targets.label_count < 12) {
    const lv_color_t color = lv_obj_get_style_text_color(obj, LV_PART_MAIN);
    uint8_t role = 0;
    if (theme_color_matches(color, DARK_THEME.text_primary)) role = 1;
    else if (theme_color_matches(color, DARK_THEME.text_muted)) role = 2;
    if (role) {
      targets.labels[targets.label_count] = obj;
      targets.roles[targets.label_count++] = role;
    }
  }
  const uint32_t count = lv_obj_get_child_cnt(obj);
  for (uint32_t i = 0; i < count; ++i)
    theme_collect_static_labels(lv_obj_get_child(obj, i), targets);
}

inline void theme_apply_static_page(void *context, const ThemePalette &theme) {
  const auto &targets = *static_cast<ThemeStaticPageTargets *>(context);
  if (!targets.page) return;
  lv_obj_set_style_bg_color(targets.page, lv_color_hex(theme.background), LV_PART_MAIN);
  for (uint8_t i = 0; i < targets.label_count; ++i)
    lv_obj_set_style_text_color(targets.labels[i],
        lv_color_hex(targets.roles[i] == 1 ? theme.text_primary : theme.text_muted), LV_PART_MAIN);
  for (uint8_t i = 0; i < targets.action_count; ++i)
    lv_obj_set_style_bg_color(targets.actions[i], lv_color_hex(theme.setup_action), LV_PART_MAIN);
}

inline bool register_theme_static_page(lv_obj_t *page, bool include_labels = true) {
  if (!page) return false;
  ThemeStaticPageTargets *slot = nullptr;
  for (auto &entry : theme_static_pages())
    if (entry.page == page) { slot = &entry; break; }
  if (slot) return true;
  for (auto &entry : theme_static_pages())
    if (!entry.page) { slot = &entry; break; }
  if (!slot) return false;
  *slot = {};
  slot->page = page;
  if (include_labels) theme_collect_static_labels(page, *slot);
  if (!register_theme_refresh(page, theme_apply_static_page, slot)) {
    *slot = {};
    return false;
  }
  lv_obj_add_event_cb(page, [](lv_event_t *event) {
    lv_obj_t *owner = static_cast<lv_obj_t *>(lv_event_get_target(event));
    unregister_theme_refresh(owner);
    for (auto &entry : theme_static_pages())
      if (entry.page == owner) entry = {};
  }, LV_EVENT_DELETE, nullptr);
  apply_current_theme();
  return true;
}
