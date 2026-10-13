#pragma once

// One shared lifecycle for main-grid and subpage Power cards. The view borrows
// this context; opening it adds no HA subscriptions or timers.
// Contract coverage marker: "power".
namespace espcontrol::cards {
inline bool power_driver_matches(const Context &context) {
  return context.runtime.driver == card_runtime::CardDriverId::POWER;
}
inline bool power_driver_setup_visual(BtnSlot &slot, const ParsedCfg &config,
                                       const Context &context, const CardPalette &palette) {
  if (!power_driver_matches(context)) return false;
  sensor_driver_apply_background(slot, palette);
  lv_obj_add_flag(slot.icon_lbl, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(slot.sensor_container, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(slot.secondary_lbl, LV_OBJ_FLAG_HIDDEN);
  const auto label = config.label.empty() || config.label == "Power Dashboard"
    ? std::string(espcontrol_i18n("Power Dashboard")) : config.label;
  power_set_text(slot.text_lbl, label);
  power_set_text(slot.sensor_lbl, "--");
  power_set_text(slot.unit_lbl, "");
  power_set_text(slot.secondary_lbl, espcontrol_i18n("Unavailable"));
  layout_button_label_with_secondary(slot, true, true);
  apply_push_button_transition(slot.btn);
  return true;
}
inline bool power_driver_refresh_layout(BtnSlot &slot, const Context &context) {
  if (!power_driver_matches(context)) return false;
  layout_button_label_with_secondary(slot, true, true);
  return true;
}
inline void power_driver_event(lv_event_t *event) {
  auto *button = static_cast<lv_obj_t *>(lv_event_get_target(event));
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    auto *ctx = power_card_for_button(button);
    if (ctx && !ctx->slot.config) power_flow_open(ctx);
  } else {
    power_card_apply_theme_for_button(button);
  }
}
inline bool power_driver_bind_data(BtnSlot &slot, const ParsedCfg &config,
                                    const Context &context, const DisplayProfile &display) {
  if (!power_driver_matches(context)) return false;
  auto *ctx = new PowerCardCtx();
  ctx->slot = slot;
  ctx->display = display;
  ctx->precision = parse_precision(config.precision);
  const auto bindings = espcontrol::power::decode_config_v1(config);
  ctx->data.model.set_polarity({bindings.invert_grid, bindings.invert_battery});
  ctx->next = power_card_contexts();
  power_card_contexts() = ctx;
  if (context.surface == Surface::SUBPAGE) grid_delete_with_owner(slot.btn, ctx);
  else grid_track_runtime_allocation(slot.btn, ctx);
  ha_retained_state_invalidated_handler() = power_cards_disconnected;
  lv_obj_remove_event_cb(slot.btn, power_driver_event);
  for (auto event : {LV_EVENT_PRESSED, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST, LV_EVENT_CLICKED})
    lv_obj_add_event_cb(slot.btn, power_driver_event, event, nullptr);
  // Model order is Solar, Home, Grid, Battery; SOC is a fifth independent input.
  const std::string entities[5] = {bindings.solar_entity, bindings.home_entity,
    bindings.grid_entity, bindings.battery_entity, bindings.battery_soc_entity};
  HaCallbackOwnerScope owner(ctx);
  for (size_t i = 0; i < 5; ++i) {
    if (entities[i].empty()) continue;
    const bool state = ha_subscribe_state(entities[i], [ctx, i](esphome::StringRef value) {
      ctx->data.state(i, std::string_view(value.c_str(), value.size()));
      power_card_update(ctx);
    }, HA_SUBSCRIPTION_SCOPE_DEFAULT, true);
    const bool unit = ha_subscribe_attribute(entities[i], "unit_of_measurement",
      [ctx, i](esphome::StringRef value) {
        ctx->data.unit(i, std::string_view(value.c_str(), value.size()));
        power_card_update(ctx);
      }, HA_SUBSCRIPTION_SCOPE_DEFAULT, true);
    if (!state || !unit) ESP_LOGW("power", "Unable to bind Power input %s", entities[i].c_str());
  }
  power_card_update(ctx);
  return true;
}
inline bool power_driver_handle_main_click(const Context &context, const ParsedCfg &, lv_obj_t *button) {
  if (!power_driver_matches(context)) return false;
  power_flow_open(power_card_for_button(button));
  return true;
}
} // namespace espcontrol::cards
