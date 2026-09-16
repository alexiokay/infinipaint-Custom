"""Source/contrast regressions only; these do not render or launch InfiniPaint."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def source(path):
    return (ROOT / path).read_text(encoding="utf-8")


def luminance(rgb):
    linear = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in rgb]
    return sum(v * w for v, w in zip(linear, (.2126, .7152, .0722)))


class GraphiteUI(unittest.TestCase):
    def test_default_palette_contrast(self):
        text = source("src/GUIStuff/Elements/GUIStuffHelpers.cpp")
        colors = {}
        for name, values in re.findall(r"theme->(\w+Color\d) = \{([^}]+)\}", text):
            colors[name] = [float(v.strip().rstrip("f")) for v in values.split(",")][:3]
        for foreground in ("frontColor1", "frontColor2"):
            for background in ("backColor1", "backColor2"):
                ratio = (luminance(colors[foreground]) + .05) / (luminance(colors[background]) + .05)
                self.assertGreaterEqual(ratio, 4.5, (foreground, background, ratio))
        self.assertGreaterEqual((luminance(colors["fillColor1"]) + .05) / (luminance(colors["backColor2"]) + .05), 3)

    def test_theme_compatibility_and_controls(self):
        theme = source("src/GUIStuff/Elements/GUIStuffHelpers.hpp")
        self.assertIn("controlHeight = 28", theme)
        self.assertIn("controlCorners = 6", theme)
        self.assertRegex(theme, r"NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT\(Theme,[^\n]*controlHeight, controlCorners\)")
        loader = source("src/GUIHolder.cpp")
        textbox = source("src/GUIStuff/Elements/TextBox.impl.hpp")
        self.assertIn("static_cast<float>(io.theme->controlHeight)", textbox)
        self.assertIn("io.theme->controlCorners", textbox)
        self.assertIn("std::clamp<uint16_t>(theme->controlHeight, 24, 48)", loader)
        for control in ("CheckBox", "RadioButton"):
            code = source(f"src/GUIStuff/Elements/{control}.cpp")
            self.assertIn("std::string_view label", code)
            self.assertIn("gui.strArena.std_str_to_clay_str(label)", code)
            self.assertIn("gui.io.theme->controlHeight", code)

    def test_shared_inspectors(self):
        for tool in ("Brush", "Eraser"):
            code = source(f"src/DrawingProgram/Tools/{tool}Tool.cpp")
            for method in ("gui_toolbox", "gui_phone_toolbox"):
                body = code.split(f"void {tool}Tool::{method}(", 1)[1].split("\nvoid ", 1)[0]
                self.assertIn("gui_inspector();", body)
        brush = source("src/DrawingProgram/Tools/BrushTool.cpp")
        self.assertIn("if (advancedSettingsOpen)", brush)
        self.assertIn("if (main.toolConfig.brush.samplePath() && main.conf.tabletOptions.penFilter.enabled)", brush)
        eraser = source("src/DrawingProgram/Tools/EraserTool.cpp")
        self.assertIn('"Whole objects", false, "Portions", true', eraser)

    def test_compact_panel_and_geometry_wiring(self):
        panel = source("src/DrawingProgram/DrawingProgram.cpp")
        for token in ('CLAY_ATTACH_TO_ROOT', '"panel pin"', '"panel reset"',
                      'toolPanelDragPen', 'toolPanelDragFinger', 'window.windowFocus',
                      'if (!world.main.toolConfig.toolPanel.pinned && toolPanelExpanded)',
                      't.quick_colors();'):
            self.assertIn(token, panel)
        self.assertIn("toolPanel", source("src/DrawingProgram/ToolConfiguration.hpp"))
        slider = source("src/GUIStuff/Elements/NumberSlider.hpp")
        self.assertIn("UIControlGeometry::sliderPosition", slider)
        self.assertIn("UIControlGeometry::sliderFraction", slider)
        scroll = source("src/GUIStuff/Elements/ScrollArea.cpp")
        self.assertIn("gutterY+2", scroll)
        self.assertIn(".containerDimensions = viewport", scroll)
        self.assertIn("canvas->drawRRect(outline,outlinePaint)", source("src/GUIStuff/GUIManager.cpp"))
        load = source("src/MainProgram.cpp")
        self.assertLess(load.index('j.at("toolConfig").get_to(toolConfig)'), load.index("toolConfig.brush.migrateCorrection"))

    def test_panel_height_has_explicit_float_conversion(self):
        # Clay's sizing macro uses C++ aggregate initialization. A runtime uint16_t
        # argument narrows to float there, even when the theme value is small.
        panel = source("src/DrawingProgram/DrawingProgram.cpp")
        self.assertNotRegex(panel, r"CLAY_SIZING_FIXED\(\s*io\.theme->controlHeight\s*\)")
        self.assertEqual(panel.count("CLAY_SIZING_FIXED(static_cast<float>(io.theme->controlHeight))"), 2)

    def test_cursor_overlay_is_separate_from_erase_path(self):
        draw = source("src/DrawingProgram/Tools/EraserTool.cpp").split("void EraserTool::draw(", 1)[1]
        self.assertIn("ToolCursor::visible", draw)
        self.assertIn("ToolCursor::draw", draw)
        self.assertIn("drawData.takingScreenshot", draw)
        self.assertIn("input.pen.previousPos", draw)
        self.assertIn("genData.deviceType == InputManager::MouseDeviceType::PEN", draw)
        self.assertIn("isErasing && !main.conf.realTimeEraser", draw)
        self.assertIn("genData.brushPoints.back().width, transform.scale", draw)
        self.assertNotIn("erase_on_path()", draw)
        self.assertNotIn("erasePath =", draw)
        overlay = source("src/DrawingProgram/Tools/CanvasToolCursor.hpp")
        self.assertIn("SkColor4f{0, 0, 0, 1}", overlay)
        self.assertIn("SkColor4f{1, 1, 1, 1}", overlay)

    def test_zen_launcher_and_brush_presets(self):
        panel = source("src/DrawingProgram/DrawingProgram.cpp")
        self.assertIn('"tool launcher circle"', panel)
        self.assertIn('"tool panel card"', panel)
        self.assertIn('"BRUSH SETUPS"', panel)
        self.assertIn('"preset save"', panel)
        self.assertIn('"panel close"', panel)
        self.assertIn("toolPanelDragMoved", panel)

        tool_cfg = source("src/DrawingProgram/ToolConfiguration.hpp")
        self.assertIn("struct BrushPreset", tool_cfg)
        self.assertIn("std::vector<BrushPreset> presets;", tool_cfg)
        self.assertIn("init_default_presets_if_empty()", tool_cfg)
        self.assertIn("apply_brush_preset(DrawingProgram& drawP, size_t index)", tool_cfg)
        self.assertIn("save_current_brush_preset(DrawingProgram& drawP", tool_cfg)
        self.assertIn("delete_brush_preset(size_t index)", tool_cfg)
        self.assertIn("NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ToolConfiguration, brush, toolPanel, eraser, ellipseDraw, rectDraw, eyeDropper, lineDraw, screenshot, globalConf, presets, selectedPreset)", tool_cfg)

        impl = source("src/DrawingProgram/ToolConfiguration.cpp")
        self.assertIn('"Studio Pen"', impl)
        self.assertIn('"Fine Liner"', impl)
        self.assertIn('"Soft Pencil"', impl)
        self.assertIn('"Marker"', impl)


if __name__ == "__main__":
    unittest.main()
