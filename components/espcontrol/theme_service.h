#pragma once

#include <cstdint>

namespace espcontrol::theme {

enum class ThemeMode : uint8_t {
  DARK,
  LIGHT,
  AUTO,
};

enum class AutoStrategy : uint8_t {
  TIME,
  SUNRISE_SUNSET,
};

enum class ActiveTheme : uint8_t {
  DARK,
  LIGHT,
};

enum class ColorRole : uint8_t {
  BACKGROUND,
  SURFACE,
  SURFACE_SECONDARY,
  TEXT_PRIMARY,
  TEXT_SECONDARY,
  BORDER,
  CONTROL_BACKGROUND,
  DISABLED,
  MODAL_BACKGROUND,
  OVERLAY,
  TRACK,
};

struct ThemePalette {
  uint32_t background;
  uint32_t surface;
  uint32_t surface_secondary;
  uint32_t text_primary;
  uint32_t text_secondary;
  uint32_t border;
  uint32_t control_background;
  uint32_t disabled;
  uint32_t modal_background;
  uint32_t overlay;
  uint32_t track;

  constexpr uint32_t color(ColorRole role) const {
    switch (role) {
      case ColorRole::BACKGROUND: return background;
      case ColorRole::SURFACE: return surface;
      case ColorRole::SURFACE_SECONDARY: return surface_secondary;
      case ColorRole::TEXT_PRIMARY: return text_primary;
      case ColorRole::TEXT_SECONDARY: return text_secondary;
      case ColorRole::BORDER: return border;
      case ColorRole::CONTROL_BACKGROUND: return control_background;
      case ColorRole::DISABLED: return disabled;
      case ColorRole::MODAL_BACKGROUND: return modal_background;
      case ColorRole::OVERLAY: return overlay;
      case ColorRole::TRACK: return track;
    }
    return background;
  }
};

// Raw RGB values are deliberately centralized here. Display-specific colour
// correction remains an LVGL adapter concern so the resolver stays testable.
constexpr ThemePalette DARK_PALETTE{
    0x000000,  // background
    0x313131,  // surface
    0x212121,  // surface secondary
    0xFFFFFF,  // text primary
    0xB0B0B0,  // text secondary
    0x313131,  // border
    0x313131,  // control background
    0x313131,  // disabled
    0x212121,  // modal background
    0x000000,  // overlay
    0x313131,  // track
};

// Initial light baseline. Keep these values together so physical-display
// testing can tune the palette without touching component implementations.
constexpr ThemePalette LIGHT_PALETTE{
    0xF2F2F2,  // background
    0xFFFFFF,  // surface
    0xE7E7E7,  // surface secondary
    0x202124,  // text primary
    0x666A70,  // text secondary
    0xC8CBD0,  // border
    0xE3E5E8,  // control background
    0xA7ABB0,  // disabled
    0xFFFFFF,  // modal background
    0x000000,  // overlay
    0xC8CBD0,  // track
};

constexpr const ThemePalette &palette_for(ActiveTheme theme) {
  return theme == ActiveTheme::LIGHT ? LIGHT_PALETTE : DARK_PALETTE;
}

struct ClockState {
  bool available{false};
  uint16_t minute_of_day{0};
};

struct SolarState {
  bool available{false};
  uint16_t sunrise_minute{0};
  uint16_t sunset_minute{0};
};

class ThemeService {
 public:
  static constexpr uint16_t MINUTES_PER_DAY = 24 * 60;
  static constexpr uint16_t DEFAULT_LIGHT_START = 6 * 60;
  static constexpr uint16_t DEFAULT_DARK_START = 18 * 60;

  ThemeMode mode() const { return mode_; }
  AutoStrategy auto_strategy() const { return auto_strategy_; }
  ActiveTheme active_theme() const { return active_theme_; }
  uint16_t light_start_minute() const { return light_start_minute_; }
  uint16_t dark_start_minute() const { return dark_start_minute_; }
  const ClockState &clock() const { return clock_; }
  const SolarState &solar() const { return solar_; }
  uint32_t revision() const { return revision_; }

  bool set_mode(ThemeMode mode) {
    mode_ = mode;
    return refresh();
  }

  bool set_auto_strategy(AutoStrategy strategy) {
    auto_strategy_ = strategy;
    return refresh();
  }

  bool set_time_schedule(uint16_t light_start_minute,
                         uint16_t dark_start_minute) {
    light_start_minute_ = light_start_minute;
    dark_start_minute_ = dark_start_minute;
    return refresh();
  }

  bool update_clock(bool available, uint16_t minute_of_day) {
    clock_ = {available && minute_of_day < MINUTES_PER_DAY, minute_of_day};
    return refresh();
  }

  bool update_solar(bool available, uint16_t sunrise_minute,
                    uint16_t sunset_minute) {
    solar_ = {available && sunrise_minute < MINUTES_PER_DAY &&
                            sunset_minute < MINUTES_PER_DAY,
              sunrise_minute, sunset_minute};
    return refresh();
  }

  bool refresh() {
    const ActiveTheme resolved = resolve();
    if (resolved == active_theme_) return false;
    active_theme_ = resolved;
    ++revision_;
    return true;
  }

  ActiveTheme resolve() const {
    if (mode_ == ThemeMode::LIGHT) return ActiveTheme::LIGHT;
    if (mode_ == ThemeMode::DARK || !clock_.available) {
      return ActiveTheme::DARK;
    }

    if (auto_strategy_ == AutoStrategy::TIME) {
      if (!valid_window(light_start_minute_, dark_start_minute_)) {
        return ActiveTheme::DARK;
      }
      return inside_window(clock_.minute_of_day, light_start_minute_,
                           dark_start_minute_)
                 ? ActiveTheme::LIGHT
                 : ActiveTheme::DARK;
    }

    if (!solar_.available ||
        !valid_window(solar_.sunrise_minute, solar_.sunset_minute)) {
      return ActiveTheme::DARK;
    }
    return inside_window(clock_.minute_of_day, solar_.sunrise_minute,
                         solar_.sunset_minute)
               ? ActiveTheme::LIGHT
               : ActiveTheme::DARK;
  }

  static constexpr bool valid_window(uint16_t start, uint16_t end) {
    return start < MINUTES_PER_DAY && end < MINUTES_PER_DAY && start != end;
  }

  static constexpr bool inside_window(uint16_t minute, uint16_t start,
                                      uint16_t end) {
    if (!valid_window(start, end) || minute >= MINUTES_PER_DAY) return false;
    if (start < end) return minute >= start && minute < end;
    return minute >= start || minute < end;
  }

 private:
  ThemeMode mode_{ThemeMode::DARK};
  AutoStrategy auto_strategy_{AutoStrategy::TIME};
  ActiveTheme active_theme_{ActiveTheme::DARK};
  uint16_t light_start_minute_{DEFAULT_LIGHT_START};
  uint16_t dark_start_minute_{DEFAULT_DARK_START};
  ClockState clock_{};
  SolarState solar_{};
  uint32_t revision_{0};
};

}  // namespace espcontrol::theme
