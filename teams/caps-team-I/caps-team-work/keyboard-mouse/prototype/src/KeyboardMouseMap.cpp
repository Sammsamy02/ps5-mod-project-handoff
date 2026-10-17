// Team I2 prototype (Mac-only). Mapping table, generated once from the real
// runtime enum (native-coherent-runtime-1/.../Input/KeyDef.hpp) and checked
// by tests (C++: uniqueness + string_to_key round trip; Python: every enum
// member covered, HID column equal to SDL_scancode.h). See KeyboardMouseMap.hpp.
#include <kbm/KeyboardMouseMap.hpp>

#include <array>
#include <cstring>

namespace modbridge::kbm
{
    namespace
    {
        constexpr KeyRow k_rows[] = {
        {Key::LEFT_MOUSE_BUTTON, "LEFT_MOUSE_BUTTON", "LeftMouseButton", 0x00, "mouse button: engine route only (no HID keyboard usage)"},
        {Key::RIGHT_MOUSE_BUTTON, "RIGHT_MOUSE_BUTTON", "RightMouseButton", 0x00, "mouse button: engine route only (no HID keyboard usage)"},
        {Key::CANCEL, "CANCEL", nullptr, 0x00, "PC Ctrl+Break; no engine key, no HID usage"},
        {Key::MIDDLE_MOUSE_BUTTON, "MIDDLE_MOUSE_BUTTON", "MiddleMouseButton", 0x00, "mouse button: engine route only (no HID keyboard usage)"},
        {Key::XBUTTON_ONE, "XBUTTON_ONE", "ThumbMouseButton", 0x00, "mouse button: engine route only (no HID keyboard usage)"},
        {Key::XBUTTON_TWO, "XBUTTON_TWO", "ThumbMouseButton2", 0x00, "mouse button: engine route only (no HID keyboard usage)"},
        {Key::BACKSPACE, "BACKSPACE", "BackSpace", 0x2A, nullptr},
        {Key::TAB, "TAB", "Tab", 0x2B, nullptr},
        {Key::CLEAR, "CLEAR", nullptr, 0x9C, "no engine key"},
        {Key::RETURN, "RETURN", "Enter", 0x28, "keypad Enter (HID 0x58) is an alias"},
        {Key::PAUSE, "PAUSE", "Pause", 0x48, nullptr},
        {Key::CAPS_LOCK, "CAPS_LOCK", "CapsLock", 0x39, nullptr},
        {Key::IME_KANA, "IME_KANA", nullptr, 0x88, "same value as IME_HANGUEL/IME_HANGUL; HID International2 (JP kana), alias HID 0x90 LANG1 (KR hangul); no engine key; enum aliases: IME_HANGUEL, IME_HANGUL"},
        {Key::IME_ON, "IME_ON", nullptr, 0x00, "PC IME control; no engine key, no HID usage"},
        {Key::IME_JUNJA, "IME_JUNJA", nullptr, 0x00, "PC IME control; no engine key, no HID usage"},
        {Key::IME_FINAL, "IME_FINAL", nullptr, 0x00, "PC IME control; no engine key, no HID usage"},
        {Key::IME_HANJA, "IME_HANJA", nullptr, 0x91, "same value as IME_KANJI; HID LANG2 (KR hanja); no engine key; enum aliases: IME_KANJI"},
        {Key::IME_OFF, "IME_OFF", nullptr, 0x00, "PC IME control; no engine key, no HID usage"},
        {Key::ESCAPE, "ESCAPE", "Escape", 0x29, "PS5 system may keep this key; to be checked on console"},
        {Key::IME_CONVERT, "IME_CONVERT", nullptr, 0x8A, "HID International4 (JP henkan); no engine key"},
        {Key::IME_NONCONVERT, "IME_NONCONVERT", nullptr, 0x8B, "HID International5 (JP muhenkan); no engine key"},
        {Key::IME_ACCEPT, "IME_ACCEPT", nullptr, 0x00, "PC IME control; no engine key, no HID usage"},
        {Key::IME_MODECHANGE, "IME_MODECHANGE", nullptr, 0x00, "PC IME control; no engine key, no HID usage"},
        {Key::SPACE, "SPACE", "SpaceBar", 0x2C, nullptr},
        {Key::PAGE_UP, "PAGE_UP", "PageUp", 0x4B, nullptr},
        {Key::PAGE_DOWN, "PAGE_DOWN", "PageDown", 0x4E, nullptr},
        {Key::END, "END", "End", 0x4D, nullptr},
        {Key::HOME, "HOME", "Home", 0x4A, nullptr},
        {Key::LEFT_ARROW, "LEFT_ARROW", "Left", 0x50, nullptr},
        {Key::UP_ARROW, "UP_ARROW", "Up", 0x52, nullptr},
        {Key::RIGHT_ARROW, "RIGHT_ARROW", "Right", 0x4F, nullptr},
        {Key::DOWN_ARROW, "DOWN_ARROW", "Down", 0x51, nullptr},
        {Key::SELECT, "SELECT", nullptr, 0x77, "no engine key"},
        {Key::PRINT, "PRINT", nullptr, 0x00, "PC-only virtual key; no HID usage"},
        {Key::EXECUTE, "EXECUTE", nullptr, 0x74, "no engine key"},
        {Key::PRINT_SCREEN, "PRINT_SCREEN", nullptr, 0x46, "no engine key; PS5 system may keep it"},
        {Key::INS, "INS", "Insert", 0x49, nullptr},
        {Key::DEL, "DEL", "Delete", 0x4C, nullptr},
        {Key::HELP, "HELP", nullptr, 0x75, "no engine key"},
        {Key::ZERO, "ZERO", "Zero", 0x27, nullptr},
        {Key::ONE, "ONE", "One", 0x1E, nullptr},
        {Key::TWO, "TWO", "Two", 0x1F, nullptr},
        {Key::THREE, "THREE", "Three", 0x20, nullptr},
        {Key::FOUR, "FOUR", "Four", 0x21, nullptr},
        {Key::FIVE, "FIVE", "Five", 0x22, nullptr},
        {Key::SIX, "SIX", "Six", 0x23, nullptr},
        {Key::SEVEN, "SEVEN", "Seven", 0x24, nullptr},
        {Key::EIGHT, "EIGHT", "Eight", 0x25, nullptr},
        {Key::NINE, "NINE", "Nine", 0x26, nullptr},
        {Key::A, "A", "A", 0x04, nullptr},
        {Key::B, "B", "B", 0x05, nullptr},
        {Key::C, "C", "C", 0x06, nullptr},
        {Key::D, "D", "D", 0x07, nullptr},
        {Key::E, "E", "E", 0x08, nullptr},
        {Key::F, "F", "F", 0x09, nullptr},
        {Key::G, "G", "G", 0x0A, nullptr},
        {Key::H, "H", "H", 0x0B, nullptr},
        {Key::I, "I", "I", 0x0C, nullptr},
        {Key::J, "J", "J", 0x0D, nullptr},
        {Key::K, "K", "K", 0x0E, nullptr},
        {Key::L, "L", "L", 0x0F, nullptr},
        {Key::M, "M", "M", 0x10, nullptr},
        {Key::N, "N", "N", 0x11, nullptr},
        {Key::O, "O", "O", 0x12, nullptr},
        {Key::P, "P", "P", 0x13, nullptr},
        {Key::Q, "Q", "Q", 0x14, nullptr},
        {Key::R, "R", "R", 0x15, nullptr},
        {Key::S, "S", "S", 0x16, nullptr},
        {Key::T, "T", "T", 0x17, nullptr},
        {Key::U, "U", "U", 0x18, nullptr},
        {Key::V, "V", "V", 0x19, nullptr},
        {Key::W, "W", "W", 0x1A, nullptr},
        {Key::X, "X", "X", 0x1B, nullptr},
        {Key::Y, "Y", "Y", 0x1C, nullptr},
        {Key::Z, "Z", "Z", 0x1D, nullptr},
        {Key::LEFT_WIN, "LEFT_WIN", "LeftCommand", 0xE3, "GUI/Windows key; the engine calls it Command; PS5 system may keep it"},
        {Key::RIGHT_WIN, "RIGHT_WIN", "RightCommand", 0xE7, "GUI/Windows key; the engine calls it Command"},
        {Key::APPS, "APPS", nullptr, 0x65, "menu key; no engine key"},
        {Key::SLEEP, "SLEEP", nullptr, 0x00, "not on the keyboard usage page"},
        {Key::NUM_ZERO, "NUM_ZERO", "NumPadZero", 0x62, nullptr},
        {Key::NUM_ONE, "NUM_ONE", "NumPadOne", 0x59, nullptr},
        {Key::NUM_TWO, "NUM_TWO", "NumPadTwo", 0x5A, nullptr},
        {Key::NUM_THREE, "NUM_THREE", "NumPadThree", 0x5B, nullptr},
        {Key::NUM_FOUR, "NUM_FOUR", "NumPadFour", 0x5C, nullptr},
        {Key::NUM_FIVE, "NUM_FIVE", "NumPadFive", 0x5D, nullptr},
        {Key::NUM_SIX, "NUM_SIX", "NumPadSix", 0x5E, nullptr},
        {Key::NUM_SEVEN, "NUM_SEVEN", "NumPadSeven", 0x5F, nullptr},
        {Key::NUM_EIGHT, "NUM_EIGHT", "NumPadEight", 0x60, nullptr},
        {Key::NUM_NINE, "NUM_NINE", "NumPadNine", 0x61, nullptr},
        {Key::MULTIPLY, "MULTIPLY", "Multiply", 0x55, nullptr},
        {Key::ADD, "ADD", "Add", 0x57, nullptr},
        {Key::SEPARATOR, "SEPARATOR", nullptr, 0x9F, "no engine key"},
        {Key::SUBTRACT, "SUBTRACT", "Subtract", 0x56, nullptr},
        {Key::DECIMAL, "DECIMAL", "Decimal", 0x63, nullptr},
        {Key::DIVIDE, "DIVIDE", "Divide", 0x54, nullptr},
        {Key::F1, "F1", "F1", 0x3A, nullptr},
        {Key::F2, "F2", "F2", 0x3B, nullptr},
        {Key::F3, "F3", "F3", 0x3C, nullptr},
        {Key::F4, "F4", "F4", 0x3D, nullptr},
        {Key::F5, "F5", "F5", 0x3E, nullptr},
        {Key::F6, "F6", "F6", 0x3F, nullptr},
        {Key::F7, "F7", "F7", 0x40, nullptr},
        {Key::F8, "F8", "F8", 0x41, nullptr},
        {Key::F9, "F9", "F9", 0x42, nullptr},
        {Key::F10, "F10", "F10", 0x43, nullptr},
        {Key::F11, "F11", "F11", 0x44, nullptr},
        {Key::F12, "F12", "F12", 0x45, nullptr},
        {Key::F13, "F13", nullptr, 0x68, "engine has F1-F12 only"},
        {Key::F14, "F14", nullptr, 0x69, "engine has F1-F12 only"},
        {Key::F15, "F15", nullptr, 0x6A, "engine has F1-F12 only"},
        {Key::F16, "F16", nullptr, 0x6B, "engine has F1-F12 only"},
        {Key::F17, "F17", nullptr, 0x6C, "engine has F1-F12 only"},
        {Key::F18, "F18", nullptr, 0x6D, "engine has F1-F12 only"},
        {Key::F19, "F19", nullptr, 0x6E, "engine has F1-F12 only"},
        {Key::F20, "F20", nullptr, 0x6F, "engine has F1-F12 only"},
        {Key::F21, "F21", nullptr, 0x70, "engine has F1-F12 only"},
        {Key::F22, "F22", nullptr, 0x71, "engine has F1-F12 only"},
        {Key::F23, "F23", nullptr, 0x72, "engine has F1-F12 only"},
        {Key::F24, "F24", nullptr, 0x73, "engine has F1-F12 only"},
        {Key::NUM_LOCK, "NUM_LOCK", "NumLock", 0x53, nullptr},
        {Key::SCROLL_LOCK, "SCROLL_LOCK", "ScrollLock", 0x47, nullptr},
        {Key::BROWSER_BACK, "BROWSER_BACK", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::BROWSER_FORWARD, "BROWSER_FORWARD", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::BROWSER_REFRESH, "BROWSER_REFRESH", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::BROWSER_STOP, "BROWSER_STOP", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::BROWSER_SEARCH, "BROWSER_SEARCH", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::BROWSER_FAVORITES, "BROWSER_FAVORITES", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::BROWSER_HOME, "BROWSER_HOME", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::VOLUME_MUTE, "VOLUME_MUTE", nullptr, 0x7F, "no engine key; PS5 system may keep it"},
        {Key::VOLUME_DOWN, "VOLUME_DOWN", nullptr, 0x81, "no engine key; PS5 system may keep it"},
        {Key::VOLUME_UP, "VOLUME_UP", nullptr, 0x80, "no engine key; PS5 system may keep it"},
        {Key::MEDIA_NEXT_TRACK, "MEDIA_NEXT_TRACK", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::MEDIA_PREV_TRACK, "MEDIA_PREV_TRACK", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::MEDIA_STOP, "MEDIA_STOP", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::MEDIA_PLAY_PAUSE, "MEDIA_PLAY_PAUSE", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::LAUNCH_MAIL, "LAUNCH_MAIL", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::LAUNCH_MEDIA_SELECT, "LAUNCH_MEDIA_SELECT", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::LAUNCH_APP1, "LAUNCH_APP1", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::LAUNCH_APP2, "LAUNCH_APP2", nullptr, 0x00, "media/browser key: USB consumer page, not keyboard page; unmapped"},
        {Key::OEM_ONE, "OEM_ONE", "Semicolon", 0x33, "US layout position (;)"},
        {Key::OEM_PLUS, "OEM_PLUS", "Equals", 0x2E, "US layout position (=)"},
        {Key::OEM_COMMA, "OEM_COMMA", "Comma", 0x36, "US layout position (,)"},
        {Key::OEM_MINUS, "OEM_MINUS", "Hyphen", 0x2D, "US layout position (-)"},
        {Key::OEM_PERIOD, "OEM_PERIOD", "Period", 0x37, "US layout position (.)"},
        {Key::OEM_TWO, "OEM_TWO", "Slash", 0x38, "US layout position (/)"},
        {Key::OEM_THREE, "OEM_THREE", "Tilde", 0x35, "US layout position (`)"},
        {Key::OEM_FOUR, "OEM_FOUR", "LeftBracket", 0x2F, "US layout position ([)"},
        {Key::OEM_FIVE, "OEM_FIVE", "Backslash", 0x31, "US layout position (\\)"},
        {Key::OEM_SIX, "OEM_SIX", "RightBracket", 0x30, "US layout position (])"},
        {Key::OEM_SEVEN, "OEM_SEVEN", "Apostrophe", 0x34, "US layout position (')"},
        {Key::OEM_EIGHT, "OEM_EIGHT", nullptr, 0x00, "layout-specific key with no fixed position; unmapped"},
        {Key::OEM_102, "OEM_102", nullptr, 0x64, "ISO extra key (<>); no engine key"},
        {Key::IME_PROCESS, "IME_PROCESS", nullptr, 0x00, "PC-only virtual key; unmapped"},
        {Key::PACKET, "PACKET", nullptr, 0x00, "PC-only virtual key; unmapped"},
        {Key::ATTN, "ATTN", nullptr, 0x00, "PC-only virtual key; unmapped"},
        {Key::CRSEL, "CRSEL", nullptr, 0xA3, "no engine key"},
        {Key::EXSEL, "EXSEL", nullptr, 0xA4, "no engine key"},
        {Key::EREOF, "EREOF", nullptr, 0x00, "PC-only virtual key; unmapped"},
        {Key::PLAY, "PLAY", nullptr, 0x00, "PC-only virtual key; unmapped"},
        {Key::ZOOM, "ZOOM", nullptr, 0x00, "PC-only virtual key; unmapped"},
        {Key::NONAME, "NONAME", nullptr, 0x00, "PC-only virtual key; unmapped"},
        {Key::PA1, "PA1", nullptr, 0x00, "PC-only virtual key; unmapped"},
        {Key::OEM_CLEAR, "OEM_CLEAR", nullptr, 0x00, "PC-only virtual key; unmapped"},
        };

        constexpr ModifierRow k_modifiers[] = {
            {ModifierKey::SHIFT, "LeftShift", "RightShift", 0xE1, 0xE5},
            {ModifierKey::CONTROL, "LeftControl", "RightControl", 0xE0, 0xE4},
            {ModifierKey::ALT, "LeftAlt", "RightAlt", 0xE2, 0xE6},
        };

        constexpr HidAlias k_aliases[] = {
            {0x58, Key::RETURN, "keypad Enter"},
            {0x90, Key::IME_HANGUL, "LANG1 (KR hangul/english); same UE4SS value as IME_KANA"},
        };

        // Dense index: UE4SS key value -> row (nullptr = not a bindable key).
        struct Index
        {
            std::array<const KeyRow*, 256> by_key{};
            Index()
            {
                for (const auto& row : k_rows) by_key[static_cast<uint8_t>(row.key)] = &row;
            }
        };
        const Index& index()
        {
            static const Index instance;
            return instance;
        }
    } // namespace

    std::span<const KeyRow> key_table() { return k_rows; }
    std::span<const ModifierRow> modifier_table() { return k_modifiers; }
    std::span<const HidAlias> hid_aliases() { return k_aliases; }

    const KeyRow* row_for(Key key) { return index().by_key[static_cast<uint8_t>(key)]; }

    std::optional<Key> key_for_fkey(std::string_view fkey)
    {
        for (const auto& row : k_rows)
            if (row.fkey && fkey == row.fkey) return row.key;
        return std::nullopt;
    }

    std::optional<Key> key_for_hid(uint16_t usage)
    {
        if (usage == 0) return std::nullopt;
        for (const auto& row : k_rows)
            if (row.hid == usage) return row.key;
        for (const auto& alias : k_aliases)
            if (alias.hid == usage) return alias.key;
        return std::nullopt;
    }
} // namespace modbridge::kbm
