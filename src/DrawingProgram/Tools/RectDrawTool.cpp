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

#include "RectDrawTool.hpp"
#include "../DrawingProgram.hpp"
#include "../../MainProgram.hpp"
#include "../../DrawData.hpp"
#include "DrawingProgramToolBase.hpp"
#include "../../CanvasComponents/RectangleCanvasComponent.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"

#include "../../GUIStuff/ElementHelpers/RadioButtonHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/CheckBoxHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/ToolInspectorHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/NumberSliderHelpers.hpp"
#include "../../GUIStuff/Elements/DropDown.hpp"

RectDrawTool::RectDrawTool(DrawingProgram& initDrawP):
    DrawingProgramToolBase(initDrawP)
{}

DrawingProgramToolType RectDrawTool::get_type() {
    return DrawingProgramToolType::RECTANGLE;
}

void RectDrawTool::gui_inspector() {
    using namespace GUIStuff;
    using namespace ElementHelpers;

    auto& gui = drawP.world.main.g.gui;
    auto& toolConfig = drawP.world.main.toolConfig;
    auto& fillStrokeMode = toolConfig.rectDraw.fillStrokeMode;
    auto& relativeRadiusWidth = toolConfig.rectDraw.relativeRadiusWidth;

    if(toolConfig.rectDraw.aspectRatioMode < 0 || toolConfig.rectDraw.aspectRatioMode > 6)
        toolConfig.rectDraw.aspectRatioMode = 0;
    if(toolConfig.rectDraw.perfectSquare && toolConfig.rectDraw.aspectRatioMode == 0)
        toolConfig.rectDraw.aspectRatioMode = 1;

    gui.new_id("rect draw tool", [&] {
        tool_inspector(gui, "Rectangle / Square", [&] {
            inspector_section(gui, "GEOMETRY", [&] {
                left_to_right_line_layout(gui, [&]() {
                    text_label(gui, "Ratio:");
                    gui.element<DropDown<int>>("aspect ratio select", &toolConfig.rectDraw.aspectRatioMode, std::vector<std::string>{
                        "Free",
                        "1:1 Square",
                        "16:9",
                        "9:16",
                        "4:3",
                        "3:2",
                        "Custom"
                    }, DropdownOptions{
                        .onClick = [&] {
                            toolConfig.rectDraw.perfectSquare = (toolConfig.rectDraw.aspectRatioMode == 1);
                            gui.set_to_layout();
                        }
                    });
                });
                if(toolConfig.rectDraw.aspectRatioMode == 6) {
                    slider_scalar_field(gui, "custom_aspect_x", "Ratio X", &toolConfig.rectDraw.customAspectX, 0.1f, 100.0f);
                    slider_scalar_field(gui, "custom_aspect_y", "Ratio Y", &toolConfig.rectDraw.customAspectY, 0.1f, 100.0f);
                }
                checkbox_boolean_field(gui, "from center", "Draw from center (Start at click)", &toolConfig.rectDraw.fromCenter);
                slider_scalar_field(gui, "relradiuswidth", "Corner Radius", &relativeRadiusWidth, 0.0f, 40.0f);
                inspector_hint(gui, "Hold Shift for 1:1 square, or Alt to draw from center.");
            });
            inspector_section(gui, "STYLE", [&] {
                radio_button_selector(gui, "fill type", &fillStrokeMode, {
                    {"Fill only", 0},
                    {"Outline only", 1},
                    {"Fill and Outline", 2}
                });
            });
            if(fillStrokeMode == 1 || fillStrokeMode == 2) {
                inspector_section(gui, "OUTLINE", [&] {
                    toolConfig.relative_width_gui(drawP, "Outline Size");
                });
            }
        });
    });
}

void RectDrawTool::gui_toolbox(Toolbar&) {
    gui_inspector();
}

void RectDrawTool::gui_phone_toolbox(PhoneDrawingProgramScreen&) {
    gui_inspector();
}

void RectDrawTool::input_mouse_button_on_canvas_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if(button.button == InputManager::MouseButton::LEFT) {
        if(button.down && drawP.layerMan.is_a_layer_being_edited() && !objInfoBeingEdited && !drawP.world.main.g.gui.cursor_obstructed()) {
            auto& toolConfig = drawP.world.main.toolConfig;
            auto& fillStrokeMode = toolConfig.rectDraw.fillStrokeMode;
            auto& relativeRadiusWidth = toolConfig.rectDraw.relativeRadiusWidth;

            auto relativeWidthResult = drawP.world.main.toolConfig.get_relative_width_stroke_size(drawP, drawP.world.drawData.cam.c.inverseScale);
            auto relativeRadiusWidthResult = drawP.world.main.toolConfig.get_relative_width_from_value(drawP, drawP.world.drawData.cam.c.inverseScale, relativeRadiusWidth);
            if(!relativeWidthResult.first.has_value() || !relativeRadiusWidthResult.first.has_value()) {
                drawP.world.main.toolConfig.print_relative_width_fail_message(relativeWidthResult.second);
                return;
            }
            float width = relativeWidthResult.first.value();
            float radiusWidth = relativeRadiusWidthResult.first.value();

            CanvasComponentContainer* newContainer = new CanvasComponentContainer(drawP.world.netObjMan, CanvasComponentType::RECTANGLE);
            RectangleCanvasComponent& newRectangle = static_cast<RectangleCanvasComponent&>(newContainer->get_comp());

            newContainer->coords = drawP.world.drawData.cam.c;
            startAt = newContainer->coords.from_cam_space_to_this(drawP.world, button.pos);
            newRectangle.d.strokeColor = toolConfig.globalConf.foregroundColor;
            newRectangle.d.fillColor = toolConfig.globalConf.backgroundColor;
            newRectangle.d.cornerRadius = radiusWidth;
            newRectangle.d.strokeWidth = width;
            newRectangle.d.p1 = startAt;
            newRectangle.d.p2 = startAt;
            newRectangle.d.p2 = ensure_points_have_distance(newRectangle.d.p1, newRectangle.d.p2, MINIMUM_DISTANCE_BETWEEN_BOUNDS);
            newRectangle.d.fillStrokeMode = static_cast<uint8_t>(fillStrokeMode);
            objInfoBeingEdited = drawP.layerMan.add_component_to_layer_being_edited(newContainer);
        }
        else if(!button.down && objInfoBeingEdited)
            commit();
    }
}

void RectDrawTool::input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) {
    if(objInfoBeingEdited) {
        NetworkingObjects::NetObjOwnerPtr<CanvasComponentContainer>& containerPtr = objInfoBeingEdited->obj;
        Vector2f newPos = containerPtr->coords.from_cam_space_to_this(drawP.world, motion.pos);
        auto& config = drawP.world.main.toolConfig.rectDraw;
        bool shiftHeld = drawP.world.main.input.key(InputManager::KEY_GENERIC_LSHIFT).held;
        bool fromCenter = config.fromCenter || drawP.world.main.input.key(InputManager::KEY_GENERIC_LALT).held;

        float targetRatio = 0.0f;
        if(shiftHeld || config.perfectSquare || config.aspectRatioMode == 1) {
            targetRatio = 1.0f;
        } else if(config.aspectRatioMode == 2) {
            targetRatio = 16.0f / 9.0f;
        } else if(config.aspectRatioMode == 3) {
            targetRatio = 9.0f / 16.0f;
        } else if(config.aspectRatioMode == 4) {
            targetRatio = 4.0f / 3.0f;
        } else if(config.aspectRatioMode == 5) {
            targetRatio = 3.0f / 2.0f;
        } else if(config.aspectRatioMode == 6) {
            float cx = std::max(config.customAspectX, 0.001f);
            float cy = std::max(config.customAspectY, 0.001f);
            targetRatio = cx / cy;
        }

        RectangleCanvasComponent& rectangle = static_cast<RectangleCanvasComponent&>(containerPtr->get_comp());
        if(fromCenter) {
            Vector2f offset = newPos - startAt;
            float rx = std::fabs(offset.x());
            float ry = std::fabs(offset.y());
            if(targetRatio > 0.0f) {
                if(rx / targetRatio > ry)
                    ry = rx / targetRatio;
                else
                    rx = ry * targetRatio;
            }
            rectangle.d.p1 = startAt - Vector2f(rx, ry);
            rectangle.d.p2 = startAt + Vector2f(rx, ry);
        } else {
            float dx = std::fabs(newPos.x() - startAt.x());
            float dy = std::fabs(newPos.y() - startAt.y());
            if(targetRatio > 0.0f) {
                if(dx / targetRatio > dy)
                    dy = dx / targetRatio;
                else
                    dx = dy * targetRatio;
                float signX = (newPos.x() >= startAt.x()) ? 1.0f : -1.0f;
                float signY = (newPos.y() >= startAt.y()) ? 1.0f : -1.0f;
                newPos = startAt + Vector2f(signX * dx, signY * dy);
            }
            rectangle.d.p1 = cwise_vec_min(startAt, newPos);
            rectangle.d.p2 = cwise_vec_max(startAt, newPos);
        }
        rectangle.d.p2 = ensure_points_have_distance(rectangle.d.p1, rectangle.d.p2, MINIMUM_DISTANCE_BETWEEN_BOUNDS);
        commitUpdate = true;
    }
}

void RectDrawTool::erase_component(CanvasComponentContainer::ObjInfo* erasedComp) {
    if(objInfoBeingEdited == erasedComp) {
        objInfoBeingEdited = nullptr;
        commitUpdate = false;
    }
}

void RectDrawTool::right_click_popup_gui(Toolbar& t, Vector2f popupPos) {
    t.paint_popup(popupPos);
}

void RectDrawTool::switch_tool(DrawingProgramToolType newTool) {
    commit();
}

void RectDrawTool::tool_update() {
    if(commitUpdate && objInfoBeingEdited) {
        NetworkingObjects::NetObjOwnerPtr<CanvasComponentContainer>& containerPtr = objInfoBeingEdited->obj;
        containerPtr->commit_update(drawP);
        containerPtr->send_comp_update(drawP, false);
        commitUpdate = false;
    }
}

bool RectDrawTool::prevent_undo_or_redo() {
    return objInfoBeingEdited;
}

void RectDrawTool::commit() {
    if(objInfoBeingEdited) {
        NetworkingObjects::NetObjOwnerPtr<CanvasComponentContainer>& containerPtr = objInfoBeingEdited->obj;
        containerPtr->commit_update(drawP);
        containerPtr->send_comp_update(drawP, true);
        commitUpdate = false;
        if(containerPtr->get_world_bounds().has_value())
            drawP.layerMan.add_undo_place_component(objInfoBeingEdited);
        else {
            auto& components = containerPtr->parentLayer->get_layer().components;
            components->erase(components, containerPtr->objInfo);
        }
        objInfoBeingEdited = nullptr;
    }
}

void RectDrawTool::draw(SkCanvas* canvas, const DrawData& drawData) {
}
