#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <type_traits>

#include "power_flow_model.h"

using namespace espcontrol::power;
using V = CalculationValidity;
constexpr double MISSING = std::numeric_limits<double>::quiet_NaN();
static_assert(std::is_trivially_copyable<Model>::value, "model stores no dynamic ownership");
static_assert(sizeof(Model) <= 256, "keep per-card power state bounded");

struct Scenario {
  const char *name;
  std::array<double, 4> watts;
  Polarity polarity;
  std::array<double, 7> inferred;
  V validity;
  const char *secondary;
  ReadingValidity absent_validity = ReadingValidity::MISSING;
};

// Solar, Home, Grid, Battery; then SH, SB, SG, BH, BG, GH, GB.
const Scenario scenarios[] = {
  {"solar home", {1000,1000,0,0}, {}, {1000,0,0,0,0,0,0}, V::VALID, "Idle"},
  {"solar battery", {1500,1000,0,-500}, {}, {1000,500,0,0,0,0,0}, V::VALID, "Charging"},
  {"solar grid", {1500,1000,-500,0}, {}, {1000,0,500,0,0,0,0}, V::VALID, "Exporting"},
  {"battery home", {0,1000,0,1000}, {}, {0,0,0,1000,0,0,0}, V::VALID, "Discharging"},
  {"battery grid", {0,0,-500,500}, {}, {0,0,0,0,500,0,0}, V::VALID, "Discharging · Exporting"},
  {"grid home", {0,1000,1000,0}, {}, {0,0,0,0,0,1000,0}, V::VALID, "Importing"},
  {"grid battery", {0,0,500,-500}, {}, {0,0,0,0,0,0,500}, V::VALID, "Charging · Importing"},
  {"import charging", {300,1000,1000,-300}, {}, {300,0,0,0,0,700,300}, V::VALID, "Charging · Importing"},
  {"discharge export", {300,700,-400,800}, {}, {300,0,0,400,400,0,0}, V::VALID, "Discharging · Exporting"},
  {"solar charge export", {1800,1000,-300,-500}, {}, {1000,500,300,0,0,0,0}, V::VALID, "Charging · Exporting"},
  {"zero", {0,0,0,0}, {}, {}, V::VALID, "Idle"},
  {"near zero", {.5,.5,.5,-.5}, {}, {}, V::VALID, "Idle"},
  {"idle boundary", {0,1,1,0}, {}, {}, V::VALID, "Idle"},
  {"above idle", {0,1.1,1.1,0}, {}, {0,0,0,0,0,1.1,0}, V::VALID, "Importing"},
  {"accepted residual", {300,295,0,0}, {}, {295,0,0,0,0,0,0}, V::VALID, "Idle"},
  {"absolute boundary", {100,90,0,0}, {}, {90,0,0,0,0,0,0}, V::VALID, "Idle"},
  {"absolute outside", {100,89,0,0}, {}, {}, V::INCONSISTENT, "Idle"},
  {"negative absolute boundary", {90,100,0,0}, {}, {90,0,0,0,0,0,0}, V::VALID, "Idle"},
  {"negative absolute outside", {89,100,0,0}, {}, {}, V::INCONSISTENT, "Idle"},
  {"relative boundary", {1200,1176,0,0}, {}, {1176,0,0,0,0,0,0}, V::VALID, "Idle"},
  {"relative outside", {1200,1175,0,0}, {}, {}, V::INCONSISTENT, "Idle"},
  {"negative relative boundary", {1176,1200,0,0}, {}, {1176,0,0,0,0,0,0}, V::VALID, "Idle"},
  {"negative relative outside", {1175,1200,0,0}, {}, {}, V::INCONSISTENT, "Idle"},
  {"large imbalance retains status", {500,1000,200,-100}, {}, {}, V::INCONSISTENT, "Charging · Importing"},
  {"missing solar", {MISSING,1000,700,300}, {}, {}, V::UNAVAILABLE, "Discharging · Importing"},
  {"missing home", {500,MISSING,-200,-300}, {}, {}, V::UNAVAILABLE, "Charging · Exporting"},
  {"missing grid", {500,500,MISSING,0}, {}, {}, V::UNAVAILABLE, "Unavailable"},
  {"missing battery", {500,1000,500,MISSING}, {}, {}, V::UNAVAILABLE, "Importing"},
  {"invalid grid", {0,1000,MISSING,1000}, {}, {}, V::UNAVAILABLE, "Discharging", ReadingValidity::INVALID},
  {"both statuses missing", {500,500,MISSING,MISSING}, {}, {}, V::UNAVAILABLE, "Unavailable"},
  {"grid inversion", {0,1000,-1000,0}, {true,false}, {0,0,0,0,0,1000,0}, V::VALID, "Importing"},
  {"battery inversion", {0,1000,0,-1000}, {false,true}, {0,0,0,1000,0,0,0}, V::VALID, "Discharging"},
  {"both inversions", {300,1000,-1000,300}, {true,true}, {300,0,0,0,0,700,300}, V::VALID, "Charging · Importing"},
  {"no automatic inversion", {300,1000,-1000,300}, {}, {}, V::INCONSISTENT, "Discharging · Exporting"},
  {"hidden allocation consumes capacity", {1000,999.5,50,-50}, {}, {999.5,.5,0,0,0,0,49.5}, V::VALID, "Charging · Importing"},
};

void near(double actual, double expected) { assert(std::abs(actual - expected) < 1e-8); }

void assert_capacity(const Calculation &result) {
  const auto &f = result.inferred_w;
  const auto &n = result.nodes;
  auto capacity = [&](size_t i, bool sink) {
    return n[i].valid() ? std::max(0.0, n[i].value * (sink ? -1 : 1)) : 0.0;
  };
  assert(f[0] + f[1] + f[2] <= capacity(0,false) + 1e-8);
  assert(f[3] + f[4] <= capacity(3,false) + 1e-8);
  assert(f[5] + f[6] <= capacity(2,false) + 1e-8);
  assert(f[0] + f[3] + f[5] <= capacity(1,false) + 1e-8);
  assert(f[1] + f[6] <= capacity(3,true) + 1e-8);
  assert(f[2] + f[4] <= capacity(2,true) + 1e-8);
  assert(!(result.edge_active(Edge::GRID_BATTERY) && result.edge_active(Edge::BATTERY_GRID)));
}

void table_tests() {
  for (const auto &test : scenarios) for (const char *unit : {"W", "kW"}) {
    std::array<Reading,4> readings{};
    for (size_t i = 0; i < readings.size(); ++i) {
      if (std::isnan(test.watts[i])) { readings[i].validity = test.absent_validity; continue; }
      std::ostringstream state;
      state.precision(17);
      state << test.watts[i] / (std::string(unit) == "kW" ? 1000 : 1);
      readings[i] = parse_power(state.str(), unit, static_cast<Node>(i));
    }
    const auto result = calculate(readings, test.polarity);
    if (result.validity != test.validity) std::cerr << test.name << " " << unit << '\n';
    assert(result.validity == test.validity);
    assert(result.secondary_label() == test.secondary);
    for (size_t i = 0; i < result.inferred_w.size(); ++i) {
      assert(std::isfinite(result.inferred_w[i]) && result.inferred_w[i] >= 0);
      near(result.inferred_w[i], test.inferred[i]);
      assert(result.edge_active(static_cast<Edge>(i)) == (test.inferred[i] > IDLE_THRESHOLD_W));
    }
    for (size_t i = 0; i < readings.size(); ++i) {
      assert(result.nodes[i].valid() == readings[i].valid());
      if (readings[i].valid()) near(std::abs(result.nodes[i].value), std::abs(test.watts[i]));
    }
    assert_capacity(result);
  }
  // Sweep balanced source/sink combinations to test conservation beyond examples.
  for (double solar : {0.,.5,1.,1.1,100.,1000.})
    for (double grid : {-500.,-1.,0.,1.,500.})
      for (double battery : {-500.,-1.,0.,1.,500.}) {
        const double home = solar + grid + battery;
        if (home < 0) continue;
        auto result = calculate({{{solar,ReadingValidity::VALID},{home,ReadingValidity::VALID},
                                  {grid,ReadingValidity::VALID},{battery,ReadingValidity::VALID}}});
        assert(result.validity == V::VALID);
        near(result.residual_w, 0);
        assert_capacity(result);
        if ((solar == 0 || solar > 1) && (grid == 0 || std::abs(grid) > 1) &&
            (battery == 0 || std::abs(battery) > 1) && (home == 0 || home > 1)) {
          double total = 0;
          for (double value : result.inferred_w) total += value;
          near(total, solar + std::max(grid,0.0) + std::max(battery,0.0));
        }
      }
}

void parsing_tests() {
  for (const char *state : {"", "unknown", "unavailable", " \t "})
    assert(parse_power(state,"W",Node::HOME).validity == ReadingValidity::MISSING);
  for (const char *state : {"NaN", "nan", "inf", "-inf", "1e309", "1e-400", "100 W", "12bad",
                           "1 2", "1,2", "0x10", "0x1p2", "+", ".", "1e", "1e+", "--1"})
    assert(parse_power(state,"W",Node::GRID).validity == ReadingValidity::INVALID);
  for (const char *unit : {"", "KW", "w", "MW", "Wh", "kWh", "%", "watts"})
    assert(!parse_power("1",unit,Node::HOME).valid());
  near(parse_power(" \t+1.25e1\r\n", " kW ", Node::HOME).value, 12500);
  near(parse_power("-.5", "W", Node::GRID).value, -.5);
  near(parse_power("-1", "W", Node::SOLAR).value, -1); // tolerated idle-band noise
  assert(!parse_power("-1.01", "W", Node::SOLAR).valid());
  assert(!parse_power("-2", "W", Node::HOME).valid());
  assert(!parse_power("1e300", "kW", Node::GRID).valid());
  assert(!parse_power("2147483", "W", Node::GRID).valid());
  assert(normalize_power({MAX_POWER_W,ReadingValidity::VALID},"W",Node::HOME).valid());
  assert(!normalize_power({MAX_POWER_W+1,ReadingValidity::VALID},"W",Node::HOME).valid());
  for (double value : {std::numeric_limits<double>::infinity(), MISSING})
    assert(!normalize_power({value,ReadingValidity::VALID},"W",Node::HOME).valid());
  for (const char *state : {"0", "78", "100"}) assert(parse_soc(state,"%").valid());
  for (const char *state : {"-1", "100.01", "101", "NaN", "inf", "78%"}) assert(!parse_soc(state,"%").valid());
  assert(!parse_soc("78","W").valid());
  assert(parse_soc("unavailable","%").validity == ReadingValidity::MISSING);
}

void status_tests() {
  const char *expected[4][4] = {
    {"Unavailable","Unavailable","Importing","Exporting"},
    {"Unavailable","Idle","Importing","Exporting"},
    {"Charging","Charging","Charging · Importing","Charging · Exporting"},
    {"Discharging","Discharging","Discharging · Importing","Discharging · Exporting"},
  };
  const double battery[] = {MISSING,0,-10,10}, grid[] = {MISSING,0,10,-10};
  for (size_t b = 0; b < 4; ++b) for (size_t g = 0; g < 4; ++g) {
    std::array<Reading,4> nodes{};
    if (b) nodes[3] = {battery[b],ReadingValidity::VALID};
    if (g) nodes[2] = {grid[g],ReadingValidity::VALID};
    const auto result = calculate(nodes);
    assert(result.validity == V::UNAVAILABLE);
    assert(result.secondary_label() == expected[b][g]);
  }
  Calculation translated;
  translated.battery_status = Status::CHARGING;
  translated.grid_status = Status::EXPORTING;
  set_espcontrol_language("it");
  assert(translated.secondary_label() == "In carica · Esportazione");
  set_espcontrol_language("en");
}

void transition_tests() {
  Model model;
  model.set_power(Node::SOLAR,"0","W");
  model.set_power(Node::HOME,"1000","W");
  model.set_power(Node::GRID,"1000","W");
  model.set_power(Node::BATTERY,"0","W");
  assert(model.calculation.edge_active(Edge::GRID_HOME));
  model.set_power(Node::SOLAR,"bad","W");
  assert(model.calculation.validity == V::UNAVAILABLE);
  assert(model.calculation.nodes[0].validity == ReadingValidity::INVALID);
  assert(model.calculation.nodes[1].value == 1000);
  assert(model.calculation.secondary_label() == "Importing");
  for (double value : model.calculation.inferred_w) assert(value == 0);
  model.set_power(Node::SOLAR,"0","W");
  assert(model.calculation.edge_active(Edge::GRID_HOME));
  model.set_power(Node::HOME,"2000","W");
  assert(model.calculation.validity == V::INCONSISTENT);
  for (double value : model.calculation.inferred_w) assert(value == 0);
  model.set_power(Node::HOME,"1000","W");
  assert(model.calculation.edge_active(Edge::GRID_HOME));
  model.set_soc("78","%");
  assert(model.battery_soc.value == 78);
  model.set_soc("101","%");
  assert(!model.battery_soc.valid());
  assert(model.calculation.edge_active(Edge::GRID_HOME));
  model.set_polarity({true,false});
  assert(model.calculation.validity == V::INCONSISTENT);
  model.set_power(Node::GRID,"-1000","W");
  assert(model.calculation.edge_active(Edge::GRID_HOME));
  // Recalculation never inverts the stored raw normalized value a second time.
  model.set_polarity({true,false});
  assert(model.calculation.edge_active(Edge::GRID_HOME));
  model.invalidate();
  assert(model.calculation.validity == V::UNAVAILABLE);
  assert(model.calculation.secondary_label() == "Unavailable");
  for (const auto &reading : model.readings) assert(!reading.valid());
  assert(!model.battery_soc.valid());
  assert(model.polarity.invert_grid); // reconnect preserves configuration
}

int main() {
  table_tests();
  parsing_tests();
  status_tests();
  transition_tests();
  std::cout << "35 scenarios in W/kW; conservation, parsing, statuses and lifecycle passed\n";
}
