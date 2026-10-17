"""cap09 anchored hunks for the two LIVE candidate-tree files (UE4SSProgram.cpp, LuaMod.cpp).

Every hunk is (anchor, replacement). The anchor must occur EXACTLY ONCE in the base
file, otherwise nothing is written and the tool exits 1. The base sha256 the hunks
were staged against is recorded; a different base is reported (DRIFT) but still
patched only if every anchor is still exact-once.

Modes (Mac-only; the tool never writes into the candidate tree unless --apply-to is
given, which is the integrator's step):
  python3 -I apply_anchored.py --stage          # read live candidate, write PATCH/candidate/src/<file>
  python3 -I apply_anchored.py --check          # read live candidate, verify anchors + staged == regenerated
  python3 -I apply_anchored.py --apply-to <T>   # integrator: patch <T>/candidate/src/<file> in place
  python3 -I apply_anchored.py --base-dir <D> --check   # same, base files taken from <D>/<file> (tests)
Prints items_checked; exit 0 = all anchors exact-once (and staged == regenerated for
--check), 1 = any failure, 2 = zero items.
"""
import argparse
import hashlib
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
T = Path("/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4")

UE4SS_H1_ANCHOR = "#include <Mod/CppMod.hpp>\n"
UE4SS_H1_REPL = """#include <Mod/CppMod.hpp>
#ifdef __PROSPERO__
// [CAP09 B1] native C++ mods under the activation snapshot (engine-free planner).
#include <Platform/NativeCppModInventory.hpp>
#endif
"""

UE4SS_H2_ANCHOR = """    auto UE4SSProgram::setup_mods() -> void
    {
        ProfilerScope();
"""
UE4SS_H2_REPL = """#ifdef __PROSPERO__
    // [CAP09 B1] Startup order of the native C++ mods created by setup_mods
    // under the activation snapshot (one program per process). Narrow ASCII
    // names, widened with widen_ascii_native_key at every lookup.
    static auto native_cpp_start_order() -> std::vector<std::string>&
    {
        static std::vector<std::string> order;
        return order;
    }

    // [CAP09 B1] Start the snapshot-enabled C++ mods in startup order. One mod
    // that refuses or throws is logged and skipped; it never stops the others
    // or the Lua mods (an escaping exception would abort the whole runtime).
    static auto start_native_cpp_mods_in_order() -> void
    {
        for (const auto& name : native_cpp_start_order())
        {
            const auto wide_name = widen_ascii_native_key(name);
            auto* mod = UE4SSProgram::find_mod_by_name<CppMod>(wide_name, UE4SSProgram::IsInstalled::Yes);
            if (!mod)
            {
                Output::send<LogLevel::Warning>(STR("[NMOD] {}: not started (module was not loaded, see the line above)\\n"), wide_name);
                continue;
            }
            if (mod->is_started()) continue;
            try
            {
                mod->start_mod();
                Output::send(STR("[NMOD] {}: start returned started={}\\n"), wide_name, mod->is_started() ? STR("yes") : STR("no"));
            }
            catch (const std::exception& error)
            {
                Output::send<LogLevel::Error>(STR("[NMOD] {}: start refused: {}\\n"), wide_name, widen_ascii_native_key(std::string{error.what()}));
            }
            catch (...)
            {
                Output::send<LogLevel::Error>(STR("[NMOD] {}: start refused: unknown exception\\n"), wide_name);
            }
        }
    }
#endif

    auto UE4SSProgram::setup_mods() -> void
    {
        ProfilerScope();
"""

UE4SS_H3_ANCHOR = """                if (!find_mod_by_name<LuaMod>(name))
                    m_mods.emplace_back(std::make_unique<LuaMod>(*this, std::move(name), widen_ascii(mod.path)));
            }
            return;
"""
UE4SS_H3_REPL = """                if (!find_mod_by_name<LuaMod>(name))
                    m_mods.emplace_back(std::make_unique<LuaMod>(*this, std::move(name), widen_ascii(mod.path)));
            }
            // [CAP09 B1] Native C++ mods. Enabled = the snapshot (mods.txt
            // "Name : 1" or the mod's enabled.txt, as upstream UE4SS); the PRX
            // comes from <mods directory>/<Name>/dlls/ on disk. This runs inside
            // start_native_mods(), i.e. during the runtime's module_start, the
            // only phase in which /data is known to be reachable (backend
            // comment; launch 14 shows /data creation failing later). The
            // CppMod constructor loads the PRX (sceKernelLoadStartModule).
            // Fail closed and contained: a planner error skips C++ mods only;
            // an error in one mod skips that mod only [CAP09 F2].
            native_cpp_start_order().clear();
            try
            {
                const auto& source = *m_native_context.mod_source;
                const std::filesystem::path mods_dir = m_mods_directories.empty() ? std::filesystem::path{} : m_mods_directories.front();
                std::optional<std::string_view> mods_txt;
                const auto mods_txt_file = source.read("ue4ss/Mods/mods.txt");
                if (mods_txt_file) mods_txt = mods_txt_file->bytes;
                const auto inventory = Platform::NativeMods::discover_cpp_mods(mods_dir, mods_txt, [&source](std::string_view mod_name) {
                    return source.read("ue4ss/Mods/" + std::string(mod_name) + "/enabled.txt").has_value();
                });
                for (const auto& note : inventory.notes)
                    Output::send(STR("[NMOD] {}\\n"), widen_ascii(note));
                // [CAP09 F2] One try/catch PER candidate: a refusal or exception
                // (bad_alloc in a log line, a throwing constructor) skips that
                // mod only, never the ones after it. The start-order row is
                // written BEFORE the module is created and kept, so a mod that is
                // in m_mods is always in the start order (no loaded-but-never-
                // started mod); a row whose module was not kept is reported by
                // the start step ("not started ... see the line above").
                for (const auto& candidate : inventory.start_order)
                {
                    try
                    {
                        auto name = widen_ascii(candidate.name);
                        if (find_mod_by_name<CppMod>(name)) continue;
                        auto mod_dir = widen_ascii(candidate.mod_dir.string());
                        native_cpp_start_order().push_back(candidate.name);
                        Output::send(STR("[NMOD] loading C++ mod {} from {}/dlls (if the log stops here, the module load hung)\\n"), name, mod_dir);
                        auto mod = std::make_unique<CppMod>(*this, std::move(name), std::move(mod_dir));
                        const bool installable = mod->is_installable();
                        m_mods.emplace_back(std::move(mod));
                        Output::send(STR("[NMOD] load step returned for {} installable={}\\n"), widen_ascii(candidate.name),
                                     installable ? STR("yes") : STR("no"));
                    }
                    catch (const std::exception& error)
                    {
                        Output::send<LogLevel::Error>(STR("[NMOD] {}: refused: C++ mod setup failed: {}\\n"), widen_ascii(candidate.name),
                                                      widen_ascii(std::string{error.what()}));
                    }
                    catch (...)
                    {
                        Output::send<LogLevel::Error>(STR("[NMOD] {}: refused: C++ mod setup failed: unknown exception\\n"), widen_ascii(candidate.name));
                    }
                }
            }
            catch (const std::exception& error)
            {
                Output::send<LogLevel::Error>(STR("[NMOD] refused: C++ mod setup failed: {}\\n"), widen_ascii(std::string{error.what()}));
            }
            catch (...)
            {
                Output::send<LogLevel::Error>(STR("[NMOD] refused: C++ mod setup failed: unknown exception\\n"));
            }
            return;
"""

UE4SS_H5_ANCHOR = """    auto UE4SSProgram::queue_reinstall_mods() -> void
    {
#ifdef __PROSPERO__
        if (!native_mods_enabled()) return;
#endif
        if (!is_event_loop_thread())
        {
            queue_event([this]() { queue_reinstall_mods(); });
            return;
        }
"""
UE4SS_H5_REPL = """    auto UE4SSProgram::queue_reinstall_mods() -> void
    {
#ifdef __PROSPERO__
        if (!native_mods_enabled()) return;
#endif
        if (!is_event_loop_thread())
        {
            queue_event([this]() { queue_reinstall_mods(); });
            return;
        }
#ifdef __PROSPERO__
        // [CAP09 F3] Hot reload is refused, loudly, while any native C++ mod is
        // present. An ABI-v1 module stays resident (it may own threads), so a
        // reinstall would dlopen the resident image again outside module_start,
        // re-enter its entry, and start nothing (install_cpp_mods is not part
        // of a reinstall). Nothing is uninstalled: every mod keeps running.
        // Without C++ mods this is the unchanged R11 path. Restart the game to
        // reload mods.
        {
            size_t native_cpp_mods = 0;
            for (const auto& mod : m_mods)
                if (dynamic_cast<CppMod*>(mod.get())) ++native_cpp_mods;
            if (native_cpp_mods != 0)
            {
                Output::send<LogLevel::Warning>(STR("[NMOD] refused: hot reload of all mods while {} native C++ mod(s) are loaded; nothing was reloaded, every mod keeps running (restart the game to reload)\\n"),
                                                native_cpp_mods);
                return;
            }
        }
#endif
"""

UE4SS_H4_ANCHOR = """                    if (mod && !mod->is_started()) mod->start_mod();
                }
            return {};
"""
UE4SS_H4_REPL = """                    if (mod && !mod->is_started()) mod->start_mod();
                }
            // [CAP09 B1] C++ mods created by setup_mods under the snapshot.
            if constexpr (std::is_same_v<ModType, CppMod>)
                start_native_cpp_mods_in_order();
            return {};
"""

LUAMOD_L1_ANCHOR = """#define UE4SS_LUAMOD_PRINTF std::printf
#endif
"""
LUAMOD_L1_REPL = """#define UE4SS_LUAMOD_PRINTF std::printf
#endif

#ifdef __PROSPERO__
// [CAP09] defined in src/Mod/CppMod.cpp (NativeModEngineServices.inc).
namespace RC::Platform { void NativeModsGameThreadFrame(int source) noexcept; }
#endif
"""
LUAMOD_L2_ANCHOR = """            process_delayed_actions<kFuncPtrMarshalExecutor>(LuaMod::m_delayed_game_thread_actions);

            process_pending_notify_on_new_object_callbacks();
"""
LUAMOD_L2_REPL = """            process_delayed_actions<kFuncPtrMarshalExecutor>(LuaMod::m_delayed_game_thread_actions);

            process_pending_notify_on_new_object_callbacks();
    #ifdef __PROSPERO__
            // [CAP09] native C++ mods: game-thread tasks + per-frame callbacks
            // on the same data-only pump (never throws).
            RC::Platform::NativeModsGameThreadFrame(0);
    #endif
"""
LUAMOD_L3_ANCHOR = """                    if (std::chrono::steady_clock::now() < drain_deadline)
                    {
                        process_pending_notify_on_new_object_callbacks();
                    }
"""
LUAMOD_L3_REPL = """                    if (std::chrono::steady_clock::now() < drain_deadline)
                    {
                        process_pending_notify_on_new_object_callbacks();
                    }
                    // [CAP09] native C++ mods (source 1 = copied-vtable Tick pump).
                    RC::Platform::NativeModsGameThreadFrame(1);
"""

# [FIX F1] port header made reachable by the patch (CppMod.cpp -> FindNativeCppModModule):
# the throwing is_regular_file(path) becomes the error_code overload. Staged as a
# candidate copy of the PORT file (a CHANGES files row: the build's header overlay
# replaces the port path with candidate/include/Platform/NativeCppModPath.hpp).
PORT = Path("/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port")
NCMP_P1_ANCHOR = """#include <stdexcept>
#include <string>
#include <string_view>
"""
NCMP_P1_REPL = """#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
"""
NCMP_P2_ANCHOR = """    inline auto FindNativeCppModModule(const std::filesystem::path& directory,
                                      std::string_view mod_name) -> std::optional<std::filesystem::path>
"""
NCMP_P2_REPL = """    // [CAP09 F1] Never throws a std::filesystem error: every probe uses the
    // error_code overload. An OS error (permission denied, I/O error) stops
    // the lookup and is reported through *error (fail closed: no module). A
    // missing file or a dangling link is not an error (status not_found).
    inline auto FindNativeCppModModule(const std::filesystem::path& directory,
                                      std::string_view mod_name,
                                      std::error_code* error = nullptr) -> std::optional<std::filesystem::path>
"""
NCMP_P3_ANCHOR = """            if (std::filesystem::is_regular_file(path)) return path;
"""
NCMP_P3_REPL = """            // status(p, ec) reports a missing file as file_type::not_found AND
            // sets ec (ENOENT/ENOTDIR): that is "try the next name", not an error.
            std::error_code ec;
            const auto st = std::filesystem::status(path, ec);
            if (st.type() == std::filesystem::file_type::not_found) continue;
            if (ec)
            {
                if (error) *error = ec;
                return std::nullopt;
            }
            if (st.type() == std::filesystem::file_type::regular) return path;
"""

FILES = {
    "UE4SSProgram.cpp": {
        # known bases: the R11 row, then the live tree after the integrator merged cap #2/#4 (2026-10-13 00:05)
        "base_sha256": ["121f2383e02e77e638dff31cd0e4b75fdd58df1715f77e7f3a1b6cacc01123da",
                        "ad3ae621a7c86d771f4ed56e54eddf3330b8db6a1525c5a73a8466a6c57078cf",
                        # live after the integrator backed cap #2 out again (2026-10-13 00:15)
                        "fe9fe7bc1f856f2268290336dc65c4f5273aa27f8766426535ccfca0a317e45a"],
        "hunks": [("H1 include planner", UE4SS_H1_ANCHOR, UE4SS_H1_REPL),
                  ("H2 start-order helpers", UE4SS_H2_ANCHOR, UE4SS_H2_REPL),
                  ("H3 setup_mods snapshot branch creates CppMods", UE4SS_H3_ANCHOR, UE4SS_H3_REPL),
                  ("H4 start_mods<CppMod> starts them", UE4SS_H4_ANCHOR, UE4SS_H4_REPL),
                  ("H5 hot reload refused while native C++ mods are loaded [F3]", UE4SS_H5_ANCHOR, UE4SS_H5_REPL)],
    },
    "NativeCppModPath.hpp": {
        "base_path": PORT / "ue4ss/include/Platform/NativeCppModPath.hpp",
        "stage_rel": "candidate/include/Platform/NativeCppModPath.hpp",
        "apply_rel": "candidate/include/Platform/NativeCppModPath.hpp",
        "original_rel": "original/include/Platform/NativeCppModPath.hpp",
        "base_sha256": ["4286deac7d3e7ed3d4903c7987242a6ac159c9152388c86c37d08921ff0a2154"],
        "hunks": [("P1 include system_error", NCMP_P1_ANCHOR, NCMP_P1_REPL),
                  ("P2 error_code out-parameter", NCMP_P2_ANCHOR, NCMP_P2_REPL),
                  ("P3 status error_code overload replaces the throwing is_regular_file [F1]", NCMP_P3_ANCHOR, NCMP_P3_REPL)],
    },
    "LuaMod.cpp": {
        "base_sha256": ["96e8ff4d5eb8c2a98ec37f1d54282687e169fca8dfac816b46f0299e1c38999e",
                        "1128b6eb8e3cc856fc39063a1866163d9820b3de496ea014bfb2067d9a3e9a6d"],
        "hunks": [("L1 pump declaration", LUAMOD_L1_ANCHOR, LUAMOD_L1_REPL),
                  ("L2 R3 marshal drain call", LUAMOD_L2_ANCHOR, LUAMOD_L2_REPL),
                  ("L3 VMT Tick drain call", LUAMOD_L3_ANCHOR, LUAMOD_L3_REPL)],
    },
}

sha = lambda b: hashlib.sha256(b).hexdigest()


def patch(text, hunks, report):
    for label, anchor, repl in hunks:
        n = text.count(anchor)
        report.append((label, n))
        if n != 1:
            return None
    for label, anchor, repl in hunks:
        text = text.replace(anchor, repl, 1)
    return text


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--stage", action="store_true")
    g.add_argument("--check", action="store_true")
    g.add_argument("--apply-to")
    ap.add_argument("--base-dir", help="take base files from this directory instead of the live candidate tree")
    ap.add_argument("--out-dir", help="with --stage: write the patched files here instead of PATCH/candidate/src (e.g. the R11-base variant)")
    a = ap.parse_args()
    items, fails, pending = 0, [], []
    for name, spec in FILES.items():
        if "base_path" in spec:  # a port file (not in the candidate tree): its base is the pinned port copy
            base_path = Path(a.base_dir) / name if a.base_dir and (Path(a.base_dir) / name).exists() else spec["base_path"]
        else:
            base_path = Path(a.base_dir) / name if a.base_dir else (Path(a.apply_to) if a.apply_to else T) / "candidate/src" / name
        raw = base_path.read_bytes()
        base_sha = sha(raw)
        known = spec["base_sha256"]
        drift = base_sha not in known
        print(f"  {name}: base {base_path} sha={base_sha[:8]} " + (f"DRIFT (known bases: {', '.join(k[:8] for k in known)}; anchors decide)" if drift else "(known base)"))
        report = []
        out = patch(raw.decode(), spec["hunks"], report)
        for label, n in report:
            items += 1
            ok = n == 1
            print(("  pass " if ok else "  FAIL ") + f"{name} {label}: anchor occurs {n}x (must be exactly 1)")
            if not ok:
                fails.append(f"{name} {label}")
        if out is None:
            continue
        # every replacement must itself be present exactly once afterwards
        for label, anchor, repl in spec["hunks"]:
            items += 1
            n = out.count(repl)
            print(("  pass " if n == 1 else "  FAIL ") + f"{name} {label}: replacement present {n}x")
            if n != 1:
                fails.append(f"{name} {label} replacement")
        staged = Path(a.out_dir) / name if a.out_dir else HERE / spec.get("stage_rel", "candidate/src/" + name)
        if a.stage:
            staged.parent.mkdir(parents=True, exist_ok=True)
            staged.write_text(out)
            print(f"  staged {staged} sha={sha(out.encode())[:8]}")
        elif a.check:
            items += 1
            same = staged.exists() and staged.read_bytes() == out.encode()
            print(("  pass " if same else "  FAIL ") + f"{name}: staged copy == regenerated from the live base")
            if not same:
                fails.append(f"{name} staged copy differs")
        elif a.apply_to:
            if "apply_rel" in spec:
                dest = Path(a.apply_to) / spec["apply_rel"]
                orig = Path(a.apply_to) / spec["original_rel"]
                for path, want in ((dest, None), (orig, raw)):
                    if path.exists() and want is not None and path.read_bytes() != want:
                        print(f"  FAIL {name}: {path} exists and differs from the pinned port base")
                        fails.append(f"{name} original copy differs")
                    if path.exists() and want is None:
                        print(f"  FAIL {name}: {path} already exists (re-apply refused)")
                        fails.append(f"{name} candidate exists")
                pending.append((dest, out))
                pending.append((orig, raw.decode()))
            else:
                pending.append((base_path, out))
    # --apply-to writes nothing unless EVERY file and hunk passed.
    if a.apply_to and not fails:
        for path, out in pending:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(out)
            print(f"  applied {path} sha={sha(out.encode())[:8]}")
    print(f"apply_anchored items_checked={items} failed={len(fails)}")
    if items == 0:
        print("FAIL: zero items")
        sys.exit(2)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
