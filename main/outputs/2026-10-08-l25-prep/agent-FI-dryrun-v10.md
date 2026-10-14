# agent-FI — zzdiag v10: Claude's menu dry run, renumbered M1-M6

Date: 2026-10-08-l25-prep wave (task: dry-run v10 extending FA's v9).
Deliverables: `zzdiag-v10/main.lua`, `zzdiag-v10/EXPECTED-MARKERS.txt`,
this report. No console contact, no uploads, no loadlist/driver/source/
CHANGES.json edits. Root deploys via `harvest/lua_fast_deploy.sh` (which
computes the loadlist size row itself — no sidecar needed).

## Artifact

- `zzdiag-v10/main.lua` — **4092 bytes** (< 4096 limit; size-exact row
  = 4092), ASCII-only, `luac -p` clean (host luac 5.5.1; file uses only
  Lua 5.4/5.5-common constructs: no `//`, no `<close>`, no goto),
  sha256 `79a4cc85072746e3094c641a7845108ba67c8fd5b87842786073863254cd50fd`.
- Log mechanism is v9's: `print` (registered LuaMod.cpp:1687 →
  `LuaLibrary::global_print`, LuaLibrary.cpp:35-73, the F4 fixed codec;
  every line lands as `[Lua] [zz] ...` in the UE4SS log).

## Structure = v9 chassis + renumbered ladder

Kept verbatim-in-semantics from v9 (`zzdiag-v9/main.lua` read as base):

| Piece | v9 name | v10 behavior |
|---|---|---|
| 6s auto-probe | `[zz] T<n> db=.. pc=..` | unchanged (st() token contract v/n/i/e) |
| +18s one-shot | `[DRY] Q 18s` → LoopAsync(18000) enqueues dry via ExecuteInGameThread, returns false (port Loop = remove on non-true) | unchanged; runs after T1/T2 (6s/12s) established |
| +45s watchdog | `[DRY] WD no-drain` | unchanged (ran flag set as first statement of the game-thread body) |
| F7 manual rerun | `[zz] K1 f7` / `[zz] K2 q` | unchanged (same dry closure re-enqueued) |
| error detail | 3-space + first 24 chars, `file:line:` stripped | unchanged (el()) |
| wrappers-never-nil law | resolution judged ONLY by `pcall obj:IsValid()` | unchanged; FindAllOf keeps its real-nil contract (n=0) |

Renumbered steps (one line each, format `[DRY-<n>] <step> PASS/FAIL/SKIP <fact>`):

| Step | Line | Call (exact form) | PASS when |
|---|---|---|---|
| M1 | `[DRY-1] pcs V n=<c> iv=<t> ipc=<t>` | `pcall(FindAllOf,"PlayerController")` + `#`; `pcall pc:IsValid()`; `pcall pc:IsPlayerController()` | n>0 ∧ iv=1 ∧ ipc=1 (all three pcall results reported: iv/ipc each 1/0/e, s=no-instance; n=-1 = call threw) |
| M2 | `[DRY-2] cdo V y/n/e` | `pcall(StaticFindObject,"/Script/UMG.Default__WidgetBlueprintLibrary")` → IsValid | wrapper resolves valid (y); exercises EC's long-name arm |
| M3 | `[DRY-3] uwcls V y/n/e` | `pcall(StaticFindObject,"/Script/UMG.UserWidget")` → IsValid | resolves (class acquisition via registered API only) |
| M4 | `[DRY-4] create V y/n/e / SKIP no-arg` | `pcall wl:Create(vpc, uc, vpc)` | returned widget IsValid; SKIP when no valid pc+CDO+class |
| M5 | `[DRY-5] sco V y/n/e / FAIL cls=n / SKIP no-arg` | in-step `StaticFindObject("/Script/UMG.TextBlock")`, then `pcall StaticConstructObject(tb, w or vpc, FName("zzdry10"))` | constructed object IsValid; cls=n = class lookup invalid |
| M6 | `[DRY-6] stext V ftext/e / SKIP no-M5` | `pcall tbo:SetText(FText("test"))` | pcall clean; runs only when M5 PASSed |

Plus the v9-style end attestation `[DRY] no-viewport` (no AddToViewport
call exists in the file; the string occurs only in the header LAW comment).
FName seed "zzdry10" (short, wide-char-writer-safe, distinct from v9's).

## Registration citations (live l26 candidate tree:
`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/`)

Every API used, with the site verified this session:

| API used by v10 | Registration site (candidate/) |
|---|---|
| `print` | src/LuaMod.cpp:1687 → src/LuaLibrary.cpp:35 (`global_print`) |
| `Key.F7` | src/LuaMod.cpp:714 (`Key` table made global at :775) |
| `StaticFindObject` (overload #1, string-only) | src/LuaMod.cpp:1695-1733 |
| `FindFirstOf` (T-probe) | src/LuaMod.cpp:1806-1854 (Prospero FNAME_Find-first, LFB bridge) |
| `FindAllOf` | src/LuaMod.cpp:1857-1920 (real nil on empty; table otherwise) |
| `RegisterKeyBind` | src/LuaMod.cpp:2118 |
| `StaticConstructObject` (Class, Outer, FName Name #optional) | **src/LuaMod.cpp:2431** (cited in script header per task; Name param takes FName userdata at :2465-2472) |
| `LoopAsync` | src/LuaMod.cpp:3663 (bool continuation contract) |
| `ExecuteInGameThread` | src/LuaMod.cpp:5745 |
| `FName` global ctor | src/LuaMod.cpp:7607-7610 (`LuaType::FName::construct` + `lua_setglobal`) |
| `FText` global ctor | src/LuaMod.cpp:7612-7614 (`LuaType::FText::construct` + `lua_setglobal`) |
| wrapper `IsValid` | include/LuaType/LuaUObject.hpp:769 (UObject remote-check incl. global-map + IsUnreachable; base binding :186) |
| reflected `obj:Fn(...)` dispatch (IsPlayerController / Create / SetText) | src/LuaUObject.cpp:2413-2438 `handle_unreal_property_value` → C++ `GetFunctionByNameInChain` — the wrapper's generic __index path, NOT a Lua-side FindFunction (v2 crash law honored) |
| FText→TextProperty push (M6 marshal side) | src/LuaUObject.cpp:1851-1861 `push_textproperty` (Set = SetString/ToFString) |

No Lua-side `LuaType/*.cpp` directory exists in the delta tree; the
compiled surface is these registrations plus the vendored LuaType
headers pulled in by `LuaMod.cpp` (the FText/FName global pre-loads at
:7607-7614 prove both constructor tables exist at runtime).

## SKIP-by-design analysis (task's gating questions)

- **UMG API registered?** NO dedicated UMG Lua API exists in this port
  (no CreateWidget global, no UMG table — grep over candidate/src finds
  only engine-side self-test strings, UnrealInitializer.cpp:2463-2464).
  However M2/M3/M4 do NOT depend on one: M2/M3 acquire objects via the
  REGISTERED `StaticFindObject`; M4's `Create` is a REFLECTED UFunction
  call through the generic dispatch (LuaUObject.cpp:2413-2438) in the
  mod's own proven 3-param form (TownfallItemSpawner MOD:509-511; v9 D5
  staged-and-reviewed with the identical call). So M4 RUNS (gated on
  valid args) rather than SKIPping 'api-not-registered' — with one
  caveat documented in EXPECTED-MARKERS: if a future build also broke
  the reflected path, M4 degrades to FAIL e (dispatch throw), never a
  silent pass.
- **M5 StaticConstructObject** — registered (LuaMod.cpp:2431, cited in
  the script comment as required). RUNS. Its only SKIP is `no-arg`
  (no valid outer: M4 dead AND pc invalid).
- **M6** — SKIP no-M5 by design when M5 didn't construct.
- **M1→M4/M5 tightening vs v9:** M4/M5 outer requires a VALID pc
  (`iv=="1"`), not merely a non-nil wrapper (v9 accepted the wrapper).
  An invalid-pc outer adds crash surface without diagnostic value.

## Deviation note (task text vs implementation)

Task shorthand wrote M2 as `StaticFindObject("/Script/UMG.WidgetBlueprintLibrary")`;
Claude's spec says "UMG WidgetBlueprintLibrary CDO", and the CDO literal
is `/Script/UMG.Default__WidgetBlueprintLibrary` (MOD:499, v9 D3,
boot self-test UnrealInitializer.cpp:2464). v10 uses the CDO literal —
that is also the object M4 must call Create on.

## Verification (host, no console)

- `luac -p` clean; 4092 B; ASCII-only; no executable AddToViewport/
  FindFunction/bpfn (comment-only occurrences); sha256 above.
- Pure-Lua host harness (stub env: FindAllOf/FindFirstOf/
  StaticFindObject/StaticConstructObject/FName/FText/LoopAsync/
  ExecuteInGameThread/RegisterKeyBind/Key), six cases, all observed:
  1. HAPPY — M1..M6 all PASS in order + `[DRY] no-viewport`.
  2. FFO-NIL (FindAllOf→nil) — `M1 FAIL n=0 iv=s ipc=s`; M4/M5
     `SKIP no-arg`, M6 `SKIP no-M5`; M2/M3 evaluated independently.
  3. NO-DRAIN (ExecuteInGameThread no-op) — only Q/Z0/T lines;
     `[DRY] WD no-drain` at the 45s tick; zero M-lines.
  4. CREATE-THROW — `M4 FAIL e` + 24-char error tail; M5 still
     attempted with outer=pc and PASSes; M6 PASSes.
  5. CDO-DEAD (CDO lookup → invalid wrapper) — `M2 FAIL n`;
     M4 `SKIP no-arg`; M3/M5/M6 independent legs still evaluated.
  6. F7 manual rerun — `K1 f7` → full M-set → `K2 q`.

## Risks / honest limits (inherited from v9, unchanged)

- Reflected UFunction calls (IsPlayerController/Create/SetText/
  StaticConstructObject's internal ProcessEvent) remain
  console-UNPROVEN in this lineage; pcall fences Lua-level errors but
  cannot fence a native crash — that is inherent to the probe's purpose.
  The `[DRY] no-viewport` attestation line doubles as a completion
  marker: M-lines stopping mid-sequence without SKIPs = native death;
  pull klog.
- The Lua-string→engine seam may still empty every lookup at runtime;
  the M2/M3/M5 FAIL-n + `[StaticFindObject] long-name part` refusal
  lines and the [FFO] markers carry the seam-vs-pool split (see
  EXPECTED-MARKERS.txt oracle summary).
- 18s assumes the menu is up and the pump armed; F7 reruns later.

## Consumption law

Read as base: `zzdiag-v9/main.lua` + agent-FA-zzdiag-v9.md,
agent-CX-v7-probe.md (wrapper-never-nil law, token contract),
agent-CI-marshaling-audit.md §1.4 (Create/SetText/FText forms),
agent-EC-staticfindobject-fix.md (long-name arm + refusal marker),
harvest/lua_fast_deploy.sh (deploy path only — NOT executed).
Registrations re-verified directly in
outputs/2026-10-08-l26-runtime/typescan-r4/candidate/ this session.
No source files, loadlists, drivers, or CHANGES.json touched.
