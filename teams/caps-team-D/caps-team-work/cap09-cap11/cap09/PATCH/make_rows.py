"""Regenerate CHANGES-ROWS.json (cap09) from the staged files' current sha256.
Run: python3 -I cap09/PATCH/make_rows.py   (Mac-only, writes only this folder)"""
import hashlib, json
from pathlib import Path
HERE = Path(__file__).resolve().parent
W = "/WORKSPACE/.codex/worktrees/translation-core/what"
T = W + "/outputs/2026-10-08-l26-runtime/typescan-r4"
PORT_CPPMOD = W + "/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/Mod/CppMod.cpp"
PORT_NCMP = W + "/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeCppModPath.hpp"
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
rows = {
    "note": "Rows the integrator adds to typescan-r4/CHANGES.json for #9 (APPLY.md). 'path' = where the staged file is copied; sha256 must match after copying.",
    "files_rows": [{"original": PORT_CPPMOD, "copied_original": T + "/original/src/Mod/CppMod.cpp", "candidate": T + "/candidate/src/Mod/CppMod.cpp",
                    "original_sha256": sha(PORT_CPPMOD), "copied_original_sha256": sha(PORT_CPPMOD),
                    "candidate_sha256": sha(HERE / "candidate/src/Mod/CppMod.cpp"), "from": "cap09/PATCH/candidate/src/Mod/CppMod.cpp"},
                   # [F1] port header the patch makes reachable: error_code overload only. The build's
                   # header overlay maps the port path to the candidate copy (same as the other port-header rows).
                   {"original": PORT_NCMP, "copied_original": T + "/original/include/Platform/NativeCppModPath.hpp",
                    "candidate": T + "/candidate/include/Platform/NativeCppModPath.hpp",
                    "original_sha256": sha(PORT_NCMP), "copied_original_sha256": sha(PORT_NCMP),
                    "candidate_sha256": sha(HERE / "candidate/include/Platform/NativeCppModPath.hpp"),
                    "from": "cap09/PATCH/candidate/include/Platform/NativeCppModPath.hpp",
                    "apply": "python3 -I cap09/PATCH/apply_anchored.py --apply-to " + T + "   (writes candidate + original copies; anchors exact-once)"}],
    "added_files": [{"path": T + "/" + dst, "sha256": sha(HERE / src), "from": "cap09/PATCH/" + src} for src, dst in [
        ("new/include/Mod/NativeModAbi.h", "include/Mod/NativeModAbi.h"),
        ("new/include/Platform/HookAdmission.hpp", "include/Platform/HookAdmission.hpp"),
        ("new/include/Platform/NativeModHostCore.hpp", "include/Platform/NativeModHostCore.hpp"),
        ("new/include/Platform/NativeModLoaderPolicy.hpp", "include/Platform/NativeModLoaderPolicy.hpp"),
        ("new/include/Platform/NativeCppModInventory.hpp", "include/Platform/NativeCppModInventory.hpp"),
        ("candidate/src/Mod/NativeModEngineServices.inc", "candidate/src/Mod/NativeModEngineServices.inc")]],
    # [fixer] the two LIVE candidate files are patched by exact-once anchors
    # (apply_anchored.py --apply-to T); refresh their existing CHANGES rows.
    "files_rows_refresh": [{"candidate": T + "/candidate/src/" + n, "base_sha256_live": sha(T + "/candidate/src/" + n),
                            "candidate_sha256": sha(HERE / "candidate/src" / n), "from": "cap09/PATCH/candidate/src/" + n,
                            "diff": "cap09/PATCH/diffs/" + n + ".diff", "apply": "python3 -I cap09/PATCH/apply_anchored.py --apply-to " + T}
                           for n in ("UE4SSProgram.cpp", "LuaMod.cpp")],
    "luamod_warning": "LuaMod.cpp on disk (96e8ff4d) already differs from its R11 CHANGES row (95591da5): another session edits it. apply_anchored.py re-anchors the 3 hunks on whatever base is live (exact-once anchors) and reports DRIFT.",
    "scope_cap09": "[CAP09] native C++ mod ABI v1 (mbm_mod_entry_v1): CppMod.cpp (Prospero) admits only ABI-v1 modules into the engine-free NativeModHostCore (validation, fault isolation, revoke->drain->destroy, hook admission); runtime binding NativeModEngineServices.inc; LuaMod.cpp calls RC::Platform::NativeModsGameThreadFrame from the R3 marshal drain (0) and the VMT Tick drain (1); UE4SSProgram.cpp (Prospero, activation-snapshot branch) creates a CppMod for every snapshot-enabled mod with a dlls/ folder (NativeCppModInventory.hpp) and start_mods<CppMod> starts them in startup order, each contained. [ship fixes] F1 NativeCppModPath.hpp (port header row) uses only the error_code is_regular_file; F2 one try/catch per C++ mod, start-order row written before the module is kept; F3 hot reload refused loudly while native C++ mods are loaded, stopped host records pruned; F4 revoked/unregistered registrations erased; F5 per-frame service (GUI) at most once per engine frame (FrameGate); F6 host services refuse a mod that is not starting/running. No new import (target_compile_check vs R11 runtime-l26-A.o), no detour, no address.",
}
(HERE / "CHANGES-ROWS.json").write_text(json.dumps(rows, indent=1) + "\n")
print("rows written:", len(rows["files_rows"]), "files_rows,", len(rows["added_files"]), "added_files")
