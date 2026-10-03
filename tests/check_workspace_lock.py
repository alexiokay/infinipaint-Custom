"""Application wiring checks; behavior is tested in workspace_lock.cpp."""
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
class WorkspaceLockWiring(unittest.TestCase):
    def test_lock_controls_use_fixed_icon_slots_in_top_bar(self):
        text = (ROOT / "src/DrawingProgram/DrawingProgram.cpp").read_text(encoding="utf-8")
        controls = text.split("void DrawingProgram::workspace_lock_toolbar_gui()", 1)[1].split("void DrawingProgram::toolbar_gui", 1)[0]
        self.assertEqual(controls.count("svg_icon_button("), 2)
        self.assertNotIn("text_button", controls)
        rail = text.split("void DrawingProgram::toolbar_gui", 1)[1].split("\nvoid ", 1)[0]
        self.assertNotIn('"Workspace lock', rail)
        toolbar = (ROOT / "src/Toolbar.cpp").read_text(encoding="utf-8")
        top = toolbar.split("void Toolbar::top_toolbar()", 1)[1].split("\nvoid ", 1)[0]
        self.assertIn("main.world->drawProg.workspace_lock_toolbar_gui();", top)

    def test_saved_setting_is_not_overwritten(self):
        text = (ROOT / "src/DrawingProgram/DrawingProgram.cpp").read_text(encoding="utf-8")
        self.assertNotIn("conf.disableTouchForDrawing =", text)
        self.assertIn("WorkspaceLock::blocksTouch", text)
        self.assertIn("toolBeforeWorkspaceLock", text)
        drag = text.split("auto makeDragCallbacks", 1)[1].split("if (!toolPanelExpanded)", 1)[0]
        self.assertEqual(drag.count("if (workspace_panel_locked()) return;"), 3)
        self.assertIn("!m.penContact", drag)

    def test_mutation_entry_points_guarded(self):
        paths = {
            "src/DrawingProgram/DrawingProgram.cpp": ["input_paste_callback", "input_text_callback", "add_file_to_canvas_by_path", "add_file_to_canvas_by_data"],
            "src/DrawingProgram/DrawingProgramSelection.cpp": ["delete_all", "paste_clipboard", "push_selection_to_front", "push_selection_to_back"],
            "src/WorldUndoManager.cpp": ["undo", "redo"],
        }
        for path, names in paths.items():
            text = (ROOT / path).read_text(encoding="utf-8")
            for name in names:
                start = text.index("::" + name + "(")
                body = text[text.index("{", start) + 1:].lstrip()
                self.assertIn("workspace_edits_blocked()", body.splitlines()[0], (path, name))

    def test_assets_and_policy_registered(self):
        for icon in ["workspace-locked.svg", "workspace-unlocked.svg"]:
            self.assertTrue((ROOT / "assets/data/icons" / icon).is_file())
        self.assertIn("add_test(NAME workspace_lock", (ROOT / "tests/CMakeLists.txt").read_text())
        self.assertIn("bool workspaceLocked = false", (ROOT / "src/DrawingProgram/DrawingProgram.hpp").read_text())

    def test_unified_touch_policy_and_contact_handoff(self):
        text = (ROOT / "src/DrawingProgram/DrawingProgram.cpp").read_text(encoding="utf-8")
        start = text.index("void DrawingProgram::input_finger_touch_callback")
        touch = text[start:].split("\nstd::optional", 1)[0]
        self.assertIn("WorkspaceLock::blocksTouch", touch)
        self.assertIn("workspace_edits_blocked() && navigationTool", touch)
        self.assertIn("lastTouch = touch.clone()", touch)
        self.assertIn("std::erase_if", touch)
        lock = text.split("void DrawingProgram::set_workspace_lock", 1)[1].split("\nvoid ", 1)[0]
        self.assertIn("release.action.type = FingerInput::ActionType::UP", lock)
        self.assertIn("PointerDownState::FINGER_DISABLED", lock)
        self.assertIn("cam.clear_control_mode()", lock)
        self.assertNotIn("controls.leftClickHeld", text)

if __name__ == "__main__":
    unittest.main()
