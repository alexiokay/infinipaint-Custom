#pragma once
#include <nlohmann/json.hpp>

namespace WorkspaceLock {
struct Preferences {
    bool blockCanvasEdits = true;
    bool blockTouchDrawing = true;
    bool lockPanelPosition = true;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Preferences, blockCanvasEdits, blockTouchDrawing, lockPanelPosition)
};
inline bool blocksEdits(bool active, const Preferences& p) { return active && p.blockCanvasEdits; }
inline bool blocksTouch(bool savedSetting, bool active, const Preferences& p) {
    return savedSetting || (active && p.blockTouchDrawing);
}
}
