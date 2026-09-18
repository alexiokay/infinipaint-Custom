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
        self.assertIn("NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RectDrawToolConfig, relativeWidth, relativeRadiusWidth, fillStrokeMode, perfectSquare, fromCenter)", config)

        rect_cpp = source("src/DrawingProgram/Tools/RectDrawTool.cpp")
        self.assertIn('"perfect square"', rect_cpp)
        self.assertIn('"from center"', rect_cpp)
        self.assertIn("config.perfectSquare", rect_cpp)
        self.assertIn("config.fromCenter", rect_cpp)
        self.assertIn("KEY_GENERIC_LALT", rect_cpp)
        self.assertIn("KEY_GENERIC_LSHIFT", rect_cpp)

if __name__ == "__main__":
    unittest.main()
