#include <cstdlib>

#include "theme_service.h"

using espcontrol::theme::ActiveTheme;
using espcontrol::theme::AutoStrategy;
using espcontrol::theme::ColorRole;
using espcontrol::theme::ThemeMode;
using espcontrol::theme::ThemeService;

namespace {

bool expect(bool condition) { return condition; }

}  // namespace

int main() {
  ThemeService service;
  if (!expect(service.mode() == ThemeMode::DARK &&
              service.auto_strategy() == AutoStrategy::TIME &&
              service.active_theme() == ActiveTheme::DARK)) {
    return EXIT_FAILURE;
  }
  if (!expect(espcontrol::theme::palette_for(ActiveTheme::DARK)
                  .color(ColorRole::BACKGROUND) == 0x000000 &&
              espcontrol::theme::palette_for(ActiveTheme::LIGHT)
                  .color(ColorRole::BACKGROUND) == 0xF2F2F2)) {
    return EXIT_FAILURE;
  }

  if (!expect(service.set_mode(ThemeMode::LIGHT) &&
              service.active_theme() == ActiveTheme::LIGHT &&
              !service.set_mode(ThemeMode::LIGHT))) {
    return EXIT_FAILURE;
  }
  if (!expect(service.set_mode(ThemeMode::DARK) &&
              service.active_theme() == ActiveTheme::DARK)) {
    return EXIT_FAILURE;
  }

  service.set_mode(ThemeMode::AUTO);
  service.update_clock(true, 12 * 60);
  if (!expect(service.active_theme() == ActiveTheme::LIGHT)) return EXIT_FAILURE;
  service.update_clock(true, 18 * 60);
  if (!expect(service.active_theme() == ActiveTheme::DARK)) return EXIT_FAILURE;

  service.set_time_schedule(22 * 60, 6 * 60);
  service.update_clock(true, 23 * 60);
  if (!expect(service.active_theme() == ActiveTheme::LIGHT)) return EXIT_FAILURE;
  service.update_clock(true, 5 * 60 + 59);
  if (!expect(service.active_theme() == ActiveTheme::LIGHT)) return EXIT_FAILURE;
  service.update_clock(true, 6 * 60);
  if (!expect(service.active_theme() == ActiveTheme::DARK)) return EXIT_FAILURE;

  service.set_time_schedule(8 * 60, 8 * 60);
  service.update_clock(true, 8 * 60);
  if (!expect(service.active_theme() == ActiveTheme::DARK)) return EXIT_FAILURE;
  service.set_time_schedule(ThemeService::MINUTES_PER_DAY, 18 * 60);
  if (!expect(service.active_theme() == ActiveTheme::DARK)) return EXIT_FAILURE;

  service.set_auto_strategy(AutoStrategy::SUNRISE_SUNSET);
  service.update_solar(true, 7 * 60, 17 * 60);
  service.update_clock(true, 12 * 60);
  if (!expect(service.active_theme() == ActiveTheme::LIGHT)) return EXIT_FAILURE;
  service.update_clock(true, 17 * 60);
  if (!expect(service.active_theme() == ActiveTheme::DARK)) return EXIT_FAILURE;

  service.update_solar(false, 0, 0);
  if (!expect(service.active_theme() == ActiveTheme::DARK)) return EXIT_FAILURE;
  service.update_solar(true, 7 * 60, 17 * 60);
  service.update_clock(false, 0);
  if (!expect(service.active_theme() == ActiveTheme::DARK)) return EXIT_FAILURE;

  service.update_clock(true, 12 * 60);
  if (!expect(service.active_theme() == ActiveTheme::LIGHT)) return EXIT_FAILURE;
  service.set_mode(ThemeMode::DARK);
  if (!expect(service.active_theme() == ActiveTheme::DARK)) return EXIT_FAILURE;
  service.set_mode(ThemeMode::AUTO);
  if (!expect(service.active_theme() == ActiveTheme::LIGHT)) return EXIT_FAILURE;
  service.set_auto_strategy(AutoStrategy::TIME);
  if (!expect(service.active_theme() == ActiveTheme::DARK)) return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
