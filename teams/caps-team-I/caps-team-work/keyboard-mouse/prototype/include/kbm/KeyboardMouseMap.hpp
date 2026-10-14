#pragma once
// Team I2 prototype (Mac-only, not in any runtime build): key mapping for
// "USB keyboard and mouse on PS5".
//
// One row per UE4SS key value (the Lua `Key.X` a PC mod can bind). Columns:
//   fkey : the Unreal engine key name (EKeys) used by the ENGINE route
//          (option C, recommended). Source 1: Epic's public EKeys API docs.
//          Source 2: the live engine itself (the runtime looks the name up
//          in the game's name table at run time; a missing name = that key
//          is unmapped on that game, fail closed).
//   hid  : USB HID usage, keyboard page 0x07, used ONLY by the device route
//          (option A, not recommended for shipping). Source 1: USB-IF HID
//          Usage Tables. Source 2: SDL_scancode.h (public, checked by
//          tests/check_table.py). Whether the PS5 keyboard API reports these
//          numbers is UNKNOWN (no public source says so): console capture first.
// The table is generated from the runtime's real KeyDef.hpp enum, so every
// bindable UE4SS key appears exactly once (enum aliases noted in `note`).
#include <Input/KeyDef.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace modbridge::kbm
{
    using RC::Input::Key;
    using RC::Input::ModifierKey;

    struct KeyRow
    {
        Key key;          // real RC::Input::Key value
        const char* name; // UE4SS spelling (string_to_key / Lua Key.NAME)
        const char* fkey; // engine key name; nullptr = the engine has no such key
        uint16_t hid;     // USB HID keyboard-page usage; 0 = none
        const char* note; // nullptr or a caveat / reason a column is empty
    };

    struct ModifierRow
    {
        ModifierKey modifier; // SHIFT / CONTROL / ALT
        const char* fkey_left;
        const char* fkey_right;
        uint16_t hid_left;
        uint16_t hid_right;
    };

    // Extra HID usages that mean an existing key (not a row's primary usage).
    struct HidAlias
    {
        uint16_t hid;
        Key key;
        const char* note;
    };

    std::span<const KeyRow> key_table();
    std::span<const ModifierRow> modifier_table();
    std::span<const HidAlias> hid_aliases();

    const KeyRow* row_for(Key key); // nullptr if the value is not a bindable key
    std::optional<Key> key_for_fkey(std::string_view fkey);
    std::optional<Key> key_for_hid(uint16_t usage); // primary usages + aliases

    inline const char* fkey_for(Key key)
    {
        const KeyRow* row = row_for(key);
        return row ? row->fkey : nullptr;
    }
} // namespace modbridge::kbm
