# FOLD — how to fold worker-pad-coexist into the composition (proposal-only)

Nothing in this packet is folded. This is the exact instruction for the
controller/chief who owns the next fixed-edition build. All paths relative to the
worktree root. DO NOT edit any in-use launch10-build file; this fold is a NEW
fixed-script edition child of the l10 driver, exactly like the l10 edition was a
child of the comp6 driver.

## 0. Summary — four staged sources, four units, ZERO caller changes

| unit (composition name) | kind | staged replacement | sha256 |
|---|---|---|---|
| `native-pad-input` | retained runtime unit (base receipt `selected_runtime_units['native-pad-input']`) | `outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/native-pad-input-coexist/NativePadInput.cpp` | `03c9f6d049609c81fae6adb0f28b9f0bb14d716c8fa92e5a60c6b7f80d471d6a` |
| `native-pad-PS5PadSource` | retained runtime unit | `outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/port/PS5PadSource.cpp` | `24692ec70e9137f156286e3c2ffa30fc8713a94b1bc1ce78f1de5a5cb26dd986` |
| `native-pad-PS5PadNative` | retained runtime unit | `outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/port/PS5PadNative.cpp` (**content byte-identical to base**; relocated only so its quoted includes resolve to the staged header) | `501d77afbe85585630a2e31a4ff958c46d37f1236a4d0896fb9b390db4515447` (== base) |
| `input-handler` | retained runtime unit (base `native-coherent-runtime-1/source/port/first/src/Input/Handler.cpp`) | `outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/input-handler-coexist/Handler.cpp` | `f7539c6094944ca294e4e017685e2e44a39f370070b5583186cd3673c8655e13` |

PLUS the wpd1 program staging, REQUIRED as-is by this packet (see §2):
`outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp`
(sha256 `a16c8972e22b9b3076e99a33896afad41085bf014acee15cd2db11d4c602df37`).

Caller (`ue4ss_backend.cpp`, bootstrap unit `ue4ss_backend`) is UNCHANGED — the
deferral still happens at the origin; no `--replace-bootstrap-unit`, no settings or
INI changes, no `Handler.hpp` / `NativePadInput.hpp` / `PS5PadConfig.*` changes
(all byte-unchanged and stay pinned at their attested hashes).

## 1. New admission needed for ALL FOUR units (checked, not present)

In `outputs/2026-10-04-zcode-controller/launch10-build/build_runtime_composition6_fixed_l10.py`:
- `RETAINED_REPLACEABLE={'program','luamod','cppmod','settings','asmhelper-asmhelper'}`
  (:52) — NONE of the four units is a member;
- `--replace-runtime-unit` eligibility (:86,:144-145) requires the unit to be in the
  freshly compiled `affected` set (:118-125, asserted to exactly
  `{unreal-unrealinitializer, native-ownedmappedscansource, native-psscanbinding,
  native-ownedmappedcandidateselection, native-loadercontext, native-programfactory}`)
  or in `RETAINED_REPLACEABLE`.

Required (SEC32-3 mechanism, precedent = how comp6 admitted `program` and how the
wpd1 FOLD specifies `native-pad-input`):
1. Fork the l10 driver into a NEW fixed edition (distinct ID, e.g.
   `build_runtime_composition6_fixed_l12_padcoexist.py`), byte-identical except
   `RETAINED_REPLACEABLE` gains `'native-pad-input'`, `'native-pad-PS5PadSource'`,
   `'native-pad-PS5PadNative'`, `'input-handler'` (all four are retained prebuilt
   objects — the SEC32-3 "own pinned argv" path applies; this packet's four compile
   gates are the exact argv re-proof: source/-o/-MF redirected only, all exit 0).
2. Author a NEW `--replace-receipt` (hash-pinned attestation) covering the five
   staged sources above (four + wpd1 program) and every spike project file the
   replacements touch. Two layout facts MUST be pinned:
   - The provider pair (`PS5PadSource.cpp` + `PS5PadNative.cpp`) must compile
     against the SAME (staged) header. Either pin the staged mirror directory
     (`staging/ue4ss-native-pad-provider-coexist/port/`, which carries the staged
     header + byte-identical `PS5PadNative.cpp`/`PadServiceLease.hpp` and the
     byte-identical vendored SDL header under its mirrored
     `primary-sources/` layout), or add an `include_dir_map` redirect
     provider-port → staged-port for exactly these two units. Mixing TUs across
     headers is ABI-TOLERATED for reads (append-only members keep every
     pre-existing offset; the only inline state reader is `status()` → `m_status`)
     but MUST NOT allocate: `PS5PadNative.cpp`'s `make_shared` must see the staged
     member set — the relocation exists precisely for that.
   - `PS5PadSource.hpp` is an appended-member superset of the base header; the
     `pad-config` and `native-pad-input` TUs may keep reading the BASE header via
     the -I order (verified consistent; this is also exactly how this packet's
     pinned-argv gates compiled them).
3. Invoke:
   `--replace-runtime-unit native-pad-input=<staged NativePadInput.cpp>`
   `--replace-runtime-unit native-pad-PS5PadSource=<staged PS5PadSource.cpp>`
   `--replace-runtime-unit native-pad-PS5PadNative=<staged PS5PadNative.cpp>`
   `--replace-runtime-unit input-handler=<staged Handler.cpp>`
   `--replace-runtime-unit program=<wpd1 staged UE4SSProgram.cpp>`
   `--replace-receipt <new receipt>`
   Everything else (extra units, diagnostics substitution, bridge_cli,
   undefined-delta/SDK gates, rust/host receipts) is UNCHANGED from l10.
4. Post-link check (fold-time, not yet executed): `nm` on the new
   `combined-supplier.o` must show `diagnostic_event` still DEFINED (in-link by
   diagnostic.c, EDITION.json edit n=3) and the undefined-symbol delta vs the l10
   baseline EMPTY (the staged TUs add no new extern references: the four new diag
   emissions use the same already-declared in-link C symbol).

## 2. Why the wpd1 program staging is REQUIRED by this packet

With the deferred-ATTACHED source, `Handler::init` and `set_input_source` succeed,
but `UE4SSProgram::validate_native_services` (pre-wpd1 bytes) still throws on every
deferred boot because `General.InputSource` defaults to non-empty `"Default"`
(`SettingsManager.hpp:42`, never parsed on this target) and the old gate refuses
when `!is_input_available()`. The wpd1 program downgrade (throw only on explicit
`EnableDebugKeyBindings`; otherwise `pad_input_deferred_input_gate` diagnostic) is
exactly the remaining piece. This packet does NOT modify the program source — the
wpd1 receipt (`worker-pad-defer/compile-program/`) stands.

## 3. Expected observable behavior after fold (what to verify on the next boot)

- Startup proceeds past the pad exactly as wpd1: sdiag gains
  `pad_input_deferred` (=6, PadPhase::Open) + `pad_input_deferred_error`
  (=2157051908, 0x80920004) + `pad_input_deferred_input_gate` (=1);
  `runtime_start_failed` ABSENT; status reaches `mods-started`.
- NEW vs wpd1: UE4SS.log shows the Item Spawner load banner (the six
  RegisterKeyBind calls now SUCCEED against the attached-but-unavailable source —
  wpd1 logged "Exception during mod startup: Input registration unavailable...").
- NEW while running: the poll-side retry is INVISIBLE until the game frees the
  handle; if/when it frees, sdiag gains exactly
  `pad_open_class` (=0, once) → `pad_retry` (=1) → `pad_available` (=<handle>) and
  the next physical OPTIONS press opens the Item Spawner menu (R4→R5 rung lines:
  `<N> spawnable pickups found. Pages=<M>` + `Menu created.`). If the handle never
  frees, `pad_retry`/`pad_retry_failed` accumulate on the backoff schedule
  (1st ~1 s, then ~2 s, ~4 s ... capped ~16 s between attempts) — bounded, no busy
  loop, startup and frame pacing unaffected (host-proven schedule).
- Diagnostic volume bound: ≤3 small int records per retry attempt; at the cap that
  is ≤~0.2 records/s steady state, all on the existing supplier-diag channel.

## 4. What this fold does NOT do

- No second-handle open class: production only ever calls the proven standard
  `scePadOpen` (SDK-ANALYSIS.md §5 verdict). The probe variant (SDK-ANALYSIS.md §6)
  is the launch-window companion that may justify a one-line open-class switch
  LATER (its own bounded, separately-reviewed change).
- No bootstrap/settings/INI changes, no LuaMod/EngineTick work (R3 gap G-AP2 is
  untouched), no claim that outcome B WILL occur on console — only that the
  composition now cannot be the reason it fails, and that outcome A is preserved
  in every case (host-proven).
