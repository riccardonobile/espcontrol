#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>

#include "theme_palette.h"

enum class ThemeMode : uint8_t { DARK, LIGHT, SCHEDULE, SUN };
enum class EffectiveTheme : uint8_t { DARK, LIGHT };

struct ThemeSettings {
  ThemeMode mode = ThemeMode::DARK;
  int light_start = 7 * 60;
  int dark_start = 20 * 60;
};

struct ThemeConditions {
  bool time_valid = false;
  int local_minute = 0;
  bool sun_valid = false;
  int sunrise_minute = 0;
  int sunset_minute = 0;
};

// An unavailable clock or solar calculation retains the last effective color.
// A fresh boot starts Dark; the configured mode is never replaced by fallback.
struct ThemeResolver {
  EffectiveTheme effective = EffectiveTheme::DARK;
};

inline bool parse_theme_time(const std::string &text, int &minute) {
  if (text.size() != 5 || text[2] != ':') return false;
  for (int i : {0, 1, 3, 4}) {
    if (text[i] < '0' || text[i] > '9') return false;
  }
  const int hour = (text[0] - '0') * 10 + (text[1] - '0');
  const int min = (text[3] - '0') * 10 + (text[4] - '0');
  if (hour > 23 || min > 59) return false;
  minute = hour * 60 + min;
  return true;
}

inline ThemeMode parse_theme_mode(const std::string &mode) {
  if (mode == "Light") return ThemeMode::LIGHT;
  if (mode == "Schedule") return ThemeMode::SCHEDULE;
  if (mode == "Sun") return ThemeMode::SUN;
  return ThemeMode::DARK;
}

inline bool theme_light_between(int now, int light_start, int dark_start) {
  if (light_start == dark_start) return false;  // Equal boundaries mean Dark.
  if (light_start < dark_start)
    return now >= light_start && now < dark_start;
  return now >= light_start || now < dark_start;
}

inline EffectiveTheme resolve_theme(const ThemeSettings &settings,
                                   const ThemeConditions &conditions,
                                   EffectiveTheme previous) {
  switch (settings.mode) {
    case ThemeMode::DARK: return EffectiveTheme::DARK;
    case ThemeMode::LIGHT: return EffectiveTheme::LIGHT;
    case ThemeMode::SCHEDULE:
      if (!conditions.time_valid) return previous;
      return theme_light_between(conditions.local_minute, settings.light_start,
                                 settings.dark_start)
                 ? EffectiveTheme::LIGHT : EffectiveTheme::DARK;
    case ThemeMode::SUN:
      if (!conditions.time_valid || !conditions.sun_valid) return previous;
      return theme_light_between(conditions.local_minute,
                                 conditions.sunrise_minute,
                                 conditions.sunset_minute)
                 ? EffectiveTheme::LIGHT : EffectiveTheme::DARK;
  }
  return EffectiveTheme::DARK;
}

inline bool apply_theme_resolution(ThemeResolver &resolver,
                                   const ThemeSettings &settings,
                                   const ThemeConditions &conditions) {
  const auto next = resolve_theme(settings, conditions, resolver.effective);
  const ThemePalette &palette = next == EffectiveTheme::LIGHT ? LIGHT_THEME : DARK_THEME;
  resolver.effective = next;
  if (&current_theme() == &palette) return false;
  set_active_theme_palette(palette);
  apply_current_theme();
  return true;
}

inline ThemeResolver &current_theme_resolver() {
  static ThemeResolver resolver{};
  return resolver;
}
