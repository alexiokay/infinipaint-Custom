"""Tests for Fill Tool, Brush Flat Overlap, and Shape Geometry modes (1:1 & from-center)."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

def source(path):
    return (ROOT / path).read_text(encoding="utf-8")

class FillAndFlatOverlapWiring(unittest.TestCase):
    def test_fill_tool_enum_and_allocation(self):
        tool_base_h = source("src/DrawingProgram/Tools/DrawingProgramToolBase.hpp")
        self.assertIn("FILL,", tool_base_h)
        tool_base_cpp = source("src/DrawingProgram/Tools/DrawingProgramToolBase.cpp")
        self.assertIn("DrawingProgramToolType::FILL", tool_base_cpp)
        self.assertIn("LassoFillTool", tool_base_cpp)

    def test_fill_tool_cmakelists(self):
        cmake = source("CMakeLists.txt")
        self.assertIn("src/DrawingProgram/Tools/LassoFillTool.cpp", cmake)

    def test_fill_tool_toolbar_and_icons(self):
        dp = source("src/DrawingProgram/DrawingProgram.cpp")
        self.assertIn('"Fill Toolbar Button"', dp)
        self.assertIn('"data/icons/fill.svg"', dp)
        self.assertIn('case DrawingProgramToolType::FILL: return "Fill Tool";', dp)
        self.assertIn('case DrawingProgramToolType::FILL: return "data/icons/fill.svg";', dp)
        self.assertIn('type == DrawingProgramToolType::FILL', dp)
        self.assertIn('KEY_DRAW_TOOL_FILL', dp)

    def test_input_manager_fill_key(self):
        input_h = source("src/InputManager.hpp")
        self.assertIn("KEY_DRAW_TOOL_FILL,", input_h)
        self.assertIn('{InputManager::KEY_DRAW_TOOL_FILL, "Fill Tool"}', input_h)
        input_cpp = source("src/InputManager.cpp")
        self.assertIn("defaultKeyAssignments[{0, SDLK_G}] = KEY_DRAW_TOOL_FILL;", input_cpp)

    def test_fill_icon_asset_exists(self):
        svg_file = ROOT / "assets/data/icons/fill.svg"
        self.assertTrue(svg_file.exists())
        content = svg_file.read_text(encoding="utf-8")
        self.assertIn("<svg", content)

    def test_flat_overlap_config(self):
        config = source("src/BrushPressureConfig.hpp")
        self.assertIn("bool flatOverlap = false;", config)
        self.assertIn('{"flatOverlap", c.flatOverlap}', config)
        self.assertIn('j.contains("flatOverlap")', config)

    def test_flat_overlap_inspector_ui(self):
        brush = source("src/DrawingProgram/Tools/BrushTool.cpp")
        self.assertIn('"Flat coloring (No overlap stripes)"', brush)
        self.assertNotIn('"Paper Grain / Tooth"', brush)

    def test_flat_overlap_merge_logic(self):
        brush = source("src/DrawingProgram/Tools/BrushTool.cpp")
        self.assertIn("drawP.world.main.toolConfig.brush.flatOverlap", brush)
        self.assertIn("canMergeWithPrevious", brush)
        self.assertIn("SkPathOp::kUnion_SkPathOp", brush)
        self.assertIn("EditTransformCanvasComponentWorldUndoAction", brush)
        self.assertIn("CanvasComponentContainer::calculate_draw_transform", brush)
        brush_h = source("src/DrawingProgram/Tools/BrushTool.hpp")
        self.assertIn("bool canMergeWithPrevious = false;", brush_h)

    def test_touch_and_pen_scroll_improvements(self):
        slider_h = source("src/GUIStuff/Elements/NumberSlider.hpp")
        self.assertIn("penScrollingAway", slider_h)
        self.assertIn("touchScrollingAway", slider_h)
        scroll_cpp = source("src/GUIStuff/Elements/ScrollArea.cpp")
        self.assertIn("penScrollStartPos", scroll_cpp)
        self.assertIn("touchScrollStartPos", scroll_cpp)
        btn_cpp = source("src/GUIStuff/Elements/SelectableButton.cpp")
        self.assertIn("penStartPos", btn_cpp)
        self.assertIn("touchStartPos", btn_cpp)

    def test_toolbar_active_color_unification(self):
        toolbar = source("src/Toolbar.cpp")
        self.assertNotIn("active studio color", toolbar)

    def test_ellipse_aspect_ratio_and_center_mode(self):
        config = source("src/DrawingProgram/ToolConfiguration.hpp")
        self.assertIn("bool perfectCircle = false;", config)
        self.assertIn("bool fromCenter = false;", config)
        self.assertIn("NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EllipseDrawToolConfig, relativeWidth, fillStrokeMode, perfectCircle, fromCenter)", config)

        ellipse_cpp = source("src/DrawingProgram/Tools/EllipseDrawTool.cpp")
        self.assertIn('"perfect circle"', ellipse_cpp)
        self.assertIn('"from center"', ellipse_cpp)
        self.assertIn("config.perfectCircle", ellipse_cpp)
        self.assertIn("config.fromCenter", ellipse_cpp)
        self.assertIn("KEY_GENERIC_LALT", ellipse_cpp)
        self.assertIn("KEY_GENERIC_LSHIFT", ellipse_cpp)

    def test_rectangle_aspect_ratio_and_center_mode(self):
        config = source("src/DrawingProgram/ToolConfiguration.hpp")
        self.assertIn("bool perfectSquare = false;", config)
        self.assertIn("bool fromCenter = false;", config)
        self.assertIn("int aspectRatioMode = 0;", config)
        self.assertIn("float customAspectX = 16.0f;", config)
        self.assertIn("float customAspectY = 9.0f;", config)
        self.assertIn("NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RectDrawToolConfig, relativeWidth, relativeRadiusWidth, fillStrokeMode, perfectSquare, fromCenter, aspectRatioMode, customAspectX, customAspectY)", config)

        rect_cpp = source("src/DrawingProgram/Tools/RectDrawTool.cpp")
        self.assertIn('"aspect ratio select"', rect_cpp)
        self.assertIn('"1:1 Square"', rect_cpp)
        self.assertIn('"16:9"', rect_cpp)
        self.assertIn('"9:16"', rect_cpp)
        self.assertIn('"4:3"', rect_cpp)
        self.assertIn('"3:2"', rect_cpp)
        self.assertIn('"Custom"', rect_cpp)
        self.assertIn("custom_aspect_x", rect_cpp)
        self.assertIn("custom_aspect_y", rect_cpp)
        self.assertIn('"from center"', rect_cpp)
        self.assertIn("config.fromCenter", rect_cpp)
        self.assertIn("KEY_GENERIC_LALT", rect_cpp)
        self.assertIn("KEY_GENERIC_LSHIFT", rect_cpp)
        self.assertIn("targetRatio", rect_cpp)

    def test_selection_export_resolution_multiplier(self):
        sel_h = source("src/DrawingProgram/DrawingProgramSelection.hpp")
        self.assertIn("int exportResolutionMultiplier = 2;", sel_h)

        sel_cpp = source("src/DrawingProgram/DrawingProgramSelection.cpp")
        self.assertIn('"export res"', sel_cpp)
        self.assertIn('"1x"', sel_cpp)
        self.assertIn('"2x HD"', sel_cpp)
        self.assertIn('"4x Print"', sel_cpp)
        self.assertIn("exportResolutionMultiplier", sel_cpp)
        self.assertIn("mult = static_cast<float>(std::max(1, exportResolutionMultiplier));", sel_cpp)

    def test_edit_tool_selection_actions(self):
        edit_cpp = source("src/DrawingProgram/Tools/EditTool.cpp")
        self.assertIn("drawP.selection.selection_gui(t);", edit_cpp)
        self.assertIn("drawP.selection.phone_selection_gui(t);", edit_cpp)
        self.assertIn("drawP.selection.color_picker_color(oldColor);", edit_cpp)
        self.assertIn("drawP.selection.phone_selection_bottom_toolbar(t);", edit_cpp)

    def test_fill_tool_crash_safety_and_toolbar_null_checks(self):
        lasso_h = source("src/DrawingProgram/Tools/LassoFillTool.hpp")
        self.assertNotIn("color_picker_color", lasso_h)

        lasso_cpp = source("src/DrawingProgram/Tools/LassoFillTool.cpp")
        self.assertNotIn("color_picker_color", lasso_cpp)
        self.assertIn("bounds.width() >= 4.0f", lasso_cpp)

        tb_h = source("src/Toolbar.hpp")
        self.assertIn("colorLeftButton = nullptr;", tb_h)
        self.assertIn("colorRightButton = nullptr;", tb_h)

        tb_cpp = source("src/Toolbar.cpp")
        self.assertIn("(!b || !b->mouseHovering)", tb_cpp)

    def test_line_tool_advanced_options(self):
        config = source("src/DrawingProgram/ToolConfiguration.hpp")
        self.assertIn("int lineStyle = 0;", config)
        self.assertIn("float dashLength = 3.0f;", config)
        self.assertIn("float dashGap = 2.0f;", config)
        self.assertIn("int arrowMode = 0;", config)
        self.assertIn("bool snapAngles = false;", config)
        self.assertIn("NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(LineDrawToolConfig, hasRoundCaps, relativeWidth, lineStyle, dashLength, dashGap, arrowMode, snapAngles)", config)

        line_h = source("src/DrawingProgram/Tools/LineDrawTool.hpp")
        self.assertIn("void gui_inspector();", line_h)

        line_cpp = source("src/DrawingProgram/Tools/LineDrawTool.cpp")
        self.assertIn('"line pattern select"', line_cpp)
        self.assertIn('"Solid"', line_cpp)
        self.assertIn('"Dashed"', line_cpp)
        self.assertIn('"Dotted (Circles)"', line_cpp)
        self.assertIn('"Dash-Dot"', line_cpp)
        self.assertIn('"Dash-Dot-Dot"', line_cpp)
        self.assertIn('"line arrow select"', line_cpp)
        self.assertIn('"End Arrow ->"', line_cpp)
        self.assertIn('"Start Arrow <-"', line_cpp)
        self.assertIn('"Both Ends <->"', line_cpp)
        self.assertIn('"dashlength"', line_cpp)
        self.assertIn('"dashgap"', line_cpp)
        self.assertIn('"snapangles"', line_cpp)
        self.assertIn("generate_line_path", line_cpp)
        self.assertIn("add_capsule", line_cpp)
        self.assertIn("add_arrow", line_cpp)

if __name__ == "__main__":
    unittest.main()


