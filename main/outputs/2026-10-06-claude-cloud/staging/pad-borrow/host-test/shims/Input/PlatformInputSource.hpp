#pragma once
// Host-test stand-in for the port's Input/PlatformInputSource.hpp: only the
// surface PS5PadSource.{hpp,cpp} touch (Key/ModifierKeys/InputEvent, Handler's
// two accessors, the PlatformInputSource virtuals).
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
namespace RC::Input {
enum Key : uint8_t { RESERVED_START_OF_ENUM = 0, RETURN = 0x0D, LEFT_ARROW = 0x25, UP_ARROW = 0x26,
                     RIGHT_ARROW = 0x27, DOWN_ARROW = 0x28, F7 = 0x76 };
constexpr size_t max_keys = 256;
enum ModifierKey : uint32_t { SHIFT = 0, CONTROL = 1, ALT = 2 };
struct ModifierKeys { uint32_t keys{}; };
struct InputEvent { Key key; ModifierKeys modifier_keys; };
class Handler {
public:
    std::array<bool, max_keys> subs{};
    bool allow{true};
    const std::array<bool, max_keys>& get_subscribed_keys() const { return subs; }
    bool get_allow_input() const { return allow; }
};
class PlatformInputSource {
public:
    virtual ~PlatformInputSource() = default;
    virtual bool is_available() = 0;
    virtual bool activate() = 0;
    virtual bool deactivate() = 0;
    virtual std::vector<InputEvent>& process_event(Handler*) = 0;
    virtual const char* get_name() = 0;
    virtual int source_priority() = 0;
};
}
