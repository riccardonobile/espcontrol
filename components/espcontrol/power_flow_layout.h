#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace espcontrol::power {

// v0.32 flowDiagram geometry only, independent of readings and LVGL.
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
inline FlowGeometry flow_geometry(int width, int height, FlowFamily family) {
  FlowGeometry out;
  const bool landscape = family == FlowFamily::LANDSCAPE;
  const bool square = family == FlowFamily::SQUARE;
  const double w = landscape ? 620 : 520, h = landscape || square ? 430 : 620;
  const double dx = landscape ? 192 : 158, dy = landscape ? 143 : square ? 132 : 174;
  const double cx = w / 2, cy = h / 2;
  out.nodes = {{{cx, cy - dy}, {cx + dx, cy}, {cx - dx, cy}, {cx, cy + dy}}};
  out.ports = {{{{{cx - 18, cy - dy + 54}, {cx, cy - dy + 54}, {cx + 18, cy - dy + 54}}},
                {{{cx + dx - 69, cy - 18}, {cx + dx - 69, cy}, {cx + dx - 69, cy + 18}}},
                {{{cx - dx + 69, cy - 18}, {cx - dx + 69, cy}, {cx - dx + 69, cy + 18}}},
                {{{cx - 18, cy + dy - 54}, {cx, cy + dy - 54}, {cx + 18, cy + dy - 54}}}}};
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
  const std::array<Point, 7> directions = {{{1,0}, {0,1}, {-1,0}, {1,0}, {-1,0}, {1,0}, {0,1}}};
  for (size_t i = 0; i < 7; ++i) {
    const Point end = out.paths[i].back(), dir = directions[i];
    const Point base{end.x - dir.x * 11, end.y - dir.y * 11};
    out.heads[i] = {{{base.x - dir.y * 5.5, base.y + dir.x * 5.5}, end,
                     {base.x + dir.y * 5.5, base.y - dir.x * 5.5}}};
  }
  out.scale = std::max(0.0, std::min(width / w, height / h));
  const double ox = (width - w * out.scale) / 2, oy = (height - h * out.scale) / 2;
  auto transform = [&](Point &p) { p = {ox + p.x * out.scale, oy + p.y * out.scale}; };
  for (auto &p : out.nodes) transform(p);
  for (auto &ports : out.ports) for (auto &p : ports) transform(p);
  for (auto &path : out.paths) for (auto &p : path) transform(p);
  for (auto &head : out.heads) for (auto &p : head) transform(p);
  return out;
}
} // namespace espcontrol::power
