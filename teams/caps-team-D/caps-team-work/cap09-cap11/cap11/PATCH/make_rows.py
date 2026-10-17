"""Regenerate CHANGES-ROWS.json (cap11, ON TOP of cap09) from current sha256.
Run: python3 -I cap11/PATCH/make_rows.py   (Mac-only, writes only this folder)"""
import hashlib, json
from pathlib import Path
HERE = Path(__file__).resolve().parent
W = "/WORKSPACE/.codex/worktrees/translation-core/what"
T = W + "/outputs/2026-10-08-l26-runtime/typescan-r4"
IM = W + "/.superpowers/native-dependency-spike/ue4ss-real-lua-link-route-1/source-dependencies/imgui"
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
staged = [("new/include/Platform/mbm_imconfig.h", "include/Platform/mbm_imconfig.h"),
          ("new/include/Platform/mbm_imgui_math.h", "include/Platform/mbm_imgui_math.h"),
          ("new/include/Platform/mbm_imgui_fingerprint.h", "include/Platform/mbm_imgui_fingerprint.h"),  # [F7]
          ("new/include/Platform/NativeGuiHost.hpp", "include/Platform/NativeGuiHost.hpp"),
          ("new/include/Platform/ImGuiCanvasBatcher.hpp", "include/Platform/ImGuiCanvasBatcher.hpp"),
          ("new/include/Platform/ImGuiAtlasRuns.hpp", "include/Platform/ImGuiAtlasRuns.hpp"),
          ("new/include/Platform/StoredPng.hpp", "include/Platform/StoredPng.hpp"),
          ("candidate/src/Mod/NativeGuiEngineBackend.inc", "candidate/src/Mod/NativeGuiEngineBackend.inc"),
          ("candidate/src/Mod/NativeGuiImGuiUnity.inc", "candidate/src/Mod/NativeGuiImGuiUnity.inc")]
imgui = ["imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_tables.cpp", "imgui_internal.h", "imstb_rectpack.h", "imstb_textedit.h", "imstb_truetype.h"]
rows = {
    "note": "Apply AFTER cap09's rows. Replaces cap09's CppMod.cpp candidate with the cap11 version and adds the GUI files + pinned Dear ImGui 1.92.1 sources (copied, sha-verified; imgui.h/imconfig.h stay where the unit already includes them from).",
    "files_rows_replace": [{"candidate": T + "/candidate/src/Mod/CppMod.cpp", "candidate_sha256": sha(HERE / "candidate/src/Mod/CppMod.cpp"), "from": "cap11/PATCH/candidate/src/Mod/CppMod.cpp"}],
    "added_files": [{"path": T + "/" + d, "sha256": sha(HERE / s), "from": "cap11/PATCH/" + s} for s, d in staged] +
                   [{"path": T + "/include/" + f, "sha256": sha(IM + "/" + f), "from": IM + "/" + f} for f in imgui],
    "imgui_header_already_in_unit": {"imgui.h": sha(IM + "/imgui.h"), "imconfig.h": sha(IM + "/imconfig.h")},
    "scope_cap11": "[CAP11] native GUI host: one shared Dear ImGui 1.92.1 context (host allocator, error recovery, no file I/O, math on already-imported C functions only), mod tabs via mbm_gui_v1, frames drawn through reflected Canvas/UMG functions into a canvas render target shown by one UMG image; pad keys via register_keydown_event; gameplay input blocked only once the player controller is resolved and undone only if applied (F1); atlas and block redone after a surface rebuild / map change (F2); [ship fixes] the GUI renders at most once per engine frame (F5, FrameGate in cap09), the mod refuses UE4SS_ENABLE_IMGUI unless sizeof(ImGuiContext) and the ImGui config fingerprint match (F7). No new import (target_compile_check of the full #11 composition vs R11 runtime-l26-A.o), no detour, no address.",
}
(HERE / "CHANGES-ROWS.json").write_text(json.dumps(rows, indent=1) + "\n")
print("rows written:", len(rows["added_files"]), "added_files")
