#include <cassert>

#include "button_grid_style.h"

struct ThemeSample {
  uint32_t background = 0;
  uint32_t text = 0;
  int applications = 0;
  const ThemePalette *previous = nullptr;
};

void apply_theme_sample(void *context, const ThemePalette &theme) {
  auto &sample = *static_cast<ThemeSample *>(context);
  sample.background = theme.background;
  sample.text = theme.text_primary;
  sample.previous = &theme_refresh_previous();
  ++sample.applications;
}

int main() {
  assert(&current_theme() == &DARK_THEME);
  assert(current_theme().background == 0x000000);
  assert(current_theme().surface_primary == 0x313131);
  assert(current_theme().surface_secondary == 0x212121);
  assert(current_theme().text_primary == 0xFFFFFF);
  assert(current_theme().text_muted == 0xB0B0B0);
  assert(current_theme().text_inverted == 0x000000);
  assert(current_theme().text_disabled == 0x707070);
  assert(current_theme().border == 0x313131);
  assert(current_theme().control_neutral == 0x313131);
  assert(current_theme().track_background == 0x313131);
  assert(current_theme().overlay == 0x000000);
  assert(current_theme().setup_action == 0x333333);
  assert(readable_text_color_for_bg(0xFFFFFF) ==
         theme_display_color(DARK_THEME.surface_secondary));
  assert(readable_text_color_for_bg(0x000000) == DARK_THEME.text_primary);

  set_current_button_primary_color(0xAABBCC);
  ThemeSample sample;
  assert(register_theme_refresh(&sample, apply_theme_sample, &sample));
  assert(register_theme_refresh(&sample, apply_theme_sample, &sample));
  ThemeSample others[16];
  for (int i = 0; i < 15; ++i)
    assert(register_theme_refresh(&others[i], apply_theme_sample, &others[i]));
  assert(!register_theme_refresh(&others[15], apply_theme_sample, &others[15]));
  apply_current_theme();
  apply_current_theme();
  assert(sample.applications == 0);  // Dark -> Dark changes no live styles.

  ThemePalette alternate = DARK_THEME;
  alternate.background = 0x123456;
  alternate.surface_primary = 0x445566;
  alternate.surface_secondary = 0x112233;
  alternate.text_primary = 0xEEEEEE;
  set_active_theme_palette(alternate);
  apply_current_theme();
  assert(sample.background == 0x123456 && sample.text == 0xEEEEEE);
  assert(sample.applications == 1);
  assert(sample.previous == &DARK_THEME);
  for (int i = 0; i < 15; ++i) assert(others[i].applications == 1);
  unregister_theme_refresh(&others[0]);
  assert(register_theme_refresh(&others[15], apply_theme_sample, &others[15]));
  apply_current_theme();
  assert(others[15].applications == 1);
  apply_current_theme();
  assert(sample.applications == 1);
  CardPalette card;
  assert(card.on_val == DEFAULT_ACCENT_COLOR);
  assert(card.off_val == theme_display_color(alternate.surface_primary));
  assert(card.sensor_val == theme_display_color(alternate.surface_secondary));
  assert(current_button_primary_color() == 0xAABBCC);
  assert(CARD_ACCENT_TEXT_COLOR == 0xFFFFFF);
  assert(readable_text_color_for_bg(0x000000) == CARD_ACCENT_TEXT_COLOR);
  assert(readable_text_color_for_bg(0xFFFFFF) == CARD_CONTRAST_DARK_COLOR);

  set_active_theme_palette(DARK_THEME);
  apply_current_theme();
  assert(sample.background == 0x000000 && sample.text == 0xFFFFFF);
  assert(sample.applications == 2);
  assert(sample.previous == &alternate);
  assert(others[0].applications == 1);
  assert(others[15].applications == 2);
  unregister_theme_refresh(&sample);
  for (int i = 1; i < 16; ++i) unregister_theme_refresh(&others[i]);
  return 0;
}
