#pragma once

// Device-independent ownership policy, shared with lifecycle regression tests.
template <class Contact>
class ContactLifecycle {
    Contact owner{};
    bool running = false;
public:
    bool active() const { return running; }
    bool begin(const Contact& contact) {
        if (running) return false;
        owner = contact;
        running = true;
        return true;
    }
    template <class Event>
    bool accepts(const Event& event, bool pen, bool contact = true) const {
        return running && event.deviceType == owner.deviceType &&
            (!pen || (contact && event.penId == owner.penId));
    }
    template <class Event>
    bool finish(const Event& event, bool pen) {
        if (!accepts(event, pen)) return false;
        running = false;
        return true;
    }
    void cancel() { running = false; }
};
