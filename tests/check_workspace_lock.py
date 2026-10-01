"""Application wiring checks; behavior is tested in workspace_lock.cpp."""
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
class WorkspaceLockWiring(unittest.TestCase):
    def test_saved_setting_is_not_overwritten(self):
        text = (ROOT / "src/DrawingProgram/DrawingProgram.cpp").read_text(encoding="utf-8")
        self.assertNotIn("conf.disableTouchForDrawing =", text)
        self.assertIn("WorkspaceLock::blocksTouch", text)
        self.assertIn("toolBeforeWorkspaceLock", text)

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

if __name__ == "__main__":
    unittest.main()
