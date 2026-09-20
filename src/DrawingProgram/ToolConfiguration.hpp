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

#pragma once
#include "../SharedTypes.hpp"
#include "../BrushPressureConfig.hpp"
#include "../ToolPanelPreferences.hpp"
#include "nlohmann/json.hpp"
#include "../GUIStuff/GUIManager.hpp"
#include "../WorldScreenshot.hpp"
#include "Tools/DrawingProgramToolBase.hpp"

class ToolConfiguration {
    public:
        using BrushToolConfig = BrushPressure::Config;
        BrushToolConfig brush;

        ToolPanelPreferences toolPanel;

        struct EraserToolConfig {
            float relativeWidth = 15.0f;
            bool eraseDetail = false;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EraserToolConfig, relativeWidth, eraseDetail)
        } eraser;

        struct EllipseDrawToolConfig {
            float relativeWidth = 15.0f;
            unsigned fillStrokeMode = 1;
            bool perfectCircle = false;
            bool fromCenter = false;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EllipseDrawToolConfig, relativeWidth, fillStrokeMode, perfectCircle, fromCenter)
        } ellipseDraw;

        struct RectDrawToolConfig {
            float relativeWidth = 15.0f;
            float relativeRadiusWidth = 10.0f;
            int fillStrokeMode = 1;
            bool perfectSquare = false;
            bool fromCenter = false;
            int aspectRatioMode = 0;
            float customAspectX = 16.0f;
            float customAspectY = 9.0f;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RectDrawToolConfig, relativeWidth, relativeRadiusWidth, fillStrokeMode, perfectSquare, fromCenter, aspectRatioMode, customAspectX, customAspectY)
        } rectDraw;

        struct EyeDropperToolConfig {
            bool selectingStrokeColor = true;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EyeDropperToolConfig, selectingStrokeColor)
        } eyeDropper;

        struct LineDrawToolConfig {
            bool hasRoundCaps = true;
            float relativeWidth = 15.0f;
            int lineStyle = 0;
            float dashLength = 3.0f;
            float dashGap = 2.0f;
            int arrowMode = 0;
            bool snapAngles = false;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(LineDrawToolConfig, hasRoundCaps, relativeWidth, lineStyle, dashLength, dashGap, arrowMode, snapAngles)
        } lineDraw;

        struct ScreenshotToolConfig {
            int setDimensionSize = 1000;
            bool setDimensionIsX = true;
            WorldScreenshotInfo::ScreenshotType selectedType = WorldScreenshotInfo::ScreenshotType::JPG;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ScreenshotToolConfig, setDimensionSize, setDimensionIsX, selectedType)
        } screenshot;

        struct GlobalConfig {
            Vector4f foregroundColor{1.0f, 1.0f, 1.0f, 1.0f};
            Vector4f backgroundColor{0.0f, 0.0f, 0.0f, 1.0f};
            float relativeWidth = 15.0f;
            bool useGlobalRelativeWidth = false;
            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(GlobalConfig, useGlobalRelativeWidth, foregroundColor, backgroundColor, relativeWidth)
        } globalConf;

        struct BrushPreset {
            std::string name = "Brush";
            float relativeWidth = 15.0f;
            Vector4f color{1.0f, 1.0f, 1.0f, 1.0f};
            bool hasRoundCaps = true;
            BrushPressure::Engine engine = BrushPressure::Engine::Compatibility;
            BrushPressure::Response pressureResponse = BrushPressure::Response::Time;
            BrushPressure::Rendering rendering = BrushPressure::Rendering::Polyline;
            double pressureTimeMs = 40.0;
            bool localCorrection = false;
            float smoothingFactor = 0.707f;
            float minimumSize = 0.0f;
            bool pressureAffectsWidth = true;
            float grainIntensity = 0.0f;

            friend void to_json(nlohmann::json& j, const BrushPreset& p) {
                const char* mode = p.pressureResponse == BrushPressure::Response::Preserve ? "preserve" :
                    p.pressureResponse == BrushPressure::Response::Peak ? "peak" :
                    p.pressureResponse == BrushPressure::Response::Time ? "time" : "original";
                j = {
                    {"name", p.name},
                    {"relativeWidth", p.relativeWidth},
                    {"color", {p.color[0], p.color[1], p.color[2], p.color[3]}},
                    {"hasRoundCaps", p.hasRoundCaps},
                    {"engine", p.engine == BrushPressure::Engine::Compatibility ? "original" : "samples"},
                    {"pressureResponse", mode},
                    {"rendering", p.rendering == BrushPressure::Rendering::Polyline ? "polyline" : "bounded"},
                    {"pressureTimeMs", p.pressureTimeMs},
                    {"localCorrection", p.localCorrection},
                    {"smoothingFactor", p.smoothingFactor},
                    {"minimumSize", p.minimumSize},
                    {"pressureAffectsWidth", p.pressureAffectsWidth},
                    {"grainIntensity", p.grainIntensity}
                };
            }

            friend void from_json(const nlohmann::json& j, BrushPreset& p) {
                p = BrushPreset{};
                if (j.contains("name") && j["name"].is_string()) p.name = j["name"].get<std::string>();
                if (j.contains("relativeWidth") && j["relativeWidth"].is_number()) p.relativeWidth = j["relativeWidth"].get<float>();
                if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 4) {
                    p.color = Vector4f{j["color"][0].get<float>(), j["color"][1].get<float>(), j["color"][2].get<float>(), j["color"][3].get<float>()};
                }
                if (j.contains("hasRoundCaps") && j["hasRoundCaps"].is_boolean()) p.hasRoundCaps = j["hasRoundCaps"].get<bool>();
                if (j.value("engine", "") == "samples") p.engine = BrushPressure::Engine::Samples;
                else p.engine = BrushPressure::Engine::Compatibility;
                if (j.contains("pressureResponse")) {
                    std::string r = j["pressureResponse"].get<std::string>();
                    if (r == "preserve") p.pressureResponse = BrushPressure::Response::Preserve;
                    else if (r == "peak") p.pressureResponse = BrushPressure::Response::Peak;
                    else if (r == "time") p.pressureResponse = BrushPressure::Response::Time;
                    else p.pressureResponse = BrushPressure::Response::Original;
                }
                if (j.value("rendering", "") == "bounded") p.rendering = BrushPressure::Rendering::BoundedCurves;
                else p.rendering = BrushPressure::Rendering::Polyline;
                if (j.contains("pressureTimeMs") && j["pressureTimeMs"].is_number()) p.pressureTimeMs = j["pressureTimeMs"].get<double>();
                if (j.contains("localCorrection") && j["localCorrection"].is_boolean()) p.localCorrection = j["localCorrection"].get<bool>();
                if (j.contains("smoothingFactor") && j["smoothingFactor"].is_number()) p.smoothingFactor = j["smoothingFactor"].get<float>();
                if (j.contains("minimumSize") && j["minimumSize"].is_number()) p.minimumSize = j["minimumSize"].get<float>();
                if (j.contains("pressureAffectsWidth") && j["pressureAffectsWidth"].is_boolean()) p.pressureAffectsWidth = j["pressureAffectsWidth"].get<bool>();
                if (j.contains("grainIntensity") && j["grainIntensity"].is_number()) p.grainIntensity = j["grainIntensity"].get<float>();
            }
        };

        std::vector<BrushPreset> presets;
        int selectedPreset = -1;

        void init_default_presets_if_empty();
        void apply_brush_preset(DrawingProgram& drawP, size_t index);
        void save_current_brush_preset(DrawingProgram& drawP, const std::string& customName = "");
        void delete_brush_preset(size_t index);

        enum class RelativeWidthFailCode {
            SUCCESS,
            TOO_ZOOMED_IN,
            TOO_ZOOMED_OUT
        };

        float& get_stroke_size_relative_width_ref(DrawingProgramToolType toolType);
        const float& get_stroke_size_relative_width_ref(DrawingProgramToolType toolType) const;
        std::pair<std::optional<float>, RelativeWidthFailCode> get_relative_width_from_value(DrawingProgram& drawP, const WorldScalar& camInverseScale, float relativeWidth) const;
        std::pair<std::optional<float>, RelativeWidthFailCode> get_relative_width_stroke_size(DrawingProgram& drawP, const WorldScalar& camInverseScale) const;
        void print_relative_width_fail_message(RelativeWidthFailCode failCode);
        void relative_width_gui(DrawingProgram& drawP, const char* label);

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ToolConfiguration, brush, toolPanel, eraser, ellipseDraw, rectDraw, eyeDropper, lineDraw, screenshot, globalConf, presets, selectedPreset)
};
