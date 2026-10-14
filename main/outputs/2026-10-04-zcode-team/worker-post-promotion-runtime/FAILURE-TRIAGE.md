# FAILURE-TRIAGE.md — composed runtime does not start (or stops early): what to check, in order

Worker: worker-post-promotion-runtime, 2026-10-04. Companion to
RUNTIME-STAGING-PLAN.md (§ numbering below refers to it). All failure strings quoted
below are literal messages in the retained sources; every check is read-only evidence
collection (FTP RETR / log read), never a bypass. A refusal is a gate working.

Evidence files (freshness FIRST — stale logs are not evidence):
`/data/townfall_supplier_composition.log` (module diagnostics),
`/data/plugins/UE4SS.log` (runtime), `<root>/runtime-status.txt` (ack),
ploader surface text, `/data/modbridge/roots/PPSA33286/loadlist.txt`.

## T1. PRX load failure (nothing happened at all)

Symptoms: ploader surface reports FAIL or no injection line ~1 minute in;
`/data/townfall_supplier_composition.log` absent or stale.

1. ploader.ini shape: exactly `[PPSA33286]` + `/data/plugins/<module>.prx = 2`
   (mode 2 = LATE). Any other shape is undocumented — fix the line, not the loader.
2. Mode-2 re-send: was the payload re-sent for THIS launch? A module sent for launch N
   is not present for launch N+1 (STAGING-CHECKLIST §4).
3. Readback oracle: FTP RETR the staged PRX; ftpsrv auto-decrypts FSELF, so expect
   inner ELF `7f 45 4c 46` with sha256 `b8bfae77…` (30756791 B). Wrapped magic
   `4f 15 3d 1d` = decryption oracle broken — stop, stage nothing else.
4. SDK stamp precedent: an ESDKVERSION `0x80020063` load refusal means the candidate
   lacked the SDK stamp; the composition-3 candidate carries stamp 0x11000000 (packaging
   static check 20/20). If a DIFFERENT PRX was staged by mistake, restage file 1 of §4.
5. Collect ploader surface text + any log tail; do NOT relaunch repeatedly without a
   changed implementation or a specific open question.

## T2. Module started, died early (diagnostic log exists, only seq-1 `entry`)

Only `module_start_entered`/`entry` events, no later sequence: load + channel OK,
failure localized inside module_start. Check the constructor events
(`constructor_begin`/`constructor_return` index where it stops) — the first failing
constructor index names the subsystem. Cross-check `constructor_count` against the
receipt (366-unit composition).

## T3. Settings parse failure (ini)

Symptom: runtime never reaches the log lines; on `__PROSPERO__` the parser error is
stored, not printed (`create_emergency_console_for_early_error`,
UE4SSProgram.cpp:858-861 "The IniParser failed to parse: ..."), and surfaces via
`native_startup_error_message()` -> backend throws "Native UE4SS construction
rejected: ..." -> ack `failed` detail (if the ack path is wired) or the module
diagnostic log.

1. Was the CORRECTED ini staged? The 779-B canonical (sha `9f3fa127…`) passes all five
   native gates + all ten expected values (worker-ue4ss-settings host test). The old
   221-B bundle ini (sha `f6d06cad…`) PARSES but throws later in
   `validate_native_services` (CrashDump/SimpleConsole/GuiConsole defaults true,
   RenderMode != ExternalThread) — check which bytes are actually on the console via
   RETR + sha256, not which file was intended.
2. Parser wedges documented by measurement: a section containing ONLY comments hard-errors
   at the next section header; inline `;` comments corrupt values (bool falls back to
   default). Do not paste doc snippets with inline comments as bytes.
3. `loadlist.txt` row size must equal the real byte count (779): mfa refuses
   all-or-nothing on any size mismatch — and a refused activation means NO runtime
   start at all (see T6).

## T4. Layout pair missing or rejected (the CRITICAL prerequisite)

Symptoms: `UE4SS.log` EXISTS (log device opens before setup_unreal) with startup lines
up to init, then stops; last ack line `failed` (when wired) with one of:

- `Native activation requires both member and vtable layout files`
  (UE4SSProgram.cpp:901-902): one or both of `ue4ss/MemberVariableLayout.ini` /
  `ue4ss/VTableLayout.ini` absent from the activation. Check loadlist rows exist, sizes
  match (588 / 186), files staged, RETR hash == `f291d552…` / `9c20f7c9…`. A pair
  staged under `/data/plugins` instead of the activation root is NOT consumed — the
  native path reads these keys from the mod source, never from disk next to the PRX.
- Any `Required NativeLayout metadata field is missing` style refusal (ParsePair/
  assembler vocabulary, e.g. stripped BuildId): the staged INI was edited or truncated —
  restage the exact emitted bytes; never hand-repair identity fields.
- Publish-time refusal from `PublishStagedNativeLayoutMaps` (S2-ARM §1: not staged /
  axes unresolved / published-facts disagreement / staged-identity disagreement with
  resolved engine facts): the engine's own resolution disagreed with the pair's
  `EngineMajor=5/EngineMinor=6/NonShipping=0/Stats=0`. This is a REAL finding about the
  running image — record it; same-bundle/same-facts retry is the only retry; never
  edit the pair to match a refusal.

Consequence shape (expected, not a bug): without the pair the run ALWAYS ends inside
setup_unreal before any Lua — no `[TownfallItemSpawner]` lines, no engine-ready.

## T5. PRX-side environment failures

1. `UE4SS.log` absent entirely but module log healthy: log device open failed —
   `/data/plugins` not writable (the runtime writes UE4SS.log next to the PRX,
   UE4SSProgram.cpp:255-257). Fix writability of the PRX directory (caller decision;
   worker-ue4ss-settings flagged this as unsolved-by-settings).
2. Input failures: `Native supplied input source is unavailable or failed activation`
   / `Native input service is unavailable; disable bindings and clear InputSource`
   (UE4SSProgram.cpp:739-741): the retained PS5 pad provider did not activate (user
   sign-in state, pad open) — `General.InputSource` defaults to "Default" and is never
   parsed from the ini, so an available injected source is a hard prerequisite with
   HAS_INPUT composed in.
3. Pad config refusals: `PS5Pad.ini line N: <message>` / `must contain 1..16384 bytes`
   / `must be ASCII text without control bytes` / `requires at least one binding`
   (PS5PadConfig.cpp): the staged `ue4ss/PS5Pad.ini` (2960 B, sha `1d919fbf…`) was
   altered or truncated. All parser errors are hard refusals with line numbers.
4. Native C++ mod startup error: `Native C++ mod startup reported a program error`
   (UE4SSProgram.cpp:405-406) — none staged (Lua-only run); this would mean an
   unexpected dlls/native_plugin entry in the activation.
5. Activation wiring state (not a crash): healthy module log + NO
   `runtime-status.txt` + NO `UE4SS.log` = the launched PRX never reached
   `PrepareActivatedUE4SSRuntime` — expected for composition-3 as linked
   (MakeRuntimeAckConfig/StartActivatedRuntimeWithAck absent from its symbol table);
   requires the composition-4 recomposition (worker-integration-checklist STEP 6).
   Do not misread as T1.

## T6. Activation (mfa) failure — nothing installed / all-or-nothing

Symptoms: no ack file at all (the sink opens only AFTER the activation is consumed,
ue4ss_backend.cpp:100-102), module log shows the activation error path. mfa error codes
map directly (modforge_activation.h):

| code | meaning | check |
|---|---|---|
| MFA_E_NO_LOADLIST (-2) | loadlist.txt missing | root really is the installed generation under `/data/modbridge/roots/PPSA33286` |
| MFA_E_LOADLIST (-3) | malformed (err.line = load-list line) | row grammar `file <role> <path> <size>` |
| MFA_E_TITLE (-4) | installed for another game | header `title PPSA33286` vs cfg.title_id |
| MFA_E_LIMIT (-5) | too many files/bytes, path too long | totals (35390 B staged) far under limits — indicates wrong tree |
| MFA_E_OPEN/-E_READ (-6/-7) | file could not be opened/read | /data readable ONLY during module_start — activation must run there |
| MFA_E_SIZE (-8) | file shorter/longer than listed | loadlist size vs real bytes for EVERY row (779/588/186/2960/21415/0/9962) |
| MFA_E_DUPLICATE (-12) | same path listed twice | loadlist hygiene |

Recovery is always: fix the loadlist/bundle, re-run the installer verbs (register ->
prepare -> install); NEVER hand-edit files inside `roots/` (bypasses the transactional
installer and recovery journal).

## T7. Mod-level failures (engine up, mod down)

1. Mod not started: `enabled.txt` missing (presence enables; 0 bytes is fine) or the
   mod dir key shape wrong (`ue4ss/Mods/<Mod>/Scripts/main.lua`; `scripts` lowercase
   tolerated but mixed casing across files refused; `shared` is never a mod).
2. require() failure for UEHelpers: resolves ONLY to
   `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` (LuaMod.cpp:868/1256-1257/1387-1388) —
   check key path + preload.
3. Malformed mods.txt (only if one is ever staged): rows must be `Name : 0|1`;
   malformed throws (NativeModSource.cpp:113-132). Current plan stages none.
4. Engine-wait expiry: bounded object-construction wait (`GetNumElements() >= 10000`)
   with a 5-minute deadline, cooperative cancellation (UnrealInitializer); a deadline
   refusal means the engine never reached the expected state this launch — a real
   finding, record and stop.

## T8. Standing honesty gates during triage

- Never fake a verdict/scan fact/identity value; never edit the pair or ini to silence a
  refusal; never bypass the decryption oracle or the STOR gate.
- One launch per staged build; collect evidence between launches; do not replay failed
  registrations or duplicate payloads.
- "No crash" is not success; absence of facts is not correctness; only fresh log/ack
  mtimes prove this launch happened.
