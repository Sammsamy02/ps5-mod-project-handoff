"""Team I2 table gate (Mac-only, stdlib only, run with python3 -I).

Checks src/KeyboardMouseMap.cpp against independent inputs, read-only:
  1. the runtime's REAL Key enum (KeyDef.hpp) and spelling table (KeyDef.cpp):
     every bindable UE4SS key is covered exactly once (row or noted enum alias);
  2. HID column == SDL_scancode.h (public, "usage page 0x07"), the second
     independent source next to the USB-IF HID Usage Tables;
  3. engine key names are all in Epic's PUBLIC EKeys list (API docs page
     "EKeys", names only; no Epic source is read or copied).
Prints how many items it checked; exits non-zero on any mismatch or if it
checked nothing (project gate law).

usage: check_table.py --keydef-hpp P --keydef-cpp P --table P --sdl P
"""
import argparse
import re
import sys

# Epic public API docs, EKeys (keyboard + mouse members), fetched 2026-10-13.
PUBLIC_EKEYS = set("""
A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
Zero One Two Three Four Five Six Seven Eight Nine
F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11 F12
BackSpace CapsLock Delete Down End Enter Escape Home Insert Left NumLock PageDown PageUp Pause Right ScrollLock SpaceBar Tab Up
LeftAlt LeftCommand LeftControl LeftShift RightAlt RightCommand RightControl RightShift
Ampersand Apostrophe Asterix Backslash Caret Colon Comma Equals Exclamation Hyphen LeftBracket LeftParantheses Period Quote
RightBracket RightParantheses Section Semicolon Slash Tilde Underscore Dollar A_AccentGrave E_AccentAigu E_AccentGrave C_Cedille
NumPadZero NumPadOne NumPadTwo NumPadThree NumPadFour NumPadFive NumPadSix NumPadSeven NumPadEight NumPadNine
Add Subtract Multiply Divide Decimal
LeftMouseButton RightMouseButton MiddleMouseButton ThumbMouseButton ThumbMouseButton2 MouseScrollUp MouseScrollDown
""".split())

# UE4SS key name -> SDL_SCANCODE_<name> for every row that carries a HID usage.
SDL_NAME = {
    "BACKSPACE": "BACKSPACE", "TAB": "TAB", "CLEAR": "CLEAR", "RETURN": "RETURN", "PAUSE": "PAUSE",
    "CAPS_LOCK": "CAPSLOCK", "IME_KANA": "INTERNATIONAL2", "IME_HANJA": "LANG2", "ESCAPE": "ESCAPE",
    "IME_CONVERT": "INTERNATIONAL4", "IME_NONCONVERT": "INTERNATIONAL5", "SPACE": "SPACE",
    "PAGE_UP": "PAGEUP", "PAGE_DOWN": "PAGEDOWN", "END": "END", "HOME": "HOME", "LEFT_ARROW": "LEFT",
    "UP_ARROW": "UP", "RIGHT_ARROW": "RIGHT", "DOWN_ARROW": "DOWN", "SELECT": "SELECT", "EXECUTE": "EXECUTE",
    "PRINT_SCREEN": "PRINTSCREEN", "INS": "INSERT", "DEL": "DELETE", "HELP": "HELP",
    "LEFT_WIN": "LGUI", "RIGHT_WIN": "RGUI", "APPS": "APPLICATION",
    "MULTIPLY": "KP_MULTIPLY", "ADD": "KP_PLUS", "SEPARATOR": "SEPARATOR", "SUBTRACT": "KP_MINUS",
    "DECIMAL": "KP_PERIOD", "DIVIDE": "KP_DIVIDE", "NUM_LOCK": "NUMLOCKCLEAR", "SCROLL_LOCK": "SCROLLLOCK",
    "VOLUME_MUTE": "MUTE", "VOLUME_UP": "VOLUMEUP", "VOLUME_DOWN": "VOLUMEDOWN",
    "OEM_ONE": "SEMICOLON", "OEM_PLUS": "EQUALS", "OEM_COMMA": "COMMA", "OEM_MINUS": "MINUS",
    "OEM_PERIOD": "PERIOD", "OEM_TWO": "SLASH", "OEM_THREE": "GRAVE", "OEM_FOUR": "LEFTBRACKET",
    "OEM_FIVE": "BACKSLASH", "OEM_SIX": "RIGHTBRACKET", "OEM_SEVEN": "APOSTROPHE", "OEM_102": "NONUSBACKSLASH",
    "CRSEL": "CRSEL", "EXSEL": "EXSEL",
}
for i, n in enumerate(["ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"]):
    SDL_NAME[n] = str(i)
    SDL_NAME["NUM_" + n] = "KP_" + str(i)
for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
    SDL_NAME[c] = c
for i in range(1, 25):
    SDL_NAME["F%d" % i] = "F%d" % i
SDL_MODIFIER = {"LeftControl": "LCTRL", "LeftShift": "LSHIFT", "LeftAlt": "LALT",
                "RightControl": "RCTRL", "RightShift": "RSHIFT", "RightAlt": "RALT"}
SDL_ALIAS = {0x58: "KP_ENTER", 0x90: "LANG1"}


def enum_members(hpp):
    body = hpp[hpp.index("enum Key : uint8_t"):hpp.index("RC_INPUT_API auto string_to_key")]
    out = []
    for line in body.splitlines():
        s = line.strip()
        if s.startswith("//"):
            continue
        m = re.match(r"([A-Z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", s)
        if m and m.group(1) != "RESERVED_START_OF_ENUM":
            out.append((m.group(1), int(m.group(2), 0)))
    return out


def main():
    ap = argparse.ArgumentParser()
    for a in ("--keydef-hpp", "--keydef-cpp", "--table", "--sdl"):
        ap.add_argument(a, required=True)
    args = ap.parse_args()
    hpp = open(args.keydef_hpp, encoding="utf-8").read()
    cpp = open(args.keydef_cpp, encoding="utf-8").read()
    table = open(args.table, encoding="utf-8").read()
    sdl = open(args.sdl, encoding="utf-8").read()

    sdl_codes = {m.group(1): int(m.group(2)) for m in re.finditer(r"SDL_SCANCODE_([A-Z0-9_]+)\s*=\s*(\d+)", sdl)}
    spellings = set(re.findall(r'\{STR\("([a-z0-9_]+)"\),\s*Key::', cpp)) - {"reserved_start_of_enum"}
    rows = [(m.group(1), m.group(2), m.group(3), int(m.group(4), 16), m.group(5))
            for m in re.finditer(r'\{Key::([A-Z0-9_]+), "([A-Z0-9_]+)", (nullptr|"[^"]*"), 0x([0-9A-F]+), (nullptr|"(?:[^"\\]|\\.)*")\}', table)]
    mods = re.findall(r'\{ModifierKey::[A-Z]+, "([A-Za-z]+)", "([A-Za-z]+)", 0x([0-9A-F]+), 0x([0-9A-F]+)\}', table)
    aliases = [(int(h, 16), k) for h, k in re.findall(r'\{0x([0-9A-F]+), Key::([A-Z0-9_]+), "', table)]

    errors, checked = [], 0
    members = enum_members(hpp)
    by_value = {}
    for name, value in members:
        by_value.setdefault(value, []).append(name)
    row_names = {r[0] for r in rows}
    alias_names = set()
    for r in rows:
        m = re.search(r"enum aliases: ([A-Z0-9_, ]+)", r[4])
        if m:
            alias_names |= {x.strip() for x in m.group(1).split(",")}
    # 1. enum coverage, each value once
    for value, names in by_value.items():
        checked += 1
        primary = [n for n in names if n in row_names]
        if len(primary) != 1:
            errors.append("enum value 0x%02X %r: %d rows" % (value, names, len(primary)))
        for n in names:
            if n not in row_names and n not in alias_names:
                errors.append("enum member %s not covered" % n)
    for r in rows:
        checked += 1
        if r[0] != r[1]:
            errors.append("row %s spelled %s" % (r[0], r[1]))
        if r[1].lower() not in spellings:
            errors.append("row %s has no string_to_key spelling" % r[1])
    for sp in spellings:
        checked += 1
        if sp.upper() not in row_names and sp.upper() not in alias_names:
            errors.append("string_to_key spelling %s not covered" % sp)
    # 2. HID column vs SDL (second public source)
    hid_rows = 0
    for name, _, fkey, hid, _ in rows:
        if not hid:
            if name in SDL_NAME:
                errors.append("%s has an SDL usage but no HID value" % name)
            continue
        hid_rows += 1
        checked += 1
        sdl_name = SDL_NAME.get(name)
        if sdl_name is None or sdl_codes.get(sdl_name) != hid:
            errors.append("%s HID 0x%02X != SDL_SCANCODE_%s=%s" % (name, hid, sdl_name, sdl_codes.get(sdl_name)))
    for left, right, hl, hr in mods:
        for fk, h in ((left, hl), (right, hr)):
            checked += 1
            if sdl_codes.get(SDL_MODIFIER.get(fk, "?")) != int(h, 16):
                errors.append("modifier %s HID 0x%s != SDL" % (fk, h))
    for h, k in aliases:
        checked += 1
        if sdl_codes.get(SDL_ALIAS.get(h, "?")) != h:
            errors.append("alias 0x%02X (%s) != SDL" % (h, k))
    # 3. engine names in the public EKeys list
    engine_rows = 0
    for name, _, fkey, _, _ in rows:
        if fkey == "nullptr":
            continue
        engine_rows += 1
        checked += 1
        if fkey.strip('"') not in PUBLIC_EKEYS:
            errors.append("%s engine name %s not in public EKeys list" % (name, fkey))
    for left, right, _, _ in mods:
        checked += 2
        if left not in PUBLIC_EKEYS or right not in PUBLIC_EKEYS:
            errors.append("modifier names %s/%s not public" % (left, right))

    print("check_table: enum_values=%d rows=%d spellings=%d hid_rows=%d engine_rows=%d modifiers=%d aliases=%d checked=%d errors=%d"
          % (len(by_value), len(rows), len(spellings), hid_rows, engine_rows, len(mods), len(aliases), checked, len(errors)))
    for e in errors:
        print("  ERROR " + e)
    if checked == 0 or not rows or not members or not sdl_codes:
        print("check_table: FAIL (nothing checked)")
        return 3
    print("check_table: " + ("PASS" if not errors else "FAIL"))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
