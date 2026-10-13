#pragma once

#include "power_card_data.h"
#include "power_flow_layout.h"

struct PowerCardCtx;
inline PowerCardCtx *&power_card_contexts() { static PowerCardCtx *head = nullptr; return head; }
inline void power_flow_hide();
inline void power_flow_update(PowerCardCtx *ctx);

struct PowerCardCtx {
  BtnSlot slot{};
  DisplayProfile display{};
  espcontrol::power::CardData data{};
  int precision = 0;
  PowerCardCtx *next = nullptr;
  ~PowerCardCtx();
};
inline bool power_set_text(lv_obj_t *label, const std::string &text) {
  if (!label || text == lv_label_get_text(label)) return false;
  lv_label_set_display_text(label, text.c_str());
  return true;
}
inline std::string power_reading_text(espcontrol::power::Reading reading, int precision,
                                      bool with_unit = true) {
  if (!reading.valid()) return "--";
  const bool kilo = std::abs(reading.value) >= 1000;
  char text[32];
  format_fixed_decimal(text, sizeof(text), static_cast<float>(reading.value / (kilo ? 1000 : 1)), precision);
  return std::string(text) + (with_unit ? (kilo ? " kW" : " W") : "");
}
inline PowerCardCtx *power_card_for_button(lv_obj_t *button) {
  for (auto *ctx = power_card_contexts(); ctx; ctx = ctx->next)
    if (ctx->slot.btn == button) return ctx;
  return nullptr;
}
inline void power_card_apply_theme_for_button(lv_obj_t *button) {
  auto *ctx = power_card_for_button(button);
  if (!ctx) return;
  const bool active = lv_obj_has_state(button, LV_STATE_CHECKED) ||
                      lv_obj_has_state(button, LV_STATE_PRESSED);
  const bool custom = lv_obj_has_flag(button, LV_OBJ_FLAG_USER_1) &&
                      !lv_obj_has_flag(button, LV_OBJ_FLAG_USER_3);
  if (!custom) {
    // A pressed accent can hide the default fill from tree restyling. Keep
    // Power's explicit neutral role current without changing state selectors.
    lv_obj_set_style_bg_color(button,
      lv_color_hex(display_correct_color(current_theme().surface_sensor, ctx->display)), LV_PART_MAIN);
  }
  const auto rgb = lv_color_to_32(lv_obj_get_style_bg_color(button, LV_PART_MAIN), LV_OPA_COVER);
  const uint32_t background = (uint32_t(rgb.red) << 16) | (uint32_t(rgb.green) << 8) | rgb.blue;
  const uint32_t text = active || custom
    ? readable_text_color_for_bg(background)
    : current_theme().text_primary;
  for (auto *label : {ctx->slot.sensor_lbl, ctx->slot.unit_lbl, ctx->slot.text_lbl,
                     ctx->slot.secondary_lbl, ctx->slot.subpage_lbl}) {
    if (label) lv_obj_set_style_text_color(label, lv_color_hex(text), LV_PART_MAIN);
  }
}
inline void power_card_update(PowerCardCtx *ctx) {
  if (!ctx) return;
  const auto &home = ctx->data.model.calculation.nodes[1];
  bool changed = power_set_text(ctx->slot.sensor_lbl, power_reading_text(home, ctx->precision, false));
  changed |= power_set_text(ctx->slot.unit_lbl, home.valid() ? (std::abs(home.value) >= 1000 ? "kW" : "W") : "");
  changed |= power_set_text(ctx->slot.secondary_lbl, ctx->data.model.calculation.secondary_label());
  if (changed) {
    layout_button_label_with_secondary(ctx->slot, true, true);
  }
  power_card_apply_theme_for_button(ctx->slot.btn);
  power_flow_update(ctx);
  if (changed) notify_dashboard_content_changed();
}
inline void power_cards_disconnected() {
  for (auto *ctx = power_card_contexts(); ctx; ctx = ctx->next) {
    ctx->data.disconnect();
    power_card_update(ctx);
  }
}
inline void power_cards_refresh_theme() {
  for (auto *ctx = power_card_contexts(); ctx; ctx = ctx->next)
    power_card_apply_theme_for_button(ctx->slot.btn);
}

// A single temporary view borrows the live card model, never its subscriptions.
// Point buffers live until the overlay is deleted; no canvas/frame buffer/timer.
struct PowerFlowUi {
  PowerCardCtx *card = nullptr;
  ControlModalShell shell{};
  lv_obj_t *area = nullptr;
  lv_obj_t *status = nullptr;
  lv_obj_t *nodes[4]{};
  lv_obj_t *headings[4]{};
  lv_obj_t *names[4]{};
  lv_obj_t *icons[4]{};
  lv_obj_t *values[4]{};
  lv_obj_t *details[4]{};
  lv_obj_t *lines[7]{};
  lv_obj_t *arrows[7]{};
  lv_point_precise_t points[7][espcontrol::power::FLOW_POINT_COUNT]{};
  lv_point_precise_t heads[7][3]{};
  bool laying_out = false;
};
inline PowerFlowUi &power_flow_ui() { static PowerFlowUi ui; return ui; }
inline void power_flow_hide() {
  auto &ui = power_flow_ui();
  ui.card = nullptr;
  control_modal_delete_overlay(ControlModalKind::POWER_FLOW, ui.shell.overlay);
  ui = {};
}
inline PowerCardCtx::~PowerCardCtx() {
  if (power_flow_ui().card == this) power_flow_hide();
  ha_release_callbacks_for_owner(this);
  auto **entry = &power_card_contexts();
  while (*entry && *entry != this) entry = &(*entry)->next;
  if (*entry) *entry = next;
}
inline void power_flow_set_visible(lv_obj_t *obj, bool visible) {
  if (!obj) return;
  if (visible) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}
inline void power_flow_layout() {
  auto &ui = power_flow_ui();
  if (!ui.card || !ui.area || ui.laying_out) return;
  ui.laying_out = true;
  const auto layout = control_modal_calc_layout(display_main_width_percent(ui.card->display));
  control_modal_apply_panel_layout(ui.shell.overlay, ui.shell.panel, layout,
                                   control_modal_card_radius(ui.card->slot.btn));
  control_modal_style_chrome_button(ui.shell.close_btn, layout, false);
  // Use native content bounds below the back control; HUD already has its
  // reserved panel top. Explanation occupies one existing-label-font line.
  const auto *font = display_volume_label_font(ui.card->display,
    lv_obj_get_style_text_font(ui.card->slot.text_lbl, LV_PART_MAIN));
  int status_h = font ? font->line_height + 2 : 22;
  const int top = layout.back_inset_y + layout.back_size + layout.title_gap;
  const int width = std::max<int>(1, layout.panel_w - layout.inset * 2);
  int height = std::max<int>(1, layout.panel_h - top - layout.inset - status_h);
  auto geometry = espcontrol::power::flow_geometry(width, height,
    espcontrol::power::flow_family(layout.sw, layout.sh));
  // Three native text rows must fit between the fixed top/bottom ports.
  // Reuse the existing compact font rather than adding/scaling font assets.
  if (font && font->line_height > 36 * geometry.scale) {
    font = display_small_text_font(ui.card->display, font);
    status_h = font->line_height + 2;
    height = std::max<int>(1, layout.panel_h - top - layout.inset - status_h);
    geometry = espcontrol::power::flow_geometry(width, height,
      espcontrol::power::flow_family(layout.sw, layout.sh));
  }
  lv_obj_set_size(ui.area, width, height);
  lv_obj_set_pos(ui.area, layout.inset, top);
  lv_obj_set_size(ui.status, width, status_h);
  lv_obj_set_pos(ui.status, layout.inset, layout.panel_h - layout.inset - status_h);
  if (font) lv_obj_set_style_text_font(ui.status, font, LV_PART_MAIN);
  for (size_t i = 0; i < 7; ++i) {
    for (size_t j = 0; j < espcontrol::power::FLOW_POINT_COUNT; ++j)
      ui.points[i][j] = {static_cast<lv_value_precise_t>(geometry.paths[i][j].x),
                         static_cast<lv_value_precise_t>(geometry.paths[i][j].y)};
    for (size_t j = 0; j < 3; ++j)
      ui.heads[i][j] = {static_cast<lv_value_precise_t>(geometry.heads[i][j].x),
                        static_cast<lv_value_precise_t>(geometry.heads[i][j].y)};
    lv_line_set_points(ui.lines[i], ui.points[i], espcontrol::power::FLOW_POINT_COUNT);
    lv_line_set_points(ui.arrows[i], ui.heads[i], 3);
    const int stroke = std::max<int>(1, std::lround(3.2 * geometry.scale));
    lv_obj_set_style_line_width(ui.lines[i], stroke, LV_PART_MAIN);
    lv_obj_set_style_line_width(ui.arrows[i], stroke, LV_PART_MAIN);
  }
  for (size_t i = 0; i < 4; ++i) {
    const int line_height = font ? font->line_height : 20;
    for (auto *label : {ui.names[i], ui.values[i], ui.details[i]})
      if (font) lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_height(ui.headings[i], line_height);
    lv_obj_set_size(lv_obj_get_parent(ui.icons[i]), line_height, line_height);
    const auto *icon_font = display_icon_font(ui.card->display);
    if (icon_font && icon_font->line_height)
      apply_icon_width_compensation(ui.icons[i], 256 * line_height / icon_font->line_height);
    control_modal_center_tab_icon(ui.icons[i]);
    const int node_w = std::max<int>(1, std::lround(138 * geometry.scale));
    lv_obj_set_width(ui.nodes[i], node_w);
    lv_obj_update_layout(ui.nodes[i]);
    lv_obj_set_pos(ui.nodes[i], std::lround(geometry.nodes[i].x - node_w / 2.0),
                    std::lround(geometry.nodes[i].y - lv_obj_get_height(ui.nodes[i]) / 2.0));
  }
  lv_obj_move_foreground(ui.shell.close_btn);
  ui.laying_out = false;
}
inline void power_flow_update(PowerCardCtx *ctx) {
  auto &ui = power_flow_ui();
  if (!ctx || ui.card != ctx) return;
  const auto &calc = ctx->data.model.calculation;
  for (size_t i = 0; i < 4; ++i) {
    power_set_text(ui.values[i], power_reading_text(calc.nodes[i], ctx->precision));
    std::string detail;
    if (i == 2) detail = espcontrol::power::status_text(calc.grid_status);
    if (i == 3) {
      detail = espcontrol::power::status_text(calc.battery_status);
      if (ctx->data.model.battery_soc.valid()) {
        char soc[24];
        format_fixed_decimal(soc, sizeof(soc), ctx->data.model.battery_soc.value, 0);
        // Keep optional SOC visible first even when a long translated status
        // needs ellipsis; the full status remains on the shared card label.
        detail = std::string(soc) + "% \xC2\xB7 " + detail;
      }
    }
    power_set_text(ui.details[i], detail);
  }
  bool active = false;
  for (size_t i = 0; i < 7; ++i) {
    const bool visible = calc.edge_active(static_cast<espcontrol::power::Edge>(i));
    power_flow_set_visible(ui.lines[i], visible);
    power_flow_set_visible(ui.arrows[i], visible);
    active |= visible;
  }
  const char *state = calc.validity == espcontrol::power::CalculationValidity::UNAVAILABLE
    ? "Unavailable" : calc.validity == espcontrol::power::CalculationValidity::INCONSISTENT
    ? "Inconsistent readings" : active ? "Inferred flows" : "Idle";
  power_set_text(ui.status, espcontrol_i18n(state));
}
inline lv_obj_t *power_flow_label(lv_obj_t *parent, const lv_font_t *font) {
  auto *label = lv_label_create(parent);
  if (!label) return nullptr;
  if (font) lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(current_theme().text_primary), LV_PART_MAIN);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(label, lv_pct(100));
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  return label;
}
inline void power_flow_open(PowerCardCtx *ctx) {
  if (!ctx) return;
  control_modal_force_close_active();
  auto &ui = power_flow_ui();
  ui.shell = control_modal_open_shell(ControlModalKind::POWER_FLOW, ctx->slot.btn,
    display_main_width_percent(ctx->display), display_icon_font(ctx->display), power_flow_hide);
  if (!ui.shell.overlay) return;
  ui.card = ctx;
  set_clock_bar_modal_label(espcontrol_i18n("Power Dashboard"));
  ui.area = lv_obj_create(ui.shell.panel);
  if (!ui.area) { power_flow_hide(); return; }
  lv_obj_remove_style_all(ui.area);
  lv_obj_clear_flag(ui.area, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ui.area, LV_OBJ_FLAG_CLICKABLE);
  for (size_t i = 0; i < 7; ++i) {
    ui.lines[i] = lv_line_create(ui.area);
    ui.arrows[i] = lv_line_create(ui.area);
    if (!ui.lines[i] || !ui.arrows[i]) { power_flow_hide(); return; }
    for (auto *line : {ui.lines[i], ui.arrows[i]}) {
      lv_obj_set_pos(line, 0, 0);
      lv_obj_set_style_line_color(line, lv_color_hex(current_button_primary_color()), LV_PART_MAIN);
      lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN);
      lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
    }
  }
  const auto *label_font = display_volume_label_font(ctx->display,
    lv_obj_get_style_text_font(ctx->slot.text_lbl, LV_PART_MAIN));
  const char *names[4] = {"Solar", "Home", "Grid", "Battery"};
  const char *icons[4] = {"Solar Power", "Home", "Transmission Tower", "Battery"};
  for (size_t i = 0; i < 4; ++i) {
    ui.nodes[i] = lv_obj_create(ui.area);
    if (!ui.nodes[i]) { power_flow_hide(); return; }
    lv_obj_remove_style_all(ui.nodes[i]);
    lv_obj_clear_flag(ui.nodes[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui.nodes[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(ui.nodes[i], LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ui.nodes[i], LV_FLEX_FLOW_COLUMN);
    auto *heading = lv_obj_create(ui.nodes[i]);
    if (!heading) { power_flow_hide(); return; }
    ui.headings[i] = heading;
    lv_obj_remove_style_all(heading);
    lv_obj_clear_flag(heading, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(heading, lv_pct(100));
    const int line_height = label_font ? label_font->line_height : 20;
    lv_obj_set_height(heading, line_height);
    lv_obj_set_flex_flow(heading, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(heading, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    auto *icon_cell = lv_obj_create(heading);
    if (!icon_cell) { power_flow_hide(); return; }
    lv_obj_remove_style_all(icon_cell);
    lv_obj_clear_flag(icon_cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(icon_cell, line_height, line_height);
    const auto *icon_font = display_icon_font(ctx->display);
    auto *icon = power_flow_label(icon_cell, icon_font);
    if (!icon) { power_flow_hide(); return; }
    ui.icons[i] = icon;
    lv_obj_set_width(icon, LV_SIZE_CONTENT);
    if (icon_font && icon_font->line_height)
      apply_icon_width_compensation(icon, 256 * line_height / icon_font->line_height);
    power_set_text(icon, find_icon(icons[i]));
    control_modal_center_tab_icon(icon);
    auto *name = power_flow_label(heading, label_font);
    if (!name) { power_flow_hide(); return; }
    ui.names[i] = name;
    lv_obj_set_flex_grow(name, 1);
    power_set_text(name, espcontrol_i18n(names[i]));
    ui.values[i] = power_flow_label(ui.nodes[i], label_font);
    ui.details[i] = power_flow_label(ui.nodes[i], label_font);
    if (!ui.values[i] || !ui.details[i]) { power_flow_hide(); return; }
  }
  ui.status = power_flow_label(ui.shell.panel, label_font);
  if (!ui.status) { power_flow_hide(); return; }
  lv_obj_add_event_cb(ui.shell.panel, [](lv_event_t *) { power_flow_layout(); }, LV_EVENT_SIZE_CHANGED, nullptr);
  lv_obj_add_event_cb(ui.shell.overlay, [](lv_event_t *) {
    // Covers externally deleted overlays as well as the ordinary close path.
    control_modal_clear_active(ControlModalKind::POWER_FLOW);
    power_flow_ui() = {};
  }, LV_EVENT_DELETE, nullptr);
  power_flow_update(ctx);
  power_flow_layout();
}
