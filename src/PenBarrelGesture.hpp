#pragma once

#include <cstdint>

// Capture the mapping at press time. Only the initiating button can finish it.
struct PenBarrelGesture {
    uint32_t owner = 0;
    uint8_t button = 0;
    bool paletteOnRelease = false;

    bool held() const { return button != 0; }
    bool owns(uint32_t id, uint8_t number) const { return held() && owner == id && button == number; }
    void touch(uint32_t id) { if (owner == id) paletteOnRelease = false; }
    void reset() { *this = {}; }
};
