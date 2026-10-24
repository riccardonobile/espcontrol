#pragma once

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>

#include "i18n_generated.h"

// Instantaneous power only. No LVGL, HA subscription or object lifetime state.
namespace espcontrol::power {

constexpr double IDLE_THRESHOLD_W = 1.0;
constexpr double MIN_BALANCE_TOLERANCE_W = 10.0;
constexpr double RELATIVE_BALANCE_TOLERANCE = 0.02;
// Leave room for clock_bar.h's int-based fixed-decimal formatter at precision 3.
constexpr double MAX_POWER_W = std::numeric_limits<int>::max() / 1000 - 1;

enum class ReadingValidity : uint8_t { MISSING, INVALID, VALID };
enum class Node : uint8_t { SOLAR, HOME, GRID, BATTERY };
enum class Edge : uint8_t {
  SOLAR_HOME, SOLAR_BATTERY, SOLAR_GRID, BATTERY_HOME,
  BATTERY_GRID, GRID_HOME, GRID_BATTERY,
};
enum class CalculationValidity : uint8_t { UNAVAILABLE, INCONSISTENT, VALID };
enum class Status : uint8_t { UNAVAILABLE, IDLE, CHARGING, DISCHARGING, IMPORTING, EXPORTING };

struct Reading {
  double value = 0;
  ReadingValidity validity = ReadingValidity::MISSING;
  bool valid() const { return validity == ReadingValidity::VALID; }
};

inline std::string_view trim(std::string_view value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) return {};
  return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

// Decimal HA state, consuming the whole string. strtod alone also accepts hex,
// partial numbers and non-finite spellings, none of which are power readings.
inline Reading parse_number(std::string_view state) {
  state = trim(state);
  if (state.empty() || state == "unknown" || state == "unavailable") return {};
  size_t pos = 0;
  if (state[pos] == '+' || state[pos] == '-') ++pos;
  bool digits = false;
  auto consume_digits = [&]() {
    while (pos < state.size() && state[pos] >= '0' && state[pos] <= '9') {
      digits = true;
      ++pos;
    }
  };
  consume_digits();
  if (pos < state.size() && state[pos] == '.') { ++pos; consume_digits(); }
  if (!digits) return {0, ReadingValidity::INVALID};
  if (pos < state.size() && (state[pos] == 'e' || state[pos] == 'E')) {
    ++pos;
    if (pos < state.size() && (state[pos] == '+' || state[pos] == '-')) ++pos;
    digits = false;
    consume_digits();
    if (!digits) return {0, ReadingValidity::INVALID};
  }
  if (pos != state.size()) return {0, ReadingValidity::INVALID};
  const std::string number(state);
  char *end = nullptr;
  errno = 0;
  const double value = std::strtod(number.c_str(), &end);
  if (errno == ERANGE || end != number.c_str() + number.size() || !std::isfinite(value))
    return {0, ReadingValidity::INVALID};
  return {value, ReadingValidity::VALID};
}

inline Reading normalize_power(Reading reading, std::string_view unit, Node node) {
  if (!reading.valid()) return reading;
  unit = trim(unit);
  if (unit != "W" && unit != "kW") return {0, ReadingValidity::INVALID};
  const double watts = reading.value * (unit == "kW" ? 1000.0 : 1.0);
  if (!std::isfinite(watts) || std::abs(watts) > MAX_POWER_W ||
      ((node == Node::SOLAR || node == Node::HOME) && watts < -IDLE_THRESHOLD_W))
    return {0, ReadingValidity::INVALID};
  return {watts, ReadingValidity::VALID};
}

inline Reading parse_power(std::string_view state, std::string_view unit, Node node) {
  return normalize_power(parse_number(state), unit, node);
}

inline Reading parse_soc(std::string_view state, std::string_view unit) {
  auto reading = parse_number(state);
  if (!reading.valid()) return reading;
  if (trim(unit) != "%" || reading.value < 0 || reading.value > 100)
    return {0, ReadingValidity::INVALID};
  return reading;
}

struct Polarity {
  bool invert_grid = false;
  bool invert_battery = false;
};

inline Status power_status(Reading reading, bool battery) {
  if (!reading.valid()) return Status::UNAVAILABLE;
  if (std::abs(reading.value) <= IDLE_THRESHOLD_W) return Status::IDLE;
  if (battery) return reading.value > 0 ? Status::DISCHARGING : Status::CHARGING;
  return reading.value > 0 ? Status::IMPORTING : Status::EXPORTING;
}

inline const char *status_text(Status status) {
  switch (status) {
    case Status::CHARGING: return espcontrol_i18n("Charging");
    case Status::DISCHARGING: return espcontrol_i18n("Discharging");
    case Status::IMPORTING: return espcontrol_i18n("Importing");
    case Status::EXPORTING: return espcontrol_i18n("Exporting");
    case Status::IDLE: return espcontrol_i18n("Idle");
    default: return espcontrol_i18n("Unavailable");
  }
}

struct Calculation {
  // Canonical watts after optional inversion; invalid nodes never become zero.
  std::array<Reading, 4> nodes{};
  CalculationValidity validity = CalculationValidity::UNAVAILABLE;
  Status battery_status = Status::UNAVAILABLE;
  Status grid_status = Status::UNAVAILABLE;
  double residual_w = 0;
  double tolerance_w = MIN_BALANCE_TOLERANCE_W;
  // Inferred visual allocations, NOT measured physical routes. Hidden <=1 W
  // allocations still consume source/sink capacities and remain in this array.
  std::array<double, 7> inferred_w{};

  bool edge_active(Edge edge) const {
    return validity == CalculationValidity::VALID &&
           inferred_w[static_cast<size_t>(edge)] > IDLE_THRESHOLD_W;
  }
  std::string secondary_label() const {
    std::string text;
    if (battery_status == Status::CHARGING || battery_status == Status::DISCHARGING)
      text = status_text(battery_status);
    if (grid_status == Status::IMPORTING || grid_status == Status::EXPORTING) {
      if (!text.empty()) text += " \xC2\xB7 ";
      text += status_text(grid_status);
    }
    if (!text.empty()) return text;
    return status_text(battery_status == Status::IDLE && grid_status == Status::IDLE
                       ? Status::IDLE : Status::UNAVAILABLE);
  }
};

inline Calculation calculate(std::array<Reading, 4> nodes, Polarity polarity = {}) {
  Calculation out;
  // Validate even direct numeric inputs; parsing is not the only model entry.
  for (size_t i = 0; i < nodes.size(); ++i)
    nodes[i] = normalize_power(nodes[i], "W", static_cast<Node>(i));
  if (polarity.invert_grid) nodes[2].value = -nodes[2].value;
  if (polarity.invert_battery) nodes[3].value = -nodes[3].value;
  out.nodes = nodes;
  out.battery_status = power_status(nodes[3], true);
  out.grid_status = power_status(nodes[2], false);
  for (const auto &node : nodes) if (!node.valid()) return out;

  const double s = nodes[0].value, h = nodes[1].value;
  const double g = nodes[2].value, b = nodes[3].value;
  const double sources = std::max(s, 0.0) + std::max(g, 0.0) + std::max(b, 0.0);
  const double sinks = std::max(h, 0.0) + std::max(-g, 0.0) + std::max(-b, 0.0);
  out.residual_w = s + g + b - h;
  out.tolerance_w = std::max(MIN_BALANCE_TOLERANCE_W,
                           RELATIVE_BALANCE_TOLERANCE * std::max(sources, sinks));
  if (std::abs(out.residual_w) > out.tolerance_w) {
    out.validity = CalculationValidity::INCONSISTENT;
    return out;
  }
  out.validity = CalculationValidity::VALID;
  auto capacity = [](double watts) { return watts > IDLE_THRESHOLD_W ? watts : 0.0; };
  double solar = capacity(s), home = capacity(h);
  double grid_source = capacity(g), grid_sink = capacity(-g);
  double battery_source = capacity(b), battery_sink = capacity(-b);
  auto allocate = [&](Edge edge, double &source, double &sink) {
    const double amount = std::min(source, sink);
    out.inferred_w[static_cast<size_t>(edge)] = amount;
    source -= amount;
    sink -= amount;
  };
  allocate(Edge::SOLAR_HOME, solar, home);
  allocate(Edge::SOLAR_BATTERY, solar, battery_sink);
  allocate(Edge::SOLAR_GRID, solar, grid_sink);
  allocate(Edge::BATTERY_HOME, battery_source, home);
  allocate(Edge::BATTERY_GRID, battery_source, grid_sink);
  allocate(Edge::GRID_HOME, grid_source, home);
  allocate(Edge::GRID_BATTERY, grid_source, battery_sink);
  return out;
}

struct Model {
  std::array<Reading, 4> readings{};
  Reading battery_soc{};
  Polarity polarity{};
  Calculation calculation{};

  void set_power(Node node, std::string_view state, std::string_view unit) {
    readings[static_cast<size_t>(node)] = parse_power(state, unit, node);
    calculation = calculate(readings, polarity);
  }
  void set_soc(std::string_view state, std::string_view unit) {
    battery_soc = parse_soc(state, unit);
  }
  void set_polarity(Polarity value) {
    polarity = value;
    calculation = calculate(readings, polarity);
  }
  // The future card owner calls this on disconnect/replacement before rebinding.
  void invalidate() {
    readings = {};
    battery_soc = {};
    calculation = {};
  }
};

}  // namespace espcontrol::power
