#include <cassert>

#include "theme_settings.h"

struct ThemeCounter { int calls = 0; };
void count_theme(void *context, const ThemePalette &) {
  ++static_cast<ThemeCounter *>(context)->calls;
}

int main() {
  int minute = -1;
  assert(parse_theme_time("00:00", minute) && minute == 0);
  assert(parse_theme_time("23:59", minute) && minute == 1439);
  for (const char *invalid : {"24:00", "23:60", "7:00", "07:0", "07:00x", "ab:cd"})
    assert(!parse_theme_time(invalid, minute));
  assert(parse_theme_mode("Dark") == ThemeMode::DARK);
  assert(parse_theme_mode("Light") == ThemeMode::LIGHT);
  assert(parse_theme_mode("Schedule") == ThemeMode::SCHEDULE);
  assert(parse_theme_mode("Sun") == ThemeMode::SUN);
  assert(parse_theme_mode("") == ThemeMode::DARK);  // Older saved configuration.

  ThemeResolver resolver;
  ThemeSettings settings;
  ThemeConditions conditions;
  ThemeCounter counter;
  assert(register_theme_refresh(&counter, count_theme, &counter));
  assert(!apply_theme_resolution(resolver, settings, conditions));
  settings.mode = ThemeMode::LIGHT;
  assert(apply_theme_resolution(resolver, settings, conditions));
  assert(&current_theme() == &LIGHT_THEME && counter.calls == 1);
  assert(!apply_theme_resolution(resolver, settings, conditions));
  settings.mode = ThemeMode::DARK;
  assert(apply_theme_resolution(resolver, settings, conditions));
  assert(&current_theme() == &DARK_THEME && counter.calls == 2);

  settings.mode = ThemeMode::SCHEDULE;
  conditions.time_valid = true;
  for (int now : {0, 419, 1200, 1380}) {
    conditions.local_minute = now;
    assert(resolve_theme(settings, conditions, EffectiveTheme::LIGHT) == EffectiveTheme::DARK);
  }
  for (int now : {420, 720, 1199}) {
    conditions.local_minute = now;
    assert(resolve_theme(settings, conditions, EffectiveTheme::DARK) == EffectiveTheme::LIGHT);
  }
  settings.light_start = 1200;
  settings.dark_start = 420;
  for (int now : {0, 419, 1200, 1439}) {
    conditions.local_minute = now;
    assert(resolve_theme(settings, conditions, EffectiveTheme::DARK) == EffectiveTheme::LIGHT);
  }
  for (int now : {420, 1199}) {
    conditions.local_minute = now;
    assert(resolve_theme(settings, conditions, EffectiveTheme::LIGHT) == EffectiveTheme::DARK);
  }
  settings.dark_start = settings.light_start;
  assert(resolve_theme(settings, conditions, EffectiveTheme::LIGHT) == EffectiveTheme::DARK);
  conditions.time_valid = false;
  assert(resolve_theme(settings, conditions, EffectiveTheme::LIGHT) == EffectiveTheme::LIGHT);

  settings.mode = ThemeMode::SUN;
  conditions.time_valid = true;
  conditions.sunrise_minute = 360;
  conditions.sunset_minute = 1080;
  conditions.sun_valid = true;
  conditions.local_minute = 600;
  assert(resolve_theme(settings, conditions, EffectiveTheme::DARK) == EffectiveTheme::LIGHT);
  conditions.local_minute = 1200;
  assert(resolve_theme(settings, conditions, EffectiveTheme::LIGHT) == EffectiveTheme::DARK);
  conditions.sun_valid = false;
  assert(resolve_theme(settings, conditions, EffectiveTheme::LIGHT) == EffectiveTheme::LIGHT);
  assert(resolve_theme(settings, conditions, ThemeResolver{}.effective) == EffectiveTheme::DARK);
  assert(!apply_theme_resolution(resolver, settings, conditions));
  unregister_theme_refresh(&counter);
}
