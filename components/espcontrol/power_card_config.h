#pragma once

#include <string>

// Included after ParsedCfg and compact-option primitives, like Media's config.
namespace espcontrol::power {

struct ConfigV1 {
  std::string home_entity;
  std::string solar_entity;
  std::string grid_entity;
  std::string battery_entity;
  std::string battery_soc_entity;
  bool invert_grid = false;
  bool invert_battery = false;
};

inline ConfigV1 decode_config_v1(const ParsedCfg &saved) {
  return {
    trim_saved_option_value(saved.entity),
    trim_saved_option_value(cfg_option_value(saved.options, "solar_entity")),
    trim_saved_option_value(cfg_option_value(saved.options, "grid_entity")),
    trim_saved_option_value(cfg_option_value(saved.options, "battery_entity")),
    trim_saved_option_value(cfg_option_value(saved.options, "battery_soc_entity")),
    cfg_option_token_present(saved.options, "invert_grid"),
    cfg_option_token_present(saved.options, "invert_battery"),
  };
}

inline std::string normalize_options(const std::string &options) {
  std::string out;
  for (const char *name : {"solar_entity", "grid_entity", "battery_entity", "battery_soc_entity"}) {
    const auto value = trim_saved_option_value(cfg_option_value(options, name));
    if (value.empty()) continue;
    if (!out.empty()) out += ',';
    out += std::string(name) + '=' + encode_compact_field(value);
  }
  for (const char *name : {"invert_grid", "invert_battery"}) {
    if (!cfg_option_token_present(options, name)) continue;
    if (!out.empty()) out += ',';
    out += name;
  }
  return out;
}

}  // namespace espcontrol::power
