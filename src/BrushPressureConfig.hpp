#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <nlohmann/json.hpp>

namespace BrushPressure {
enum class Response { Original, Preserve, Peak, Time };
enum class Engine { Compatibility, Samples };
enum class Rendering { Polyline, BoundedCurves };

struct Config {
    bool hasRoundCaps = true;
    float relativeWidth = 15.0f;
    Response pressureResponse = Response::Time;
    Engine engine = Engine::Compatibility;
    Rendering rendering = Rendering::Polyline;
    double pressureTimeMs = 40;
    bool flatOverlap = false;    // When true, overlapping strokes of identical color merge flat
    bool pipelineExplicit = true; // Runtime migration state; serialized by engine key.
    bool correctionIndependent = false; // Migrated once after both configs load.
    bool samplePath() const { return engine == Engine::Samples; }
    void migrateCorrection(bool& correction) {
        if (!pipelineExplicit && !correctionIndependent && pressureResponse == Response::Original) correction=false;
        correctionIndependent=true;
        if (!pipelineExplicit) {
            engine = correction || pressureResponse != Response::Original ? Engine::Samples : Engine::Compatibility;
            pipelineExplicit = true;
        }
    }
    friend void to_json(nlohmann::json& j, const Config& c) {
        const char* mode = c.pressureResponse == Response::Preserve ? "preserve" :
            c.pressureResponse == Response::Peak ? "peak" : c.pressureResponse == Response::Time ? "time" : "original";
        j = {{"hasRoundCaps", c.hasRoundCaps}, {"relativeWidth", c.relativeWidth},
            {"pressureResponse", mode}, {"correctionIndependent",c.correctionIndependent},
            {"engine", c.engine == Engine::Compatibility ? "original" : "samples"},
            {"rendering", c.rendering == Rendering::Polyline ? "polyline" : "bounded"},
            {"pressureTimeMs", c.pressureTimeMs},
            {"flatOverlap", c.flatOverlap},
            // Older fork builds understand this key, but not peak mode.
            {"preservePenPressure", c.pressureResponse == Response::Preserve}};
    }
    friend void from_json(const nlohmann::json& j, Config& c) {
        c = Config{};
        c.pressureResponse = Response::Original; // Old files keep their former width policy.
        c.pipelineExplicit = j.contains("engine");
        if (j.value("engine", nlohmann::json()) == "samples") c.engine = Engine::Samples;
        if (j.value("rendering", nlohmann::json()) == "bounded") c.rendering = Rendering::BoundedCurves;
        if (j.contains("pressureTimeMs") && j["pressureTimeMs"].is_number()) {
            const double ms = j["pressureTimeMs"].get<double>();
            if (std::isfinite(ms)) c.pressureTimeMs = std::clamp(ms, 0.0, 200.0);
        }
        if (j.contains("correctionIndependent") && j["correctionIndependent"].is_boolean())
            c.correctionIndependent=j["correctionIndependent"].get<bool>();
        if (j.contains("hasRoundCaps") && j["hasRoundCaps"].is_boolean())
            c.hasRoundCaps = j["hasRoundCaps"].get<bool>();
        if (j.contains("flatOverlap") && j["flatOverlap"].is_boolean())
            c.flatOverlap = j["flatOverlap"].get<bool>();
        if (j.contains("relativeWidth") && j["relativeWidth"].is_number()) {
            const float width = j["relativeWidth"].get<float>();
            if (std::isfinite(width) && width >= 0) c.relativeWidth = width;
        }
        if (j.contains("pressureResponse")) {
            if (j["pressureResponse"] == "preserve") c.pressureResponse = Response::Preserve;
            else if (j["pressureResponse"] == "peak") c.pressureResponse = Response::Peak;
            else if (j["pressureResponse"] == "time") c.pressureResponse = Response::Time;
        } else if (j.contains("preservePenPressure") && j["preservePenPressure"].is_boolean() &&
                   j["preservePenPressure"].get<bool>()) c.pressureResponse = Response::Preserve;
    }
};
}
