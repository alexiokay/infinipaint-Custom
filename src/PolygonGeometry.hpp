#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

// Polygon-only geometry shared by tools and behavioral tests. Mesh persistence
// and accurate triangulation do not accept Skia curve verbs.
namespace PolygonGeometry {
struct Point { float x; float y; };
inline constexpr size_t maxPatternVertices = 65536;

// Counts are checked in double precision before any conversion to int or
// allocation. Dots use rounded endpoint spacing; repeating patterns use ceil.
inline int patternCount(double length, double spacing, size_t verticesPerItem, bool dots = false) {
    if (!std::isfinite(length) || !std::isfinite(spacing) || length < 0 || spacing <= 0 ||
        verticesPerItem == 0 || verticesPerItem > maxPatternVertices) return 0;
    const double count = dots ? std::max(2.0, std::round(length / spacing) + 1) : std::ceil(length / spacing);
    const size_t limit = std::min<size_t>(10000, maxPatternVertices / verticesPerItem);
    if (!std::isfinite(count) || count < 1 || count > double(limit)) return 0;
    return static_cast<int>(count);
}

inline std::vector<Point> circle(Point center, float radius) {
    if (!std::isfinite(center.x) || !std::isfinite(center.y) ||
        !std::isfinite(radius) || radius <= 0) return {};
    // Sagitta <= 0.05 object units. Reject unreasonable tessellation rather
    // than silently generating a visibly inaccurate or unbounded polygon.
    const double angle = std::acos(std::clamp(1.0 - 0.05 / radius, -1.0, 1.0));
    const double required = std::ceil(std::numbers::pi / angle);
    if (!std::isfinite(required) || required > 4096) return {};
    const int count = std::max(24, static_cast<int>(required));
    std::vector<Point> points;
    points.reserve(count);
    for (int i = 0; i < count; ++i) {
        const double a = 2 * std::numbers::pi * i / count;
        points.push_back({center.x + radius * static_cast<float>(std::cos(a)),
                          center.y + radius * static_cast<float>(std::sin(a))});
    }
    return points;
}

inline std::vector<Point> arrow(Point tip, Point direction, float length, float width) {
    const Point normal{-direction.y, direction.x};
    const Point base{tip.x - direction.x * length, tip.y - direction.y * length};
    return {{base.x - normal.x * width / 2, base.y - normal.y * width / 2}, tip,
            {base.x + normal.x * width / 2, base.y + normal.y * width / 2}};
}

inline bool hasArea(const std::vector<Point>& points) {
    if (points.size() < 3) return false;
    for (const auto& p : points)
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) return false;
    const auto a = points.front();
    size_t second = 1;
    while (second < points.size() && points[second].x == a.x && points[second].y == a.y) ++second;
    if (second == points.size()) return false;
    const double dx = double(points[second].x) - a.x;
    const double dy = double(points[second].y) - a.y;
    for (const auto& p : points)
        if (std::abs(dx * (double(p.y) - a.y) - dy * (double(p.x) - a.x)) > 1e-6)
            return true;
    return false;
}
}
