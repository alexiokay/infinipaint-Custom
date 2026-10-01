#include "../src/ContactLifecycle.hpp"
#include <iostream>
#include <stdexcept>
struct Event { int deviceType; unsigned penId; };
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        ContactLifecycle<Event> state;
        require(!state.accepts(Event{1,10}, true), "inactive motion");
        require(state.begin({1,10}), "begin pen");
        require(!state.begin({1,11}), "foreign down replaced owner");
        require(!state.accepts(Event{1,11}, true), "foreign pen motion");
        require(!state.accepts(Event{2,0}, false), "touch motion during pen stroke");
        require(!state.accepts(Event{1,10}, true, false), "hover motion");
        require(!state.finish(Event{1,11}, true) && state.active(), "foreign release ended stroke");
        require(state.accepts(Event{1,10}, true), "owned pen motion");
        require(state.finish(Event{1,10}, true) && !state.active(), "owned release");
        require(!state.finish(Event{1,10}, true), "duplicate release");
        require(state.begin({0,0}) && state.accepts(Event{0,0}, false), "mouse stroke");
        state.cancel();
        require(!state.active() && !state.accepts(Event{0,0}, false), "cancel leaked ownership");
        require(state.begin({1,11}) && state.finish(Event{1,11}, true), "ownership after cancellation");
        std::cout << "Contact lifecycle checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
