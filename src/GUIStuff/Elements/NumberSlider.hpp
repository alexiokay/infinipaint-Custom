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
#include "Element.hpp"
#include "../GUIManager.hpp"
#include "../../TimePoint.hpp"
#include "../../UIControlGeometry.hpp"

namespace GUIStuff {

struct NumberSliderData {
    std::function<void()> onChange;
    std::function<void()> onHold;
    std::function<void()> onRelease;
};

template <typename T> class NumberSlider : public Element {
    public:
        NumberSlider(GUIManager& gui):
            Element(gui) {}

        static constexpr float HOLD_ANIMATION_TIME = 0.3f;

        void layout(const Clay_ElementId& id, T* data, T minData, T maxData, const NumberSliderData& config) {
            this->data = data;
            dd.val = *data;
            dd.minData = minData;
            dd.maxData = maxData;
            this->config = config;

            float height = gui.io.theme->controlHeight;
            CLAY(id, {
                .layout = {
                    .sizing = {.width = CLAY_SIZING_GROW(100), .height = CLAY_SIZING_FIXED(height)}
                },
                .custom = { .customData = this }
            }) {
            }
        }

        virtual void update() override {
            if(!dd.isHeld && data)
                dd.val = *data;
            smooth_two_way_animation_time(dd.holdAnimation, gui.io.deltaTime, dd.isHeld, HOLD_ANIMATION_TIME);
            smooth_two_way_animation_time(dd.hoverAnimation, gui.io.deltaTime, mouseHovering && !gui.last_interaction_is_touch(), gui.io.theme->hoverExpandTime);
            if(oldDD != dd) {
                gui.invalidate_draw_element(this, {
                    .top = 10.0f,
                    .bottom = 10.0f,
                    .left = 10.0f,
                    .right = 10.0f
                });
                oldDD = dd;
            }
        }

        virtual void clay_draw(SkCanvas* canvas, UpdateInputData& io, Clay_RenderCommand* command, bool skiaAA) override {
            auto& bb = boundingBox.value();

            canvas->save();
            canvas->translate(bb.min.x(), bb.min.y());

            float lerpTimeHover = dd.hoverAnimation / io.theme->hoverExpandTime;

            static BezierEasing easeHeight(0.68, -1.55, 0.265, 2.55);

            float lerpTimeHeld = easeHeight(dd.holdAnimation / HOLD_ANIMATION_TIME);

            float holderRadius = lerp_vec(4.0, 5.0, lerpTimeHover);
            float holderHeight = std::clamp<float>(lerp_vec(4.0, 10.0, lerpTimeHeld), 4.0f, std::max(4.0f, bb.height()*.5f-2.0f));

            const float yChange = bb.height() * 0.5f - holderRadius * 0.5f;

            const float fraction = dd.maxData != dd.minData ? lerp_time<float>(dd.val, dd.maxData, dd.minData) : 0;
            const float inset = UIControlGeometry::sliderInset(bb.width());
            float holderPos = UIControlGeometry::sliderPosition(bb.width(), fraction);

            SkRect barFull = SkRect::MakeXYWH(inset, yChange, std::max(0.0f,holderPos-inset), holderRadius);
            SkRect barEmpty = SkRect::MakeXYWH(holderPos, yChange, std::max(0.0f,bb.width()-inset-holderPos), holderRadius);

            SkPaint barFullP;
            barFullP.setAntiAlias(skiaAA);
            barFullP.setColor(convert_vec4<SkColor4f>(io.theme->fillColor1));
            canvas->drawRoundRect(barFull, 5.0f, 5.0f, barFullP);

            SkPaint barEmptyP;
            barEmptyP.setAntiAlias(skiaAA);
            barEmptyP.setColor(convert_vec4<SkColor4f>(io.theme->backColor2));
            canvas->drawRoundRect(barEmpty, 5.0f, 5.0f, barEmptyP);

            canvas->translate(holderPos, bb.height() * 0.5f);

            SkRect holderRect = SkRect::MakeLTRB(-holderRadius, -holderHeight, holderRadius, holderHeight);
            SkPaint holderBorderP;
            holderBorderP.setAntiAlias(skiaAA);
            holderBorderP.setStyle(SkPaint::kStroke_Style);
            holderBorderP.setStrokeWidth(3.0f);
            holderBorderP.setColor(convert_vec4<SkColor4f>(io.theme->fillColor1));
            canvas->drawRoundRect(holderRect, holderRadius, holderRadius, holderBorderP);

            SkPaint holderP;
            holderP.setAntiAlias(skiaAA);
            holderP.setColor(convert_vec4<SkColor4f>(io.theme->backColor2));
            canvas->drawRoundRect(holderRect, holderRadius, holderRadius, holderP);
            
            canvas->restore();
        }
        virtual void input_mouse_button_callback(const InputManager::MouseButtonCallbackArgs& button) override {
            if(button.deviceType == InputManager::MouseDeviceType::PEN) {
                if(button.button == InputManager::MouseButton::LEFT) {
                    if(button.down) {
                        penStartPos = button.pos;
                        if(data) penInitialVal = *data;
                        penScrollingAway = false;
                        penIsDragging = false;
                        penIsInteracting = mouseHovering;
                    } else {
                        if(penIsDragging) {
                            dd.isHeld = false;
                            penIsDragging = false;
                            penIsInteracting = false;
                            gui.set_post_callback_func([&] {
                                if(config.onRelease) config.onRelease();
                            });
                            gui.set_to_layout();
                        } else if(penIsInteracting && !penScrollingAway) {
                            if((button.pos - penStartPos).norm() <= 7.0f && boundingBox.has_value()) {
                                update_slider_pos(button.pos, true);
                                gui.set_post_callback_func([&] {
                                    if(config.onRelease) config.onRelease();
                                });
                            }
                            dd.isHeld = false;
                            penIsInteracting = false;
                            gui.set_to_layout();
                        }
                        penScrollingAway = false;
                    }
                }
                return;
            }

            bool oldIsHeld = dd.isHeld;
            dd.isHeld = mouseHovering && button.button == InputManager::MouseButton::LEFT && button.down;
            if(oldIsHeld && !dd.isHeld) {
                gui.set_post_callback_func([&] {
                    if(config.onRelease) config.onRelease();
                });
            }
            else if(dd.isHeld && boundingBox.has_value())
                update_slider_pos(button.pos, true);
        }

        virtual void input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) override {
            if(motion.deviceType == InputManager::MouseDeviceType::PEN) {
                if(!penIsInteracting || penScrollingAway || !boundingBox.has_value())
                    return;

                Vector2f diff = motion.pos - penStartPos;
                float dx = std::abs(diff.x());
                float dy = std::abs(diff.y());

                if(!penIsDragging) {
                    if(dy > 7.0f && dy > dx) {
                        penScrollingAway = true;
                        penIsInteracting = false;
                        dd.isHeld = false;
                        if(data) { *data = penInitialVal; dd.val = penInitialVal; }
                        gui.set_to_layout();
                        return;
                    }
                    if(dx > 5.0f && dx >= dy) {
                        penIsDragging = true;
                        dd.isHeld = true;
                        update_slider_pos(motion.pos, true);
                        gui.set_to_layout();
                        return;
                    }
                } else {
                    update_slider_pos(motion.pos, false);
                }
                return;
            }

            if(dd.isHeld && boundingBox.has_value())
                update_slider_pos(motion.pos, false);
        }

        virtual void input_finger_touch_callback(const InputManager::FingerTouchCallbackArgs& touch) override {
            if(touch.down) {
                touchStartPos = touch.pos;
                if(data) touchInitialVal = *data;
                touchScrollingAway = false;
                touchIsDragging = false;
                touchIsInteracting = mouseHovering;
            } else {
                if(touchIsDragging) {
                    dd.isHeld = false;
                    touchIsDragging = false;
                    touchIsInteracting = false;
                    gui.set_post_callback_func([&] {
                        if(config.onRelease) config.onRelease();
                    });
                    gui.set_to_layout();
                } else if(touchIsInteracting && !touchScrollingAway) {
                    if((touch.pos - touchStartPos).norm() <= 7.0f && boundingBox.has_value()) {
                        update_slider_pos(touch.pos, true);
                        gui.set_post_callback_func([&] {
                            if(config.onRelease) config.onRelease();
                        });
                    }
                    dd.isHeld = false;
                    touchIsInteracting = false;
                    gui.set_to_layout();
                }
                touchScrollingAway = false;
            }
        }

        virtual void input_finger_motion_callback(const InputManager::FingerMotionCallbackArgs& motion) override {
            if(!touchIsInteracting || touchScrollingAway || !boundingBox.has_value())
                return;

            Vector2f diff = motion.pos - touchStartPos;
            float dx = std::abs(diff.x());
            float dy = std::abs(diff.y());

            if(!touchIsDragging) {
                if(dy > 7.0f && dy > dx) {
                    touchScrollingAway = true;
                    touchIsInteracting = false;
                    dd.isHeld = false;
                    if(data) { *data = touchInitialVal; dd.val = touchInitialVal; }
                    gui.set_to_layout();
                    return;
                }
                if(dx > 5.0f && dx >= dy) {
                    touchIsDragging = true;
                    dd.isHeld = true;
                    update_slider_pos(motion.pos, true);
                    gui.set_to_layout();
                    return;
                }
            } else {
                update_slider_pos(motion.pos, false);
            }
        }

    private:
        Vector2f penStartPos{0.0f, 0.0f};
        T penInitialVal = 0;
        bool penIsInteracting = false;
        bool penIsDragging = false;
        bool penScrollingAway = false;

        Vector2f touchStartPos{0.0f, 0.0f};
        T touchInitialVal = 0;
        bool touchIsInteracting = false;
        bool touchIsDragging = false;
        bool touchScrollingAway = false;

        void update_slider_pos(const Vector2f& p, bool justHeld) {
            gui.set_post_callback_func([&, p, justHeld] {
                if(!data || !boundingBox.has_value()) return;
                float fracPosOnSlider = UIControlGeometry::sliderFraction(boundingBox.value().width(), p.x()-boundingBox.value().min.x());
                dd.val = *data = static_cast<T>(std::clamp<double>(std::lerp<double>(dd.minData, dd.maxData, fracPosOnSlider), dd.minData, dd.maxData));
                if(justHeld && config.onHold) config.onHold();
                if(config.onChange) config.onChange();
            });
        }

        T* data = nullptr;

        struct DisplayData {
            bool isHeld = false;
            T val = 0.0;

            T minData = 0.0;
            T maxData = 1.0;

            float hoverAnimation = 0.0;
            float holdAnimation = 0.0;

            bool operator!=(const DisplayData&) const = default;
            bool operator==(const DisplayData&) const = default;
        };

        DisplayData dd;
        DisplayData oldDD;

        NumberSliderData config;
};

}
