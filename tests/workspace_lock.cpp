#include "../src/WorkspaceLock.hpp"
#include <iostream>
#include <stdexcept>
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        using namespace WorkspaceLock;
        const auto defaults = nlohmann::json::object().get<Preferences>();
        require(defaults.blockCanvasEdits && defaults.blockTouchDrawing && defaults.lockPanelPosition, "defaults");
        for (bool saved : {false,true}) for (bool active : {false,true})
            for (bool touch : {false,true}) for (bool edits : {false,true}) {
                Preferences p{edits,touch,false};
                require(blocksTouch(saved,active,p) == (saved || (active && touch)), "temporary touch override");
                require(blocksEdits(active,p) == (active && edits), "edit policy");
                require(blocksTouch(saved,false,p) == saved, "unlock restored wrong touch setting");
                const auto json = nlohmann::json(p);
                require(!json.contains("active") && !json.contains("locked"), "runtime lock persisted");
                const auto restored = json.get<Preferences>();
                require(restored.blockCanvasEdits == edits && restored.blockTouchDrawing == touch && !restored.lockPanelPosition, "preferences round trip");
            }
        std::cout << "Workspace lock policy checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
