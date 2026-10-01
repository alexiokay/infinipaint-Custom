#pragma once
// Extracted from InfiniPaint MeshCanvasComponent; copyright 2025-2026 Yousef Khadadeh.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <include/core/SkPathBuilder.h>
#include <cereal/types/vector.hpp>
#include <stdexcept>
#include <string>
#include "Helpers/Serializers.hpp"

template <typename Archive> void skpath_write(const SkPath& p, Archive& a) {
    std::vector<std::vector<SkPoint>> contours;
    bool moveHappened = false;

    SkPath::Iter iter(p, true);
    for(;;) {
        std::optional<SkPath::IterRec> rec = iter.next();
        if(!rec.has_value())
            break;

        switch(rec->fVerb) {
            case SkPathVerb::kClose:
                if(moveHappened) {
                    contours.back().pop_back();
                    moveHappened = false;
                }
                break;
            case SkPathVerb::kLine:
                contours.back().emplace_back(rec->fPoints[1]);
                break;
            case SkPathVerb::kMove:
                contours.emplace_back();
                contours.back().emplace_back(rec->fPoints[0]);
                moveHappened = true;
                break;
            default:
                throw std::runtime_error("[get_predraw_data_accurate] Illegal verb " + std::to_string(static_cast<unsigned>(rec->fVerb)));
                break;
        }
    }

    a(p.getFillType() == SkPathFillType::kEvenOdd, contours);
}

template <typename Archive> SkPath skpath_read(Archive& a) {
    bool isEvenOdd;
    std::vector<std::vector<SkPoint>> contours;
    a(isEvenOdd, contours);
    // NOTE: setFillType doesn't properly set the fill type for the path IN DEBUG BUILDS. Setting fill type in builder constructor meanwhile works for both debug and release builds
    SkPathBuilder builder(isEvenOdd ? SkPathFillType::kEvenOdd : SkPathFillType::kWinding);
    for(const std::vector<SkPoint>& contour : contours) {
        if(contour.size() == 0)
            continue;
        builder.moveTo(contour[0]);
        for(uint32_t i = 1; i < contour.size(); i++)
            builder.lineTo(contour[i]);
        builder.close();
    }
    return builder.detach();
}
