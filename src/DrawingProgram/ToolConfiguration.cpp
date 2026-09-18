/*  
 * InfiniPaint
 * Copyright (C) 2025-2026 Yousef Khadadeh
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ToolConfiguration.hpp"
#include "DrawingProgram.hpp"
#include "../World.hpp"
#include "../MainProgram.hpp"
#include "Helpers/FixedPoint.hpp"
#include <optional>
#include "Helpers/Logger.hpp"
#include "Tools/DrawingProgramToolBase.hpp"

#include "../GUIStuff/ElementHelpers/NumberSliderHelpers.hpp"
#include "../GUIStuff/ElementHelpers/ButtonHelpers.hpp"

float& ToolConfiguration::get_stroke_size_relative_width_ref(DrawingProgramToolType toolType) {
    if(globalConf.useGlobalRelativeWidth)
        return globalConf.relativeWidth;
    switch(toolType) {
        case DrawingProgramToolType::LINE:
            return lineDraw.relativeWidth;
        case DrawingProgramToolType::BRUSH:
            return brush.relativeWidth;
        case DrawingProgramToolType::ELLIPSE:
            return ellipseDraw.relativeWidth;
        case DrawingProgramToolType::RECTANGLE:
            return rectDraw.relativeWidth;
        case DrawingProgramToolType::ERASER:
            return eraser.relativeWidth;
        default:
            return globalConf.relativeWidth;
    }
    return globalConf.relativeWidth;
}

const float& ToolConfiguration::get_stroke_size_relative_width_ref(DrawingProgramToolType toolType) const {
    if(globalConf.useGlobalRelativeWidth)
        return globalConf.relativeWidth;
    switch(toolType) {
        case DrawingProgramToolType::LINE:
            return lineDraw.relativeWidth;
        case DrawingProgramToolType::BRUSH:
            return brush.relativeWidth;
        case DrawingProgramToolType::ELLIPSE:
            return ellipseDraw.relativeWidth;
        case DrawingProgramToolType::RECTANGLE:
            return rectDraw.relativeWidth;
        case DrawingProgramToolType::ERASER:
            return eraser.relativeWidth;
        default:
            return globalConf.relativeWidth;
    }
    return globalConf.relativeWidth;
}

std::pair<std::optional<float>, ToolConfiguration::RelativeWidthFailCode> ToolConfiguration::get_relative_width_from_value(DrawingProgram& drawP, const WorldScalar& camInverseScale, float relativeWidth) const {
    auto& lockedCameraScale = drawP.controls.lockedCameraScale;
    if(lockedCameraScale.has_value()) {
        float lockMultiplier = static_cast<float>(WorldMultiplier(lockedCameraScale.value()) / WorldMultiplier(camInverseScale));
        if(lockMultiplier < 0.005f)
            return {std::nullopt, RelativeWidthFailCode::TOO_ZOOMED_OUT};
        else if(lockMultiplier > 200.0f)
            return {std::nullopt, RelativeWidthFailCode::TOO_ZOOMED_IN};
        return {relativeWidth * lockMultiplier, RelativeWidthFailCode::SUCCESS};
    }
    return {relativeWidth, RelativeWidthFailCode::SUCCESS};
}

std::pair<std::optional<float>, ToolConfiguration::RelativeWidthFailCode> ToolConfiguration::get_relative_width_stroke_size(DrawingProgram& drawP, const WorldScalar& camInverseScale) const {
    return get_relative_width_from_value(drawP, camInverseScale, get_stroke_size_relative_width_ref(drawP.drawTool->get_type()));
}

void ToolConfiguration::print_relative_width_fail_message(RelativeWidthFailCode failCode) {
    switch(failCode) {
        case RelativeWidthFailCode::SUCCESS:
            break;
        case RelativeWidthFailCode::TOO_ZOOMED_IN:
            Logger::get().log(Logger::LogType::USERINFO, "Zoomed in too much! Unlock size or zoom out");
            break;
        case RelativeWidthFailCode::TOO_ZOOMED_OUT:
            Logger::get().log(Logger::LogType::USERINFO, "Zoomed out too much! Unlock size or zoom in");
            break;
    }
}

void ToolConfiguration::relative_width_gui(DrawingProgram& drawP, const char* label) {
    auto& gui = drawP.world.main.g.gui;
    auto& lockedCameraScale = drawP.controls.lockedCameraScale;
    GUIStuff::ElementHelpers::slider_scalar_field(gui, "relstrokewidth", label, &get_stroke_size_relative_width_ref(drawP.drawTool->get_type()), 3.0f, 40.0f);
    GUIStuff::ElementHelpers::text_button(gui, "lock brush size", lockedCameraScale.has_value() ? "Unlock Size" : "Lock Size to Zoom", {
        .isSelected = lockedCameraScale.has_value(),
        .wide = true,
        .onClick = [&] {
            if(lockedCameraScale.has_value())
                lockedCameraScale = std::nullopt;
            else
                lockedCameraScale = drawP.world.drawData.cam.c.inverseScale;
        }
    });
}

void ToolConfiguration::init_default_presets_if_empty() {
    if (!presets.empty()) return;
    presets = {
        BrushPreset{
            .name = "Studio Pen",
            .relativeWidth = 12.0f,
            .color = Vector4f{1.0f, 1.0f, 1.0f, 1.0f},
            .hasRoundCaps = true,
            .engine = BrushPressure::Engine::Samples,
            .pressureResponse = BrushPressure::Response::Time,
            .rendering = BrushPressure::Rendering::Polyline,
            .pressureTimeMs = 40.0,
            .localCorrection = true,
            .smoothingFactor = 0.707f,
            .minimumSize = 0.1f,
            .pressureAffectsWidth = true,
            .grainIntensity = 0.0f
        },
        BrushPreset{
            .name = "Fine Liner",
            .relativeWidth = 4.0f,
            .color = Vector4f{1.0f, 1.0f, 1.0f, 1.0f},
            .hasRoundCaps = true,
            .engine = BrushPressure::Engine::Samples,
            .pressureResponse = BrushPressure::Response::Preserve,
            .rendering = BrushPressure::Rendering::Polyline,
            .pressureTimeMs = 30.0,
            .localCorrection = true,
            .smoothingFactor = 0.85f,
            .minimumSize = 0.25f,
            .pressureAffectsWidth = true,
            .grainIntensity = 0.0f
        },
        BrushPreset{
            .name = "Soft Pencil",
            .relativeWidth = 8.0f,
            .color = Vector4f{0.75f, 0.75f, 0.8f, 0.85f},
            .hasRoundCaps = true,
            .engine = BrushPressure::Engine::Samples,
            .pressureResponse = BrushPressure::Response::Time,
            .rendering = BrushPressure::Rendering::BoundedCurves,
            .pressureTimeMs = 60.0,
            .localCorrection = false,
            .smoothingFactor = 0.5f,
            .minimumSize = 0.05f,
            .pressureAffectsWidth = true,
            .grainIntensity = 0.0f
        },
        BrushPreset{
            .name = "Marker",
            .relativeWidth = 28.0f,
            .color = Vector4f{0.95f, 0.85f, 0.35f, 0.9f},
            .hasRoundCaps = false,
            .engine = BrushPressure::Engine::Compatibility,
            .pressureResponse = BrushPressure::Response::Original,
            .rendering = BrushPressure::Rendering::Polyline,
            .pressureTimeMs = 40.0,
            .localCorrection = false,
            .smoothingFactor = 0.707f,
            .minimumSize = 0.5f,
            .pressureAffectsWidth = false,
            .grainIntensity = 0.0f
        }
    };
    selectedPreset = 0;
    brush.engine = BrushPressure::Engine::Samples;
    brush.pressureResponse = BrushPressure::Response::Time;
}

void ToolConfiguration::apply_brush_preset(DrawingProgram& drawP, size_t index) {
    if (index >= presets.size()) return;
    selectedPreset = static_cast<int>(index);
    const auto& p = presets[index];
    brush.relativeWidth = p.relativeWidth;
    globalConf.foregroundColor = p.color;
    brush.hasRoundCaps = p.hasRoundCaps;
    brush.engine = p.engine;
    brush.pressureResponse = p.pressureResponse;
    brush.rendering = p.rendering;
    brush.pressureTimeMs = p.pressureTimeMs;
    drawP.world.main.conf.tabletOptions.penFilter.enabled = p.localCorrection;
    drawP.world.main.conf.tabletOptions.brushPressureSmoothingFactor = p.smoothingFactor;
    drawP.world.main.conf.tabletOptions.brushMinimumSize = p.minimumSize;
    drawP.world.main.conf.tabletOptions.pressureAffectsBrushWidth = p.pressureAffectsWidth;
    brush.grainIntensity = p.grainIntensity;
    drawP.world.main.g.gui.set_to_layout();
}

void ToolConfiguration::save_current_brush_preset(DrawingProgram& drawP, const std::string& customName) {
    init_default_presets_if_empty();
    std::string name = customName;
    if (name.empty()) {
        name = "Setup " + std::to_string(presets.size() + 1);
    }
    BrushPreset p{
        .name = name,
        .relativeWidth = brush.relativeWidth,
        .color = globalConf.foregroundColor,
        .hasRoundCaps = brush.hasRoundCaps,
        .engine = brush.engine,
        .pressureResponse = brush.pressureResponse,
        .rendering = brush.rendering,
        .pressureTimeMs = brush.pressureTimeMs,
        .localCorrection = drawP.world.main.conf.tabletOptions.penFilter.enabled,
        .smoothingFactor = drawP.world.main.conf.tabletOptions.brushPressureSmoothingFactor,
        .minimumSize = drawP.world.main.conf.tabletOptions.brushMinimumSize,
        .pressureAffectsWidth = drawP.world.main.conf.tabletOptions.pressureAffectsBrushWidth,
        .grainIntensity = brush.grainIntensity
    };
    presets.push_back(p);
    selectedPreset = static_cast<int>(presets.size() - 1);
    drawP.world.main.g.gui.set_to_layout();
}

void ToolConfiguration::delete_brush_preset(size_t index) {
    if (index >= presets.size() || presets.size() <= 1) return;
    presets.erase(presets.begin() + index);
    if (selectedPreset >= static_cast<int>(presets.size()))
        selectedPreset = static_cast<int>(presets.size() - 1);
}

