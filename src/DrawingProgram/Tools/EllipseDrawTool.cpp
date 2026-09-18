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

#include "EllipseDrawTool.hpp"
#include "../DrawingProgram.hpp"
#include "../../MainProgram.hpp"
#include "../../DrawData.hpp"
#include "DrawingProgramToolBase.hpp"
#include "Helpers/MathExtras.hpp"
#include "../../InputManager.hpp"
#include <cereal/types/vector.hpp>
#include "../../CanvasComponents/EllipseCanvasComponent.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"

#include "../../GUIStuff/ElementHelpers/RadioButtonHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/CheckBoxHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/ToolInspectorHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"

EllipseDrawTool::EllipseDrawTool(DrawingProgram& initDrawP):
    DrawingProgramToolBase(initDrawP)
{}

DrawingProgramToolType EllipseDrawTool::get_type() {
    return DrawingProgramToolType::ELLIPSE;
}

void EllipseDrawTool::gui_inspector() {
    using namespace GUIStuff;
    using namespace ElementHelpers;

    auto& gui = drawP.world.main.g.gui;
    auto& toolConfig = drawP.world.main.toolConfig;
    auto& fillStrokeMode = toolConfig.ellipseDraw.fillStrokeMode;
    gui.new_id("ellipse draw tool", [&] {
        tool_inspector(gui, "Ellipse / Circle", [&] {
            inspector_section(gui, "GEOMETRY", [&] {
                checkbox_boolean_field(gui, "perfect circle", "1:1 Circle (Constrain aspect ratio)", &toolConfig.ellipseDraw.perfectCircle);
                checkbox_boolean_field(gui, "from center", "Draw from center (Start at click)", &toolConfig.ellipseDraw.fromCenter);
                inspector_hint(gui, "Hold Shift for 1:1 circle, or Alt to draw from center.");
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

void EllipseDrawTool::gui_toolbox(Toolbar&) {
    gui_inspector();
}

void EllipseDrawTool::gui_phone_toolbox(PhoneDrawingProgramScreen&) {
    gui_inspector();
}

void EllipseDrawTool::input_mouse_button_on_canvas_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if(button.button == InputManager::MouseButton::LEFT) {
        if(button.down && drawP.layerMan.is_a_layer_being_edited() && !objInfoBeingEdited && !drawP.world.main.g.gui.cursor_obstructed()) {
            auto& toolConfig = drawP.world.main.toolConfig;

            auto relativeWidthResult = drawP.world.main.toolConfig.get_relative_width_stroke_size(drawP, drawP.world.drawData.cam.c.inverseScale);
            if(!relativeWidthResult.first.has_value()) {
                drawP.world.main.toolConfig.print_relative_width_fail_message(relativeWidthResult.second);
                return;
            }
            float width = relativeWidthResult.first.value();

            CanvasComponentContainer* newContainer = new CanvasComponentContainer(drawP.world.netObjMan, CanvasComponentType::ELLIPSE);
            EllipseCanvasComponent& newEllipse = static_cast<EllipseCanvasComponent&>(newContainer->get_comp());

            newContainer->coords = drawP.world.drawData.cam.c;
            startAt = newContainer->coords.from_cam_space_to_this(drawP.world, button.pos);
            newEllipse.d.strokeColor = toolConfig.globalConf.foregroundColor;
            newEllipse.d.fillColor =   toolConfig.globalConf.backgroundColor;
            newEllipse.d.strokeWidth = width;
            newEllipse.d.p1 = startAt;
            newEllipse.d.p2 = startAt;
            newEllipse.d.p2 = ensure_points_have_distance(newEllipse.d.p1, newEllipse.d.p2, MINIMUM_DISTANCE_BETWEEN_BOUNDS);
            newEllipse.d.fillStrokeMode = static_cast<uint8_t>(toolConfig.ellipseDraw.fillStrokeMode);

            objInfoBeingEdited = drawP.layerMan.add_component_to_layer_being_edited(newContainer);
        }
        else if(!button.down && objInfoBeingEdited)
            commit();
    }
}

void EllipseDrawTool::input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) {
    if(objInfoBeingEdited) {
        NetworkingObjects::NetObjOwnerPtr<CanvasComponentContainer>& containerPtr = objInfoBeingEdited->obj;
        Vector2f newPos = containerPtr->coords.from_cam_space_to_this(drawP.world, motion.pos);
        auto& config = drawP.world.main.toolConfig.ellipseDraw;
        bool constrain1to1 = config.perfectCircle || drawP.world.main.input.key(InputManager::KEY_GENERIC_LSHIFT).held;
        bool fromCenter = config.fromCenter || drawP.world.main.input.key(InputManager::KEY_GENERIC_LALT).held;

        EllipseCanvasComponent& ellipse = static_cast<EllipseCanvasComponent&>(containerPtr->get_comp());
        if(fromCenter) {
            Vector2f offset = newPos - startAt;
            float rx = std::fabs(offset.x());
            float ry = std::fabs(offset.y());
            if(constrain1to1) {
                float r = std::max(rx, ry);
                rx = ry = r;
            }
            ellipse.d.p1 = startAt - Vector2f(rx, ry);
            ellipse.d.p2 = startAt + Vector2f(rx, ry);
        } else {
            if(constrain1to1) {
                float dx = std::fabs(newPos.x() - startAt.x());
                float dy = std::fabs(newPos.y() - startAt.y());
                float side = std::max(dx, dy);
                float signX = (newPos.x() >= startAt.x()) ? 1.0f : -1.0f;
                float signY = (newPos.y() >= startAt.y()) ? 1.0f : -1.0f;
                newPos = startAt + Vector2f(signX * side, signY * side);
            }
            ellipse.d.p1 = cwise_vec_min(startAt, newPos);
            ellipse.d.p2 = cwise_vec_max(startAt, newPos);
        }
        ellipse.d.p2 = ensure_points_have_distance(ellipse.d.p1, ellipse.d.p2, MINIMUM_DISTANCE_BETWEEN_BOUNDS);
        commitUpdate = true;
    }
}

void EllipseDrawTool::erase_component(CanvasComponentContainer::ObjInfo* erasedComp) {
    if(objInfoBeingEdited == erasedComp) {
        objInfoBeingEdited = nullptr;
        commitUpdate = false;
    }
}

void EllipseDrawTool::right_click_popup_gui(Toolbar& t, Vector2f popupPos) {
    t.paint_popup(popupPos);
}

void EllipseDrawTool::switch_tool(DrawingProgramToolType newTool) {
    commit();
}

void EllipseDrawTool::tool_update() {
    if(commitUpdate && objInfoBeingEdited) {
        NetworkingObjects::NetObjOwnerPtr<CanvasComponentContainer>& containerPtr = objInfoBeingEdited->obj;
        containerPtr->commit_update(drawP);
        containerPtr->send_comp_update(drawP, false);
        commitUpdate = false;
    }
}

bool EllipseDrawTool::prevent_undo_or_redo() {
    return objInfoBeingEdited;
}

void EllipseDrawTool::commit() {
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

void EllipseDrawTool::draw(SkCanvas* canvas, const DrawData& drawData) {
}
