# PAK RUNTIME MOUNT — the #7 variant that changes no game file (design)

Agent GL (ZCode), 2026-10-10. Claude round-6 item 3b: *"Design the #7 version
that changes no game file at all (runtime mount through the engine's own path,
data-only). The Leon method replaces the game's index in the dump, and a game
update would undo it."*

**DESIGN ONLY.** Mac on battery: no builds, no console contact, no code to
tree, no network. Nothing in this document authorizes any console action; every
console step is a proposal behind Claude review + explicit user OK, under the
permanent PS5 rules (AGENTS.md) and the 3-pass law. Companion same-round brief:
GK's pak-overlay generalized builder (in flight) — that builder remains the
INVASIVE-path tool (staged/disk overlay); this design is its NON-invasive
runtime complement.

## 0. Definitions and hard law

- **Data-only**: our PRX never writes a byte of any file in the game dump,
  never writes engine code pages, and never modifies a game executable. The
  only mutations permitted are (i) our own private memory, (ii) a copied
  vtable published by a single vptr store on an engine FILE-OBJECT instance
  (the proven VMT-pump pattern — data pages, XOM-safe, revertible), and
  (iii) calls INTO engine functions at pinned offsets (the proven
  engine-call pattern).
- **Mod storage**: all mod bytes live under our ue4ss root
  `/data/modbridge/roots/PPSA33286/` (in-process reads from there are
  console-proven — the Lua mod path already loads from this root). Nothing is
  ever staged into the game's Paks directory.
- **Update-survival goal**: a game update may invalidate our PINS (re-pinnable
  from the new image, fail-closed) but must never find us having left anything
  behind, and must never leave the mod half-applied against new content.

## 1. Ground truth this design stands on (provenance-tagged)

Tags: **[RE]** static eboot evidence (worker-kernel-pak, pinned decrypted
eboot sha `3b6cfad4…`, UE 5.6.1-release per GJ's official-source receipt) ·
**[GAME]** console-proven in-game behavior · **[PORT]** proven in our PRX
runtime · **[TK]** training-knowledge about stock UE5, NOT locally receipted —
must be confirmed before reliance · **[GAP]** not yet researched.

- **[RE]** The full stock PakFile module ships in the eboot:
  `FPakPlatformFile::Initialize` 0x1769ee0; folder scan `IterateDirectory`
  virtual slot **+0x1c0** on the INNER platform file (`[this+8]`); FindPakFiles
  0x176b410 (`*.pak`); priority ranker 0x1770140 (folder ranks 4/3/2/1,
  project-content highest).
- **[RE]** Mount-one-pak helper **0x176ec30**: `this` = FPakPlatformFile\*
  (RDI), an engine FString path (RSI: data/num/max + a bool at +0x1c), a
  priority (RDX), and a 5th arg (RCX). It FileExists-checks via inner-file
  vtable slot **+0x80**, allocates the 0x330-byte FPakFile, ctor 0x1771fa0,
  checks the validation success flag **[pak+0x2e1]**, normalizes the mount
  point, recognizes `_P.pak` (utf16 @0x775a23a), and appends a 0x38-byte
  record to the MountedPaks TArray at **[this+0x20]/count[+0x28]/max[+0x2c]**
  (filename FString at entry+0x10, priority dword at +0x20, FGuid at +0x24).
  Wrapper **0x176b900** computes the priority from the path then calls it.
  DLC/on-demand loop **0x1764de0** is table-driven (0x38-byte records, GUID
  compare) and mounts through the same helper.
- **[RE]** The utoc partition model is in-engine: multi-partition containers
  open `<stem>.ucas` (partition 0) and `<stem>_s<i>.ucas` (partition i≥1)
  (CUE4Parse `IoStoreReader.cs` partition walk, same convention the Leon
  sidecar exploited).
- **[GAME]** The Leon disk trial (`.superpowers/native-observations/
  leon-index-overlay-trial-2026-10-01/`, `iostore-sidecar-overlay-1/`):
  derived utoc = base index merged with mod entries redirected to a partition-1
  sidecar (`_s1.ucas`), plus merged container-header package store (38,458
  entries) — **the engine fully accepted the merged content: Leon appeared,
  game ran normally**. Its invasiveness: it REPLACED the game's
  `pakchunk0-ps5.utoc` on disc (backup + rollback machinery exists) — exactly
  what a game update (or any restore) undoes, and the motivation for this doc.
  Also **[GAME]**: `global.utoc` was NOT touched — chunk-utoc + sidecar alone
  sufficed for import resolution on this title.
- **[PORT]** Engine functions are callable from our PRX at pinned image
  offsets with behavioral verification (slot-2 FName ctor bound at 0x63d590
  and behaviorally verified; engine delete-listener ops add=0x8841A0 —
  L551/561/565 of the frozen session). FString marshaling through the engine
  allocator exists (round-2/3 `push_fstring_to_lua`/`from_fname` family).
- **[PORT]** The VMT tick pump (`VmtTickPumpCore.inc`): copy an engine
  object's vtable (offset-to-top + RTTI preserved, bounded text-validated
  scan), replace one slot, publish with ONE aligned vptr store, restore on
  retire with ownership check — host-tested mechanics, never writes the
  original table or any code page.
- **[PORT]** Admission/fault-ledger discipline: image-segment/VirtualQuery
  coverage before any engine-memory read; fail-closed pins (prologue-signature
  verification against the image sha, refusal on mismatch) per the pin law;
  per-version pins carried by GJ's generalized tables (`blade-prep/
  per-version-table.json` pattern).
- **[PORT]** Injection at game-process create (ploader ptrace, 50 ms pid
  poll; Phase C §2: startup/admissions at 00:53:05-06 while engine init
  continued for minutes). The `module_start_entered t_ms` diagnostic already
  exists in the frozen PRX — the decisive timing receipt for §3/B1.
- **[GAP]/[TK]** UE5 boot order specifics (chain construction vs
  `FPlatformFileManager::Initialize` scan timing; zen/IoStore container
  registration entry; whether .ucas reads on PS5 route through
  IPlatformFile::OpenRead or direct file handles) are NOT locally pinned —
  each is called out where it matters and assigned to Phase 0 or an offline RE
  pass.

## 2. Candidate mechanisms (all data-only)

### M-A — Engine mount call (runtime mount through the engine's own path)

**A1. Classic pak, call the engine's own mount helper [RE-complete, buildable
design].** After engine init (our runtime's existing wait loop), discover the
FPakPlatformFile instance: pin the `GPlatformFileManager` static (or the
chain-head accessor) by RE + pattern, read the topmost wrapper, walk `[+8]`
inner links, and CONFIRM the FPakPlatformFile layer by matching its vtable
against the pinned FPakPlatformFile vtable identity (census-agreement rule).
Marshal the pak path as an engine-allocator FString, then call the wrapper
0x176b900 (engine computes priority from the path) or the helper 0x176ec30
with an explicit priority — from the GAME THREAD via the proven game-thread
queue/drain (the MountedPaks TArray append is not thread-safe). Verify by
engine-state readback: MountedPaks count incremented, our 0x38-byte entry
present with our filename/priority, and — engine-side functional proof — a
virtual FileExists through the pak chain for a file that exists ONLY in our
pak. The pak lives in our mod root; the engine's own FileExists already runs
against that path through the same process syscalls our Lua mod loader uses
**[PORT]**, so sandbox reachability is proven. Fail-closed pins: image sha,
helper prologue signature, vtable identity; any mismatch = refuse, game runs
vanilla. Rollback = config-off (never call) or PRX lifecycle exit; no state
outlives the process.

- UE5 IoStore late-add question: A1 as stated covers CLASSIC paks (loose
  files: .ini/.png/.res — the ModBridgeRenderDiag class). IoStore cooked
  content is NOT registered by the classic helper **[RE: it allocates an
  FPakFile only]**. PC UE4SS never mounts paks itself (GF's source-verified
  finding) — there is no PC precedent for late IoStore adds to copy.
- **A2. IoStore container add [GAP — blocked on new RE].** The engine's zen/
  IoStore registration path (dispatcher mount + package-store merge; the
  global.utoc partition model) is a separate entry chain from 0x176ec30 and
  has NOT been located in the eboot. The DLC loop 0x1764de0 mounts through
  the classic helper, so it is not the container-add seam either. An offline
  eboot RE pass (Mac-safe; strings "IoStore"/"FIoDispatcher"/zen xrefs) is
  the prerequisite; until then A2 is design-only intent. Even once located,
  late store-merge semantics (packages already resolved at boot) may require
  pre-first-request timing — same window problem as B1.

### M-B — File-layer intercept (game reads "its" files; we substitute/extend)

The seam is the userspace **IPlatformFile chain** (ftpsrv is console-side and
irrelevant to the game process; kernel/SceFs redirection is Phase B — see
§5.B7). Both variants use the VMT-pump mechanics on the platform-file
INSTANCE(S) (data-only, revertible, no code writes). Slot facts already
RE-pinned: IterateDirectory **+0x1c0**, FileExists **+0x80** (OpenRead/
FileSize slots: small additional pin pass).

**B1. Pre-scan enumeration lie (engine-native everything).** Install an
IterateDirectory (+FileExists/OpenRead/FileSize) vtable swap as soon as the
platform-file chain instance exists but BEFORE `FPakPlatformFile::Initialize`
runs the Paks scan; when the engine enumerates its own `content/paks`
directory, our hook appends virtual names (`pakchunk9999-<mod>_<guid>_P.pak`
+ `.utoc`/`.ucas` trio) backed by our mod root. The engine's own boot path
then discovers, prioritizes (`_P` recognition is in the helper **[RE]**),
mounts, registers the IoStore container, and merges the package store — with
100% engine-native semantics and timing. This is the truest "runtime mount
through the engine's own path": we only make the directory listing honest
about files that exist elsewhere.
- **Timing window [TK→Phase 0]**: in stock UE5 the chain is constructed
  during early PreInit and `FPlatformFileManager::Initialize` (which drives
  the scan) runs later in PreInit; the width of that gap on THIS build is
  unmeasured, and our injection receipt (module_start t_ms vs first
  pak-directory enumeration) is not yet timestamped. If the window is missed,
  the run degrades GRACEFULLY to vanilla (engine enumerated real files only;
  our hook then just observes) — no crash path, which is why B1 is safe to
  try first among the intercepts.
- **Acceptance risk [GAP]**: engine acceptance of a FOREIGN container (the
  unmounted frozen-trial question) is unproved; B1 with the original PC Leon
  trio is precisely the clean experiment for it (and separately the
  PC-cook→PS5 platform question, #8's recook track — keep the two claims
  distinct).

**B2. Post-init content substitution (the Leon method moved to runtime).**
Hook OpenRead/FileExists/FileSize on the chain layer(s) the engine actually
uses, and: for `pakchunk0-ps5.utoc` serve our DERIVED index (built on the Mac
by GK's generalized builder from the CURRENT dump + mod, never written to the
dump); for `pakchunk0-ps5_s1.ucas` (which does not exist on disc) serve our
sidecar from the mod root; pass everything else through untouched (including
the real `pakchunk0-ps5.ucas` — the derived index keeps base rows pointing at
the real file, Leon-verified geometry). Our hook returns our own IFileHandle
object (our memory, our vtable) satisfying engine reads — no engine code or
data is written. Content acceptance is **[GAME]**-proven (the Leon trial
proved the engine loads exactly this derived-index shape); only the SEAM is
new.
- **Base-version guard (the update answer)**: our derived index records the
  base utoc sha256 it was built against; the hook reads the REAL utoc bytes
  itself, hashes, and on mismatch serves the real bytes unmodified (clean
  vanilla) + one-shot loud marker. A game update therefore can never leave a
  stale index against new content — the failure mode of the disk method is
  structurally eliminated.
- **[GAP] Seam coverage**: whether the engine's utoc/ucas opens on PS5 flow
  through the hookable IPlatformFile chain (vs direct kernel file handles in
  the IoStore reader — a real console-UE pattern) is unobserved. If .ucas
  bypasses the chain, B2 narrows (utoc-only substitution cannot redirect the
  sidecar) and the ladder falls back to B1/A2. Phase 0 settles this.

### M-C — Hybrid

- **C1 = A1 + B1/B2** (mount call for the classic stub pak + intercept for
  the IoStore set): pragmatic combination while A2 is unpinned; the stub-pak
  mount call is unnecessary if B1/B2 already deliver the trio (the Leon
  method needed no stub pak — **[GAME]**).
- **C2 = A2-native** (mount call for the container set, no intercept at all,
  if/when the engine's container-add handles indexing): the end-state; fully
  blocked on A2's RE. This is the "none" branch of the task's hybrid
  question.
- On this title the "global index" question is settled by evidence: the Leon
  trial touched ONLY the chunk utoc + sidecar; `global.utoc` needed no
  interception (**[GAME]**). So a hybrid that intercepts "only the global
  index" would be intercepting MORE than proven-necessary; the minimal set is
  chunk-utoc + virtual sidecar.

## 3. Risk / feasibility per mechanism

| | A1 mount call (classic) | B1 enumeration lie | B2 substitution | A2/C2 native add |
|---|---|---|---|---|
| Breaks on game update? | Pins move → re-pin via GJ tables; refusal = mod quiet, game vanilla. **Nothing dump-side relied on.** | Same pin class (vtable slots + helper signatures). The trio itself needs no rebuild. | Base-utoc sha guard → clean vanilla; mod needs REBUILD from new dump (GK builder) — the one per-update cost, fail-closed | Engine-internal entry per-version RE; refusal = quiet |
| Pins needed | GPlatformFileManager static; FPakPlatformFile vtable identity; helper/wrapper prologue sigs; MountedPaks offsets (all RE-known shapes; static pin pass required for the singleton) | + IterateDirectory/FileExists/OpenRead/FileSize slots; chain-instance discovery timing | Same as B1 + base-utoc sha + derived/sidecar byte identities | A2 entry: UNKNOWN (GAP) |
| Crash risk | Low: engine validates the pak ([pak+0x2e1] observable); wrong-thread call is the main hazard → game-thread drain only | Medium: engine consumes foreign trio at boot (CE-108255-1 class if bad); mitigated by engine's own validation + one-launch law | Medium: wrong bytes = loader crash; mitigated by sha guard + passthrough-default + abort-on-first-unknown | Medium-high: store-merge timing semantics unknown |
| Rollback | Config-off / PRX exit; nothing persists | vptr restore (VMT retire, proven) or never-arm; disc never touched | Same; per-open sha guard is an additional runtime fail-clean | Unknown (unmount semantics unresearched) |
| Delivers | Loose-file mods (ini/png/res) at runtime | Full asset mods, engine-native, no per-update rebuild | Full asset mods today-shaped (merged index) | Full asset mods, cleanest |

**Sequencing logic**: A1 is the smallest safe rehearsal of "call the engine's
mount machinery" with engine-state readback and no foreign-container gamble;
B1 and B2 are the two IoStore deliverers, gated on Phase-0 evidence (B1 on the
pre-scan window, B2 on seam coverage); A2 is the end-state behind new RE.

## 4. The ladder

1. **Phase 0 — observation (GF's mount-acceptance watch, extended, ONE
   reviewed launch, read-only)**: frozen build + an observation-only PRX arm
   (no intercepts, no calls): (i) receipt `module_start_entered t_ms` vs the
   eboot-monitor create bound; (ii) log every IterateDirectory/OpenRead on
   the chain with timestamps — specifically when `content/paks` is enumerated
   and how `pakchunk0-ps5.{utoc,ucas}` are opened (layer, this-pointer,
   handle type); (iii) confirm the chain walk matches RE (inner links,
   vtables). Pass criteria: we can state, with timestamps, whether our code
   was resident before the scan, and which seam serves the utoc/ucas opens.
   This single launch decides B1-vs-B2 feasibility and feeds A1's singleton
   pins. (Observation-only hooks still require Claude review + user OK; they
   are the VMT pump with logging-only slots.)
2. **Rung 1 — A1**: classic diagnostic pak from our root, engine-state
   readback as the acceptance signal (no visual claim). Gate to rung 2:
   3/3 passes (§5).
3. **Rung 2 — B1 or B2, chosen by Phase 0**: B1 if the pre-scan window is
   real (preferred: no per-update rebuild, engine-native registration);
   B2 if the window is missed but the read seam is covered. Either way the
   acceptance bar is an in-game observable (Leon-class) + zero dump changes.
4. **Rung 3 — A2/C2**: offline RE pass to pin the zen container-add; only
   then design the native late-mount call. Supersedes the intercept for new
   containers if it proves supportable.

## 5. Console test plan (3-pass law) — PROPOSAL, per rung, behind Claude review + user OK

Common preconditions every pass: standard verified launch prep (frozen-PRX
discipline: decrypt-off readback, one ploader dispatch, klog tap to a NEW
file, eboot monitor before launch); **full game-dump hash manifest BEFORE and
AFTER** (recursive LIST + sha256 of every file under the game root; any
executable-class file hashed decrypt-off per the PS5 law; paks/utoc/ucas are
data and hash directly; param.json/mount.lnk guards recorded as in the Leon
profile); saves backed up; ONE user launch per boot; clean in-game exit.

- **Pass criteria (all three passes of a rung)**: (1) mod content visible —
  rung 1: engine-state readback (MountedPaks entry + engine-side FileExists
  of a pak-only file); rung 2: KNOWN asset substitution visible in-game
  (user observation + image receipt; directory listings/counters do NOT
  count — container-survey law); (2) **the ENTIRE dump manifest is
  byte-identical before/after** (this is the round-6 3b contract made
  testable); (3) clean rollback: a separate launch with the arm OFF shows
  fully vanilla behavior (Leon absent / no pak record) and identical dump
  hashes; (4) no klog crash-watch hits (Z1 list); identical outcomes across
  the 3 restarts with byte-identical mod inputs.
- **Fail criteria**: any crash (harvest, reboot, stop — never rapid-fire);
  any dump hash change (readback guard violated → stop, report); divergent
  outcomes across passes (stop; suspect timing window — especially rung B1);
  engine-state readback absent while bytes are provably ours (blind-replay
  prohibition: iterate on evidence, never resubmit unchanged).

## 6. Honest blockers (and the escalation law)

1. **A2 native IoStore late-add is unpinned** — the zen/FIoDispatcher
   container-registration entry and late store-merge semantics need an
   offline eboot RE pass; may prove to require boot-time discovery, in which
   case B1 is the only native-registration route and A2 closes.
2. **B1's pre-scan window is unmeasured** — stock-UE5 boot order says the
   window exists **[TK]**; our injection-time receipt is not yet
   timestamped. Miss = graceful vanilla (safe), but the rung then depends on
   B2/A2.
3. **B2's seam coverage is unobserved** — if PS5 .ucas reads bypass
   IPlatformFile (direct handles), substitution cannot redirect the sidecar;
   Phase 0 must see the actual open path before any B2 build.
4. **Foreign-container acceptance unproved** (B1's gamble); merged-index
   content acceptance proven **[GAME]** (B2's foundation). Keep the claims
   separate from the PC-cook platform question (#8).
5. **Missing static pins**: GPlatformFileManager static / chain-head
   accessor, FPakPlatformFile vtable identity, OpenRead/FileSize slots —
   small offline RE additions, needed before rung 1 builds.
6. **Late classic mounts deliver only post-mount lookups** — config already
   consumed at boot will not re-read; A1's value is runtime loose-file mods,
   not boot config injection (say so; do not oversell).
7. **Kernel file-mediation is out of scope**: SceFs-layer redirect needs new
   kernel-resident code — Phase B ladder L4, PROHIBITED this phase (GD §4.1).
   Escalation per the Phase C law: **if** Phase 0 shows both userspace seams
   unusable (mount API needs engine-internal state we cannot safely reach
   AND the intercept seams are bypassed by the engine's container reads),
   then "no game file change at all" is infeasible without kernel mediation
   — STOP and escalate to Claude with the Phase-0 artifacts; do not proceed
   to kernel code or file staging on our own authority. (The Leon disk
   method remains the proven fallback, GK's builder its tool.)

— agent GL (ZCode), 2026-10-10. Design only; no builds, no console, no tree
edits, no network. Read-only on all .superpowers records cited above.
