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

#include "LassoFillTool.hpp"
#include "../DrawingProgram.hpp"
#include "../../MainProgram.hpp"
#include "../../DrawData.hpp"
#include "DrawingProgramToolBase.hpp"
#include "Helpers/MathExtras.hpp"
#include "Helpers/SCollision.hpp"
#include "../../CoordSpaceHelper.hpp"
#include <ranges>
#include <include/core/SkPathBuilder.h>
#include <include/pathops/SkPathOps.h>
#include "../../GUIStuff/ElementHelpers/ToolInspectorHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "../../CanvasComponents/MeshCanvasComponent.hpp"
#include "../../Toolbar.hpp"

LassoFillTool::LassoFillTool(DrawingProgram& initDrawP):
    DrawingProgramToolBase(initDrawP)
{}

DrawingProgramToolType LassoFillTool::get_type() {
    return DrawingProgramToolType::FILL;
}

void LassoFillTool::gui_toolbox(Toolbar& t) {
    using namespace GUIStuff;
    using namespace ElementHelpers;
    auto& gui = drawP.world.main.g.gui;
    gui.new_id("lasso fill tool", [&] {
        tool_inspector(gui, "Fill Tool", [&] {
            inspector_section(gui, "COLOR", [&] {
                t.quick_colors();
            });
            inspector_hint(gui, "Draw a loop with pen or finger to fill the enclosed area flat with color.");
        });
    });
}

void LassoFillTool::gui_phone_toolbox(PhoneDrawingProgramScreen&) {
    using namespace GUIStuff;
    using namespace ElementHelpers;
    auto& gui = drawP.world.main.g.gui;
    gui.new_id("lasso fill tool", [&] {
        tool_inspector(gui, "Fill Tool", [&] {
            inspector_hint(gui, "Draw a loop with pen or finger to fill the enclosed area flat with color.");
        });
    });
}

void LassoFillTool::right_click_popup_gui(Toolbar& t, Vector2f popupPos) {
    t.paint_popup(popupPos);
}

void LassoFillTool::erase_component(CanvasComponentContainer::ObjInfo*) {
}

void LassoFillTool::tool_update() {
}

bool LassoFillTool::prevent_undo_or_redo() {
    return controls.isFilling;
}

Vector4f* LassoFillTool::color_picker_color(Vector4f*) {
    return &drawP.world.main.toolConfig.globalConf.foregroundColor;
}

void LassoFillTool::switch_tool(DrawingProgramToolType) {
    controls.isFilling = false;
    controls.points.clear();
}

void LassoFillTool::input_mouse_button_on_canvas_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if(button.button == InputManager::MouseButton::LEFT) {
        if(button.down && drawP.layerMan.is_a_layer_being_edited() && !controls.isFilling && !drawP.world.main.g.gui.cursor_obstructed()) {
            controls.isFilling = true;
            controls.coords = drawP.world.drawData.cam.c;
            controls.points.clear();
            Vector2f startPt = controls.coords.from_cam_space_to_this(drawP.world, button.pos);
            controls.points.emplace_back(startPt);
        } else if(!button.down && controls.isFilling) {
            if(controls.points.size() >= 3) {
                SkPathBuilder pathBuilder;
                pathBuilder.moveTo(controls.points[0].x(), controls.points[0].y());
                for(size_t i = 1; i < controls.points.size(); ++i) {
                    pathBuilder.lineTo(controls.points[i].x(), controls.points[i].y());
                }
                pathBuilder.close();
                SkPath fillPath = pathBuilder.detach();

                auto simplified = Simplify(fillPath);
                if(simplified.has_value() && !simplified->isEmpty()) {
                    fillPath = *simplified;
                }

                CanvasComponentContainer* newContainer = new CanvasComponentContainer(drawP.world.netObjMan, CanvasComponentType::MESH);
                MeshCanvasComponent& newMesh = static_cast<MeshCanvasComponent&>(newContainer->get_comp());

                newMesh.d.color = drawP.world.main.toolConfig.globalConf.foregroundColor;
                newMesh.d.meshPath = fillPath;
                newContainer->coords = controls.coords;
                newContainer->get_comp().simplify_paths();
                newContainer->normalize_object_coordinates();

                auto* objInfo = drawP.layerMan.add_component_to_layer_being_edited(newContainer);
                newContainer->commit_update(drawP);
                drawP.world.send_reliable_multi_command_to_all([&]() {
                    drawP.send_transforms_for({objInfo});
                    newContainer->send_comp_update(drawP, true);
                });
                drawP.layerMan.add_undo_place_component(objInfo);
            }
            controls.points.clear();
            controls.isFilling = false;
            drawP.world.main.g.gui.set_to_layout();
        }
    }
}

void LassoFillTool::input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) {
    if(controls.isFilling) {
        Vector2f pt = controls.coords.from_cam_space_to_this(drawP.world, motion.pos);
        if(vec_distance(controls.points.back(), pt) > 3.0f) {
            controls.points.emplace_back(pt);
        }
    }
}

void LassoFillTool::draw(SkCanvas* canvas, const DrawData& drawData) {
    if(controls.isFilling && controls.points.size() >= 2) {
        canvas->save();
        controls.coords.transform_sk_canvas(canvas, drawData);

        SkPathBuilder previewBuilder;
        previewBuilder.moveTo(controls.points[0].x(), controls.points[0].y());
        for(size_t i = 1; i < controls.points.size(); ++i) {
            previewBuilder.lineTo(controls.points[i].x(), controls.points[i].y());
        }
        previewBuilder.close();
        SkPath previewPath = previewBuilder.detach();

        const auto& col = drawP.world.main.toolConfig.globalConf.foregroundColor;
        SkPaint fillPaint;
        fillPaint.setColor4f(SkColor4f{col.x(), col.y(), col.z(), col.w() * 0.45f});
        fillPaint.setStyle(SkPaint::kFill_Style);
        fillPaint.setAntiAlias(drawData.skiaAA);
        canvas->drawPath(previewPath, fillPaint);

        SkPaint strokePaint;
        strokePaint.setColor4f(SkColor4f{col.x(), col.y(), col.z(), std::max(0.7f, col.w())});
        strokePaint.setStyle(SkPaint::kStroke_Style);
        strokePaint.setStrokeWidth(2.0f);
        strokePaint.setAntiAlias(drawData.skiaAA);
        canvas->drawPath(previewPath, strokePaint);

        canvas->restore();
    }
}
