#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>
#include "button_grid_modal_layout.h"

namespace espcontrol::power {

// v0.32 topology with outward placement in actual content bounds; no data/LVGL.
enum class FlowFamily { LANDSCAPE, SQUARE, PORTRAIT };
struct Point { double x = 0, y = 0; };
// Two tangent straights and one native quarter-circle, never a sampled polyline.
constexpr size_t FLOW_POINT_COUNT = 4;
struct FlowArc { Point center{}; int radius = 0, start_angle = 0, end_angle = 0; };
struct FlowGridCell { int col = 0, row = 0, cols = 1, rows = 1; };
struct FlowDashboardGrid {
  bool large = false;
  int cols = 1, rows = 1;
  FlowGridCell diagram{}, back{};
  std::array<FlowGridCell, 4> values{};
};
inline FlowDashboardGrid flow_dashboard_grid(const espcontrol::modal::DeviceProfile &profile,
                                             int cols, int rows, int width, int height) {
  using espcontrol::modal::LayoutFamily;
  const bool family = (profile.layout_family == LayoutFamily::WIDE_LANDSCAPE && rows == 3) ||
                      (profile.layout_family == LayoutFamily::LARGE_LANDSCAPE && rows == 4);
  if (!family || cols != 5 || width <= height) return {};
  FlowDashboardGrid grid;
  grid.large = true; grid.cols = cols; grid.rows = rows;
  grid.diagram = {0, 0, 3, rows};
  grid.back = {3, 0, 2, 1};
  grid.values = {{{3, 1, 1, 1}, {4, 1, 1, 1},
    {3, 2, rows == 4 ? 2 : 1, 1}, {rows == 4 ? 3 : 4, rows == 4 ? 3 : 2, rows == 4 ? 2 : 1, 1}}};
  return grid;
}
struct FlowViewport { int x, y, width, height; };
inline FlowViewport flow_viewport(int panel_width, int panel_height, int inset,
                                  int navigation_bottom) {
  // The full measured panel is the visual reference. Reserve navigation space
  // symmetrically, so Back/status chrome never shifts its geometric center.
  const int x = std::min(std::max(0, inset), std::max(0, (panel_width - 1) / 2));
  const int y = std::min(std::max(inset, navigation_bottom),
                         std::max(0, (panel_height - 1) / 2));
  return {x, y, std::max(1, panel_width - x * 2), std::max(1, panel_height - y * 2)};
}
template<typename Coordinate> inline Coordinate flow_coordinate(double value) {
  // The pinned renderer uses integer coordinates. Avoid directional truncation
  // bias; preserve subpixels if a future supported LVGL build enables floats.
  if constexpr (std::is_integral_v<Coordinate>) return static_cast<Coordinate>(std::lround(value));
  else return static_cast<Coordinate>(value);
}
using FlowPath = std::array<Point, FLOW_POINT_COUNT>;
struct FlowGeometry {
  std::array<Point, 4> nodes{}; // Solar, Home, Grid, Battery (model order)
  std::array<std::array<Point, 3>, 4> ports{};
  std::array<FlowPath, 7> paths{}; // Start, tangent entry, tangent exit, end
  std::array<FlowArc, 7> arcs{}; // Native clockwise sector; reversal shares it
  std::array<std::array<Point, 3>, 7> heads{};
  double scale = 1;
};
inline FlowFamily flow_family(int width, int height) {
  return width * 5 > height * 6 ? FlowFamily::LANDSCAPE
       : height * 5 > width * 6 ? FlowFamily::PORTRAIT : FlowFamily::SQUARE;
}
inline FlowGeometry flow_geometry(int width, int height, FlowFamily family,
                                  double node_width = 138, double node_height = 108) {
  FlowGeometry out;
  const double cx = width / 2.0, cy = height / 2.0;
  const double margin = std::max(4.0, std::min(width, height) * .02);
  const double hw = std::min(node_width / 2, width * .24);
  const double hh = std::min(node_height / 2, height * .24);
  const double dx = std::max(0.0, cx - hw - margin);
  const double dy = std::max(0.0, cy - hh - margin);
  const double port = std::min(18.0, std::min(hw, hh) / 3), gap = 4;
  out.nodes = {{{cx, cy - dy}, {cx + dx, cy}, {cx - dx, cy}, {cx, cy + dy}}};
  // Attachment points depend only on the content rectangle and compiled text
  // metrics, never readings, activity, SOC availability or edge visibility.
  out.ports = {{{{{cx - port, cy - dy + hh + gap}, {cx, cy - dy + hh + gap}, {cx + port, cy - dy + hh + gap}}},
                {{{cx + dx - hw - gap, cy - port}, {cx + dx - hw - gap, cy}, {cx + dx - hw - gap, cy + port}}},
                {{{cx - dx + hw + gap, cy - port}, {cx - dx + hw + gap, cy}, {cx - dx + hw + gap, cy + port}}},
                {{{cx - port, cy + dy - hh - gap}, {cx, cy + dy - hh - gap}, {cx + port, cy + dy - hh - gap}}}}};
  auto straight = [](Point a, Point b) { return FlowPath{{a, a, b, b}}; };
  auto bend = [&](size_t index, Point a, Point b, int sign, bool top) {
    // One radius for mirrored elbows, constrained by actual node clearance.
    const int r = std::max(0, static_cast<int>(std::floor(std::min({32.0,
      std::abs(b.y - a.y) * .42, std::abs(b.x - a.x) * .42}))));
    const Point center{a.x + sign * r, b.y + (top ? -r : r)};
    out.paths[index] = {{{a}, {a.x, center.y}, {center.x, b.y}, {b}}};
    const int start = top ? (sign > 0 ? 90 : 0) : (sign > 0 ? 180 : 270);
    out.arcs[index] = {center, r, start, start + 90};
  };
  bend(0, out.ports[0][2], out.ports[1][0], 1, true);
  out.paths[1] = straight(out.ports[0][1], out.ports[3][1]);
  bend(2, out.ports[0][0], out.ports[2][0], -1, true);
  bend(3, out.ports[3][2], out.ports[1][2], 1, false);
  bend(4, out.ports[3][0], out.ports[2][2], -1, false);
  out.paths[5] = straight(out.ports[2][1], out.ports[1][1]);
  std::reverse_copy(out.paths[4].begin(), out.paths[4].end(), out.paths[6].begin());
  out.arcs[6] = out.arcs[4]; // Identical sector, opposite traversal/arrowhead.
  const double ref_w = family == FlowFamily::LANDSCAPE ? 620 : 520;
  const double ref_h = family == FlowFamily::PORTRAIT ? 620 : 430;
  out.scale = std::max(0.0, std::min(width / ref_w, height / ref_h));
  const std::array<Point, 7> directions = {{{1,0}, {0,1}, {-1,0}, {1,0}, {-1,0}, {1,0}, {0,1}}};
  for (size_t i = 0; i < 7; ++i) {
    const Point end = out.paths[i].back(), dir = directions[i];
    const Point base{end.x - dir.x * 11 * out.scale, end.y - dir.y * 11 * out.scale};
    out.heads[i] = {{{base.x - dir.y * 5.5 * out.scale, base.y + dir.x * 5.5 * out.scale}, end,
                     {base.x + dir.y * 5.5 * out.scale, base.y - dir.x * 5.5 * out.scale}}};
  }
  return out;
}
} // namespace espcontrol::power
