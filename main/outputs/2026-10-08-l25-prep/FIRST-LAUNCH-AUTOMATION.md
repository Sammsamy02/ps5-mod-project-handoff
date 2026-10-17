# FIRST-LAUNCH AUTOMATION — on-console profile generation + self-test

Agent GI (ZCode), 2026-10-08-l25-prep. DESIGN ONLY, battery mode: no builds,
no console contact, no candidate-tree edits. Binding inputs:
`CLAUDE-ROUND5-ADDENDA.md` (A1-A5), `FZ-GAME2-PROFILE-DRYRUN.md`,
`outputs/2026-10-08-l26-runtime/profile/units/*` (how the pinned profile is
received today), `UnrealInitializer.cpp` admission phases,
`phasea-tools/engine_match.py`, `mod-manager/modmanager.py` (GG),
`mod-ladder/MANIFEST.md` + `VERDICT-SCHEMA.md` (FO), the ueselftest laws
(OTHER-ENGINES-RESEARCH.md section 0 restatement). Knowledge-source labels as
GH used them: [LOCAL-SOURCE], [TRAINING-KNOWLEDGE], [INFERENCE], [DESIGN].

Companion artifacts of this task:
- `phasea-tools/release_audit.py` (Rule-10 gate; section 4)
- `tables/ue5.6-prospero-shipping-nostats.layout-table.example.json`
  (schema example; section 3)
- `agent-GI-firstlaunch.md` (compact report + audit findings)

---

## 0. The claim this architecture must make true

A5 [LOCAL-SOURCE: CLAUDE-ROUND5-ADDENDA.md]: the profile generator AND the
self-test run ON THE PS5 at a new game's first launch, from per-version
layout tables that SHIP WITH THE TOOL, measured once per Unreal version on
the Mac. Zero Mac/AI/manual steps per game. Coverage is per ENGINE (~95% of
Unreal games, A1). The blind test (A2): the second game gets a
generator-only profile — no hand values — under Rule 10 extended to games.

Everything below is the conversion of today's Mac-side pipeline
(FZ-GAME2-PROFILE-DRYRUN.md section 3: identity → axes → anchors →
member/vtable offsets → unit emission) into an on-console flow whose every
step reuses an already-console-proven primitive.

---

## 1. Inventory: what exists and where it plugs in

| Stage (FZ section 3) | Today (Mac, offline) | First-launch (console) | Reused runtime machinery |
|---|---|---|---|
| Image identity | sha256/size/extent of the downloaded eboot | live module view + phdr spans | `module_view_base()`, `bridge_code_cache_context` whole-image identity, `bridge_readable_span_*` [LOCAL-SOURCE: UnrealInitializer.cpp Phase 1] |
| Axes (family/version/config/stats) | `ref_axes.py` / `engine_match.py` over the 244-marker table | marker pack shipped per version; scan the loaded image in memory | admission-owned readable-span reads; utf-16le scan semantics identical to `scan_markers` [LOCAL-SOURCE: engine_match.py] |
| Anchor pins | `run-blade-profile.py` (GJ, LANDED): census/consensus arms — LEA-xref census, prefix consensus, UTF-16 census — with the patternsleuth pattern sets kept as PROBES (they went 0/9 GUO and 0/14 GM image-wide on Townfall: compiler idiom drift; a pattern-only agreement bar wrongly refused the positive control) | same arms as table data; census + consensus over the live image | prologue-signature verification (`rc::fnameabi::signature`), `rc_seed::classify_seed_address` (DataReadable/CodeCallOnly), pin registry `rc::codecache::named_entry` [LOCAL-SOURCE: UnrealInitializer.cpp Phases 2-6, CodeSignatureProfile.hpp; agent-GJ-blade-prep.md section 2 correction] |
| Member/vtable offsets | STATIC-FACTS/REFLECTION-FACTS/CLASS-SIZE evidence families, two-source agreement | per-version table rows (engine-level); NOT measured per game | the pin table becomes data instead of compiled `kRequiredMembers` [LOCAL-SOURCE: RuntimeBuildProfile.hpp] |
| Verification | (console: seed admission) | behavioral probes retargeted at generated pins | slot-2 ctor ByteProperty probe, pool words ('None'/'ByteProperty' ids), GMalloc V1/V2, SCO empty-callback [LOCAL-SOURCE: UnrealInitializer.cpp] |
| Emission | `generate_profile.py` → HPP units | JSON profile persisted to the game's mod-root slot | none needed — the JSON *is* the deliverable |

The key architectural move: **the compiled-in pinned profile
(`RuntimeBuildProfile.hpp` / `CodeSignatureProfile.hpp` / seed table)
becomes a runtime-GENERATED, runtime-VERIFIED, PERSISTED JSON**, produced on
console from shipped tables. The admission phases do not change — their
input changes from compile-time constants to generated+verified pins.

---

## 2. Architecture (the on-console first-launch pipeline)

```
game launch -> ploader PRX at process start (unchanged)
   |
   v
UnrealInitializer boot
   |
   [FL-0] profile slot present?
   |--- yes -> load JSON -> re-verify (2f) -> arm admission phases (today's path)
   |--- no  -> FIRST-LAUNCH GENERATION (this doc)
                |
   2a DETECT --+-- scan image under admission budget (marker pack)
                |     verdict: family/version/config/stats + margins + tier
                |     tier REFUSED => loud refusal, no mods (Astro Bot path)
                |
   2b GENERATE -+-- select layout-table cell; digest check
                |     anchor pattern-scan (readable spans, candidate caps)
                |     multi-pattern agreement -> image-relative labels
                |     member/vtable rows from the table
                |     VERIFY: behavioral probes (Phase-4c-style) on the pins
                |     any refusal => fail-closed, no partial profile
                |
   2c PERSIST --+-- write profile.json to the game's mod-root slot
                |
   2d SELF-TEST-+-- GB's ueselftest rides once (Lua, game-agnostic)
                |     N-of-M >= 95% AND critical families pass => PASS
                |     non-critical shortfall => mods load with WARN
                |     critical failure => SAFE-MODE (mods off, diag on)
                |
   2e MODS ---->+-- PASS/WARN -> mod-manager enable-set arms (~5 mods,
                   one per type); handoff to GG's deploy
```

### 2a. DETECT — engine family + version at runtime

**Marker pack as a runtime signature table.** The patternsleuth
engine_version table (30 versions UE4.7-5.8, 244 markers, gates for
build-config/stats) [LOCAL-SOURCE: engine_version.rs, used by ref_axes.py,
engine_match.py] ships as data inside the per-version layout tables'
`detection` section — per version: the triage subset (16 widest-coverage
ungated markers), the full marker list applicability, and the tier
thresholds. This is engine-level data; no game names (Rule 10, section 4).

**Scan target: the loaded image in memory.** The runtime already
establishes the module view, base, and whole-image identity at Phase 1
[LOCAL-SOURCE: UnrealInitializer.cpp `consume_required_slot_seeds` Phase 1:
`module_view_base()` + `bridge_code_cache_context`]. Detection scans the
admission-owned readable spans of that image — the same spans
`bridge_readable_span_at` enumerates and PropertyReadBytes-class guards
police. utf-16le presence semantics are byte-identical to
`engine_match.py::scan_markers` so the Townfall ground truth (54 markers
present, stats-off margin 12, C+Python agreement
[LOCAL-SOURCE: RuntimeBuildProfile.hpp]) reproduces on console.

**Verdict + confidence.** The cost solve (`cost_solve`: cost 5 for
present-but-uncovered, 1 for covered-but-absent; per-axis margins) runs
unchanged, with the same tier ladder [LOCAL-SOURCE: engine_match.py]:

- `REFUSED` — triage < 3 or full-scan < 8 present: not an Unreal image.
  Loud refusal; profile generation MUST refuse. This is the Astro Bot
  0/244 outcome promoted to a product feature [LOCAL-SOURCE:
  FZ-GAME2-PROFILE-DRYRUN.md section 2].
- `FAMILY_ONLY` — UE4 vs UE5 resolved, minor version ambiguous: generate
  with the family's anchor pack; version recorded as unresolved; the
  self-test critical bar still applies. [DESIGN: no versioned member rows
  may be armed in this tier — anchors only, structural pins from the
  nearest compatible cell are NOT armed.]
- `VERSION_TENTATIVE` (margin >= 4) / `VERSION_CONFIDENT` (margin >= 12) —
  generate with the cell's full table.

### 2b. GENERATE — on-console profile generation from layout tables

**Table selection.** `(family, version, build_configuration, stats,
platform)` cell -> one layout-table JSON. Platform is fixed prospero for
PS5 eboots; the cell axes come from 2a. Table digest verified before use
(`compat.digest` in the schema example); missing/failed table => refusal.

**Anchor arms (census + consensus, not pattern-only).** GJ's landed
runner proved — by wrongly refusing the Townfall positive control at first —
that pattern-only multi-pattern agreement is NOT the arm that pins real
images: the shipped patternsleuth pattern sets went 0/9 (GUObjectArray) and
0/14 (GMalloc) image-wide on Townfall (compiler idiom drift)
[LOCAL-SOURCE: agent-GJ-blade-prep.md section 2 honesty note; worker-
required-slots-feasibility REPORT.md]. The anchors that ACTUALLY pinned
Townfall, and which the on-console generator therefore implements, are the
census/consensus arms [LOCAL-SOURCE: run-blade-profile.py]:

- **xref census**: count LEA rip-relative references image-wide; rank target
  labels into a candidate POOL (GUObjectArray label = the 1565-lea receipt
  reproduced exactly). A census top-rank is a candidate, never a pin by
  itself — hot globals can outrank the target (GJ honest limit (a)).
- **prefix consensus**: expected prologue prefix bytes at candidate
  function labels (the SeedTable derivation route: UTF-16 module-string
  cluster → xref census → `mov edx,1` + nearest `e8 rel32` consensus →
  call-site re-verification).
- **UTF-16 census**: unique engine module strings (Engine, Renderer,
  AnimGraphRuntime, Landscape, RenderCore; FMallocBinned3 x4) — the
  module-string cluster that seeds ctor detection on any version.

The table's `anchors` therefore carry **arms** (census/consensus specs +
extraction rules) with the byte patterns demoted to `probe_patterns`
(advisory corroboration; a probe hit is a candidate hint, a probe miss is
NOT a refusal). Selection from the census candidate pool is completed only
by the verify pass below (live shape validators + behavioral probes +
two-source law); `agreement.min_arms` (default 2 independent arms) replaces
`min_patterns`. Every candidate address is still classified by range class
(`rc_seed::classify_seed_address`: DataReadable for data anchors,
CodeCallOnly for function anchors) and candidate-count overflow still
refuses [DESIGN: generalizes GJ's landed arms to the runtime; pattern
vocabulary unchanged from the offline scans].

**Safe-scan envelope (the laws that make console-side scanning safe):**
[DESIGN, each clause anchored to a proven constraint]

1. READ-ONLY: the generator never writes, never mprotects, never installs
   anything. It only reads admission-classified readable spans. (Code
   anchors are prologue-VERIFIED through the existing owned-copyin
   signature path — exactly how Phase 4 verifies `kPrefix` today — never
   raw-dereferenced.)
2. ADMISSION-BUDGETED: generation runs INSIDE the existing shared
   admission budget regime (`kAdmissionBudgetMs` class; the L14 one-total-
   budget doctrine). Budget expiry mid-scan => refuse; no retry storm, no
   second budget. The [Seeds] phase-armoring precedent (loud-before-
   rethrow) applies verbatim.
3. GUARD-CLASS: scanned reads go through the PropertyReadBytes-class
   reader (the `NativePropertyGuard::Install` family), so every read is
   span-checked the same way property reads are today.
4. CANDIDATE CAPS: per-anchor candidate maximum (schema
   `scan_envelope.max_candidates_per_anchor`, default 8); exceeded =>
   refuse. Prevents pathological multi-hit images from saturating the
   budget.
5. FAIL-CLOSED: every refusal is `seed_refused`-semantics — terminal for
   this launch, loud in the log, zero partial state. The P1 fault-ledger
   doctrine (refuse-on-unknown, never saturate) carries over unchanged.

**Pin-verification pass (generate -> verify).** After pins are derived,
each anchor's `verify` rule runs the PROVEN self-test primitives — this is
"Phase 4c-style behavioral probes" implemented as retargeting of the
existing phases [LOCAL-SOURCE: UnrealInitializer.cpp]:

| Anchor | Verify primitive (existing code path) | Console-proven at |
|---|---|---|
| FNameCtorWchar | slot-2 ctor behavioral ByteProperty probe: `FNAME_Find` (never Add), pool ToString roundtrip == "ByteProperty", probe id == pool id | Phase 4, Townfall sessions |
| FName pool | pool words admission: 'None' anchor, CurrentBlock/Blocks[0] sanity, image-segment + VirtualQuery + copyin sources | Phase 3 (L14) |
| GMalloc | V1 instance match + V2 vptr match (RELRO link-constant compare) | Phase 2b |
| GUObjectArray | pointer probe + shape check (ObjObjects chunked-array walk bounded) | Phase 2 |
| StaticConstructObject | empty-callback ABI admission (no native call claimed) | Phase 5 |

A generated pin that fails its behavioral probe is a WRONG pin by
definition — refuse the whole profile. This is the same all-or-nothing
arming law the seed table uses today ("table present, any of the 4 missing
-> compiled to the named loud refusal ONLY"
[LOCAL-SOURCE: UnrealInitializer.cpp arming semantics]).

### 2c. PERSIST — the game's mod-root profile slot

Generated profile JSON is written by the runtime to the game's mod root:

```
/data/modbridge/roots/<TITLEID>/profile.json      (runtime-owned state)
```

- `<TITLEID>` comes from the mod-bridge root the runtime was loaded into —
  never compiled in (see section 4 finding F1: today's
  `kDefaultModBridgeRoot` is exactly the anti-pattern).
- Content: image identity (sha256/size/extent — live-observed), detected
  cell + margins + tier, pins (image-relative labels + prologue prefix
  bytes + sizes), member/vtable table digest, per-anchor verify outcomes,
  generator + table versions, UTC timestamp. NO absolute runtime addresses
  are persisted (base is ASLR; re-observed every launch —
  `ET_SCE_EXEC_ASLR` confirmed [LOCAL-SOURCE: FZ dry-run section 2]).
- Subsequent launches (the cheap path): load slot -> re-verify:
  1. image identity vs live module (patch/update detection),
  2. every code pin's prologue prefix via `code_identity` (the pin-registry
     admission already does this),
  3. cheap data-pin probes (pool words, GMalloc V1/V2).
  Any mismatch => treat as absent -> regenerate (2a-2c). A game patch
  changes the image sha; regeneration is the designed response, not an
  error. Fail-closed: a slot that cannot be fully re-verified never arms.
- The mod manager must NOT manage this file (GG scope: `ue4ss/Mods/...`,
  `ue4ss/PS5Pad.ini`, `loadlist.txt` [LOCAL-SOURCE: modmanager.py SCOPE]);
  `profile.json` is runtime-written state inside the game root, outside the
  loadlist, preserved by the sidecar law.

### 2d. SELF-TEST AUTO-RUN — ueselftest once per generation

GB's ueselftest (rides as a Lua mod, game-agnostic laws a-f
[LOCAL-SOURCE: OTHER-ENGINES-RESEARCH.md section 0]) executes ONCE,
automatically, after a successful generation+persist on first launch. The
manual keybind re-run remains available (GB design).

**Verdict math.** The self-test reports N-of-M over its core probes; M is
the count APPLICABLE to the detected engine cell (UE4-only probes drop out
for UE5 cells and vice versa — applicability comes from the layout table's
probe list, never from game identity). Bar:

```
PASS      ratio >= 0.95 AND every CRITICAL-family probe passed
WARN      ratio >= 0.95 failed but no critical family failed
SAFE-MODE any CRITICAL family failed
```

**Critical families** (defined; each maps to an admission capability whose
failure makes mod execution unsafe rather than merely degraded):
`fname_pool`, `fname_ctor`, `guobjectarray`, `gmalloc`,
`process_event_reflection` (UObject reflection/ProcessEvent marshaling).
Rationale: a broken FName or GUObjectArray pin means every mod's every
call can read garbage — crash-class. A missing Lua nicety (say, a
console-command handler edge) degrades one mod type — not crash-class.

**Failure handling (the proposal the task asked for): load mods unless
CRITICAL families failed.**

- On PASS or WARN: mods load. WARN additionally writes a diagnostic bundle
  to `/data/modbridge/roots/<TITLEID>/diag/` (UE4SS.log tail, all [FL]
  lines, ueselftest marker lines, profile.json copy) and surfaces a WARN
  marker in the log. No safe-mode gate for non-critical shortfalls: the
  mod-per-type ladder (2e) already communicates which types actually work.
- On SAFE-MODE: mod rows are not armed (the runtime refuses mod-start; the
  loadlist itself is untouched — arming is runtime state, matching today's
  seed-refusal behavior where the game continues unmodded); ueselftest +
  diag bundle still run/ persist so the user sees WHY. One-shot law: the
  auto-run happens once per generation event; a manual combo re-run is
  always available; no launch-loop retry of a failed self-test with
  unchanged evidence.

### 2e. MOD-PER-TYPE — after PASS, the standard ~5 mods

Per A2: one PC UE4SS Lua mod per type — event/hook, value/array-write,
config file (io), sustained tick, menu (Item Spawner class covers menu).
After a PASS/WARN first-launch verdict:

1. The runtime writes the verdict + detected cell into the game-root state
   (next to profile.json).
2. GG's mod manager `deploy` synthesizes the enable-set for the game root:
   the five per-type mods from its store, keymap regenerated for the new
   mod set (FV ladder), loadlist rows size-exact [LOCAL-SOURCE:
   modmanager.py]. The manager does NOT read engine internals — its input
   is the runtime's verdict artifact + its own store. Handoff contract:
   `profile.json` (runtime-owned) + `verdict.json` (runtime-owned) in the
   game root; manager scope unchanged.
3. The five mods double as the automatic acceptance ladder (GC ladder v2
   becomes this flow, per the round-5 work mapping). LADDER-V2 SUPERSEDES
   the old VERDICT-SCHEMA stage list; its acceptance law applies: the same
   per-mod pass markers reproduced across 3 launches (one launch + up to 2
   restarts), one real mod per type all enabled together, per-mod
   attribution via each mod's own Lua state + distinct markers
   [LOCAL-SOURCE: mod-ladder/LADDER-V2.md sections 0-1]. GJ's blind-test
   pick set (section 3 of agent-GJ-blade-prep.md) is the game-agnostic
   instantiation: CheatManagerEnabler (event/hook), ConsoleEnabler
   (value/array-write), ueselftest io rows + ConsoleCommands (config-io),
   StaminaMod (sustained tick), ConsoleEnabler's UConsole SCO+Set as the
   engine-surface menu proxy (no game-agnostic UMG menu mod exists in the
   13 — stated honestly as N/A + proxy, not papered over).

For the BLIND TEST specifically: same generator, same tables, same
self-test, same five mods — nothing about the second game is special, and
section 4's gate proves it.

---

## 3. PER-VERSION LAYOUT TABLES — measurement, schema, coverage

### 3a. What a table is

One JSON per `(engine family.major.minor, platform, build_configuration,
stats)` cell. Schema: see
`tables/ue5.6-prospero-shipping-nostats.layout-table.example.json`
(the Townfall cell, expressed as shipped data). Sections:
`detection` (marker pack slice), `scan_envelope`, `anchors` (census/
consensus arms + probe patterns + extraction + agreement + verify),
`structures` (member offsets, shapes, FMalloc vtable slot order, FNamePool
stride), `verify_probes`, `critical_families`, `compat` (format version +
digest). Engine-level facts ONLY — the release audit treats this directory
as PROFILE-TABLE class (section 4).

**Schema alignment with GJ's landed table (the reconciliation GJ's
`alignment_with_gi` note requested).** GJ's
`blade-prep/per-version-table.json` is the AUTHORING form: version-keyed
entries (7 bands), per-field `status` (`measured` / `official-source` /
`seed`) + `confidence` (1.0 measured / 0.0 seed), `expected_deltas_vs_5_6`,
`confirm_when` lists, `base_policy` (live base observation, ASLR law), and
a `laws` block that GJ declared the invariant contract. The shipped per-cell
table (the example above) is the PACKED form derived from it at pack time.
Alignment rules [LOCAL-SOURCE: blade-prep/PER-VERSION-TABLE.md section 5;
per-version-table.json laws/alignment_with_gi]:

1. The `laws` block survives verbatim in every shipped cell: Rule-10
   version-keying (no game names/offsets/sha keys), measured-vs-seed
   discipline, labels-are-image-relative-provenance (per-version
   transferable facts are SHAPES/OFFSETS/PATTERNS/ORDERS, never labels),
   and the confirm_law (seed → measured upgrade path).
2. Field mapping: GJ entry fields `fname_pool` / `guobjectarray` /
   `fname_ctor_wchar_slot2` / `fname_tostring_internal` /
   `static_construct_object_internal` / `engine_uobject_listener_ops` /
   `fmalloc` → the cell's `structures` rows + corresponding `anchors` /
   `verify_probes` entries; GJ `marker_classifier` → the cell's `detection`
   slice; GJ `image_facts_positive_control` → `provenance.evidence_receipts`
   (receipt ids only — game identity stays out).
3. Seed bands do NOT ship as cell values: a band with `status: seed`
   (confidence 0.0) packs to NO cell — the generator refuses that version
   until the confirm_law upgrades it. Only `measured` rows produce shipped
   cells (today: exactly one, 5.6). Forward rows (5.7-5.8) keep the
   detection axis continuous without promising layouts.
4. `base_policy` from GJ is adopted as the cell-level law: the generator
   emits `base_status` and never treats an unobserved base as fact;
   Townfall's 0x400000 is one observation, not a law.
5. Where this doc's first draft said `agreement.min_patterns`, the schema
   now says `agreement.min_arms` (census/consensus arms; section 2b
   correction) — the example JSON is updated accordingly.

### 3b. Measurement procedure — ONCE per Unreal version, on the Mac

Per version V in UE4.0..UE5.6 (priority: 5.6 done; 4.2x-5.5 next per the
blind-test target's era; full sweep after):

1. **Gather measurement material for V** (see 3c inventory). At least one
   real shipping binary of V (PS5 preferred; Win64 acceptable for
   structure rows with PS5 anchor re-measurement) + the official engine
   declarations for V.
2. **Measure, two-source law**: every structure row needs two agreeing
   sources — engine-declaration compile controls (the OFFICIAL-TOTAL-FACTS
   method: compile the declarations with the pinned generic/Unix adapter
   and read the offsets) AND image evidence (disassembly/string windows).
   Unknown/mismatch/conflict => the row ships as absent => runtime refuses
   that anchor (never guesses). This is exactly the l19-l25 Townfall
   evidence discipline, re-run per version [LOCAL-SOURCE:
   RuntimeBuildProfile.hpp provenance strings].
3. **Anchor arms**: start from the census/consensus arm parameters that
   pinned the measured row (GJ's landed runner: LEA-xref census window,
   prefix-consensus byte counts, UTF-16 module-string set for V)
   [LOCAL-SOURCE: run-blade-profile.py]; carry the patternsleuth resolver
   patterns for V as PROBES only [TRAINING-KNOWLEDGE seed — re-measure
   before shipping; they went 0/9-0/14 on Townfall, so a probe can never
   be load-bearing]. Validate every arm against the V binary; record
   census-rank statistics; arms that do not reproduce on real V binaries
   are dropped, not trusted.
4. **Marker pack slice**: from the engine_version table, compute the
   triage subset + expected presence vector for the cell (engine_match.py
   logic, run offline; only results ship).
5. **Behavioral verify**: the verify_probes are the fixed console-proven
   set (2b table); per version, assert the probe CONSTANTS (e.g. the
   ByteProperty id check works against V's pool) on the Mac against the V
   image where statically possible.
6. **Pack**: canonical JSON + digest; receipt IDs (hashes) recorded in
   `provenance.evidence_receipts` — no game names in receipts that ship
   (section 4 boundary).

### 3c. Material inventory — what exists, what is needed per version

| Material | Have today | Per-version need |
|---|---|---|
| PS5 shipping eboot, UE5.6.1 | YES — Townfall image (decrypt-on view; the entire measured cell) [LOCAL-SOURCE: townfall-static-executable-2026-10-02] | one per remaining version; acquired per the FZ section-4 procedure ONLY from user-owned titles, decrypt-law honored |
| Official engine declarations | partial — generic/Unix adapter + official-evidence compile chain exists for 5.6-shape [LOCAL-SOURCE: 2026-10-07-l19-r4-total-evidence] | Epic per-version source/headers for each V (license law: measured facts ship, Epic source NEVER ships — A4) |
| Marker/anchor pattern seeds | YES — patternsleuth engine_version.rs (30 versions) + resolvers [TRAINING-KNOWLEDGE] | re-measure per V against real binaries; drop non-reproducing patterns |
| Community offset tables (UE4SS MemberVariableLayout per version) | not local | usable as TRAINING-KNOWLEDGE cross-check seeds only; every row still needs 3b's two sources before shipping |
| Additional PS5 eboots | second UE game's eboot NOT on Mac [LOCAL-SOURCE: FZ section 4]; Astro Bot available but NOT Unreal (validates refusal) | the blind-test game's eboot is NOT needed on the Mac — that is the point of on-console generation; it arrives as the first-launch blind test itself |

### 3d. 95%-coverage strategy + honest unknowns

The bet: for a given (version, platform, config, stats) cell, engine ABI
structure offsets are identical across games, because they are determined
by the engine's class declarations + toolchain, not by game code. Game
code changes WHERE symbols land (hence pattern anchors per game) but not
WHAT the layouts are (hence per-version tables). Anchor patterns +
behavioral probes are the safety net for every deviation. [INFERENCE from
the Townfall measurement chain + PC UE4SS's per-version (not per-game)
member layouts; to be confirmed per version by measurement.]

Unknowns stated honestly:

1. **Licensee engine forks** (the ~5%): custom UE builds shift anchors and
   occasionally layouts. Detection: multi-pattern disagreement or
   behavioral probe failure => refuse (A4's adaptation-kit path handles
   these later). NOT silently covered.
2. **Toolchain drift within a version**: licensee-built-with-different-
   clang eboots can change codegen shapes. Mitigation: anchors carry
   several pattern roles; prologue verify is prefix-based, not
   whole-function. Residual risk: refusal, not corruption.
3. **Shipping vs test builds + stats on/off**: cells exist per config
   (the cost solve already discriminates). Unknown: how many REAL PS5
   eboots ship non-shipping configs — expected rare; REFUSED/covered by
   table absence => refuse. [TRAINING-KNOWLEDGE: PC distribution is
   shipping-dominant; PS5 unverified]
4. **Win64 -> PS5 deltas**: member offsets are ABI-level and expected
   shared (same class declarations, same x86-64 layout rules); CODE
   patterns (prologues, call shapes) differ SysV-vs-Win64 — anchor
   patterns must be PS5-measured. The 3c priority reflects this: PS5
   binaries for anchors; declarations for structures.
5. **UE4.0-4.22 legacy shapes**: pre-4.23 FUObjectArray + TNameEntryArray
   FName (no FNamePool) need a distinct anchor+decoder family
   [LOCAL-SOURCE: engine_match.py anchor_pack_recommendation]. Nothing
   measured yet; tables for that era are authored only when a real
   target exists (never speculative).
6. **Version-in-between patches** (5.6.1 vs 5.6.2): cell granularity is
   major.minor; patch-level engine changes are rare and caught by
   verify-probes; a refusal just triggers table maintenance.

---

## 4. BLIND-TEST LAW ENFORCEMENT — Rule 10 extended to games

### 4a. The gate

`phasea-tools/release_audit.py` (this task; static, read-only):

- INPUT: game specs (names/title-ids) via CLI — the tool itself hardcodes
  and examplifies NO real game name (it must pass its own audit).
- WALK: `--scope runtime` = the PRX-compiled tree (candidate src/include/
  generated_include + profile units injected at build) + the shipped
  tables directory; `--scope full` adds the whole worktree.
- CLASSIFY every textual hit: `RUNTIME` (compiles into the PRX),
  `PROFILE-TABLE` (shipped tables + generated profile units),
  `DIAGNOSTIC` (authored tools/docs/receipts), `IGNORE` (binaries,
  console-harvested logs).
- REGULAR FILES ONLY: the walker stats every file and skips non-regular
  nodes — clr-debug-pipe FIFOs exist under `outputs/*/tmp/` and a naive
  read blocks forever (found live during the retry run; the first
  full-scope attempt hung on exactly this).
- VERDICT: RUNTIME/PROFILE-TABLE hit for ANY audited game = hard VIOLATION
  (exit 2). DIAGNOSTIC hit for the CURRENT development game = allowed.
  DIAGNOSTIC hit for a NON-current (blind-test) game = REVIEW warning —
  legitimate only in session receipts and blind-test mission docs; never
  in tools, per-game sidecars, or anything that could ride a release.

### 4b. Audit run NOW — findings, honestly

Command (runtime scope; the release-gate form):

```
python3 outputs/2026-10-08-l25-prep/phasea-tools/release_audit.py \
  --game "StellarBlade|Stellar Blade|PPSA13197" \
  --game "Townfall|PPSA33286|Silent Hill Townfall" \
  --current-game "Townfall|PPSA33286|Silent Hill Townfall" \
  --scope runtime
```

Result: **exit 2 — 10 violations, all Townfall-class. The blind-test game
is CLEAN (zero occurrences in runtime + profile-table scope).** Run receipt
saved: `phasea-tools/release-audit-run1-runtime.txt` (re-executed and
re-verified on the GI retry delivery; exit code captured without a pipe
masking it). Full-scope receipt: `phasea-tools/release-audit-run2-full.txt`
(diagnostics classification; REVIEW items counted in the report
`agent-GI-firstlaunch.md`).

Classification of every runtime-scope hit:

| # | File:line | Class | Kind | Disposition |
|---|---|---|---|---|
| F1 | `LuaPathShim.hpp:80` `kDefaultModBridgeRoot="/data/modbridge/roots/PPSA33286"` | RUNTIME | **compiled-in per-game default (behavioral)** | MUST GO. Root must be derived from the loaded game's root/config at runtime. This default would silently point a new game at Townfall's mod root — a real generality bug, not cosmetics. |
| F2 | `LuaPathShim.hpp:9` (comment: TownfallMiniMap) | RUNTIME | comment, mod name | scrub at release (reword to "a HUD mod"); non-behavioral |
| F3 | `LuaPathShim.cpp:89` (comment: roots/PPSA33286) | RUNTIME | comment, title-id | scrub at release |
| F4 | `LuaMod.cpp:8566` (comment: TownfallItemSpawner) | RUNTIME | comment, mod name | scrub at release |
| F5 | `UnrealInitializer.cpp:1961` (comment: townfall-ps5-ue56 profile) | RUNTIME | comment, profile name | scrub at release (profile ids become sha-derived) |
| F6-F10 | `profile/units/CodeSignatureProfile.hpp:8,37`, `RuntimeBuildProfile.hpp:7,9`, `CompiledNativeLayoutDeclarations.hpp:6` | PROFILE-TABLE | generated pinned-profile strings (game path, title-id, profile name, PT_SCE path provenance) | resolved BY THIS ARCHITECTURE: the compiled-in pinned profile is replaced by generated+persisted JSON in the game root; profile identity becomes image-sha-derived, not game-named |

Full-scope run (diagnostics classification; receipt
`release-audit-run2-full.txt`, 5177 files scanned): 10 violations (same
set as runtime scope), 82 REVIEW warnings — ALL blind-test-game
DIAGNOSTIC hits, no current-game violations. Disposition classes of the
82: (a) session receipts / mission / planning docs, including this doc and
the GJ/FZ artifacts — allowed by 4c.3; (b) pre-pivot translation-core-era
docs and legacy mod-bridge test fixtures (`ps5-mod-bridge/tests/*`,
`examples/target-unknown.json` use the game name as Nexus-catalog fixture
data) — legacy, never ride the UE4SS release, but MUST be scrubbed if that
tree ever ships under A4; (c) ONE actionable: `blade-prep/
per-version-table.json` line 4 carries the blind-test game name inside its
`generated_by` provenance string — the AUTHORING form may keep it, but the
PACKER must strip/whitelist provenance fields so no game name can flow
into a shipped cell (the shipped example cell is already clean). Audit
receipt files themselves are REVIEW-class by construction (they contain
the audited names); they stay in diagnostics and never ride a release.

### 4c. The Rule-10 boundary (proposal)

1. **RUNTIME + PROFILE-TABLE: zero game facts.** No names, title-ids,
   per-game offsets, or special cases — machine-enforced by
   release_audit.py as a release gate; exit 2 blocks the release. This
   extends Rule 10 to games exactly as A2 requires.
2. **DIAGNOSTICS may name the CURRENT development game only.** Test/
   probe scripts, receipts, per-game symbol sidecars (e.g.
   `game-symbols-<current>.json`), manager configs — fine for the current
   game.
3. **The blind-test game may be named ONLY in**: session receipts/report
   artifacts and the mission/planning docs that define the blind test.
   NEVER in: tools (names enter via CLI only), per-game sidecars (a
   symbols file for the blind game would be hand-collected knowledge =
   test contamination), or anything that rides a release. After the blind
   test passes, the game becomes a normal supported title and normal
   diagnostics rules apply (still never runtime).
4. **Gate wiring**: the release checklist runs release_audit.py over the
   release candidate tree (runtime scope) with the full game registry
   (every supported + tested title); CI-shaped: exit 2 = stop. The Z9/
   G-gate lineage extends: this is the same "prove the law" pattern,
   applied to names instead of edit paths.

---

## 5. RUNTIME GENERATOR SKELETON (design-level; NO tree edits)

Shape follows the [KB] skeleton discipline (PHASEB section 5): one TU,
default-OFF compile gate, narrow+wide [FL] markers, fail-closed admission
state, never throws past its bounds. In the shipped build the first-launch
generator is always compiled IN (it is the product), but every write is
state-file-only and every scan is read-only; the compile gate below
mirrors the staging discipline during bring-up.

```cpp
// FirstLaunchProfile.cpp — [FL] on-console profile generator (agent GI
// design, 2026-10-08). DESIGN ONLY — not in the tree. Runs INSIDE the
// seed-admission budget regime; read-only scans; state writes only to the
// game mod-root profile slot; fail-closed ladder identical to seed_refused.

#include <cstdint>

#if defined(__PROSPERO__)
// Composition-provided (existing doctrine): Output::send narrow+wide,
// module_view_base(), bridge_code_cache_context, bridge_readable_span_*,
// rc_seed range classes, rc::fnameabi signature/invoke, pool-word checks,
// GMalloc V1/V2, admission::Budget/L13AdmissionOps — the SAME seams the
// admission phases use. The layout-table loader parses the shipped JSON
// from the mod-adjacent tables asset (digest-gated).

namespace rc::firstlaunch {

enum class Verdict : unsigned { Refused=0, FamilyOnly=1, Tentative=2, Confident=3 };

struct DetectResult {
    Verdict tier; unsigned family, major, minor;   // 4/5, x.y
    unsigned present_count, version_margin, config_margin, stats_margin;
    const char* cell_id;                           // table cell selected
};

struct GeneratedPin {
    const char* anchor_id;      // "GUObjectArray" | "FNameCtorWchar" | ...
    uint64_t    image_relative; // label = runtime_addr - base
    uint32_t    size;
    uint8_t     prologue[32];   // verified prefix (code anchors)
    bool        verified;       // behavioral probe passed
};

struct ProfileSlot {                    // 2c persisted artifact (schema v1)
    char     image_sha256[65];
    uint64_t image_size, image_extent;
    char     cell_id[64];
    unsigned tier, margins[3];
    GeneratedPin pins[8];
    uint32_t table_digest[8];           // sha256 of the used layout table
    uint64_t generated_utc_ms;
    uint32_t slot_schema;               // 1
};

// ---- the fail-closed ladder -------------------------------------------
// Every rung refuses LOUD ([FL] refuse line + seed_refused semantics:
// terminal this launch, zero partial state). Rung order is fixed:
//   detect -> table -> probe -> verify -> persist -> self-test -> mods
// and NO later rung executes after any earlier refusal.

static bool fl_detect(DetectResult& out);          // 2a: marker scan under
                                                   // admission budget; the
                                                   // cost solve + tiers run
                                                   // in-table (engine_match
                                                   // semantics)
static bool fl_load_table(const char* cell_id);    // 2b: digest-gated load
static bool fl_probe_anchors(/*pins out*/);        // 2b: census arms (LEA-xref
                                                   // census, prefix consensus,
                                                   // UTF-16 census), range-class,
                                                   // candidate pool + min_arms
static bool fl_verify_pins(/*pins in-out*/);       // 2b: behavioral probes
                                                   // (ByteProperty ctor test,
                                                   // pool words, GMalloc V1V2,
                                                   // SCO empty callback)
static bool fl_persist(const ProfileSlot& s);      // 2c: game-root slot write
static void fl_arm_admission(/*pins*/);            // retarget consume_required_
                                                   // slot_seeds inputs at the
                                                   // generated pins (same
                                                   // phases, data not constants)

extern "C" void RC_first_launch_profile()
{
    // [FL] profile slot=absent|loaded|regenerate reason=<image-sha-mismatch|
    //      prologue-mismatch|probe-failure>          (narrow+wide one-shot)
    DetectResult d{};
    if (!fl_detect(d)) {
        // "[FL] detect refused present=%u/244 tier=REFUSED"  <- Astro-path
        return;                                   // loud refuse, no mods
    }
    // "[FL] detect markers=%u/244 tier=%u version=%u.%u config=%u stats=%u
    //      margins=%u/%u/%u cell=%s"
    if (!fl_load_table(d.cell_id)) {
        // "[FL] table refused cell=%s digest=fail|missing"
        return;
    }
    // "[FL] table ok cell=%s digest=<8hex>"
    GeneratedPin pins[8]; unsigned n_pins = 0;
    if (!fl_probe_anchors(pins, &n_pins)) {
        // "[FL] probe refused anchor=%s hits=%u agree=%u class=%u"
        return;                                   // disagreement = refuse
    }
    // per-anchor: "[FL] probe anchor=%s hits=%u agree=%u label=0x%llX"
    if (!fl_verify_pins(pins, n_pins)) {
        // "[FL] verify refused anchor=%s probe=%s"   // WRONG pin by defn
        return;
    }
    // "[FL] verify ok anchor=%s probe=%s label=0x%llX"  (per pin)
    ProfileSlot slot{}; /* fill from d + pins + table digest */
    if (!fl_persist(slot)) {
        // "[FL] persist failed slot=<path> errno=%d"   // session-arm + WARN:
        fl_arm_admission(pins, n_pins);                // verified pins may arm
        // "[FL] persist fallback session-armed=1 warn=persist-retry-next-launch"
    } else {
        // "[FL] persist ok slot=<path> sha=<8hex> schema=1"
        fl_arm_admission(pins, n_pins);
    }
    // 2d fires from the mod-start side: ueselftest auto-run ONCE per
    // generation event; its N-of-M feeds:
    // "[FL] self-test n=%u m=%u pct=%u critical=%u verdict=PASS|WARN|SAFE-MODE"
    // PASS/WARN -> mods arm; SAFE-MODE -> mod-start refused + diag bundle.
}
} // namespace rc::firstlaunch
#endif // __PROSPERO__
```

### 5a. [FL] marker contract (complete list)

| Marker (narrow utf-8 template; wide one-shot literal twin) | Fires |
|---|---|
| `[FL] profile slot=absent` / `slot=loaded sha=<8hex>` / `slot=regenerate reason=<...>` | boot, before detection |
| `[FL] detect markers=%u/%u tier=%s version=%s config=%s stats=%s margins=%u/%u/%u cell=%s` | after cost solve |
| `[FL] detect refused present=%u tier=REFUSED` | non-UE image (fail-closed) |
| `[FL] table ok cell=%s digest=%8s` / `[FL] table refused cell=%s` | table load |
| `[FL] probe anchor=%s hits=%u agree=%u label=0x%llX` | per admitted anchor |
| `[FL] probe refused anchor=%s hits=%u agree=%u class=%u` | disagreement/cap (fail-closed) |
| `[FL] verify ok anchor=%s probe=%s` / `[FL] verify refused anchor=%s probe=%s` | behavioral verify |
| `[FL] persist ok slot=%s sha=%8s schema=1` / `[FL] persist failed ... errno=%d` | slot write |
| `[FL] persist fallback session-armed=1 warn=persist-retry-next-launch` | verified-but-unpersistable |
| `[FL] self-test n=%u m=%u pct=%u critical=%u verdict=%s` | ueselftest verdict |
| `[FL] safe-mode mods=off diag=%s` | critical-family failure |

### 5b. Fail-closed ladder (restated as law)

1. REFUSED detect => no table, no pins, no mods. (Correct for non-UE.)
2. Any table problem (missing/digest/absent cell) => refuse.
3. Any anchor disagreement/cap/class violation => refuse.
4. Any behavioral verify failure => refuse — a wrong pin is never armed.
5. Persist failure with all verifies passed => session-arm + WARN + retry
   persist next launch (the only non-terminal rung, because safety was
   already proven; state is just not yet durable).
6. Self-test critical failure => SAFE-MODE: mods off, diag on.
7. Nothing outside the ladder ever arms; no rung re-runs within one
   launch (one-shot [FL] accounting, the [KB]/[ST] precedent).

### 5c. Integration checklist (when promoted from design to TU)

Following the PHASEB section 5.1 pattern exactly [LOCAL-SOURCE]:

1. NEW_UNITS registration in `outputs/2026-10-08-l26-runtime/
   build_variants.py` (new TUs cannot ride CHANGES rows):
   `'firstlaunch':{'source':.../'FirstLaunchProfile.cpp','template':
   'unreal-unrealinitializer','note':'agent GI [FL] first-launch generator
   (on-console profile generation)'}`; census 391+len(NEW_UNITS).
2. Compile define `-DMODBRIDGE_FL_GENERATOR` in the unit argv AND the
   packaging driver define list (marker gate parity).
3. CHANGES part-file: append scope id `typescan-r4:scope_fl_first_launch`
   to CHANGES.json changes[].
4. markers-declared.json: every section 5a marker, exact text, utf-8
   templates + utf-16le wide literals, under
   `outputs/2026-10-08-l25-prep/harvest/markers-declared.json`.
5. Snapshot discipline: harvest/snapshot_tree.sh; its SNAPSHOT-ID as
   BRIDGE_SNAPSHOT_ID for --execute.
6. verify_build.sh --manifest: N/N [FL] markers in the built PRX (both
   encodings); nonzero exit = do not upload.
7. Upload discipline: text+PRX per law; decrypt-OFF readback magic +
   sha256 byte-exact; prior resident backed up.
8. Arm discipline: ONE loadpayload dispatch after confirmed exit; klog
   tap NEW file BEFORE launch; one user-launched test per boot; harvest
   UE4SS.log — [FL] lines are the parse contract (absent = ABSENT,
   never guessed).
9. NEW for this feature: the tables asset + ueselftest mod rows ride the
   loadlist via the mod manager (2e); profile.json stays runtime-owned.

---

## 6. Sequencing proposal (Mac-only until user-authorized console steps)

1. GJ's per-version table measurement chain has LANDED
   (`blade-prep/per-version-table.json`: 1 measured row 5.6.1-release +
   6 seed bands at confidence 0.0 + the laws block; runner
   `run-blade-profile.py` proven on both local binaries — Townfall
   ACCEPTED, Astro REFUSED). Next versions enter by the 3b procedure;
   this design's schema alignment is section 3a.
2. This skeleton -> TU under the 5c checklist (Claude review first).
3. Host-side rehearsal: the generator ladder runs against a reconstructed
   image on the Mac (no console) — detect/table/probe/verify produce a
   profile JSON byte-comparable to the Townfall pinned facts.
4. First console pass on Townfall itself with generation ON and the
   compiled-in pinned profile REMOVED — the port's own game must pass the
   blind-test conditions before the second game does (eat our own dogfood;
   also forces F1's root-derivation fix).
5. THEN the blind test on the second game, generator-only, under the
   section 4 gate.

— agent GI, 2026-10-08-l25-prep. Design only; no builds, no console
contact, no tree edits. Nothing herein authorizes any console action.


## GW CORRECTION (2026-10-10 — CODE-READ-SEAM.md, superseding §detect/generate wording)

The runtime NEVER reads running code: the prologue gate consumes a
user-staged DECRYPTED FILE COPY streamed once at module_start
(CodeImageCache, runtime_start_bridge.cpp:1932->:1213, measured ~91 MB/s
on console, evidence/l24-diag.jsonl). Live text is excluded by policy AND
hardware (R-and-not-X). Therefore:
- DETECT can scan in-memory readable spans (data segments) as written.
- GENERATE needs the DISK SEAM (the decrypted eboot copy) as its text
  source: generalize the CodeSignatureProfile path/sha inputs to runtime
  inputs per game. In-process acquisition is nil (/app0 is the encrypted
  SELF).
- Built-in fallback: refuse-and-degrade to seed bands + behavioral
  verification (no Phase B needed). Later upgrade: the Phase B
  REMOTE_SYSCALL path as the only zero-staging option.
- The GT TU's [FL] detect stage aligns; its GENERATE arm must target the
  staged-copy cache, not live text (verify at wiring time).
