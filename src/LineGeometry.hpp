#pragma once
// Extracted from InfiniPaint LineDrawTool; copyright 2025-2026 Yousef Khadadeh.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <Eigen/Core>
#include <include/core/SkPathBuilder.h>
#include <include/pathops/SkPathOps.h>
#include <numbers>
#include <cmath>
#include "PolygonGeometry.hpp"

namespace LineGeometry {
using Vector2f = Eigen::Vector2f;

static void add_circle_polygon(SkPathBuilder& builder, float x, float y, float radius) {
    const auto points = PolygonGeometry::circle({x, y}, radius);
    if (points.empty()) return;
    builder.moveTo(points.front().x, points.front().y);
    for (size_t i = 1; i < points.size(); ++i) builder.lineTo(points[i].x, points[i].y);
    builder.close();
}

static SkPath create_arrow_path(const Vector2f& tip, const Vector2f& dir, float arrowLen, float arrowWidth) {
    const auto points = PolygonGeometry::arrow({tip.x(), tip.y()}, {dir.x(), dir.y()}, arrowLen, arrowWidth);
    SkPathBuilder b;
    // Clockwise order in screen coordinates: w2 -> tip -> w1 -> close
    b.moveTo(points[0].x, points[0].y);
    b.lineTo(points[1].x, points[1].y);
    b.lineTo(points[2].x, points[2].y);
    b.close();
    return b.detach();
}

static SkPath create_segment_path(const Vector2f& p1, const Vector2f& p2, float width, bool roundCapStart, bool roundCapEnd) {
    Vector2f diff = p2 - p1;
    float len = diff.norm();
    float r = std::max(width * 0.5f, 0.5f);
    if(len < 0.25f) {
        SkPathBuilder b;
        if(roundCapStart || roundCapEnd)
            add_circle_polygon(b, p1.x(), p1.y(), r);
        return b.detach();
    }

    Vector2f dir = diff / len;
    Vector2f normal(-dir.y(), dir.x());

    Vector2f a1 = p1 + normal * r;
    Vector2f a2 = p1 - normal * r;
    Vector2f b1 = p2 + normal * r;
    Vector2f b2 = p2 - normal * r;

    std::vector<SkPoint> pts;
    pts.reserve(32);

    // 1. From a2 along the edge to b2
    pts.push_back(SkPoint::Make(a2.x(), a2.y()));
    pts.push_back(SkPoint::Make(b2.x(), b2.y()));

    // 2. Semicircular end cap at p2 (clockwise)
    if(roundCapEnd) {
        constexpr int ARC_STEPS = 12;
        float theta = std::atan2(dir.y(), dir.x());
        for(int k = 1; k < ARC_STEPS; ++k) {
            float alpha = (theta - std::numbers::pi_v<float> * 0.5f) + (static_cast<float>(k) / ARC_STEPS) * std::numbers::pi_v<float>;
            Vector2f arcPt = p2 + Vector2f(std::cos(alpha), std::sin(alpha)) * r;
            pts.push_back(SkPoint::Make(arcPt.x(), arcPt.y()));
        }
    }
    pts.push_back(SkPoint::Make(b1.x(), b1.y()));

    // 3. From b1 along the edge back to a1
    pts.push_back(SkPoint::Make(a1.x(), a1.y()));

    // 4. Semicircular start cap at p1 (clockwise)
    if(roundCapStart) {
        constexpr int ARC_STEPS = 12;
        float theta = std::atan2(-dir.y(), -dir.x());
        for(int k = 1; k < ARC_STEPS; ++k) {
            float alpha = (theta - std::numbers::pi_v<float> * 0.5f) + (static_cast<float>(k) / ARC_STEPS) * std::numbers::pi_v<float>;
            Vector2f arcPt = p1 + Vector2f(std::cos(alpha), std::sin(alpha)) * r;
            pts.push_back(SkPoint::Make(arcPt.x(), arcPt.y()));
        }
    }

    SkPathBuilder builder;
    builder.addPolygon({pts.data(), pts.size()}, true);
    return builder.detach();
}

template <class Config>
inline SkPath generate_line_path(const Vector2f& start, const Vector2f& end, float strokeWidth, const Config& config, bool useUnion = true) {
    if (!start.allFinite() || !end.allFinite() || !std::isfinite(strokeWidth) || strokeWidth <= 0 ||
        !std::isfinite(config.dashLength) || !std::isfinite(config.dashGap)) return SkPathBuilder().detach();
    Vector2f diff = end - start;
    float totalLength = diff.norm();
    if (!std::isfinite(totalLength)) return SkPathBuilder().detach();
    float radius = std::max(strokeWidth * 0.5f, 0.5f);
    if(totalLength < 0.5f) {
        SkPathBuilder b;
        if(config.hasRoundCaps)
            add_circle_polygon(b, start.x(), start.y(), radius);
        return b.detach();
    }

    if(config.lineStyle == 0 && config.arrowMode == 0) {
        return create_segment_path(start, end, strokeWidth, config.hasRoundCaps, config.hasRoundCaps);
    }

    Vector2f dir = diff / totalLength;
    float arrowLen = std::min(strokeWidth * 3.5f, totalLength * 0.45f);
    float arrowWidth = std::max(strokeWidth * 2.8f, strokeWidth + 2.0f);

    bool hasStartArrow = (config.arrowMode == 2 || config.arrowMode == 3);
    bool hasEndArrow = (config.arrowMode == 1 || config.arrowMode == 3);

    SkPathBuilder arrowsBuilder;
    if(hasStartArrow) {
        arrowsBuilder.addPath(create_arrow_path(start, -dir, arrowLen, arrowWidth));
    }
    if(hasEndArrow) {
        arrowsBuilder.addPath(create_arrow_path(end, dir, arrowLen, arrowWidth));
    }
    SkPath arrowsPath = arrowsBuilder.detach();

    // For solid lines, overlap the shaft and arrow base before boolean union.
    // For non-solid lines: pattern runs between arrow bases without overlapping
    float overlap = (config.lineStyle == 0) ? std::min(arrowLen * 0.4f, strokeWidth * 0.8f) : 0.0f;

    Vector2f effectiveStart = hasStartArrow ? (start + dir * (arrowLen - overlap)) : start;
    Vector2f effectiveEnd = hasEndArrow ? (end - dir * (arrowLen - overlap)) : end;

    // Flush flat cap where the line connects to an arrow base, round cap only on free ends
    bool roundStart = config.hasRoundCaps && !hasStartArrow;
    bool roundEnd = config.hasRoundCaps && !hasEndArrow;

    Vector2f bodyDiff = effectiveEnd - effectiveStart;
    float bodyLength = bodyDiff.dot(dir);

    SkPathBuilder bodyBuilder;
    if(bodyLength > 0.5f) {
        int style = config.lineStyle;
        float dashLen = std::max(config.dashLength, 0.5f) * strokeWidth;
        float gapLen = std::max(config.dashGap, 0.5f) * strokeWidth;

        if(style == 0) { // Solid
            bodyBuilder.addPath(create_segment_path(effectiveStart, effectiveEnd, strokeWidth, roundStart, roundEnd));
        }
        else if(style == 1) { // Dashed
            float period = dashLen + gapLen;
            const int count = PolygonGeometry::patternCount(bodyLength, period, 28);
            if (!count) return SkPathBuilder().detach();
            for(int i = 0; i < count; ++i) {
                const float t = static_cast<float>(double(i) * period);
                float tEnd = std::min(t + dashLen, bodyLength);
                if(tEnd - t < 0.5f) continue;
                Vector2f p1 = effectiveStart + dir * t;
                Vector2f p2 = effectiveStart + dir * tEnd;
                bodyBuilder.addPath(create_segment_path(p1, p2, strokeWidth, config.hasRoundCaps, config.hasRoundCaps));
            }
        }
        else if(style == 2) { // Dotted (Circles)
            float dotSpacing = std::max(config.dashGap * strokeWidth, radius * 2.0f + 2.0f);
            const auto circle = PolygonGeometry::circle({0, 0}, radius);
            const int numDots = PolygonGeometry::patternCount(bodyLength, dotSpacing, circle.size(), true);
            if (!numDots) return SkPathBuilder().detach();
            float step = bodyLength / static_cast<float>(numDots - 1);
            for(int i = 0; i < numDots; ++i) {
                Vector2f center = effectiveStart + dir * (i * step);
                add_circle_polygon(bodyBuilder, center.x(), center.y(), radius);
            }
        }
        else if(style == 3) { // Dash-Dot
            float dotDiameter = radius * 2.0f;
            float cycleLen = dashLen + gapLen + dotDiameter + gapLen;
            const auto circle = PolygonGeometry::circle({0, 0}, radius);
            if (circle.empty()) return SkPathBuilder().detach();
            const int count = PolygonGeometry::patternCount(bodyLength, cycleLen, 28 + circle.size());
            if (!count) return SkPathBuilder().detach();
            for(int i = 0; i < count; ++i) {
                const float t = static_cast<float>(double(i) * cycleLen);
                float dashEnd = std::min(t + dashLen, bodyLength);
                if(dashEnd - t >= 0.5f) {
                    Vector2f d1 = effectiveStart + dir * t;
                    Vector2f d2 = effectiveStart + dir * dashEnd;
                    bodyBuilder.addPath(create_segment_path(d1, d2, strokeWidth, config.hasRoundCaps, config.hasRoundCaps));
                }

                float dotCenterT = t + dashLen + gapLen + radius;
                if(dotCenterT <= bodyLength) {
                    Vector2f dotCenter = effectiveStart + dir * dotCenterT;
                    add_circle_polygon(bodyBuilder, dotCenter.x(), dotCenter.y(), radius);
                }
            }
        }
        else if(style == 4) { // Dash-Dot-Dot
            float dotDiameter = radius * 2.0f;
            float cycleLen = dashLen + gapLen + dotDiameter + gapLen + dotDiameter + gapLen;
            const auto circle = PolygonGeometry::circle({0, 0}, radius);
            if (circle.empty()) return SkPathBuilder().detach();
            const int count = PolygonGeometry::patternCount(bodyLength, cycleLen, 28 + 2 * circle.size());
            if (!count) return SkPathBuilder().detach();
            for(int i = 0; i < count; ++i) {
                const float t = static_cast<float>(double(i) * cycleLen);
                float dashEnd = std::min(t + dashLen, bodyLength);
                if(dashEnd - t >= 0.5f) {
                    Vector2f d1 = effectiveStart + dir * t;
                    Vector2f d2 = effectiveStart + dir * dashEnd;
                    bodyBuilder.addPath(create_segment_path(d1, d2, strokeWidth, config.hasRoundCaps, config.hasRoundCaps));
                }

                float dot1CenterT = t + dashLen + gapLen + radius;
                if(dot1CenterT <= bodyLength) {
                    Vector2f dot1Center = effectiveStart + dir * dot1CenterT;
                    add_circle_polygon(bodyBuilder, dot1Center.x(), dot1Center.y(), radius);
                }

                float dot2CenterT = t + dashLen + gapLen + dotDiameter + gapLen + radius;
                if(dot2CenterT <= bodyLength) {
                    Vector2f dot2Center = effectiveStart + dir * dot2CenterT;
                    add_circle_polygon(bodyBuilder, dot2Center.x(), dot2Center.y(), radius);
                }
            }
        }
    }
    SkPath bodyPath = bodyBuilder.detach();

    if(arrowsPath.isEmpty()) {
        return bodyPath;
    }
    if(bodyPath.isEmpty()) {
        return arrowsPath;
    }

    std::optional<SkPath> unionResult;
    if (useUnion) unionResult = Op(bodyPath, arrowsPath, SkPathOp::kUnion_SkPathOp);
    if(unionResult.has_value() && !unionResult.value().isEmpty()) {
        return unionResult.value();
    }

    // Safe fallback with matching positive winding
    SkPathBuilder fallback;
    fallback.addPath(bodyPath);
    fallback.addPath(arrowsPath);
    return fallback.detach();
}

} // namespace

