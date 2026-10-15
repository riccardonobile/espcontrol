#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace espcontrol::power {

// v0.32 topology with outward placement in actual content bounds; no data/LVGL.
enum class FlowFamily { LANDSCAPE, SQUARE, PORTRAIT };
struct Point { double x = 0, y = 0; };
constexpr size_t FLOW_POINT_COUNT = 11;
using FlowPath = std::array<Point, FLOW_POINT_COUNT>;
struct FlowGeometry {
  std::array<Point, 4> nodes{}; // Solar, Home, Grid, Battery (model order)
  std::array<std::array<Point, 3>, 4> ports{};
  std::array<FlowPath, 7> paths{}; // Model Edge order
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
  auto straight = [](Point a, Point b) {
    FlowPath path{};
    for (size_t i = 0; i < path.size(); ++i) {
      const double t = double(i) / (path.size() - 1);
      path[i] = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    }
    return path;
  };
  auto bend = [](Point a, Point b, int sign, bool top) {
    FlowPath path{};
    const double r = std::min({top ? 40.0 : 30.0, std::abs(b.y - a.y) * .42,
                               std::abs(b.x - a.x) * .42});
    const Point start{a.x, b.y + (top ? -r : r)}, end{a.x + sign * r, b.y};
    path[0] = a;
    // Fixed quadratic samples: endpoints and tangents match the SVG Q command.
    for (size_t i = 1; i < path.size() - 1; ++i) {
      const double t = double(i - 1) / (path.size() - 3), u = 1 - t;
      path[i] = {u * u * start.x + 2 * u * t * a.x + t * t * end.x,
                 u * u * start.y + 2 * u * t * b.y + t * t * end.y};
    }
    path.back() = b;
    return path;
  };
  out.paths[0] = bend(out.ports[0][2], out.ports[1][0], 1, true);
  out.paths[1] = straight(out.ports[0][1], out.ports[3][1]);
  out.paths[2] = bend(out.ports[0][0], out.ports[2][0], -1, true);
  out.paths[3] = bend(out.ports[3][2], out.ports[1][2], 1, false);
  out.paths[4] = bend(out.ports[3][0], out.ports[2][2], -1, false);
  out.paths[5] = straight(out.ports[2][1], out.ports[1][1]);
  std::reverse_copy(out.paths[4].begin(), out.paths[4].end(), out.paths[6].begin());
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
