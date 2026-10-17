# EXEC-WRITABILITY-PROBE-DESIGN — IGW-1, the definitive in-game exec-writability probe

Worker: worker-exec-writability, 2026-10-04. PROPOSAL-ONLY: nothing built, nothing staged, no
console contact. The controller + user own staging and launch (see §8; consistent with
R-EXEC-01's requirement that any probe touching exec pages be a NEW falsifiable design with
user coordination). Companion to `EXEC-WRITABILITY-EVIDENCE.md` (evidence + citations; path
abbreviations reused: `TEAM=`, `SDK=`, `CON=`).

## 0. The one question, falsifiable

Can the in-game module, via `mprotect` (raw syscall 74, the only mechanism — EVIDENCE §3),
change the game text span `[0x400000,0x787c000)` (live VMMAP prot=0x4)?

- **H1 (RX):** `mprotect(page, 0x4000, PROT_READ|PROT_EXEC)` on one far text page returns 0
  and a fresh mapscan shows prot=5.
- **H2 (RWX):** `mprotect(page, 0x4000, PROT_READ|PROT_WRITE|PROT_EXEC)` returns 0 and the map
  shows prot=7, and a one-byte write+readback+restore on a mapping-roundup address succeeds.
- **H3 (restore):** after each success, `mprotect(page, 0x4000, 0x4)` returns 0 and the map
  shows prot=4 again.
- Each H is refuted by a recorded nonzero rc (errno captured) or a post-syscall crash
  (localized by which record is missing — the igf lesson, EVIDENCE §2).

## 1. Hard constraints (each traced to a lesson)

| # | Constraint | Source lesson |
|---|---|---|
| C-1 | Return session, ONE module, ONE launch, dispatched EXACTLY ONCE | return-session law (`TEAM:worker-return-session-master-plan/RETURN-SESSION-MASTER-PLAN.md` header); never relaunch an identical rejected layout |
| C-2 | Fresh trusted map persisted (as a record) BEFORE any transition attempt; every transition verified against a NEW mapscan afterward | igf4/igf5/igf7 map-gating discipline; controller correction in `TEAM:worker-code-anchor-reads/REPORT.md` §2 |
| C-3 | Write-through per-attempt records: the sink's synchronous `sceKernelWrite` (O_APPEND) flushes each record BEFORE the next syscall is issued | igf5/igf6 recovery pattern — the missing record localizes the fatal attempt (EVIDENCE §2.2) |
| C-4 | Incremental transitions with restore; never two pages in flight; order X->RX first, then X->RWX, restore+verify between | task directive + igf5 semantics (host-tested restore + anomaly flag, `TEAM:worker-fname-runtime-probe/REPORT.md` items 2, 8) |
| C-5 | ALL transition pages far from every known hook target (Tick 0x33ac8c0, SCO 0x890aa0, FName ctor 0x63d590 + 9 call sites); the RWX write-test targets an address PAST the file-image extent 0x7879b1c (mapping roundup — cannot be image code) | EVIDENCE §1 items 5-6; SLOTS-FEASIBILITY §1 LOAD0 extent |
| C-6 | NO new imports: single inline syscall-74 helper; forbidden-symbol gate keeps `sceKernelMprotect` out; constants parsed at build time from pinned SDK headers (`SYS_mprotect 74` `SDK:sys/syscall.h:82`; PROT_* `SDK:sys/mman.h:52-55`; `PAGE_SHIFT 14` `SDK:machine/param.h:99`) | igf5/igf7 build gates (`TEAM:worker-igf5-diagnosis-rebuild/REPORT.md` Part 2; `TEAM:worker-combined-igf7/STAGE-PLAN.md:56-63`) |
| C-7 | No repeated text-read attempts beyond the design; a refusal or anomaly STOPS the ladder (fail-closed) | user directive in `zcode_igf6_RESULTS.accurate_recording`; igf5 refusal semantics (no read unless mprotect succeeded) |
| C-8 | Distinct log path (single-writer discipline): `/data/modbridge-exec-writability.jsonl` — NOT the qdrv1-owned `/data/modbridge-ingame-facts.jsonl` | `TEAM:worker-game-session-ridealongs/REPORT.md` §1 item 3 (interaction audit) |
| C-9 | No threads, module stays loaded but INERT after module_start returns 0 | igf7 packaging contract (`TEAM:worker-combined-igf7/STAGE-PLAN.md:56-60`) |
| C-10 | No signal handling exists or is attempted (no sigaction import in the frozen contract); a syscall fault is UNTRAPPABLE — containment is by record-localization, not by recovery | frozen-191 audit (`TEAM:worker-import-audit-weakify/AUDIT-RESULTS.md`); `TEAM:worker-code-anchor-reads/REPORT.md` §2 |

## 2. Import contract

Exactly the frozen 191-strong rows (`TEAM:worker-import-audit-weakify/AUDIT-RESULTS.md` §2.4);
the build asserts: 191 UND imports all GLOBAL, zero weak, `sceKernelMprotect` on the
forbidden-symbol list, exactly ONE raw `syscall` instruction site (0F 05) in the inner ELF
.text (the helper; igf5 had 2 sites, igf7 asserted 0 — this build is a new, deliberately
syscall-bearing design whose gate is `== 1`). sysctl (202) for mapscans and getpid (20) are
inside the proven 191 (NID-verified, same audit).

## 3. Module and session identity

- Module: `modbridge_exec_writability.prx`, build_id `igw1-<8hex>`; log
  `/data/modbridge-exec-writability.jsonl`; same ploader packaging recipe as the igf4/igf6
  lineage (import_references-first combine, Packager.dll in place, `finalize_sdk.py` SDK
  0x11000000 stamp, pinned make_fself; same-module-name replacement discipline applies to
  IGW-1 only, never to a divergent/unowned file).
- Determinism: two-run byte-equal build gate (lineage standard).
- Probes vendored byte-identical from the igf5 lineage: `record_sink.c`
  (sha `2b6a8c24…`, `TEAM:worker-fname-runtime-probe/INTEGRATION.md` §1), mapscan, startup
  plumbing. The mprotect helper is igf5's `igcar_sys_mprotect` pattern
  (`TEAM:worker-code-anchor-reads/REPORT.md` §3) reduced to one call site.

## 4. Target page arithmetic (build-time computed, recorded in every record)

- Transition page: the LAST 16-KiB page of the live span, `page = 0x7878000`
  (covers `[0x7878000,0x787c000)`). Distances: Tick page 0x33ac000 is 0x44CC000 below;
  FName-ctor page 0x63c000 is 0x14C000 below; SCO page 0x890000 far below. All known targets
  clear (C-5).
- The page straddles the file-image edge: LOAD0 file bytes end at runtime 0x7879b1c
  (SLOTS-FEASIBILITY §1); `[0x7879b1c,0x787c000)` is mapping roundup. **Write-test address:
  `0x787b800`** (> 0x7879b1c, in roundup — no file-backed byte can be code there). Read-test
  address: `0x787b800` as well (expect zeros or garbage; RECORDED, never gated: the RX test
  gates on rc + map prot, not on byte values, since roundup contents have no retained-image
  expectation).
- Build script computes all of the above from the SDK-parsed PAGE_SHIFT and the recorded live
  span constants; nothing hand-magic'd in C.

## 5. Record contract (seq, envelope, fields)

Envelope = lineage exact: `{"marker":"modbridge.ingamefacts","schema":1,
"build_id":"igw1-<8hex>","seq":N,"exp":"<name>","rc":<int>,"errno":<int>,"fields":{…}}`
(igf-lineage writer, byte-identical vendored sink). All records write-through before the next
action (C-3).

| seq | exp | fields (beyond defaults) | stop-on |
|---|---|---|---|
| 1 | `entry` | `phase=module_start`, note | — |
| 2 | `startup` | `ctors_run`, `channel_open_rc`, `entry_marker_written` | — |
| 3 | `map_pre` | full mapscan context: `total`, `ksz_144`, `trusted`, `readable_spans`, `exec_span_start/end`, `exec_prot_hex` (must be 0x4; **gate: if exec span not found or trusted!=1 → seq 12 summary `refused=map_untrusted`, NO transition, exit 0**) | map untrusted |
| 4 | `rx_request` | `transition="X->RX"`, `page_start=0x7878000`, `page_size=0x4000`, `pages=1`, `prot_requested_hex=0x5`, `prot_before_hex=0x4`, `prot_before_source=current_map`, `syscall_impl="inline74"`, `attempt=1` | — |
| 5 | `rx_result` | `mprotect_rc`, `mprotect_errno_hex`, `returned=(rc==0)` — **recorded BEFORE anything else runs; if rc!=0 → seq 12 summary `refused=rx_denied`, NO read, NO further syscall** | rc!=0 |
| 6 | `rx_mapverify` | fresh mapscan context; `map_prot_after_hex` (expect 0x5), `map_matches_rc` | mismatch → anomaly, still continue to restore |
| 7 | `rx_readtest` | `read_addr=0x787b800`, `bytes_read_hex` (16 B), `read_completed`, `note=roundup_no_expectation` | — |
| 8 | `restore_rx` | `restore_called`, `restore_rc`, `restore_errno_hex`, `restore_prot_hex=0x4`, second fresh mapscan `map_prot_after_hex`, `mprotect_restored`, `anomaly` (**anomaly stops the ladder: never continue from an unrestorable page**) | anomaly |
| 9 | `rwx_request` | `transition="X->RWX"`, same shape as seq 4, `prot_requested_hex=0x7`, `attempt=2` (only reached if 5-8 all clean) | — |
| 10 | `rwx_result` | `mprotect_rc`, `mprotect_errno_hex` (THE number the port needs; if rc!=0 → seq 12 summary `refused=w_denied` with errno) | rc!=0 |
| 11 | `rwx_verify` | fresh mapscan `map_prot_after_hex` (expect 0x7); `write_addr=0x787b800`, `orig_byte_hex` (read), `written=0xA5`, `readback_ok`, `reverted_ok` (orig byte re-written + re-read), then **restore** block identical to seq 8 (`restore_rc`, `map_prot_after_hex`, `mprotect_restored`, `anomaly`) | write-readback fail → anomaly |
| 12 | `summary` | `rx_allowed`, `rx_errno_hex`, `rwx_allowed`, `rwx_errno_hex`, `restores_ok`, `anomalies`, `map_trusted_every_scan`, `decision_input={W_WORKS,W_FAILS_R_WORKS,BOTH_FAIL}` per §7 | terminal |

Conditional skips emit their seq as a refusal record with `refused=<reason>` so the sequence is
always contiguous 1..12 in the log (auditable; a missing seq localizes a fatal fault to the
preceding action, per C-3).

## 6. Launch integration — ride the comp6 session

1. **Session:** the comp6 launch session of `RETURN-SESSION-MASTER-PLAN.md` (comp6 one-pass
   build → console batch → launch). IGW-1 rides as an additional late module — it does NOT
   alter the composition candidate or any staged pair.
2. **ploader.ini:** append-only edit AFTER the composition line, per ridealongs law (RETR
   pre-image first, append-only, never recreate; `TEAM:worker-game-session-ridealongs/REPORT.md`
   §1 item 2):
   `/data/plugins/modbridge_exec_writability.prx = 2`
   - mode 2 = late (stable 1 s, settle 0.6 s, 25 s cap/plugin, 120 s overall), injection in
     file order → IGW-1 loads LAST, after the composition line and the existing ridealongs.
   - Slot check: pre-image has 4 lines (3 active + 1 mode-0); ridealongs bring 6 total lines
     (5 registered); +IGW-1 = 7 lines, 6-7 registered — inside MAX_PLUGINS=10/title.
   - Rollback: flip the added line to `= 0` (parser-skipped, consumes no slot) or remove the
     appended line; both are append-scoped edits, no other byte touched.
   - **Coordination note:** the master plan's Phase-0 gate pins a pre-image sha (`8d331b3f…`);
     this deliberate addition changes the post-image sha — the master plan must be updated
     EXPLICITLY with the new expected pre-image before Phase 0, never silently.
3. **Staging:** upload + SHT-strip readback per the pinned readback law (ridealongs SC steps
   pattern); hash pin recorded in the staging receipt before any launch.
4. **What the session must NOT contain:** no other exec-touching attempt in the same launch
   (FC-1 stays held until this probe's outcome is reconciled;
   `TEAM:worker-project-state-index/INDEX.md:135`).

## 7. Decision tree (post-probe, bounded follow-ups)

### 7.1 W-works (seq10 rc==0, seq11 readback+restore ok)
- **Meaning:** the PLH mechanism (RWX-then-patch-then-restore, x64Detour.cpp:398-408) is
  kernel-permitted on this target. The port's hook architecture runs as designed, including
  the init-armed FName-verifier and EngineTick detours and on-demand RegisterHook/ProcessEvent.
- **Still required before any hook arms (engineering, not research):**
  1. Implement the PolyHook Prospero branches: `MemProtector.cpp` translate
     (0x1/0x2/0x4 <-> ProtFlag) + restore, `MemAccessor.cpp` old-protection retrieval via the
     proven sysctl-mapscan (igf restore discipline), `Misc.cpp` executable-near allocator via
     `mmap(PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANON)` (or PLH VALLOC2/CODE_CAVE
     schemes) — the missing verified contracts (`SPIKE:ue4ss-polyhook-platform-port-1/report.md:20-22`).
  2. Host-prove the full PLH hook() path against a synthetic RX page (the FC-1 host-fixture
     pattern).
  3. First real hook target = EngineTick ONLY, with the loud-throw path intact
     (`SINI:1384-1387`), user-coordinated.
- **Caveat carried honestly:** one page's success strongly implies (but does not formally
  prove) sibling pages in the same mapping behave identically (EVIDENCE §4); the integration
  plan re-verifies the actual target page (Tick page) via mapscan before the first install.

### 7.2 W-fails, R-works (seq5 rc==0, seq10 rc!=0 with errno recorded)
- **Meaning:** reads can be enabled (RX) but text is not writable. PLH detours are DEAD in
  their patch-the-prologue form — **honest PLH assessment: PLH2 offers NO no-write hook mode**;
  `hook()` always writes the target (jmp + nops); there is no scheme that redirects control
  flow by reading alone. "Trampoline pages in writable memory + a one-time…" anything still
  requires the one-time WRITE into the target's instruction stream or its dispatch data. The
  port must switch interception to DISPATCH-DATA writes, which live in serviced writable
  memory (proven write class: igf4 GUA writeback, prot=0x3 span):
  - **vtable-swap hooks** for vtable-dispatched targets: EngineTick (C4; the settings lever
    `EngineTickResolveMethod=VTable` already resolves the address data-side,
    `TEAM:worker-tick-witness-design/TICK-WITNESS-DESIGN.md` §6 item 2), ProcessEvent (C8),
    the C5-C9 vtable family. Swap the vtable slot to a module-owned thunk that calls the
    original — a data write, no exec-page write.
  - **GNatives/function-table swap** for BP-native stepping (slot 8) — data table.
  - **FName ctor verifier (C1):** replace the detour with the already-prepared FC-1 validator
    — NOW LAUNCHABLE because RX works (its mprotect-first read is exactly H1; it was held
    pending this decision, `TEAM:worker-project-state-index/INDEX.md:135`).
  - **Reads unblocked:** vtable walks' jmp-probe (FLAG-2), ProcessInternal derivation
    (FLAG-3), PLH disassembly — all proceed with RX enabled per page (integration re-verifies
    each target page via mapscan first).
- **Design sketch for the swap (bounded):** module-owned thunk pages via
  mmap(PROT_READ|PROT_WRITE|PROT_EXEC) at init (or RW-then-mprotect-exec if RWX anon mmap is
  refused — a separate small probe question, same syscall, on OUR OWN mapping, zero game-text
  exposure); patch = single 8-byte vtable slot store + original saved; restore = store back;
  ordering per igf discipline (map-verify -> write -> readback-verify -> restore-verify,
  anomaly fail-closed).

### 7.3 Both-fail (seq5 rc!=0, or any fatal fault; RX denied)
- **Meaning:** exec pages are untouchable from ring 3 on this target: the hook architecture is
  DEAD here. All init-armed detours must be disabled or replaced (settings:
  `HookProcessInternal=false`, `HookProcessLocalScriptFunction=false`; FName verifier gated on
  seed provenance or FC-1-if-RX-ever-opens; EngineTick hook abandoned) — otherwise C1/C4 throw
  at init (loud) and the composed runtime never reaches mods-started.
- **What the port could still do data-side (from SIGNATURES-INVENTORY verdicts):** seed-bound
  data slots (0/2/3/4), layout inis + profile publication (C12/C14), Lua VM + mods (D1),
  FName ctor via derived constant — CALLS still EXECUTE (X stays set; execution is unaffected
  by lack of read permission… as far as the igf6 evidence characterizes reads; calls into text
  by the game itself obviously run), Conv_NameToString fallback, StaticConstructObject spawn,
  property access/reflection, MemberVariableLayout-driven behavior, direct vtable-target
  INVOCATION (calling ProcessEvent, not hooking it). What is lost: interception of engine/BP
  execution, engine-tick closures (E7) unless replaced by a worker-thread polling loop or
  Lua-timer cadence, RegisterHook-style mod hooks (E4/E5).
- **Escalation:** stop; reconcile with controller; the decision input `{BOTH_FAIL}` plus
  recorded errnos is the terminal fact for this architecture on this firmware.

### 7.4 Fatal-fault branch (a seq is missing = the probe died at the preceding action)
- If death follows seq 4/9 (i.e., inside the mprotect syscall): this is igf5's shape — the
  syscall itself is fatal in ring 0. Terminal for ANY syscall-74 approach; no retry, no
  variant of the mechanism; hook architecture dead per §7.3 and the syscall route
  permanently closed (`zcode_igf5_diagnosis` action row already forbids reusing the shim).
- If death follows seq 6/11 (mapscan/read): a NEW fact class (post-transition mapscan or
  roundup-page read faulting); stop, preserve, reconcile — not a replay.

## 8. Risk statement (explicit, for the go/no-go)

The probe retains igf5's residual risk class: if syscall 74 is wrong-numbered or fatal on this
kernel, the likely outcome is console + game loss (observed once, attribution leading-not-
confirmed). C-2/C-3 bound the blast radius of KNOWLEDGE (we always learn which attempt died)
but NOT of the crash itself. RX-first ordering means the highest-value question (can reads be
enabled — unblocking FLAG-2/FLAG-3/FC-1 and the C2 anchors) is answered by the FIRST syscall,
before the riskier W attempt. If the controller judges console-crash risk unacceptable, IGW-1
does not run and the port proceeds on §7.3's data-side plan with the hook architecture treated
as dead on this target. Idle discipline: no re-submission of an identical rejected layout; any
IGW-2 must be a distinct falsifiable design.
