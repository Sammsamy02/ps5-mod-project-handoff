# Agent J — UE4SS.log digest (l24 harvests, 2026-10-08)

Primary file: `/tmp/l24-ue4ss-4.log` (108,503 bytes, 609 lines = 608-line crashing session + 1 appended 18:53 failure line).
All other harvests are separate but structurally identical sessions (verified by timestamp-normalized diff; only wall-clock
and `elapsed=` jitter differ). The 608-line UE4SS session in `-4` and in `-full` is byte-identical.

## 0. Harvest inventory (question: are the files different sessions or copies?)

| File | Session (first…last UE4SS line) | Lines | Relation |
|---|---|---|---|
| /tmp/l24-ue4ss.log | 13:38:13 → 13:38:16 | 608 | own session, same structure |
| /tmp/l24-ue4ss-2.log | 13:41:42 → 13:41:48 | 608 | own session, same structure |
| /tmp/l24-ue4ss-3.log | 13:45:35 → 13:45:38 | 608 | own session, same structure |
| /tmp/l24-ue4ss-full.log | 13:47:23 → 13:47:29 | 608 | **the crashing session** (CE-108255-1) |
| /tmp/l24-ue4ss-4.log | 13:47:23 → 13:47:29 (+ appended line) | 609 | `-full` + exactly ONE appended line (line 609, the 18:53 modbridge failure) |

Normalized diffs show the ONLY content differences between sessions are timestamps and 1–15 ms jitter in
`[Seeds][L14] check=... elapsed=` values (lines 499–511 region). Four separate boots/launches (13:38, 13:41, 13:45, 13:47)
all init-identically and all end at the same milestone, "Adding shutdown listener".

Untimestamped lines in `-4`: blank separators 7, 12, 13, 19, 20; plus line 609 (modbridge `[t=… pid=…]` format, not UE4SS format).

## 1. Marker-family census (line numbers refer to /tmp/l24-ue4ss-4.log)

| Family | Count | First | Last | Notes |
|---|---|---|---|---|
| `[l22]` | **0** | — | — | family absent from this log |
| `[l23]` | **0** | — | — | family absent from this log |
| `[NativeLayout]` | 1 | 22 | 22 | declaration_provider=compiled, source=compiled-audited-townfall-l23 |
| `[BuildProfile]` | 402 | 25 | 426 | 1 digest line + 401 `check=` lines, **all result=PASS, 0 FAIL** |
| `[LayoutPreflight]` | 67 | 427 | 493 | 51 PRESENT, 13 MISSING (all conditional), 2 DEFERRED; summary line 493: `checked=66 startup-required=50 conditional-map=14 runtime-deferred=2 missing-required=0` |
| `[Seeds]` (bare) | 8 | 23 | 539 | arm (23), unbound-by-design (24), slot binds 0/3 (494–495), pool admit+validate (528–529), slot 2 (538), slot 4 (539) |
| `[Seeds][L14]` | 37 | 496 | 536 | 1 pool-admission header, 8 image, 14 VirtualQuery, 13 check, 1 total (`outcome=admitted`, 2627ms/60s) |
| `[Seeds][L15]` | 2 | 537 | 540 | FName output verified; total `outcome=verified-and-bound` (2684ms shared budget) |
| `[Seeds][L18]` | 2 | 531 | 532 | FName ctor sig matched, source=authenticated-disk-XOM-signature, 32B @ 0x63D590 |
| `[WalkRxGuard]` | 4 | 583 | 595 | all "jmp-follow skipped (exec-only text; no read)" for LoadMap/Tick/InitGameState/ProcessEvent entries |
| `[R3]` | 2 | 589 | 590 | 1 refusal + 1 deferral (see §3) |
| `[TypeScan]` | 4 | 600 | 603 | 3 `missing-layout-key` + `scan-complete missing-keys=3 core-ready=true` |
| "Scan failed" | 1 | 588 | 588 | GameEngine::Tick only |
| "falling back" | 1 | 588 | 588 | fallback to vtable address for GameEngine::Tick |
| "Already constructed" | 22 | 551 | 572 | CoreUObject/Engine/UMG/MovieScene classes |
| "skipped" | 6 | 23 | 595 | lines 23, 538, 583, 586, 592, 595 |
| "failed" | 2 | 588 | 609 | 588 Scan failed; 609 modbridge module_start failed |
| Warnings (LogLevel style) | 0 | — | — | none |
| Errors (UE4SS) | 0 | — | — | none inside the 13:47 UE4SS session |
| `mfa_activate_failed` | 1 | 609 | 609 | appended 18:53 line only |

## 2. Init timeline (crashing session, 13:47:23 → 13:47:29, ~6 s)

**13:47:23** (lines 1–21)
1. Banner: native console/GUI/crash-dump/fs-watch unavailable; UE4SS v3.0.1 Git SHA e3ba101; timezone DB unavailable; Build Config "Native experiment (Clang)"
2. Config: default (no game-specific), `/data/plugins/UE4SS-settings.ini`
3. Directories: root/working `/data/plugins`, game exe dir `/app0` (exe unobserved), mods `[0] ue4ss/Mods`, log + dumper `/data/plugins`
4. `Setting up mods...` (line 21 — early constructor position, same as upstream)
5. `[NativeLayout]` compiled declaration provider (line 22)

**13:47:24** (lines 23–599)
6. `[Seeds]` required-slot table armed (0/2/3/4); ps_scan arm + both Lua override scans skipped (23); unbound-by-design list (24)
7. `[BuildProfile]` digest + 401 PASS checks (25–426): image SHA 3b6cfad4…, base 0x400000, engine 5.6, 244-marker classification 54 present
8. `[LayoutPreflight]` 66 keys → missing-required=0 (427–493)
9. Seed binds: slot 0 g_uobject_array @ 0x97ba110 (494); slot 3 gmalloc 0x970add8→0x97089b8 (495)
10. `[Seeds][L14]` FName pool admission @ 0x9765EC0 → `outcome=admitted` in 2627/60000 ms (496–527); slot 1 admitted (528); pool validated, 'None' anchor ok, ByteProperty id 0x3 (529)
11. `[Seeds][L18]` FName ctor signature matched @ 0x63D590 (530–532); `[Seeds][L15]` output verified, slot 2 bound (537–538); slot 4 static_construct_object_internal @ 0x890aa0 (539); L15 total verified-and-bound (540)
12. Build configuration: shipping, detected (541); Stats: off (542); engine version 5.6 (543)
13. Object-construction wait: KismetStringLibrary / Conv_NameToString / CDO located (544–550); 22 "Already constructed" (551–572); Object/Struct CDOs (573–574)
14. Finds: GameEngine (575–576), GameModeBase (577–578), Actor CDO (579), GameViewportClient (580–581), LoadMap @ 0x3c4d460 (582–584), **Tick scan 0x0 → fallback to vtable 0x33ac8c0 (585–588)**, InitGameState @ 0x33db2b0 (591–593), ProcessEvent @ 0x859830 (594–596)
15. `[R3]` refusal + EngineTick-detour-unavailable deferral (589–590)
16. Initializing global FName variables (597); Constructed 0 of 0 objects (598); Initializing type system (599)

**13:47:29** (~5 s later — the type-system scan; lines 600–608)
17. `[TypeScan]` 3 missing keys, core-ready=true (600–603); FText size 0x10 (604); FSoftObjectPath layout (605); FExecVTableOffsetInLocalPlayer: 0 (606)
18. Post-initialization: GMalloc 0x970add8 → 0x97089b8 (607)
19. **`Adding shutdown listener` (608) — LAST UE4SS line; nothing after it**

**Appended (18:53 session)** (line 609): `[t=4657636ms pid=222] modbridge module_start failed: mfa_activate_failed code=-8 detail=size file is longer than the load list says ue4ss/UE4SS-settings.ini`

## 3. Everything abnormal / skipped / fallback / refusal

- L1: native console, GUI, **crash dumping** and filesystem watching unavailable → no UE4SS-side crash artifact will exist for CE-108255-1
- L3: timezone database unavailable (cosmetic)
- L5: no game-specific config → default config file
- L11: game executable unobserved on this platform
- L23: ps_scan arm and **both Lua override scans skipped** (consequence of armed seed table)
- L24: **unbound by design**: `fname_tostring` (Kismet fallback owns it), `gnatives`, `fuobject_hash_tables_get`
- L494/495: slots 0 and 3 bound with "identity and ABI **unverified**"
- L538: slot 2 FName ctor — "**detour verification skipped**"
- L539: slot 4 — "statically_verified_empty_callback; **native success not observed**"
- L583/586/592/595: `[WalkRxGuard]` **jmp-follow skipped ×4** (exec-only text, no read) — entries used as-is for LoadMap/Tick/InitGameState/ProcessEvent
- L587/588: GameEngine::Tick scan found 0x0 → "**Scan failed, falling back to vtable address for GameEngine::Tick**"
- L589: `[R3] refused: EngineTick pre-callback registrations (GameThreadInitializer + QueryHarnessDispatch)` — detour registration compiled out under R3 marshal mode
- L590: `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` — **the pump that would drive later stages never engages in this log**
- L480–492: 13 conditional layout keys MISSING (AActor::BeginPlay/EndPlay/Tick, UGameViewportClient::Tick, UObject::ProcessConsoleExec, UStruct::Link, FProperty::RepNotifyFunc / HasIntrusiveUnsetOptionalState / IsIntrusiveOptionalValueSet / InitializeValueInternal, FMapProperty::KeyProp / ValueProp, FSetProperty::ElementProp) — declared non-blocking ("existing lazy checks remain authoritative")
- L474/475: FText keys DEFERRED (by design, runtime discovery)
- L598: "Constructed **0 of 0** objects"
- L600–602: TypeScan missing-layout-key for the 3 map/set property keys (core-ready=true)
- L609: appended modbridge module_start failure (different session; see §5)
- Health positives: BuildProfile 401/401 PASS; preflight missing-required=0; FName pool admitted within budget; FName ctor verified-and-bound; engine 5.6, shipping, stats off — the fork's own layout/admission gates all passed.

## 4. Missing milestones after "Adding shutdown listener" (vs upstream UE4SS v3.0.1)

Upstream reference (`UE4SS/src/UE4SSProgram.cpp` @ tag v3.0.1, fetched from UE4SS-RE/RE-UE4SS): the constructor logs banner/config/dirs
then `setup_mods()` ("Setting up mods…", line 972) *before* engine init; `init()` calls `setup_unreal()` → `UnrealInitializer::Initialize`
(the block that ends "Post-initialization: GMalloc…" / "Adding shutdown listener"), and after it returns upstream logs, in order:

1. **"Unreal Engine modules (modular/non-modular):" + per-module "X @ 0x… size=…" lines** — absent
2. (silent) fire_unreal_init_for_cpp_mods, setup_unreal_properties, UAssetRegistry limit — no upstream log lines; plus possible
   "ProcessLocalScriptFunction/ProcessInternal is not available…" warnings — absent (fork's hook config differs, inconclusive alone)
3. **"m_shared_functions: 0x…" (share_lua_functions)** — absent
4. **on_program_start path artifacts** — absent entirely: ObjectDumper::init, `install_lua_mods()`, `LuaMod::on_program_start()`,
   `fire_program_start_for_cpp_mods()`
5. **The mod-start block** — absent entirely: "Starting mods (from mods.txt load order)…" / "Starting mods (from enabled.txt, no defined
   load order)…" / "Starting Lua mod '<name>'" / "Mod '<name>' has enabled.txt, starting mod." → **no Lua states created, no scripts/main.lua
   ever ran, mods never started**
6. **"Event loop start" (update thread banner)** — absent
7. GUI/console/debug-keybinding init: expected absent on PS5 (declared unavailable at L1); crash dumper likewise unavailable

Fork-relevant corollary: mod *enumeration* ("Setting up mods…") happened early (L21, matching upstream constructor order), but the
**start** phase lives behind the game-thread pump that the R3 refusal (L589–590) deferred to the "R3 marshal" — and no pump/marshal
line was ever logged before the session ended, so even without the crash the mod-start milestones had not yet been reached.
All four earlier sessions (13:38/13:41/13:45/13:47) stop at exactly the same place.

## 5. 18:53 session contribution — CONFIRMED single line

`/tmp/l24-ue4ss-4.log` is byte-identical to `/tmp/l24-ue4ss-full.log` for the first 108,344 bytes (entire 13:47 session) plus exactly
one appended line (609). That line uses the modbridge `[t=4657636ms pid=222]` format, not UE4SS's `[YYYY-mm-dd HH:MM:SS]` format, and no
UE4SS banner/config/init lines from an 18:53 session exist anywhere in any harvest. Therefore `module_start` (mfa_activate_failed
code=-8, "size file is longer than the load list says ue4ss/UE4SS-settings.ini") aborted **before any UE4SS initialization ran**;
the 18:53 session contributed only that one failure line. (The `code=-8` size mismatch on UE4SS-settings.ini is the actionable l25
lead: the on-disk settings file grew beyond what the modbridge load list declares for it.)
