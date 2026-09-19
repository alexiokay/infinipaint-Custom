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

#include "BrushTool.hpp"
#include "CanvasToolCursor.hpp"
#include "../../GUIStuff/ElementHelpers/ToolInspectorHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/NumberSliderHelpers.hpp"
#include <Helpers/ConvertVec.hpp>
#include "../../GUIStuff/GUIManager.hpp"
#include "../DrawingProgram.hpp"
#include "../../MainProgram.hpp"
#include "DrawingProgramToolBase.hpp"
#include "../../CanvasComponents/MeshCanvasComponent.hpp"
#include "Helpers/Networking/NetLibrary.hpp"
#include "Helpers/NetworkingObjects/NetObjTemporaryPtr.decl.hpp"
#include "../../CanvasComponents/CanvasComponentContainer.hpp"
#include "../../GUIStuff/ElementHelpers/TextLabelHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/CheckBoxHelpers.hpp"
#include "../../GUIStuff/ElementHelpers/RadioButtonHelpers.hpp"
#include <include/pathops/SkPathOps.h>
#include "../EditCanvasComponentWorldUndoAction.hpp"

BrushTool::BrushTool(DrawingProgram& initDrawP):
    DrawingProgramToolBase(initDrawP)
{}

DrawingProgramToolType BrushTool::get_type() {
    return DrawingProgramToolType::BRUSH;
}

void BrushTool::switch_tool(DrawingProgramToolType newTool) {
    commit_stroke();
    canMergeWithPrevious = false;
}

void BrushTool::erase_component(CanvasComponentContainer::ObjInfo* erasedComp) {
    if(objInfoBeingEdited == erasedComp) {
        objInfoBeingEdited = nullptr;
        commitUpdate = false;
    }
    canMergeWithPrevious = false;
}

void BrushTool::input_mouse_button_on_canvas_callback(const InputManager::MouseButtonCallbackArgs& button) {
    if(button.button == InputManager::MouseButton::LEFT) {
        auto& toolConfig = drawP.world.main.toolConfig;
        if(button.down && drawP.layerMan.is_a_layer_being_edited() && !objInfoBeingEdited && !drawP.world.main.g.gui.cursor_obstructed()) {
            auto relativeWidthResult = drawP.world.main.toolConfig.get_relative_width_stroke_size(drawP, drawP.world.drawData.cam.c.inverseScale);
            if(!relativeWidthResult.first.has_value()) {
                drawP.world.main.toolConfig.print_relative_width_fail_message(relativeWidthResult.second);
                return;
            }

            CanvasComponentContainer* newMeshContainer = new CanvasComponentContainer(drawP.world.netObjMan, CanvasComponentType::MESH);
            MeshCanvasComponent& newMesh = static_cast<MeshCanvasComponent&>(newMeshContainer->get_comp());

            newMesh.d.color = toolConfig.globalConf.foregroundColor;
            newMeshContainer->coords = drawP.world.drawData.cam.c;

            // Capture the brush policy at contact-down; never switch a live stroke's path.
            BrushComponentCode::mouse_button(drawP, genData, newMeshContainer->coords, button, relativeWidthResult.first.value(), toolConfig.brush.samplePath(), toolConfig.brush.pressureResponse == BrushPressure::Response::Peak, toolConfig.brush.pressureResponse == BrushPressure::Response::Original);

            objInfoBeingEdited = drawP.layerMan.add_component_to_layer_being_edited(newMeshContainer);
            commit_data(false);
        }
        else if(!button.down && objInfoBeingEdited && button.deviceType == genData.deviceType &&
            (button.deviceType != InputManager::MouseDeviceType::PEN || button.penId == genData.penId)) {
            BrushComponentCode::finish_pen(drawP, genData, button);
            commit_stroke();
        }
    }
}

void BrushTool::commit_data(bool final) {
    if(objInfoBeingEdited) {
        NetworkingObjects::NetObjOwnerPtr<CanvasComponentContainer>& containerPtr = objInfoBeingEdited->obj;
        MeshCanvasComponent& newMesh = static_cast<MeshCanvasComponent&>(containerPtr->get_comp());
        newMesh.d.meshPath = BrushComponentCode::brush_stroke_to_skpath(genData.brushPoints, drawP.world.main.toolConfig.brush.hasRoundCaps, genData.penPath, genData.boundedCurves, genData.penDisplayScale);
        if(final) {
            containerPtr->get_comp().simplify_paths();
            containerPtr->normalize_object_coordinates();
        }
        containerPtr->commit_update(drawP);
        if(final) {
            drawP.world.send_reliable_multi_command_to_all([&]() {
                drawP.send_transforms_for({objInfoBeingEdited});
                containerPtr->send_comp_update(drawP, true);
            });
        }
        else
            containerPtr->send_comp_update(drawP, final);
    }
    commitUpdate = false;
}

void BrushTool::input_mouse_motion_callback(const InputManager::MouseMotionCallbackArgs& motion) {
    if (objInfoBeingEdited && BrushComponentCode::pen_mapping_changed(drawP, genData)) commit_stroke();
    if(objInfoBeingEdited && motion.deviceType == genData.deviceType &&
        (motion.deviceType != InputManager::MouseDeviceType::PEN || (motion.penContact && motion.penId == genData.penId))) {
        auto& toolConfig = drawP.world.main.toolConfig;
        NetworkingObjects::NetObjOwnerPtr<CanvasComponentContainer>& containerPtr = objInfoBeingEdited->obj;
        BrushComponentCode::mouse_motion(drawP, genData, motion.pos, toolConfig.get_relative_width_stroke_size(drawP, containerPtr->coords.inverseScale).first.value(), motion.timestamp);
        commitUpdate = true;
    }
}

void BrushTool::input_pen_axis_callback(const InputManager::PenAxisCallbackArgs& axis) {
    if (genData.penPath) return; // Width travels with each contact motion sample.
    if(axis.axis == SDL_PEN_AXIS_PRESSURE && drawP.world.main.conf.tabletOptions.pressureAffectsBrushWidth) {
        genData.penWidth = axis.value;
        if(genData.penWidth != 0.0f && objInfoBeingEdited) {
            auto& toolConfig = drawP.world.main.toolConfig;
            NetworkingObjects::NetObjOwnerPtr<CanvasComponentContainer>& containerPtr = objInfoBeingEdited->obj;
            float width = toolConfig.get_relative_width_stroke_size(drawP, containerPtr->coords.inverseScale).first.value();
            BrushComponentCode::pen_pressure(drawP, genData, width);
            commitUpdate = true;
        }
    }
}

void BrushTool::tool_update() {
    if (objInfoBeingEdited && BrushComponentCode::pen_mapping_changed(drawP, genData)) commit_stroke();
    if(!drawP.world.main.g.gui.cursor_obstructed())
        drawP.world.main.input.hideCursor = true;

    if(commitUpdate && objInfoBeingEdited)
        commit_data(false);
}

void BrushTool::commit_stroke() {
    if(objInfoBeingEdited) {
        NetworkingObjects::NetObjOwnerPtr<CanvasComponentContainer>& containerPtr = objInfoBeingEdited->obj;
        if (!genData.penPath) BrushComponentCode::fix_tip(genData.brushPoints);

        bool merged = false;
        if (drawP.world.main.toolConfig.brush.flatOverlap && canMergeWithPrevious) {
            MeshCanvasComponent& newMesh = static_cast<MeshCanvasComponent&>(containerPtr->get_comp());
            newMesh.d.meshPath = BrushComponentCode::brush_stroke_to_skpath(
                genData.brushPoints,
                drawP.world.main.toolConfig.brush.hasRoundCaps,
                genData.penPath,
                genData.boundedCurves,
                genData.penDisplayScale
            );
            containerPtr->get_comp().simplify_paths();

            auto& components = containerPtr->parentLayer->get_layer().components;
            if (containerPtr->objInfo != components->begin()) {
                auto prevIt = std::prev(containerPtr->objInfo);
                CanvasComponentContainer* prevContainer = prevIt->obj.get();
                if (prevContainer && prevContainer->get_comp().get_type() == CanvasComponentType::MESH) {
                    MeshCanvasComponent& prevMesh = static_cast<MeshCanvasComponent&>(prevContainer->get_comp());
                    if (prevMesh.d.color.x() == newMesh.d.color.x() &&
                        prevMesh.d.color.y() == newMesh.d.color.y() &&
                        prevMesh.d.color.z() == newMesh.d.color.z() &&
                        prevMesh.d.color.w() == newMesh.d.color.w()) {
                        
                        auto drawTransform = CanvasComponentContainer::calculate_draw_transform(containerPtr->coords, prevContainer->coords);
                        SkMatrix m = SkMatrix::I();
                        m.postScale(1.0 / drawTransform.scale, 1.0 / drawTransform.scale)
                         .postRotate(-drawTransform.rotation)
                         .postTranslate(-drawTransform.translation.x(), -drawTransform.translation.y());
                        
                        std::optional<SkPath> currInPrevCoords = newMesh.d.meshPath.tryMakeTransform(m);
                        if (currInPrevCoords.has_value()) {
                            SkRect prevBounds = prevMesh.d.meshPath.getBounds();
                            SkRect currBounds = currInPrevCoords.value().getBounds();
                            prevBounds.outset(2.0f, 2.0f);
                            if (prevBounds.intersects(currBounds)) {
                                std::optional<SkPath> unionResult = Op(prevMesh.d.meshPath, currInPrevCoords.value(), SkPathOp::kUnion_SkPathOp);
                                if (unionResult.has_value() && !unionResult.value().isEmpty()) {
                                    drawP.world.undo.push(std::make_unique<EditTransformCanvasComponentWorldUndoAction>(
                                        prevContainer->get_comp().get_data_copy(),
                                        prevContainer->coords,
                                        drawP.world.undo.get_undoid_from_netid(prevIt->obj.get_net_id())
                                    ));
                                    prevMesh.d.meshPath = unionResult.value();
                                    prevMesh.simplify_paths();
                                    prevContainer->normalize_object_coordinates();
                                    prevContainer->commit_update(drawP);
                                    drawP.world.send_reliable_multi_command_to_all([&]() {
                                        drawP.send_transforms_for({&(*prevIt)});
                                        prevContainer->send_comp_update(drawP, true);
                                    });
                                    components->erase(components, containerPtr->objInfo);
                                    objInfoBeingEdited = nullptr;
                                    commitUpdate = false;
                                    merged = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (!merged) {
            commit_data(true);
            if(containerPtr->get_world_bounds().has_value()) {
                drawP.layerMan.add_undo_place_component(objInfoBeingEdited);
                canMergeWithPrevious = true;
            }
            else {
                auto& components = containerPtr->parentLayer->get_layer().components;
                components->erase(components, containerPtr->objInfo);
                canMergeWithPrevious = false;
            }
            objInfoBeingEdited = nullptr;
        } else {
            canMergeWithPrevious = true;
        }
    }
}

void BrushTool::gui_toolbox(Toolbar&) {
    gui_inspector();
}

void BrushTool::gui_phone_toolbox(PhoneDrawingProgramScreen&) {
    gui_inspector();
}

void BrushTool::gui_inspector() {
    using namespace GUIStuff;
    using namespace ElementHelpers;
    auto& main = drawP.world.main;
    auto& gui = main.g.gui;
    gui.new_id("brush tool", [&] {
        tool_inspector(gui, "Brush", [&] {
            inspector_section(gui, "SIZE", [&] {
                main.toolConfig.relative_width_gui(drawP, "Size");
            });
            inspector_section(gui, "STROKE & DYNAMICS", [&] {
                checkbox_boolean_field(gui, "hasroundcaps", "Round Caps", &drawP.world.main.toolConfig.brush.hasRoundCaps);
                checkbox_boolean_field(gui, "pressure width", "Pressure affects size", &main.conf.tabletOptions.pressureAffectsBrushWidth);
                checkbox_boolean_field(gui, "local correction", "Smooth wobble (Stabilizer)", &main.conf.tabletOptions.penFilter.enabled, [&] {
                    if (main.conf.tabletOptions.penFilter.enabled) {
                        main.toolConfig.brush.engine = BrushPressure::Engine::Samples;
                    }
                });
                checkbox_boolean_field(gui, "flat overlap", "Flat coloring (No overlap stripes)", &drawP.world.main.toolConfig.brush.flatOverlap);
                inspector_hint(gui, "Merges overlapping strokes of the same color into a single seamless shape.");
            });
            text_button(gui, "advanced", advancedSettingsOpen ? "Less" : "Advanced", {
                .drawType = SelectableButton::DrawType::TRANSPARENT_BORDER,
                .isSelected = advancedSettingsOpen, .wide = true,
                .onClick = [this] {
                    advancedSettingsOpen = !advancedSettingsOpen;
                    drawP.world.main.g.gui.set_to_layout();
                }
            });
            if (advancedSettingsOpen) {
                inspector_section(gui, "PEN ENGINE", [&] {
                    using Engine = BrushPressure::Engine;
                    radio_button_selector<Engine>(gui, "pen engine", &main.toolConfig.brush.engine, {
                        {"Original compatibility (default)", Engine::Compatibility},
                        {"Sample pipeline (experimental)", Engine::Samples}
                    });
                });
                if (main.toolConfig.brush.samplePath()) {
                    inspector_section(gui, "PRESSURE RESPONSE", [&] {
                        using Response = BrushPressure::Response;
                        radio_button_selector<Response>(gui, "pressure mode", &main.toolConfig.brush.pressureResponse, {
                            {"Preserve samples", Response::Preserve},
                            {"Time-based width smoothing", Response::Time},
                            {"Uniform peak width", Response::Peak},
                            {"Legacy per-report propagation", Response::Original}
                        });
                        if (main.toolConfig.brush.pressureResponse == Response::Time)
                            slider_scalar_field(gui, "pressure time", "Width decay (ms)", &main.toolConfig.brush.pressureTimeMs, 0.0, 200.0, {.decimalPrecision = 1});
                        inspector_hint(gui, "Pressure changes width only, not correction or curve rendering.");
                    });
                } else {
                    inspector_hint(gui, "Original spacing, curves and pressure behavior. Sample options are inactive.");
                    if (main.conf.tabletOptions.penFilter.enabled)
                        inspector_hint(gui, "Saved correction is ON but inactive in Original compatibility.");
                }
                inspector_section(gui, "PEN RESPONSE", [&] {
                    if (main.conf.tabletOptions.pressureAffectsBrushWidth)
                        slider_scalar_field(gui, "minimum width", "Minimum width", &main.conf.tabletOptions.brushMinimumSize, 0.0f, 1.0f, {.decimalPrecision = 3});
                    if (!main.toolConfig.brush.samplePath() || main.toolConfig.brush.pressureResponse == BrushPressure::Response::Original) {
                        slider_scalar_field(gui, "width propagation", "Width propagation", &main.conf.tabletOptions.brushPressureSmoothingFactor, 0.0f, 1.0f, {.decimalPrecision = 3});
                        inspector_hint(gui, "0.707 default; 1 spreads peak width. Also used by eraser.");
                    } else {
                        inspector_hint(gui, "Original width propagation is not used by this pen mode.");
                    }
                });
                if (main.toolConfig.brush.samplePath()) {
                    inspector_section(gui, "CURVE GEOMETRY", [&] {
                        using Rendering = BrushPressure::Rendering;
                        radio_button_selector<Rendering>(gui, "curve rendering", &main.toolConfig.brush.rendering, {
                            {"Sample polyline", Rendering::Polyline},
                            {"Bounded curves (experimental)", Rendering::BoundedCurves}
                        });
                        inspector_hint(gui, "Curves add up to 0.25 DIP from sample chords; not original interpolation.");
                    });
                }
                if (main.toolConfig.brush.samplePath() && main.conf.tabletOptions.penFilter.enabled) {
                    inspector_section(gui, "STABILIZER TUNING", [&] {
                        slider_scalar_field(gui, "radius", "Radius (DIP)", &main.conf.tabletOptions.penFilter.radius, 4.0, 20.0, {.decimalPrecision = 1});
                        slider_scalar_field(gui, "window", "Revision (seconds)", &main.conf.tabletOptions.penFilter.window, 0.040, 0.200, {.decimalPrecision = 3});
                        slider_scalar_field(gui, "cap", "Max correction (DIP)", &main.conf.tabletOptions.penFilter.cap, 0.0, 6.0, {.decimalPrecision = 1});
                        inspector_hint(gui, "Larger windows can soften detail. The recent path can revise.");
                    });
                }
            }
        });
    });
}

void BrushTool::right_click_popup_gui(Toolbar& t, Vector2f popupPos) {
    t.paint_popup(popupPos);
}

bool BrushTool::prevent_undo_or_redo() {
    return objInfoBeingEdited;
}

void BrushTool::draw(SkCanvas* canvas, const DrawData& drawData) {
    const auto& main = *drawData.main;
    const auto& input = main.input;
    if (drawData.takingScreenshot || !ToolCursor::visible(main.window.windowFocus,
        main.window.mouseFocus, input.isTouchDevice, input.pen.inProximity,
        input.pen.isDown, objInfoBeingEdited != nullptr)) return;
    if (main.g.gui.cursor_obstructed()) return;
    const bool usePenPosition = objInfoBeingEdited ? genData.deviceType == InputManager::MouseDeviceType::PEN : (input.pen.inProximity || input.pen.isDown);
    const Vector2f pos = usePenPosition ? input.pen.previousPos : input.mouse.pos;
    if (pos.x() < 0 || pos.y() < 0 || pos.x() >= main.window.size.x() || pos.y() >= main.window.size.y()) return;
    const auto size = main.toolConfig.get_relative_width_stroke_size(drawP, drawData.cam.c.inverseScale);
    if (!size.first) return;
    float diameter = *size.first;
    if (objInfoBeingEdited) diameter = genData.penPath && !genData.brushPoints.empty() ?
        genData.brushPoints.back().width : diameter * genData.penWidth;
    ToolCursor::draw(canvas, pos.x(), pos.y(), ToolCursor::radius(diameter),
        SDL_GetWindowDisplayScale(main.window.sdlWindow));
}
