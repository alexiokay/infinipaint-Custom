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

#include "RadioButton.hpp"
#include "../GUIManager.hpp"
#include "../../TimePoint.hpp"

namespace GUIStuff {

RadioButton::RadioButton(GUIManager& gui):
    Element(gui) {}

void RadioButton::layout(const Clay_ElementId& id, const std::function<bool()>& isTicked, const std::function<void()>& onClick, std::string_view label) {
    this->onClick = onClick;
    this->isTicked = isTicked;

    const float size = gui.io.theme->controlHeight;
    CLAY(id, {
        .layout = {
            .sizing = {.width = label.empty() ? CLAY_SIZING_FIXED(size) : CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(size)},
            .padding = {.left = label.empty() ? uint16_t(0) : static_cast<uint16_t>(size + 4), .right = 4, .top = 4, .bottom = 4},
            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}
        },
        .custom = { .customData = this }
    }) {
        if (!label.empty())
            CLAY_TEXT(gui.strArena.std_str_to_clay_str(label), CLAY_TEXT_CONFIG({
                .textColor = convert_vec4<Clay_Color>(gui.io.theme->frontColor1), .fontSize = gui.io.fontSize}));
    }
}

void RadioButton::update() {
    if(smooth_two_way_animation_time_check_for_change(hoverAnimation, gui.io.deltaTime, is_hovering_animation(), RADIOBUTTON_ANIMATION_TIME))
        gui.invalidate_draw_element(this);
    if(oldIsTicked != isTicked()) {
        gui.invalidate_draw_element(this);
        oldIsTicked = isTicked();
    }
}

bool RadioButton::is_hovering_animation() {
    return mouseHovering && (!gui.last_interaction_is_touch() || isHeld);
}

void RadioButton::input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if(button.deviceType == InputManager::MouseDeviceType::PEN) {
        if(button.button == InputManager::MouseButton::LEFT) {
            if(button.down) {
                if(mouseHovering) {
                    isHeld = true;
                    penStartPos = button.pos;
                    hasMovedPen = false;
                    gui.invalidate_draw_element(this);
                }
            } else {
                if(mouseHovering && isHeld && !hasMovedPen) {
                    gui.set_post_callback_func([&] { if(onClick) onClick(); });
                }
                isHeld = false;
                hasMovedPen = false;
                gui.invalidate_draw_element(this);
            }
        }
    } else {
        if(mouseHovering && button.button == InputManager::MouseButton::LEFT && button.down) {
            gui.set_post_callback_func([&] { if(onClick) onClick(); });
            isHeld = true;
        }
        else
            isHeld = false;
    }
}

void RadioButton::input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) {
    if(motion.deviceType == InputManager::MouseDeviceType::PEN && isHeld) {
        if((motion.pos - penStartPos).norm() > 8.0f) {
            hasMovedPen = true;
            isHeld = false;
            gui.invalidate_draw_element(this);
        }
    }
}

void RadioButton::input_finger_touch_callback(const InputManager::FingerTouchCallbackArgs& touch) {
    if(touch.down) {
        if(mouseHovering) {
            isHeld = true;
            touchStartPos = touch.pos;
            hasMovedTouch = false;
            gui.invalidate_draw_element(this);
        }
    } else {
        if(mouseHovering && isHeld && !hasMovedTouch) {
            gui.set_post_callback_func([&] { if(onClick) onClick(); });
        }
        isHeld = false;
        hasMovedTouch = false;
        gui.invalidate_draw_element(this);
    }
}

void RadioButton::input_finger_motion_callback(const InputManager::FingerMotionCallbackArgs& motion) {
    if(isHeld) {
        if((motion.pos - touchStartPos).norm() > 8.0f) {
            hasMovedTouch = true;
            isHeld = false;
            gui.invalidate_draw_element(this);
        }
    }
}

void RadioButton::clay_draw(SkCanvas* canvas, UpdateInputData& io, Clay_RenderCommand* command, bool skiaAA) {
    auto& bb = boundingBox.value();

    const float size = io.theme->controlHeight;
    canvas->save();
    canvas->translate(bb.min.x() + size * 0.5f, bb.min.y() + bb.height() * 0.5f);
    canvas->scale(size, size);

    SkPaint p;
    p.setAntiAlias(skiaAA);
    p.setColor4f(isTicked() || is_hovering_animation() ? io.theme->fillColor1 : io.theme->fillColor2);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(0.06f);
    canvas->drawCircle(0, 0, 0.33f, p);
    if (isTicked()) {
        p.setStyle(SkPaint::kFill_Style);
        canvas->drawCircle(0, 0, 0.17f, p);
    }
    canvas->restore();
}

}
