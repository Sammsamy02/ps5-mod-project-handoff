#!/usr/bin/env python3
"""build_bundle.py — worker-itemspawner-bundle, 2026-10-04.

Builds the ACTUAL activation bundle for the unchanged Item Spawner mod
(BUNDLE-SPEC section 2, FIRST-RUN-CONFIG minimal instance) under
<bundle>/ inside THIS worker's directory:

  bundle/loadlist.txt                                  modforge mod root list
  bundle/ue4ss/UE4SS-settings.ini                      generated minimal config
  bundle/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua   intake member, pinned
  bundle/ue4ss/Mods/TownfallItemSpawner/enabled.txt        intake member, pinned, 0 B
  bundle/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua     distro asset, pinned

  bundle.json                                          manifest of the key map

The two REQUIRED layout keys (ue4ss/MemberVariableLayout.ini,
ue4ss/VTableLayout.ini) are deliberately ABSENT: no synthetic bytes, no
placeholder bytes in the actual bundle. See AWAITING-STAGED-PAIR.md; they are
appended only from worker-staged-pair-emission's real output.

The mod's files are read, never modified, and never executed. Byte pins are
verified before copying; a mismatch aborts the build.
"""
import hashlib
import json
import shutil
import sys
from pathlib import Path

WS = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
HERE = Path(__file__).resolve().parent
OUT = HERE / "bundle"
INTAKE = WS / ".superpowers/native-observations/item-spawner-real-mod-intake-1"
UEHELPERS_SRC = (WS / ".superpowers/native-dependency-spike/ue4ss-unreal-native-build-1"
                 "/source-dependencies/ue4ss/assets/Mods/shared/UEHelpers/UEHelpers.lua")

MAIN_LUA_SHA = "f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658"
ENABLED_SHA = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
UEHELPERS_SHA = "5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7"
# Cross-worker byte identity: the settings bytes worker-activation-bundle's
# validated build produced (build/bundle.json, 221 B). Same options, same bytes.
SETTINGS_EXPECTED_SHA = "f6d06cada43d43d0674c2986152b72a4dddecf29331acb9cd85e47172889a37f"

TITLE, ENGINE, FRAMEWORK = "PPSA33286", "unreal", "ue4ss-ps5"
MOD_ID, MOD_VERSION = "TownfallItemSpawner", "1.0.0"

# FIRST-RUN-CONFIG minimal options; every option is parsed by SettingsManager.cpp
# (BUNDLE-SPEC 1.1). [Overrides] stays EMPTY (values are empty, no override rows):
# native activation throws on either override (SettingsManager.cpp:112-113).
SETTINGS_INI = (
    "[Overrides]\n"
    "ModsFolderPath=\n"
    "ControllingModsTxt=\n"
    "\n"
    "[General]\n"
    "bUseUObjectArrayCache=False\n"
    "SecondsToScanBeforeGivingUp=120\n"
    "DefaultExecuteInGameThreadMethod=EngineTick\n"
    "DefaultFNameToStringMethod=Scan\n"
    "\n"
    "[Debug]\n"
    "ConsoleEnabled=1\n"
)

# (key, role, source path or None for generated, pinned sha or None)
FILES = [
    ("ue4ss/UE4SS-settings.ini", "config", None, SETTINGS_EXPECTED_SHA),
    ("ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua", "lua_mod",
     INTAKE / "member-00000000.bin", MAIN_LUA_SHA),
    ("ue4ss/Mods/TownfallItemSpawner/enabled.txt", "lua_mod",
     INTAKE / "member-00000001.bin", ENABLED_SHA),
    ("ue4ss/Mods/shared/UEHelpers/UEHelpers.lua", "lua_mod",
     UEHELPERS_SRC, UEHELPERS_SHA),
]
AWAITING_KEYS = ["ue4ss/MemberVariableLayout.ini", "ue4ss/VTableLayout.ini"]


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def pinned(path: Path, want: str, label: str) -> bytes:
    data = path.read_bytes()
    got = sha(data)
    if got != want:
        sys.exit(f"FATAL: {label} pin mismatch: {got} != {want}")
    return data


def main() -> None:
    payloads = {"ue4ss/UE4SS-settings.ini": SETTINGS_INI.encode("utf-8")}
    for key, role, src, pin in FILES:
        if src is not None:
            payloads[key] = pinned(src, pin, key)

    if OUT.exists():
        shutil.rmtree(OUT)
    for key, data in payloads.items():
        target = OUT / key
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    # loadlist.txt written LAST (FORMATS.md ordering; modinst treats its
    # presence as install-commit truth).
    lines = ["modforge-loadlist\t1", f"title\t{TITLE}", f"engine\t{ENGINE}",
             f"framework\t{FRAMEWORK}", f"mod\t{MOD_ID}\t{MOD_VERSION}"]
    for key, role, _src, _pin in FILES:
        lines.append(f"file\t{role}\t{key}\t{len(payloads[key])}")
    (OUT / "loadlist.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    manifest = {
        "worker": "worker-itemspawner-bundle",
        "schema_version": 1,
        "title_id": TITLE,
        "engine": ENGINE,
        "framework": FRAMEWORK,
        "mod": {"id": MOD_ID, "version": MOD_VERSION},
        "activation_root": None,
        "activation_root_reason":
            "Lua-only activation: no native_plugin entries, so the root is "
            "not required (activated_mod_source.cpp:62-67)",
        "files": [
            {
                "key": key,
                "role": role,
                "size": len(payloads[key]),
                "sha256": sha(payloads[key]),
                "source": ("generated: FIRST-RUN-CONFIG minimal options "
                           "(BUNDLE-SPEC 1.1)" if src is None else str(src)),
                "immutable_mod_bytes": src is not None,
            }
            for key, role, src, _pin in FILES
        ],
        "layout_keys_awaiting_staged_pair": [
            {"key": k, "role": "config", "bytes": None,
             "status": "awaiting-staged-pair",
             "source_when_ready":
                 "outputs/2026-10-04-zcode-team/worker-staged-pair-emission/emission-live/",
             "runtime_requirement":
                 "UE4SSProgram.cpp:899-904 load_unreal_offsets_from_file() throws "
                 "'Native activation requires both member and vtable layout files' "
                 "when either key is absent"}
            for k in AWAITING_KEYS
        ],
        "notes": [
            "No synthetic or placeholder layout bytes anywhere in this bundle.",
            "Mod Lua is never executed by this builder; host test loads bytes only.",
        ],
    }
    (HERE / "bundle.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("bundle built:", OUT)
    for key, _role, _src, _pin in FILES:
        p = OUT / key
        print(f"  {len(payloads[key]):>6} B  {sha(payloads[key])}  {key}")


if __name__ == "__main__":
    main()
