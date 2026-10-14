#!/usr/bin/env python3
"""build_bundle.py — build the Item Spawner activation bundle (host only).

Produces, under <outdir>/:
  bundle.json      the logical key -> bytes map (sha256 + size + role + source),
                   layout keys recorded as blocked-on-layout-data (no bytes)
  stage-root/      a modforge mod root: loadlist.txt + the ue4ss/ tree, exactly
                   what mfa_activate reads (layout keys omitted -> strict mode)
  stage-root-full/ same root WITH placeholder layout files (--layout-placeholders),
                   used ONLY to exercise the vendored load path; the placeholder
                   content is a comment and is NOT layout facts

The mod's own files are read, never modified: main.lua/enabled.txt are copied
byte-for-byte from the retained intake after sha256 verification, and
UEHelpers.lua from the retained distro asset likewise. Nothing here executes
mod Lua or claims engine execution.

Usage: build_bundle.py [--outdir DIR] [--layout-placeholders]
"""
import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path

WS = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
INTAKE = WS / ".superpowers/native-observations/item-spawner-real-mod-intake-1"
UEHELPERS = (WS / ".superpowers/native-dependency-spike/ue4ss-unreal-native-build-1"
             "/source-dependencies/ue4ss/assets/Mods/shared/UEHelpers/UEHelpers.lua")

MAIN_LUA_SHA = "f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658"
ENABLED_SHA = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
UEHELPERS_SHA = "5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7"

TITLE = "PPSA33286"
ENGINE = "unreal"
FRAMEWORK = "ue4ss-ps5"
MOD_ID, MOD_VERSION = "TownfallItemSpawner", "1.0.0"

# [General]/[Debug] options the mod's path depends on (BUNDLE-SPEC 1.1).
# Overrides section MUST be empty: native activation throws on either
# override (SettingsManager.cpp:112-113) and on +ModsFolderPaths rows.
SETTINGS_INI = """\
[Overrides]
ModsFolderPath=
ControllingModsTxt=

[General]
bUseUObjectArrayCache=False
SecondsToScanBeforeGivingUp=120
DefaultExecuteInGameThreadMethod=EngineTick
DefaultFNameToStringMethod=Scan

[Debug]
ConsoleEnabled=1
"""

# NOT layout facts: no identity header, no offsets. Exists only so the host
# load-path test can carry the two REQUIRED keys through mfa_activate.
LAYOUT_PLACEHOLDER = ("# blocked-on-layout-data: placeholder for host load-path test only\n"
                      "# NOT validated layout facts; StageNativeLayoutTexts rejects this\n")

FILES = [
    # (key, role, source, pinned_sha)
    ("ue4ss/UE4SS-settings.ini", "config", "generated:settings", None),
    ("ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua", "lua_mod",
     str(INTAKE / "member-00000000.bin"), MAIN_LUA_SHA),
    ("ue4ss/Mods/TownfallItemSpawner/enabled.txt", "lua_mod",
     str(INTAKE / "member-00000001.bin"), ENABLED_SHA),
    ("ue4ss/Mods/shared/UEHelpers/UEHelpers.lua", "lua_mod",
     str(UEHELPERS), UEHELPERS_SHA),
]
LAYOUT_KEYS = ["ue4ss/MemberVariableLayout.ini", "ue4ss/VTableLayout.ini"]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_pinned(path: Path, want: str) -> bytes:
    data = path.read_bytes()
    got = sha256_bytes(data)
    if got != want:
        sys.exit(f"FATAL pin mismatch for {path}: {got} != {want}")
    return data


def build_root(root: Path, payloads: dict, with_layout: bool) -> None:
    if root.exists():
        shutil.rmtree(root)  # our own output dir only
    for key, data in payloads.items():
        if key in LAYOUT_KEYS and not with_layout:
            continue
        target = root / key
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    # loadlist.txt written LAST (FORMATS.md PS5 bundle/installer ordering rule;
    # modinst.c treats its presence as install-commit truth)
    lines = ["modforge-loadlist\t1", f"title\t{TITLE}", f"engine\t{ENGINE}",
             f"framework\t{FRAMEWORK}", f"mod\t{MOD_ID}\t{MOD_VERSION}"]
    for key, role, _src, _pin in FILES:
        lines.append(f"file\t{role}\t{key}\t{len(payloads[key])}")
    if with_layout:
        for key in LAYOUT_KEYS:
            lines.append(f"file\tconfig\t{key}\t{len(payloads[key])}")
    (root / "loadlist.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--outdir", default=str(Path(__file__).resolve().parent / "build"))
    ap.add_argument("--layout-placeholders", action="store_true",
                    help="add the two layout keys with non-fact placeholder bytes "
                         "(host load-path test only; NOT layout data)")
    args = ap.parse_args()
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    payloads: dict[str, bytes] = {key: SETTINGS_INI.encode("utf-8")
                                  if src == "generated:settings" else load_pinned(Path(src), pin)
                                  for key, _role, src, pin in FILES}
    if args.layout_placeholders:
        for key in LAYOUT_KEYS:
            payloads[key] = LAYOUT_PLACEHOLDER.encode("utf-8")

    build_root(outdir / "stage-root", payloads, with_layout=False)
    if args.layout_placeholders:
        build_root(outdir / "stage-root-full", payloads, with_layout=True)

    bundle = {
        "schema_version": 1,
        "title_id": TITLE, "engine": ENGINE, "framework": FRAMEWORK,
        "mod": {"id": MOD_ID, "version": MOD_VERSION},
        "expected_title_note": "caller-supplied game/profile fact, must equal mfa_title "
                               "(activated_mod_source.cpp:24-26)",
        "activation_root": None,
        "activation_root_note": "Lua-only activation: no native_plugin entries, root not required "
                                "(activated_mod_source.cpp:62-67)",
        "files": [
            {"key": key, "role": role,
             "source": ("generated (BUNDLE-SPEC 1.1 options)" if src == "generated:settings"
                        else src),
             "size": len(payloads[key]), "sha256": sha256_bytes(payloads[key]),
             "immutable_mod_file": src != "generated:settings"}
            for key, role, src, _pin in FILES
        ],
        "blocked_on_layout_data": [
            {"key": key, "role": "config", "required_by": "UE4SSProgram.cpp:899-904",
             "consumer": "StageNativeLayoutTexts (UE4SSProgram.cpp:905; NativeLayoutBinding.cpp:136-138)",
             "content": None,
             "note": "validated Townfall layout facts must come from worker-2; "
                     "this worker does not synthesize layout content"}
            for key in LAYOUT_KEYS
        ],
        "staged_roots": {
            "strict": "stage-root (no layout keys; runtime would hard-stop at "
                      "load_unreal_offsets_from_file - see LAYOUT-GATE.md)",
            "load_path_test": ("stage-root-full (layout keys carry placeholder bytes that are "
                               "NOT layout facts; for mfa load/validate host test only)"
                               if args.layout_placeholders else "not built"),
        },
    }
    (outdir / "bundle.json").write_text(json.dumps(bundle, indent=2) + "\n", encoding="utf-8")
    print(f"bundle.json: {len(FILES)} payload files, "
          f"{len(LAYOUT_KEYS)} blocked-on-layout-data keys")
    print(f"stage-root: {outdir / 'stage-root'}")
    if args.layout_placeholders:
        print(f"stage-root-full (placeholders, NOT layout facts): {outdir / 'stage-root-full'}")


if __name__ == "__main__":
    main()
