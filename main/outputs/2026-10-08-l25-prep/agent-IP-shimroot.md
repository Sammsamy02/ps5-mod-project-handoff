# agent IP — shim root wiring (Claude round-10 item 3)

2026-10-11. Battery-safe: analysis + staged patch + ONE prospero -fsyntax-only
receipt. No builds/console, zero live-tree edits (candidate sha verified
unchanged at close). Full analysis:
`IP-SHIM-ROOT-FIX.md` (canonical copy) = `round5-staging/IP-shim-root/IP-SHIM-ROOT-FIX.md`.

## The root source (found)

The absolute modbridge root is ALREADY runtime-carried inside UE4SS — no new
pipe needed:

```
runtime_module_entry.c :74/:280/:315  MODBRIDGE_MOD_ROOT ("/data/modbridge/roots/<title>",
                                        per-game composition fact == package-profile.json)
  -> modbridge_runtime_start_with_activation(..., mod_root, ...)
  -> runtime_start_bridge.cpp :1907 (Resolution B)
       facts.configured_working_directory = mod_root
  -> Platform::NativeProgramContext.configured_working_directory
       (public, lexical-validated when non-empty)
  -> UE4SSProgram::m_native_context   [private member, process lifetime]
       ...never consumed by setup_mod_directory_path — THE GAP
```

Not the activation object itself (`mfa_activation` retains no root string);
not the PRX path (module parent = /data/plugins = log dir); not settings/env;
not the loadlist (circular + /data ENOENT post-module_start).

## The bug

`setup_mod_directory_path` native branch pins RELATIVE `"ue4ss/Mods"`
(candidate UE4SSProgram.cpp:932). The LuaMod [SHIM] lexical derivation
(LuaMod.cpp:7973-7978) strips `/Mods` → `/ue4ss` → `""` → empty root →
`LuaPathShim_install(L, mod, "")` → self-disable `reason=no-root`
(launch-2 log :782-845, 7x). IB's writable area untestable.

## The wiring (minimal)

ONE branch edit in `UE4SSProgram::setup_mod_directory_path()` (task option d
through the existing entry→bridge→context pipe):

```cpp
const std::filesystem::path shim_native_root = m_native_context.configured_working_directory;
m_mods_directories = {shim_native_root.empty() ? std::filesystem::path("ue4ss/Mods")
                                               : (shim_native_root / "ue4ss" / "Mods")};
```

ZERO LuaMod/LuaPathShim edits — the existing derivation was designed for the
absolute `<root>/ue4ss[/Mods]` shape; shim then derives
`/data/modbridge/writable` per IB. Rule 10: no title-id anywhere in the
derivation; root read verbatim from the composition at runtime; pure lexical
join (no weakly_canonical/current_path — launch-2 throw lesson); empty
configured root keeps the loud no-root disable. Consumer audit (8 sites):
every other native consumer early-returns (buffer searcher, blank
package.path, install_mods/get_mods_txt_entries native branches), is
`#ifndef __PROSPERO__` dead (fs watcher), or exists()-guarded
(/data ENOENT skip, identical to today).

## Deliverables (round5-staging/IP-shim-root/)

| File | Fact |
|---|---|
| `01-ue4ssprogram-shim-root.patch` | 1 file, 1 hunk @ :929 (+18/-1), candidate-relative; dry-run+apply+byte-identical proven on /tmp copy; sha `ee8b107f…2001d7` |
| `syntax-receipt-UE4SSProgram-shim-root.log` | prospero-clang++ -fsyntax-only, EXIT=0, 0 errors, 7 pre-existing warnings (argv = real program-unit argv from l24-build-r2 receipt + l26 candidate -I + l26-A vfs overlay) |
| `APPLY-NOTES.md` | target sha, apply cmd, CHANGES-row refresh duties, no marker changes (markers already exist, now reachable) |
| `unpatched-sha256.txt` | provenance (candidate unpatched `3cc011d7…23b9f` — live tree untouched) |

## What the next launch proves

1. The 7 `reason=no-root` disables → `[SHIM] path-resolver installed mod=<m> root=/data/modbridge/roots/<title>` (root wiring proven by this line alone).
2. NEW `[SHIM] write-area <mod> -> /data/modbridge/writable/<mod>` (IB sandbox live).
3. ST2 `io.open("ue4ss/Mods/ueselftest/progress.txt","w")` → resolves into the writable sandbox; backend success = physical /data writes (cross-launch persistence), backend fail + readback ok = in-memory spill. Post-session ftpsrv `ls /data/modbridge/writable/` = physical cross-check.
4. Regression guards: mods still load (admission untouched), CE-108255-1 baseline unchanged.
Triage: no-root persists → packaged lineage predates bridge Resolution B (:1907); relative `root=` printed → empty-fallback branch (same cause).
