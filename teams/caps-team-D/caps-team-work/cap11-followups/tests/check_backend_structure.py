"""Lexical structure check of the two engine-binding .inc files (Mac-only).

The .inc files need the PS5 platform profile, so they cannot be host-compiled; the target compile proves they
build, this check proves the follow-up rules are in the code:
  FU1  m_built guard: cleared first in build_surfaces, set right before its only `return true`, tested by prepare;
       a failed build is throttled (2 s), not retried every frame
  FU2  viewport_scale(): no refusal, no ERROR line, one warning guarded by m_scale_warned
  FU3  prepare re-checks the layout (layout_changed, k_recheck_frames), open forces a re-check, old surfaces retired
       with already-used functions only
  DPI  render target = rt_w x rt_h pixels (cap 4096), slot SetSize in units (m_w x m_h), pixel_scale() override
  FU5  NativeModsGameThreadFrame goes through Host::game_thread_pump (one gate), no per-pump game_thread_frame
  generic: engine function names used are a known set (Team D's list + GetViewportScale), no game names, no file
       I/O, no detour/kernel primitive, no new #include
Usage: check_backend_structure.py live|patched BACKEND.inc SERVICES.inc
  patched: every item must pass (exit 1 otherwise); live: informational (shows what the live tree lacks), exit 0.
Prints items_checked; exit 2 on zero items.
"""
import re
import sys
from pathlib import Path

mode, be_path, sv_path = sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3])
be = be_path.read_text()
sv = sv_path.read_text()
LIVE = Path("/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/Mod")

items, fails = 0, []


def check(cond, what):
    global items
    items += 1
    print(("  pass " if cond else ("  FAIL " if mode == "patched" else "  (live lacks) ")) + what)
    if not cond:
        fails.append(what)


def body(text, signature):
    i = text.find(signature)
    if i < 0:
        return None
    j = text.index("{", i)
    depth = 0
    for k in range(j, len(text)):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return text[j + 1:k]
    return None


def code(s):  # strip // comments
    return "\n".join(l.split("//")[0] for l in s.splitlines())


bs = body(be, "auto build_surfaces() -> bool")
st = code(body(be, "auto build_steps() -> bool") or "")
ca = code(body(be, "auto create_atlas(const std::vector<Run>& runs, int w, int h) -> ImTextureID override") or "")
pr = body(be, "auto prepare(int* w, int* h, bool* rebuilt) -> bool override")
vs = body(be, "auto viewport_scale() -> float")
lc = body(be, "auto layout_changed() -> bool")
rs = body(be, "void retire_surfaces()")
sv_fn = body(sv, "void NativeModsGameThreadFrame(int source) noexcept")
check(bs is not None and pr is not None, "build_surfaces and prepare found")
bs, pr = code(bs or ""), code(pr or "")

# FU1
check(re.search(r"\bbool m_built\{\};", be) is not None, "FU1 bool m_built{} declared")
first = [l.strip() for l in bs.splitlines() if l.strip()]
check(bool(first) and first[0] == "m_built = false;", "FU1 build_surfaces clears m_built first")
rets = [m.start() for m in re.finditer(r"return true;", bs)]
check(len(rets) == 1 and bs.rfind("m_built = true;", 0, rets[0] if rets else 0) > (rets[0] - 80 if rets else 0),
      "FU1 the only `return true` of build_surfaces is right after m_built = true")
check("m_built &&" in pr, "FU1 prepare tests m_built")
pc = bs.find("if (!valid(m_pc))")
thr = bs.find("m_next_pc_probe = now + std::chrono::milliseconds{2000};")
check(pc >= 0 and thr > pc and thr < bs.find("build_steps()"), "FU1 a part-way build is retried after 2 s, not every frame")
check("m_next_pc_probe = {};" in bs, "FU1 the throttle is cleared on success")
check(re.search(r"static constexpr int k_max_build_failures = 3;", be) is not None and "if (m_build_failures >= k_max_build_failures) return false;" in bs
      and bs.find("if (m_build_failures >= k_max_build_failures) return false;") < bs.find("build_steps()"),
      "FU1/review#1 at most 3 part-way failures in a row, then no more tries (fail closed)")
check("if (m_pc != m_failed_pc) m_build_failures = 0;" in bs, "FU1/review#1 a new player gets fresh tries")
check("m_build_failures > 0 && valid(m_target) && m_rt_w == rt_w && m_rt_h == rt_h" in st, "review#1 a retry reuses the render target it already made")

# FU2
check(vs is not None, "FU2 viewport_scale() exists")
vsc = code(vs or "")
check("refuse(" not in vsc and "frame_for(" not in vsc and "put(" not in vsc, "FU2 viewport_scale uses no refusal path (refuse/frame_for/put)")
check("MBM_LOG_ERROR" not in vsc and vsc.count("log(") == 1 and "if (!m_scale_warned)" in vsc, "FU2 one WARNING, once per session, never ERROR")
check("GetViewportScale" not in bs + st and "viewport_scale()" in st, "FU2 the build reads the scale only through viewport_scale()")

# FU3
check(lc is not None and "k_recheck_frames" in (lc or "") and "layout_changed()" in pr, "FU3 prepare re-checks the layout every k_recheck_frames")
sv_vis = code(body(be, "void set_visible(bool v) override") or "")
check("m_recheck_now = true" in sv_vis, "FU3 every open forces a re-check")
check("m_build_failures = 0" in sv_vis, "FU1/review#1 every open gets fresh build tries")
rsc = code(rs or "")
check(rs is not None and set(re.findall(r'"([A-Za-z]+)"', rsc)) <= {"SetBrushResourceObject", "ResourceObject", "SetVisibility", "InVisibility"},
      "FU3 retire_surfaces uses only functions the build already uses")
check(rsc.find('"SetVisibility"') >= 0 and rsc.find('"SetVisibility"') < rsc.find('"SetBrushResourceObject"') and rsc.count("hidden && valid(") == 2,
      "review#4 retire collapses first and clears the brushes only once hidden")

# DPI
check(re.search(r"static constexpr int k_max_rt_side = 4096;", be) is not None, "DPI render target capped at 4096 per side")
check("make_crt(rt_w, rt_h)" in st and "make_crt(m_w, m_h)" not in st, "DPI render target created in pixels (rt_w x rt_h)")
check('put_struct(g, "InSize", {{"X", m_w}, {"Y", m_h}})' in st, "DPI slot size stays in Slate units (m_w x m_h)")
check("static_cast<int64_t>(w) * h > int64_t{4096} * 4096) return 0;" in ca, "review#2 create_atlas fails closed above 4096x4096 texels")
check(re.search(r"auto pixel_scale\(\) -> float override \{ return m_px_scale; \}", be) is not None, "DPI pixel_scale() override")

# FU5
svc = code(sv_fn or "")
check(sv_fn is not None and "game_thread_pump(" in svc and "game_thread_frame(" not in svc and "static FrameGate" not in svc,
      "FU5 the C++ mod frame goes through Host::game_thread_pump (one gate), no per-pump game_thread_frame")

# generic
fn_names = set(re.findall(r'frame_for\([^,]+,\s*"([A-Za-z0-9_]+)"', be)) | set(re.findall(r'call_obj1\([^,]+,\s*"([A-Za-z0-9_]+)"', be)) | \
    set(re.findall(r'GetFunctionByNameInChain\(STR\("([A-Za-z0-9_]+)"\)\)', be))
known = {"CreateCanvasRenderTarget2D", "ClearRenderTarget2D", "BeginDrawCanvasToRenderTarget", "EndDrawCanvasToRenderTarget", "K2_DrawTriangle",
         "ImportBufferAsTexture2D", "GetViewportSize", "AddChildToCanvas", "SetPosition", "SetSize", "SetBrushResourceObject", "AddToViewport",
         "SetVisibility", "SetIgnoreMoveInput", "SetIgnoreLookInput", "GetViewportScale"}
print("  engine functions used:", " ".join(sorted(fn_names)))
check(fn_names <= known, "generic: every engine function is in the known list (Team D's + GetViewportScale)")
for bad in ("Townfall", "Stellar", "fopen", "ifstream", "ofstream", "std::filesystem", "mprotect", "Detour", "PLH::", "VirtualProtect", "kernel_", "sceKernel"):
    check(bad not in code(be) and bad not in code(sv), f"generic: no '{bad}'")
for name, text in (("NativeGuiEngineBackend.inc", be), ("NativeModEngineServices.inc", sv)):
    base = (LIVE / name).read_text()
    inc = lambda t: sorted(l.strip() for l in t.splitlines() if l.strip().startswith("#include"))
    check(inc(text) == inc(base), f"generic: {name} has the same #include lines as the live file")

print(f"backend_structure mode={mode} items_checked={items} failed={len(fails)}")
if items == 0:
    sys.exit(2)
sys.exit(1 if (fails and mode == "patched") else 0)
