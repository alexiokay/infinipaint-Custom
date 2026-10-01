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

#include "LineDrawTool.hpp"
#include "../DrawingProgram.hpp"
#include "../../MainProgram.hpp"
#include "../../DrawData.hpp"
#include "DrawingProgramToolBase.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"
#include "../../CanvasComponents/MeshCanvasComponent.hpp"
#include "Helpers/MathExtras.hpp"
#include <numbers>
#include <cmath>
#include "../../PolygonGeometry.hpp"

#include "../../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/CheckBoxHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/ToolInspectorHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/LayoutHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/NumberSliderHelpers.hpp"
#include "../../GUIStuff/Elements/DropDown.hpp"
#include <include/core/SkPathBuilder.h>
#include <include/pathops/SkPathOps.h>

#include "../../LineGeometry.hpp"
using LineGeometry::generate_line_path;

LineDrawTool::LineDrawTool(DrawingProgram& initDrawP):
    DrawingProgramToolBase(initDrawP)
{}

DrawingProgramToolType LineDrawTool::get_type() {
    return DrawingProgramToolType::LINE;
}

void LineDrawTool::gui_inspector() {
    using namespace GUIStuff;
    using namespace ElementHelpers;
    auto& gui = drawP.world.main.g.gui;
    auto& toolConfig = drawP.world.main.toolConfig;

    if(toolConfig.lineDraw.lineStyle < 0 || toolConfig.lineDraw.lineStyle > 4)
        toolConfig.lineDraw.lineStyle = 0;
    if(toolConfig.lineDraw.arrowMode < 0 || toolConfig.lineDraw.arrowMode > 3)
        toolConfig.lineDraw.arrowMode = 0;

    gui.new_id("line draw tool", [&] {
        tool_inspector(gui, "Line", [&] {
            inspector_section(gui, "STYLE", [&] {
                left_to_right_line_layout(gui, [&]() {
                    text_label(gui, "Pattern:");
                    gui.element<DropDown<int>>("line pattern select", &toolConfig.lineDraw.lineStyle, std::vector<std::string>{
                        "Solid",
                        "Dashed",
                        "Dotted (Circles)",
                        "Dash-Dot",
                        "Dash-Dot-Dot"
                    }, DropdownOptions{
                        .onClick = [&] {
                            gui.set_to_layout();
                        }
                    });
                });
                left_to_right_line_layout(gui, [&]() {
                    text_label(gui, "Arrows:");
                    gui.element<DropDown<int>>("line arrow select", &toolConfig.lineDraw.arrowMode, std::vector<std::string>{
                        "None",
                        "End Arrow ->",
                        "Start Arrow <-",
                        "Both Ends <->"
                    }, DropdownOptions{
                        .onClick = [&] {
                            gui.set_to_layout();
                        }
                    });
                });
                toolConfig.relative_width_gui(drawP, "Stroke Width");
                checkbox_boolean_field(gui, "hasroundcaps", "Round Caps", &toolConfig.lineDraw.hasRoundCaps);
            });
            if(toolConfig.lineDraw.lineStyle != 0) {
                inspector_section(gui, "SPACING & DISTANCES", [&] {
                    if(toolConfig.lineDraw.lineStyle != 2) {
                        slider_scalar_field(gui, "dashlength", "Dash Length", &toolConfig.lineDraw.dashLength, 0.5f, 10.0f);
                    }
                    slider_scalar_field(gui, "dashgap", "Gap Distance", &toolConfig.lineDraw.dashGap, 0.5f, 10.0f);
                    inspector_hint(gui, "Distance multipliers relative to stroke width.");
                });
            }
            inspector_section(gui, "SNAPPING", [&] {
                checkbox_boolean_field(gui, "snapangles", "Angle Snapping (15 deg)", &toolConfig.lineDraw.snapAngles);
                inspector_hint(gui, "Hold Shift while drawing to snap to 15 deg increments.");
            });
        });
    });
}

void LineDrawTool::gui_toolbox(Toolbar&) {
    gui_inspector();
}

void LineDrawTool::gui_phone_toolbox(PhoneDrawingProgramScreen&) {
    gui_inspector();
}

void LineDrawTool::commit_data(bool final) {
    if(objInfoBeingEdited && brushPoints.size() >= 2) {
        NetworkingObjects::NetObjOwnerPtr<CanvasComponentContainer>& containerPtr = objInfoBeingEdited->obj;
        MeshCanvasComponent& newMesh = static_cast<MeshCanvasComponent&>(containerPtr->get_comp());
        newMesh.d.meshPath = generate_line_path(brushPoints.front().pos, brushPoints.back().pos, brushPoints.front().width, drawP.world.main.toolConfig.lineDraw);
        if(final) {
            containerPtr->get_comp().simplify_paths();
            containerPtr->normalize_object_coordinates();
        }
        containerPtr->commit_update(drawP);
        if(final) {
            drawP.world.send_reliable_multi_command_to_all([&]() {
                drawP.send_transforms_for({objInfoBeingEdited});
                containerPtr->send_comp_update(drawP, final);
            });
        }
        else
            containerPtr->send_comp_update(drawP, final);
    }
    commitUpdate = false;
}

void LineDrawTool::input_mouse_button_on_canvas_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if(button.button == InputManager::MouseButton::LEFT) {
        auto& toolConfig = drawP.world.main.toolConfig;
        if(button.down && drawP.layerMan.is_a_layer_being_edited() && !objInfoBeingEdited && !drawP.world.main.g.gui.cursor_obstructed()) {
            auto relativeWidthResult = drawP.world.main.toolConfig.get_relative_width_stroke_size(drawP, drawP.world.drawData.cam.c.inverseScale);
            if(!relativeWidthResult.first.has_value()) {
                drawP.world.main.toolConfig.print_relative_width_fail_message(relativeWidthResult.second);
                return;
            }
            float width = relativeWidthResult.first.value();

            brushPoints.clear();

            CanvasComponentContainer* newMeshContainer = new CanvasComponentContainer(drawP.world.netObjMan, CanvasComponentType::MESH);
            MeshCanvasComponent& newMesh = static_cast<MeshCanvasComponent&>(newMeshContainer->get_comp());

            newMesh.d.color = toolConfig.globalConf.foregroundColor;
            newMeshContainer->coords = drawP.world.drawData.cam.c;

            Vector2f startAt = newMeshContainer->coords.from_cam_space_to_this(drawP.world, button.pos);

            BrushComponentCode::BrushPoint p;
            p.pos = startAt;
            p.width = width;
            brushPoints.emplace_back(p);
            p.pos = ensure_points_have_distance(p.pos, p.pos, 1.0f);
            brushPoints.emplace_back(p);
            objInfoBeingEdited = drawP.layerMan.add_component_to_layer_being_edited(newMeshContainer);
            commit_data(false);
        }
        else if(!button.down && objInfoBeingEdited)
            commit();
    }
}

void LineDrawTool::input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) {
    if(objInfoBeingEdited) {
        constexpr float SNAP_DIVISION_COUNT = 12.0f;
        Vector2f newPos = objInfoBeingEdited->obj->coords.from_cam_space_to_this(drawP.world, motion.pos);
        Vector2f oldPos = brushPoints.front().pos;
        auto& config = drawP.world.main.toolConfig.lineDraw;
        bool shouldSnap = config.snapAngles || drawP.world.main.input.key(InputManager::KEY_GENERIC_LSHIFT).held;
        if(shouldSnap) {
            Vector2f diff = (newPos - oldPos);
            float diffLength = diff.norm();
            if(diffLength > 0.001f) {
                diff.normalize();
                float angle = (std::atan2(diff.y(), diff.x()) / std::numbers::pi_v<float>) * SNAP_DIVISION_COUNT;
                angle = std::round(angle);
                angle = (angle * std::numbers::pi_v<float>) / SNAP_DIVISION_COUNT;
                newPos = oldPos + diffLength * Vector2f{std::cos(angle), std::sin(angle)};
            }
        }
        brushPoints.back().pos = ensure_points_have_distance(oldPos, newPos, 1.0f);
        commitUpdate = true;
    }
}

void LineDrawTool::erase_component(CanvasComponentContainer::ObjInfo* erasedComp) {
    if(objInfoBeingEdited == erasedComp) {
        objInfoBeingEdited = nullptr;
        commitUpdate = false;
    }
}

void LineDrawTool::right_click_popup_gui(Toolbar& t, Vector2f popupPos) {
    t.paint_popup(popupPos);
}

void LineDrawTool::switch_tool(DrawingProgramToolType newTool) {
    commit();
}

void LineDrawTool::tool_update() {
    if(commitUpdate && objInfoBeingEdited)
        commit_data(false);
}

bool LineDrawTool::prevent_undo_or_redo() {
    return objInfoBeingEdited;
}

void LineDrawTool::commit() {
    if(objInfoBeingEdited) {
        NetworkingObjects::NetObjOwnerPtr<CanvasComponentContainer>& containerPtr = objInfoBeingEdited->obj;
        commit_data(true);
        if(containerPtr->get_world_bounds().has_value())
            drawP.layerMan.add_undo_place_component(objInfoBeingEdited);
        else {
            auto& components = containerPtr->parentLayer->get_layer().components;
            components->erase(components, containerPtr->objInfo);
        }
        objInfoBeingEdited = nullptr;
        brushPoints.clear();
    }
}

void LineDrawTool::draw(SkCanvas* canvas, const DrawData& drawData) {
}
