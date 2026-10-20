#pragma once

#include <cstring>

#include "power_card_data.h"
#include "power_flow_layout.h"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

// Bounded diagnostics retained for the hardware retest of the draw-stack fix.
// No per-frame polling; remove after the physical open/close test passes.
inline void power_flow_diagnostic_stage(const char *stage, int item = -1) {
#ifdef ESP_PLATFORM
  ESP_LOGW("power_diag", "stage=%s item=%d task=%s core=%u stack_min_bytes=%u internal_free=%u internal_largest=%u psram_free=%u",
    stage, item, pcTaskGetName(nullptr), static_cast<unsigned>(xPortGetCoreID()),
    static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)),
    static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
    static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
    static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
#else
  ESP_LOGW("power_diag", "stage=%s item=%d", stage, item);
#endif
}
inline void power_flow_diagnostic_bounds(const char *role, lv_obj_t *object, int item = -1) {
  if (!object) return;
  lv_area_t bounds;
  lv_obj_get_coords(object, &bounds);
  ESP_LOGW("power_diag", "bounds=%s item=%d x=%ld..%ld y=%ld..%ld",
    role, item, static_cast<long>(bounds.x1), static_cast<long>(bounds.x2),
    static_cast<long>(bounds.y1), static_cast<long>(bounds.y2));
}

struct PowerCardCtx;
inline PowerCardCtx *&power_card_contexts() { static PowerCardCtx *head = nullptr; return head; }
inline void power_flow_hide();
inline void power_flow_update(PowerCardCtx *ctx);

struct PowerCardCtx {
  BtnSlot slot{};
  DisplayProfile display{};
  espcontrol::power::CardData data{};
  int precision = 0;
  int grid_cols = 1, grid_rows = 1, grid_gap = 0;
  bool continuous_corners = false;
  bool has_soc = false;
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
  // Saved precision means fractional watts; model bounds protect the formatter.
  char text[32];
  format_fixed_decimal(text, sizeof(text), static_cast<float>(reading.value), precision);
  // Preserve polarity, but suppress a sign when the displayed rounded value is zero.
  if (text[0] == '-' && std::strspn(text + 1, "0.") == std::strlen(text + 1))
    std::memmove(text, text + 1, std::strlen(text));
  return std::string(text) + (with_unit ? " W" : "");
}
inline PowerCardCtx *power_card_for_button(lv_obj_t *button) {
  for (auto *ctx = power_card_contexts(); ctx; ctx = ctx->next)
    if (ctx->slot.btn == button) return ctx;
  return nullptr;
}
inline void power_card_update(PowerCardCtx *ctx) {
  if (!ctx) return;
  const auto &home = ctx->data.model.calculation.nodes[1];
  bool changed = power_set_text(ctx->slot.sensor_lbl, power_reading_text(home, ctx->precision, false));
  changed |= power_set_text(ctx->slot.unit_lbl, home.valid() ? "W" : "");
  changed |= power_set_text(ctx->slot.secondary_lbl, ctx->data.model.calculation.secondary_label());
  if (changed) {
    layout_button_label_with_secondary(ctx->slot, true, true);
  }
  sync_card_checked_text_color(ctx->slot.btn);
  power_flow_update(ctx);
  if (changed) notify_dashboard_content_changed();
}
inline void power_cards_disconnected() {
  for (auto *ctx = power_card_contexts(); ctx; ctx = ctx->next) {
    ctx->data.disconnect();
    power_card_update(ctx);
  }
}

// A native masked arc avoids the separately rounded, quantized segment ends of
// lv_line polylines. Draw tasks use LVGL's normal layer; no canvas or frame buffer.
struct PowerFlowStroke {
  lv_point_precise_t points[espcontrol::power::FLOW_POINT_COUNT]{};
  espcontrol::power::FlowArc arc{};
  bool diagnostic_seen = false;
  int diagnostic_index = 0;
};
inline void power_flow_draw_stroke(lv_event_t *event) {
  auto *stroke = static_cast<PowerFlowStroke *>(lv_event_get_user_data(event));
  const bool diagnose = !stroke->diagnostic_seen;
  stroke->diagnostic_seen = true;
  if (diagnose) power_flow_diagnostic_stage("path.draw.begin", stroke->diagnostic_index);
  auto *object = static_cast<lv_obj_t *>(lv_event_get_target(event));
  auto *layer = lv_event_get_layer(event);
  lv_area_t bounds;
  lv_obj_get_coords(object, &bounds);
  lv_draw_line_dsc_t line;
  lv_draw_line_dsc_init(&line);
  line.base.layer = layer;
  lv_obj_init_draw_line_dsc(object, LV_PART_MAIN, &line);
  auto point = [&](size_t i) {
    return lv_point_precise_t{stroke->points[i].x + bounds.x1, stroke->points[i].y + bounds.y1};
  };
  line.p1 = point(0);
  const bool bent = stroke->arc.end_angle != stroke->arc.start_angle;
  line.p2 = point(bent ? 1 : 3);
  line.round_start = 1;
  line.round_end = stroke->arc.radius ? 0 : 1;
  lv_draw_line(layer, &line);
  if (!bent) {
    if (diagnose) power_flow_diagnostic_stage("path.draw.end", stroke->diagnostic_index);
    return;
  }
  if (stroke->arc.radius) {
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = line.color;
    arc.width = line.width;
    arc.opa = line.opa;
    arc.center = {espcontrol::power::flow_coordinate<int32_t>(stroke->arc.center.x) + bounds.x1,
                  espcontrol::power::flow_coordinate<int32_t>(stroke->arc.center.y) + bounds.y1};
    // LVGL specifies outer radius. Even stroke widths keep the centerline exactly
    // aligned with integer tangent coordinates, with no half-pixel join mismatch.
    arc.radius = stroke->arc.radius + line.width / 2;
    arc.start_angle = stroke->arc.start_angle;
    arc.end_angle = stroke->arc.end_angle;
    arc.rounded = 0;
    lv_draw_arc(layer, &arc);
  }
  line.p1 = point(2);
  line.p2 = point(3);
  line.round_start = stroke->arc.radius ? 0 : 1;
  line.round_end = 1;
  lv_draw_line(layer, &line);
  if (diagnose) power_flow_diagnostic_stage("path.draw.end", stroke->diagnostic_index);
}

// A single temporary view borrows the live card model, never its subscriptions.
// Point buffers live until the overlay is deleted; no canvas/frame buffer/timer.
struct PowerFlowNodes {
  lv_obj_t *nodes[4]{};
  lv_obj_t *headings[4]{};
  lv_obj_t *names[4]{};
  lv_obj_t *icons[4]{};
  lv_obj_t *values[4]{};
  lv_obj_t *measurements[4]{};
  lv_obj_t *units[4]{};
  lv_obj_t *details[4]{};
  lv_obj_t *detail_rows[4]{};
  lv_obj_t *soc = nullptr;
};
struct PowerFlowUi : PowerFlowNodes {
  PowerCardCtx *card = nullptr;
  ControlModalShell shell{};
  lv_obj_t *dashboard = nullptr, *diagram = nullptr, *back_area = nullptr;
  lv_obj_t *area = nullptr, *status = nullptr;
  PowerFlowNodes sidebar{};
  espcontrol::power::FlowDashboardGrid grid{};
  lv_coord_t columns[6]{}, rows[5]{};
  lv_obj_t *lines[7]{};
  lv_obj_t *arrows[7]{};
  PowerFlowStroke paths[7]{};
  lv_point_precise_t heads[7][3]{};
  bool laying_out = false;
  bool diagnostic_draw_begin = false, diagnostic_draw_end = false;
  bool diagnostic_geometry_pending = false;
  unsigned diagnostic_updates = 0;
};
inline PowerFlowUi &power_flow_ui() { static PowerFlowUi ui; return ui; }
inline const PowerFlowUi &power_flow_reset_value() {
  // ESP GCC 14.2 ICEs on an aggregate reset temporary in this translation unit.
  // The identical immutable defaults need no stack temporary or heap allocation.
  static constexpr PowerFlowUi empty{};
  return empty;
}

inline void power_flow_diagnostic_geometry() {
  auto &ui = power_flow_ui();
  power_flow_diagnostic_bounds("panel", ui.shell.panel);
  power_flow_diagnostic_bounds("diagram", ui.diagram);
  power_flow_diagnostic_bounds("paths", ui.area);
  power_flow_diagnostic_bounds("back", ui.shell.close_btn);
  for (size_t i = 0; i < 4; ++i) {
    power_flow_diagnostic_bounds("node", ui.nodes[i], i);
    power_flow_diagnostic_bounds("side", ui.sidebar.nodes[i], i);
    if (i == 3) {
      power_flow_diagnostic_bounds("battery.value", ui.values[i]);
      power_flow_diagnostic_bounds("battery.soc", ui.soc);
      power_flow_diagnostic_bounds("battery.activity", ui.details[i]);
      power_flow_diagnostic_bounds("side.battery.value", ui.sidebar.values[i]);
      power_flow_diagnostic_bounds("side.battery.soc", ui.sidebar.soc);
      power_flow_diagnostic_bounds("side.battery.activity", ui.sidebar.details[i]);
    }
  }
}

inline void power_flow_hide() {
  power_flow_diagnostic_stage("close.begin");
  auto &ui = power_flow_ui();
  ui.card = nullptr;
  control_modal_delete_overlay(ControlModalKind::POWER_FLOW, ui.shell.overlay);
  ui = power_flow_reset_value();
  power_flow_diagnostic_stage("close.end");
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
inline int power_flow_text_width(const char *text, const lv_font_t *font) {
  if (!font) return 0;
  lv_point_t size;
  lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  return size.x;
}
inline void power_flow_layout() {
  auto &ui = power_flow_ui();
  if (!ui.card || !ui.area || ui.laying_out) return;
  ui.laying_out = true;
  power_flow_diagnostic_stage("layout.begin");
  const auto layout = control_modal_calc_layout(display_main_width_percent(ui.card->display));
  control_modal_apply_panel_layout(ui.shell.overlay, ui.shell.panel, layout,
                                   control_modal_card_radius(ui.card->slot.btn));
  control_modal_style_chrome_button(ui.shell.close_btn, layout, false);
  lv_obj_update_layout(ui.shell.panel);
  const int panel_w = lv_obj_get_content_width(ui.shell.panel);
  const int panel_h = lv_obj_get_content_height(ui.shell.panel);
  ui.grid = espcontrol::power::flow_dashboard_grid(ui.card->display.modal,
    ui.card->grid_cols, ui.card->grid_rows, panel_w, panel_h);
  const bool large = ui.grid.large && ui.sidebar.nodes[0];
  lv_obj_set_size(ui.dashboard, panel_w, panel_h);
  lv_obj_set_pos(ui.dashboard, 0, 0);
  auto place = [](lv_obj_t *object, const espcontrol::power::FlowGridCell &cell) {
    lv_obj_set_grid_cell(object, LV_GRID_ALIGN_STRETCH, cell.col, cell.cols,
                         LV_GRID_ALIGN_STRETCH, cell.row, cell.rows);
  };
  if (large) {
    const int gap = ui.card->grid_gap > 0 ? ui.card->grid_gap : layout.title_gap;
    lv_obj_set_style_pad_all(ui.dashboard, layout.inset, LV_PART_MAIN);
    lv_obj_set_style_pad_column(ui.dashboard, gap, LV_PART_MAIN);
    lv_obj_set_style_pad_row(ui.dashboard, gap, LV_PART_MAIN);
    for (int col = 0; col < ui.grid.cols; ++col) ui.columns[col] = LV_GRID_FR(1);
    ui.columns[ui.grid.cols] = LV_GRID_TEMPLATE_LAST;
    for (int row = 0; row < ui.grid.rows; ++row) ui.rows[row] = LV_GRID_FR(1);
    ui.rows[ui.grid.rows] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_grid_dsc_array(ui.dashboard, ui.columns, ui.rows);
    place(ui.diagram, ui.grid.diagram);
    place(ui.back_area, ui.grid.back);
    power_flow_set_visible(ui.back_area, true);
    for (size_t i = 0; i < 4; ++i) {
      power_flow_set_visible(ui.sidebar.nodes[i], true);
      place(ui.sidebar.nodes[i], ui.grid.values[i]);
    }
  } else {
    lv_obj_set_layout(ui.dashboard, LV_LAYOUT_NONE);
    lv_obj_set_style_pad_all(ui.dashboard, 0, LV_PART_MAIN);
    lv_obj_set_pos(ui.diagram, 0, 0);
    lv_obj_set_size(ui.diagram, panel_w, panel_h);
    power_flow_set_visible(ui.back_area, false);
    for (auto *node : ui.sidebar.nodes) power_flow_set_visible(node, false);
  }
  lv_obj_update_layout(ui.dashboard);
  const auto viewport = espcontrol::power::flow_viewport(
    lv_obj_get_content_width(ui.diagram), lv_obj_get_content_height(ui.diagram), layout.inset,
    large ? layout.inset : lv_obj_get_y(ui.shell.close_btn) + lv_obj_get_height(ui.shell.close_btn) + layout.title_gap);
  const int width = viewport.width, height = viewport.height;
  const auto *body = display_volume_label_font(ui.card->display,
    lv_obj_get_style_text_font(ui.card->slot.text_lbl, LV_PART_MAIN));
  const auto *compact = display_small_text_font(ui.card->display, body);
  const auto *card_font = lv_obj_get_style_text_font(ui.card->slot.text_lbl, LV_PART_MAIN);
  if (card_font && (!compact || card_font->line_height < compact->line_height)) compact = card_font;
  auto *text_font = body;
  auto *value_font = body;
  auto measure = [&](const lv_font_t *text, const lv_font_t *value, bool with_soc = true) {
    int w = 0;
    for (size_t i = 0; i < 4; ++i)
      if ((i == 0 || i == 3) == with_soc)
        w = std::max<int>(w, power_flow_text_width(lv_label_get_text(ui.names[i]), text) + text->line_height + 4);
    const char *states[] = {with_soc ? "Charging" : "Importing",
      with_soc ? "Discharging" : "Exporting", "Idle", "Unavailable"};
    for (const char *state : states)
      w = std::max(w, power_flow_text_width((std::string(with_soc ? "100% · " : "") + espcontrol_i18n(state)).c_str(), text));
    // Fixed measurement envelope: live values never move the geometry.
    w = std::max(w, power_flow_text_width(ui.card->precision ? "-8888888.888 W" : "-8888888 W", value));
    return w + 8;
  };
  if (body && compact && (measure(body, body) > width * .42 || body->line_height * 3 + 6 > height * .42))
    text_font = value_font = compact;
  else if (compact) text_font = compact;
  const int text_h = text_font ? text_font->line_height : 20;
  const int value_h = value_font ? value_font->line_height : text_h;
  const int node_w = std::min<int>(std::lround(width * .48), measure(text_font, value_font));
  // Grid/Home never need the combined Battery row: do not consume route
  // clearance with a wider SOC/translated caption belonging to another node.
  const int side_node_w = std::min<int>(std::lround(width * .48), measure(text_font, value_font, false));
  const int node_h = text_h * 2 + value_h + 6;
  const auto geometry = espcontrol::power::flow_geometry(width, height,
    espcontrol::power::flow_family(layout.sw, layout.sh), node_w, node_h, side_node_w);
  lv_obj_set_size(ui.area, width, height);
  lv_obj_set_pos(ui.area, viewport.x, viewport.y);
  // Back and state live in the side header on large grids, never above the diagram.
  auto *status_parent = large ? ui.back_area : ui.shell.panel;
  if (lv_obj_get_parent(ui.status) != status_parent) lv_obj_set_parent(ui.status, status_parent);
  const int status_width = large ? lv_obj_get_content_width(ui.back_area) : panel_w - layout.inset * 2;
  const bool stacked_back = large && ui.grid.back.cols == 1;
  lv_label_set_long_mode(ui.status, stacked_back ? LV_LABEL_LONG_WRAP : LV_LABEL_LONG_CLIP);
  lv_obj_set_size(ui.status, stacked_back ? status_width : std::max<int>(1, status_width - layout.back_size - layout.title_gap), text_h * (stacked_back ? 2 : 1));
  if (large) {
    const int group_h = stacked_back ? layout.back_size + layout.title_gap + text_h * 2 : layout.back_size;
    const int back_y = std::max<int>(0, (lv_obj_get_height(ui.back_area) - group_h) / 2);
    const int back_x = stacked_back ? std::max<int>(0, (status_width - layout.back_size) / 2) : 0;
    lv_area_t header_bounds, panel_content;
    lv_obj_get_coords(ui.back_area, &header_bounds);
    lv_obj_get_content_coords(ui.shell.panel, &panel_content);
    lv_obj_set_pos(ui.shell.close_btn, header_bounds.x1 - panel_content.x1 + back_x,
      header_bounds.y1 - panel_content.y1 + back_y);
    lv_obj_set_pos(ui.status, stacked_back ? 0 : layout.back_size + layout.title_gap,
      stacked_back ? back_y + layout.back_size + layout.title_gap
                   : std::max<int>(0, (lv_obj_get_height(ui.back_area) - text_h) / 2));
  } else lv_obj_set_pos(ui.status, layout.inset + layout.back_size + layout.title_gap, layout.back_inset_y);
  if (text_font) lv_obj_set_style_text_font(ui.status, text_font, LV_PART_MAIN);
  lv_obj_set_style_text_color(ui.status, lv_color_hex(current_theme().text_muted), LV_PART_MAIN);
  for (size_t i = 0; i < 7; ++i) {
    for (size_t j = 0; j < espcontrol::power::FLOW_POINT_COUNT; ++j)
      ui.paths[i].points[j] = {espcontrol::power::flow_coordinate<lv_value_precise_t>(geometry.paths[i][j].x),
                         espcontrol::power::flow_coordinate<lv_value_precise_t>(geometry.paths[i][j].y)};
    for (size_t j = 0; j < 3; ++j)
      ui.heads[i][j] = {espcontrol::power::flow_coordinate<lv_value_precise_t>(geometry.heads[i][j].x),
                        espcontrol::power::flow_coordinate<lv_value_precise_t>(geometry.heads[i][j].y)};
    ui.paths[i].arc = geometry.arcs[i];
    lv_obj_set_size(ui.lines[i], width, height);
    lv_obj_invalidate(ui.lines[i]);
    lv_line_set_points(ui.arrows[i], ui.heads[i], 3);
    const int stroke = (std::max<int>(2, std::lround(3.2 * geometry.scale)) + 1) & ~1;
    lv_obj_set_style_line_width(ui.lines[i], stroke, LV_PART_MAIN);
    lv_obj_set_style_line_width(ui.arrows[i], stroke, LV_PART_MAIN);
  }
  // LVGL 9.5 retains its recursive draw frames for every ancestor. Keep the
  // text groups shallow (panel children), not below the three geometry-only
  // containers. Their screen coordinates still come from the measured diagram.
  // The grid is already resolved above; avoid another synchronous layout pass.
  lv_area_t diagram_content, panel_content;
  lv_obj_get_content_coords(ui.diagram, &diagram_content);
  lv_obj_get_content_coords(ui.shell.panel, &panel_content);
  const int node_x = diagram_content.x1 + viewport.x - panel_content.x1;
  const int node_y = diagram_content.y1 + viewport.y - panel_content.y1;
  for (size_t i = 0; i < 4; ++i) {
    for (auto *label : {ui.names[i], ui.details[i]})
      if (text_font) lv_obj_set_style_text_font(label, text_font, LV_PART_MAIN);
    if (value_font) lv_obj_set_style_text_font(ui.values[i], value_font, LV_PART_MAIN);
    lv_obj_set_height(ui.names[i], text_h);
    lv_obj_set_height(ui.values[i], value_h);
    lv_obj_set_height(ui.details[i], text_h);
    lv_obj_set_height(ui.detail_rows[i], text_h);
    lv_obj_set_style_text_color(ui.details[i], lv_color_hex(current_theme().text_muted), LV_PART_MAIN);
    lv_obj_set_height(ui.headings[i], text_h);
    lv_obj_set_size(lv_obj_get_parent(ui.icons[i]), text_h, text_h);
    const auto *icon_font = display_icon_font(ui.card->display);
    if (icon_font && icon_font->line_height)
      apply_icon_width_compensation(ui.icons[i], 256 * text_h / icon_font->line_height);
    control_modal_center_tab_icon(ui.icons[i]);
    const int this_width = i == 1 || i == 2 ? side_node_w : node_w;
    lv_obj_set_size(ui.nodes[i], this_width, node_h);
    lv_obj_set_pos(ui.nodes[i], node_x + std::lround(geometry.nodes[i].x - this_width / 2.0),
                    node_y + std::lround(geometry.nodes[i].y - node_h / 2.0));
  }
  if (text_font) lv_obj_set_style_text_font(ui.soc, text_font, LV_PART_MAIN);
  lv_obj_set_height(ui.soc, text_h);
  if (large) {
    for (size_t i = 0; i < 4; ++i) {
      auto &side = ui.sidebar;
      const int available = lv_obj_get_content_width(side.nodes[i]);
      const auto *side_text = compact ? compact : body;
      const auto *side_value = display_sensor_font(ui.card->display);
      const int unit_width = power_flow_text_width("W", side_text);
      const char *number = ui.card->precision ? "-8888888.888" : "-8888888";
      if (!side_value || power_flow_text_width(number, side_value) + unit_width > available)
        side_value = body;
      if (!side_value || power_flow_text_width(number, side_value) + unit_width > available)
        side_value = side_text;
      const int th = side_text ? side_text->line_height : text_h;
      const int vh = side_value ? side_value->line_height : value_h;
      for (auto *label : {side.names[i], side.details[i]})
        if (side_text) lv_obj_set_style_text_font(label, side_text, LV_PART_MAIN);
      if (side_value) lv_obj_set_style_text_font(side.values[i], side_value, LV_PART_MAIN);
      if (side_text) lv_obj_set_style_text_font(side.units[i], side_text, LV_PART_MAIN);
      lv_obj_set_height(side.headings[i], th);
      lv_obj_set_size(lv_obj_get_parent(side.icons[i]), th, th);
      const auto *icon_font = display_icon_font(ui.card->display);
      if (icon_font && icon_font->line_height)
        apply_icon_width_compensation(side.icons[i], 256 * th / icon_font->line_height);
      control_modal_center_tab_icon(side.icons[i]);
      lv_obj_set_height(side.names[i], th);
      lv_obj_set_height(side.values[i], vh);
      lv_obj_set_height(side.details[i], th);
      lv_obj_set_height(side.detail_rows[i], th);
      lv_obj_set_style_text_color(side.details[i], lv_color_hex(current_theme().text_muted), LV_PART_MAIN);
    }
    if (compact) lv_obj_set_style_text_font(ui.sidebar.soc, compact, LV_PART_MAIN);
    lv_obj_set_height(ui.sidebar.soc, compact ? compact->line_height : text_h);
  }
  lv_obj_move_foreground(ui.shell.close_btn);
  ui.laying_out = false;
  power_flow_diagnostic_stage("layout.end");
}
inline void power_flow_update_nodes(PowerFlowNodes &ui, PowerCardCtx *ctx) {
  if (!ui.nodes[0]) return;
  const auto &calc = ctx->data.model.calculation;
  for (size_t i = 0; i < 4; ++i) {
    power_set_text(ui.values[i], power_reading_text(calc.nodes[i], ctx->precision, !ui.units[i]));
    power_set_text(ui.units[i], calc.nodes[i].valid() ? "W" : "");
    std::string detail;
    if (i == 2) detail = espcontrol::power::status_text(calc.grid_status);
    if (i == 3) {
      detail = espcontrol::power::status_text(calc.battery_status);
      const bool available_soc = ctx->has_soc && ctx->data.model.battery_soc.valid();
      char soc[24] = "";
      if (available_soc)
        format_fixed_decimal(soc, sizeof(soc), ctx->data.model.battery_soc.value, 0);
      power_set_text(ui.soc, available_soc ? std::string(soc) + "% · " : "");
      power_flow_set_visible(ui.soc, available_soc);
    }
    power_set_text(ui.details[i], detail);
  }
}
inline void power_flow_update(PowerCardCtx *ctx) {
  auto &ui = power_flow_ui();
  if (!ctx || ui.card != ctx) return;
  const bool diagnose = ui.diagnostic_updates < 3;
  if (diagnose) {
    ui.diagnostic_geometry_pending = true;
    power_flow_diagnostic_stage("model.update.begin", ++ui.diagnostic_updates);
  }
  const auto &calc = ctx->data.model.calculation;
  power_flow_update_nodes(ui, ctx);
  power_flow_update_nodes(ui.sidebar, ctx);
  bool active = false;
  for (size_t i = 0; i < 7; ++i) {
    const bool visible = calc.edge_active(static_cast<espcontrol::power::Edge>(i));
    power_flow_set_visible(ui.lines[i], visible);
    power_flow_set_visible(ui.arrows[i], visible);
    active |= visible;
  }
  const char *state = calc.validity == espcontrol::power::CalculationValidity::UNAVAILABLE
    ? "Unavailable" : calc.validity == espcontrol::power::CalculationValidity::INCONSISTENT
    ? "Inconsistent readings" : active ? "" : "Idle";
  power_set_text(ui.status, espcontrol_i18n(state));
  power_flow_set_visible(ui.status, *state != 0);
  if (diagnose) power_flow_diagnostic_stage("model.update.end", ui.diagnostic_updates);
}
inline lv_obj_t *power_flow_label(lv_obj_t *parent, const lv_font_t *font) {
  auto *label = lv_label_create(parent);
  if (!label) return nullptr;
  if (font) lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(current_theme().text_primary), LV_PART_MAIN);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(label, lv_pct(100));
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
  return label;
}
inline bool power_flow_create_nodes(PowerFlowNodes &ui, lv_obj_t *parent,
                                      PowerCardCtx *ctx, const lv_font_t *label_font, bool cards) {
  const char *names[4] = {"Solar", "Home", "Grid", "Battery"};
  const char *icons[4] = {"Solar Power", "Home", "Transmission Tower", "Battery"};
  for (size_t i = 0; i < 4; ++i) {
    power_flow_diagnostic_stage(cards ? "side.node.begin" : "diagram.node.begin", i);
    ui.nodes[i] = cards ? create_grid_card_button(parent, control_modal_card_radius(ctx->slot.btn),
      0, label_font, lv_color_hex(current_theme().text_primary), ctx->continuous_corners)
      : lv_obj_create(parent);
    if (!ui.nodes[i]) return false;
    if (cards) {
      // Existing neutral-control ownership disambiguates equal Dark RGB roles.
      // The shared modal refresh can recolor these read-only cards in place.
      theme_set_control_neutral_fill(ui.nodes[i]);
      lv_obj_set_style_bg_color(ui.nodes[i], lv_color_hex(theme_display_color(current_theme().control_neutral)), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui.nodes[i], LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_clear_flag(ui.nodes[i], LV_OBJ_FLAG_CHECKABLE);
    } else lv_obj_remove_style_all(ui.nodes[i]);
    lv_obj_clear_flag(ui.nodes[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui.nodes[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(ui.nodes[i], LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ui.nodes[i], LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ui.nodes[i], 3, LV_PART_MAIN);
    lv_obj_set_flex_align(ui.nodes[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    auto *heading = lv_obj_create(ui.nodes[i]);
    if (!heading) return false;
    ui.headings[i] = heading;
    lv_obj_remove_style_all(heading);
    lv_obj_clear_flag(heading, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(heading, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(heading, lv_pct(100));
    const int line_height = label_font ? label_font->line_height : 20;
    lv_obj_set_height(heading, line_height);
    lv_obj_set_flex_flow(heading, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(heading, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    auto *icon_cell = lv_obj_create(heading);
    if (!icon_cell) return false;
    lv_obj_remove_style_all(icon_cell);
    lv_obj_clear_flag(icon_cell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(icon_cell, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(icon_cell, line_height, line_height);
    const auto *icon_font = display_icon_font(ctx->display);
    auto *icon = power_flow_label(icon_cell, icon_font);
    if (!icon) return false;
    ui.icons[i] = icon;
    lv_obj_set_width(icon, LV_SIZE_CONTENT);
    if (icon_font && icon_font->line_height)
      apply_icon_width_compensation(icon, 256 * line_height / icon_font->line_height);
    power_set_text(icon, find_icon(icons[i]));
    control_modal_center_tab_icon(icon);
    auto *name = power_flow_label(heading, label_font);
    if (!name) return false;
    ui.names[i] = name;
    lv_obj_set_width(name, LV_SIZE_CONTENT);
    power_set_text(name, espcontrol_i18n(names[i]));
    if (cards) {
      // Native numeric fonts intentionally omit W; retain the shared adjacent
      // number/unit grouping rather than asking that font to render unit text.
      ui.measurements[i] = create_card_sensor_container(ui.nodes[i], label_font, label_font,
        lv_color_hex(current_theme().text_primary), &ui.values[i], &ui.units[i]);
      if (!ui.measurements[i]) return false;
      power_flow_set_visible(ui.measurements[i], true);
      for (auto *label : {ui.values[i], ui.units[i]}) {
        if (!label) return false;
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(label, LV_SIZE_CONTENT);
      }
    } else ui.values[i] = power_flow_label(ui.nodes[i], label_font);
    // Exactly three rows: heading, signed watts, combined SOC/activity.
    ui.detail_rows[i] = lv_obj_create(ui.nodes[i]);
    if (!ui.detail_rows[i]) return false;
    lv_obj_remove_style_all(ui.detail_rows[i]);
    lv_obj_clear_flag(ui.detail_rows[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui.detail_rows[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(ui.detail_rows[i], LV_SIZE_CONTENT, line_height);
    lv_obj_set_flex_flow(ui.detail_rows[i], LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui.detail_rows[i], LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ui.detail_rows[i], 0, LV_PART_MAIN);
    ui.details[i] = power_flow_label(ui.detail_rows[i], label_font);
    if (ui.details[i]) lv_obj_set_width(ui.details[i], LV_SIZE_CONTENT);
    if (!ui.values[i] || !ui.details[i]) return false;
    if (i == 3) {
      // SOC is measurement text; the adjacent activity retains the muted role.
      ui.soc = power_flow_label(ui.detail_rows[i], label_font);
      if (!ui.soc) return false;
      lv_obj_set_width(ui.soc, LV_SIZE_CONTENT);
      lv_obj_move_to_index(ui.soc, 0);
    }
    power_flow_diagnostic_stage(cards ? "side.node.end" : "diagram.node.end", i);
  }
  return true;
}
inline void power_flow_open(PowerCardCtx *ctx) {
  if (!ctx) return;
  power_flow_diagnostic_stage("open.begin");
  control_modal_force_close_active();
  auto &ui = power_flow_ui();
  ui.shell = control_modal_open_shell(ControlModalKind::POWER_FLOW, ctx->slot.btn,
    display_main_width_percent(ctx->display), display_icon_font(ctx->display), power_flow_hide);
  if (!ui.shell.overlay) return;
  power_flow_diagnostic_stage("shell.ready");
  ui.card = ctx;
  set_clock_bar_modal_label(espcontrol_i18n("Power Dashboard"));
  ui.dashboard = lv_obj_create(ui.shell.panel);
  if (!ui.dashboard) { power_flow_hide(); return; }
  lv_obj_remove_style_all(ui.dashboard);
  lv_obj_clear_flag(ui.dashboard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ui.dashboard, LV_OBJ_FLAG_CLICKABLE);
  ui.diagram = lv_obj_create(ui.dashboard);
  if (!ui.diagram) { power_flow_hide(); return; }
  lv_obj_remove_style_all(ui.diagram);
  lv_obj_clear_flag(ui.diagram, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ui.diagram, LV_OBJ_FLAG_CLICKABLE);
  ui.area = lv_obj_create(ui.diagram);
  if (!ui.area) { power_flow_hide(); return; }
  lv_obj_remove_style_all(ui.area);
  lv_obj_clear_flag(ui.area, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ui.area, LV_OBJ_FLAG_CLICKABLE);
  power_flow_diagnostic_stage("diagram.containers.ready");
  for (size_t i = 0; i < 7; ++i) {
    ui.paths[i].diagnostic_index = i;
    ui.lines[i] = lv_obj_create(ui.area);
    if (ui.lines[i]) {
      lv_obj_remove_style_all(ui.lines[i]);
      lv_obj_clear_flag(ui.lines[i], LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_event_cb(ui.lines[i], power_flow_draw_stroke, LV_EVENT_DRAW_MAIN, &ui.paths[i]);
    }
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
  power_flow_diagnostic_stage("paths.ready");
  const auto *label_font = display_volume_label_font(ctx->display,
    lv_obj_get_style_text_font(ctx->slot.text_lbl, LV_PART_MAIN));
  if (!power_flow_create_nodes(ui, ui.shell.panel, ctx, label_font, false)) { power_flow_hide(); return; }
  const auto family = ctx->display.modal.layout_family;
  const bool large_capable = family == DisplayModalLayoutFamily::WIDE_LANDSCAPE ||
                             family == DisplayModalLayoutFamily::LARGE_LANDSCAPE;
  if (large_capable && ctx->grid_cols == 5 && ctx->grid_rows >= 3 && ctx->grid_rows <= 4) {
    ui.back_area = lv_obj_create(ui.dashboard);
    if (!ui.back_area) { power_flow_hide(); return; }
    lv_obj_remove_style_all(ui.back_area);
    lv_obj_clear_flag(ui.back_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui.back_area, LV_OBJ_FLAG_CLICKABLE);
    if (!power_flow_create_nodes(ui.sidebar, ui.dashboard, ctx, label_font, true)) { power_flow_hide(); return; }
  }
  power_flow_diagnostic_stage("all.nodes.ready");
  ui.status = power_flow_label(ui.shell.panel, label_font);
  if (!ui.status) { power_flow_hide(); return; }
  lv_obj_add_event_cb(ui.shell.panel, [](lv_event_t *) { power_flow_layout(); }, LV_EVENT_SIZE_CHANGED, nullptr);
  lv_obj_add_event_cb(ui.shell.overlay, [](lv_event_t *) {
    // Covers externally deleted overlays as well as the ordinary close path.
    control_modal_clear_active(ControlModalKind::POWER_FLOW);
    power_flow_ui() = power_flow_reset_value();
  }, LV_EVENT_DELETE, nullptr);
  lv_obj_add_event_cb(ui.shell.overlay, [](lv_event_t *event) {
    auto &live = power_flow_ui();
    if (lv_event_get_code(event) == LV_EVENT_DRAW_MAIN_BEGIN && live.diagnostic_geometry_pending) {
      // Draw traversal follows LVGL's own layout resolution; do not add a
      // synchronous layout pass inside the already-deep card click stack.
      live.diagnostic_geometry_pending = false;
      power_flow_diagnostic_geometry();
    }
    if (lv_event_get_code(event) == LV_EVENT_DRAW_MAIN_BEGIN && !live.diagnostic_draw_begin) {
      live.diagnostic_draw_begin = true;
      power_flow_diagnostic_stage("overlay.first_draw.begin");
    } else if (lv_event_get_code(event) == LV_EVENT_DRAW_POST_END && !live.diagnostic_draw_end) {
      live.diagnostic_draw_end = true;
      // Submission for the first partial buffer, not proof of a completed frame.
      power_flow_diagnostic_stage("overlay.first_draw.post");
    }
  }, LV_EVENT_ALL, nullptr);
  power_flow_diagnostic_stage("initial.update.begin");
  power_flow_update(ctx);
  power_flow_diagnostic_stage("initial.update.end");
  power_flow_layout();
  power_flow_diagnostic_stage("open.ready");
}
