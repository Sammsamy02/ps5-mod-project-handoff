# OVERLAY-DESIGN.md — drop-folder file overlay for the PS5 UE4SS runtime port

Worker: worker-file-overlay · 2026-10-06 · **Design + research only.**
No console contact, no execution, no uploads, no game-state or engine-acceptance
claims. Every fact below is either (a) cited to a retained artifact in this
worktree, (b) public-engine knowledge marked `[UE]`, or (c) explicitly marked
**UNMEASURED** with the probe that would decide it. Companion files:
`EVIDENCE-NOTES.md` (source census), `REPORT.md` (summary).

Target: Silent Hill: Townfall (PPSA33286), UE 5.6 (static survey of the saved
executable, RUNTIME_MAP.txt "STATIC ENGINE SURVEY"), running from the decrypted
dump at `/data/homebrew/Silent Hill Townfall PPSA33286/`.

---

## 0. The one-paragraph answer

The overlay's public face is the drop folder
`/data/modbridge/overlay/PPSA33286/` (§4). Its engine-facing mechanism is
**container projection (Approach 1)**: an installer verb repacks/classifies
dropped files and writes *add-only, owned, correctly-named* UE containers into
the decrypted dump's `townfall/content/paks/` directory before launch, where the
engine's own startup PAK/IoStore discovery mounts them — no hooks, no kernel
work, no runtime-code changes, and it rides a channel this project has already
exercised once (`pakchunk9999-ModBridge_<uuid>_P.{pak,utoc,ucas}`, §2.4).
In-process late mounting and true loose-file redirect (engine platform-file
wrapper, Fios overlay) are designed as ranked follow-ups gated by three cheap
probes (W1–W3, §6.1), because the decisive fact — *did the engine actually mount
the staged containers?* — is still unproved for this title and no design should
pretend otherwise.

---

## 1. Current file loading paths (as evidenced)

### 1.1 Process/filesystem topology at launch

```
console /data (homebrew-writable, FTP-visible)
└── homebrew/Silent Hill Townfall PPSA33286/     ← decrypted dump (D)
    ├── eboot.bin (194 MB), package.manifest, sce_sys/
    ├── townfall/content/…  townfall/content/paks/   ← UE containers (D)
    │   ├── pakchunk0-ps5.utoc (+ .ucas)  legacy .pak(s)  (D)
    │   └── pakchunk9999-ModBridge_<uuid>_P.{pak,utoc,ucas}  (staged once, cleaned) (D)
    └── decrypted/, engine/content/renderer/ …       (D)
        (mount) shadowmountplus.elf in pldmgr autoload  (D: §1.3 BOOT-PROTOCOL)
        └── /mnt/sandbox/PPSA33286_000/app0 → the dump  (D: sandbox listing; task brief)
game process (pid, e.g. 934)
├── sees app0 = dump contents; game data under /app0/townfall/content/… (D)
├── own sandbox dir /mnt/sandbox/PPSA33286_000/ (+ pfsmnt)        (D: FTP listing)
├── file IO via libSceFios2.prx (loaded before ploader injects;   (D: PLOADER-IMPORT-
│   ploader readiness gate waits for it)                             ANALYSIS §1)
└── UE 5.6: content through IoStore (.utoc/.ucas) + legacy PAK v11 (D: container
    surveys; Stellar Blade sibling title shows the same pair of formats)
```

### 1.2 The `/data` readability window — the constraint that shapes everything

Measured contract, `modforge_activation.h:6-8`: the injected runtime reads its
mod root "once, during module_start: that is the only moment /data is readable
from the game process (a fresh open afterwards fails with ENOENT)". Corroborated
by PATH_TEST (`worker-minimal-path-test`): opens of
`/data/modbridge/roots/PPSA33286/loadlist.txt` and writes to
`/data/plugins/UE4SS.log` succeed *inside module_start*; the kept-open
`runtime-status.txt` descriptor keeps working for the process lifetime
(`mfa_status_*`, header lines 25-27). Consequences:

- **C1:** anything the *engine* opens lazily (PAK files, configs, movies — all
  opened well after `main`) can never live under `/data/…` as plain files.
- **C2:** the only file trees the engine can read are the ones already visible
  in its sandbox: **app0 (= the decrypted dump)** and the process's own sandbox
 /pfsmnt area (post-closure readability of added files there: **UNMEASURED**,
  probe W2).
- **C3:** our own in-process code faces the same window. Bytes preloaded during
  module_start (today: ≤8 MiB/file, ≤64 MiB total, `mfa_limits`) and fds opened
  then and kept (the `runtime-status.txt` trick, generalizable via `pread`)
  remain usable all session.

### 1.3 What the existing loadlist/modbridge system covers — and the gap

The activation contract (`modforge_activation.h`, ACCESSIBILITY-ANALYSIS §1):
`loadlist.txt` in `/data/modbridge/roots/PPSA33286/` lists files with roles;
roles `lua_mod`, `config`, `script` are **preloaded into owned memory** during
module_start and handed to UE4SS in-process. Roles such as `pak` and
`native_plugin` are **listed but never opened** — "rows with non-preloaded roles
… are listed from the loadlist text only and never opened at activation time."
That is precisely the asset-overlay gap: the manifest knows about containers,
nothing delivers them to the engine.

### 1.4 Prior on-disk capability (this exact channel has run once)

`transport-progress-before-native-default-detour-2026-10-02.json` records a
completed Townfall container trial:
- staged `pakchunk9999-ModBridge_<uuid>_P.{pak,utoc,ucas}` into the dump's
  `townfall/content/paks/` (owned names, transactional, full readback),
  `original_game_files_overwritten: false`, later cleaned;
- a diagnostic render-control pak produced an *inconclusive* visual result;
- `engine_mount_or_asset_origin_proved: false` — the honest open verdict.
A `pakchunk0-ps5.utoc.original-<uuid>.bak` in the same listing shows the
merge-into-original-TOC route was also exercised. Host-side primitives already
exist and are tested: verified native PAK v11 writer (repak-parity), IoStore v8
None TOC-overlay merger (`iostore-toc-overlay-c-1`, 31 assertions), batch
IoStore composition, the prepared `FileOverrideTable` (µs lookups,
PS5-compiled), and the transactional installer verbs with rollback
(STAGED-E2E-COMMANDS §4).

---

## 2. Candidate approaches, analyzed

The four task options plus the two extra hook surfaces that fall out of the
platform facts. Each is scored against the constraints of §1 and the project's
laws (add-only dump writes, no kernel-code authorship, one variable per launch,
evidence before claims).

### 2.a Kernel-level VFS hook — intercept `open`/path lookup in the kernel

Most transparent in theory (redirect `/app0/...` to `/data/modbridge/overlay/...`
process-wide; even beats C1, since kernel lookups are pre-sandbox). Rejected for
now, ranked **last (5th)**:

- Kernel payload infrastructure on this console is third-party (kstuff / etaHEN
  / OnionHEN; BOOT-PROTOCOL §1.3); this project has no owned kernel-code
  lineage, and the operating contract keeps it that way.
- Per-boot kernel layout is proven variable (BOOT-PROTOCOL §1.3: kstuff
  `native=` addresses differ every generation) — a kernel hook is a
  per-boot-fragile artifact, hostile to the one-variable-per-launch law.
- Failure mode is a panic/crash costing the whole console session, vs. a
  harmless extra file for every other approach.
- Everything the product actually needs (containers into the engine, loose
  files for UE4SS) is reachable without it.
Keep as a documented non-goal; revisit only if a future requirement demands
redirecting *engine-internal* opens that no userland surface can reach.

### 2.b Library-level hook — replace/redirect the game's file I/O library

Two sub-surfaces, very different feasibility:

- **libc `open`/`openat` hooking in-process**: weak fit. UE on PS5 does its
  content IO through libSceFios2 (the ploader even waits for `libSceFios2.prx`
  before injecting), so a libc hook misses the engine's actual path; hooking
  fios entry points needs fios NID ground truth (see W3) and detours inside a
  Sony lib — possible in principle (the port has detour machinery) but the
  highest-risk in-process surface. Also still bounded by C1 for the *overlay
  source* (a redirected open must land somewhere readable post-closure).
  Ranked **4th**.
- **Engine platform-file wrapper (UE-idiomatic library-level hook)**: insert an
  `IPlatformFile` wrapper at the top of the engine's platform-file stack
  (exactly how `-fileopenlog` works internally `[UE]`) from our runtime at
  engine-ready, serving overlay lookups from the in-memory `FileOverrideTable`
  backed by module_start-preloaded bytes and/or kept-open fds served via
  `pread`. This is the *correct* loose-file design inside UE and reuses two
  built assets (override table, engine-binding machinery). It is, however,
  dependent on the same engine-binding critical path that is currently mid-
  regression (GATE-1 etc., MAC-SESSION-BRIEF §0). Ranked **2nd** — the right
  phase-2 build, not the first ship.

### 2.c Pre-montage — union filesystem over the game directory

A classical union mount needs kernel `mount` authority → same class as 2.a.
But the platform already gives us a **userland equivalent**: the game's app0 is
*itself* a mount of the writable decrypted dump made per launch (§1.1). Writing
add-only files into the dump's paks dir pre-launch IS a pre-montage overlay —
no new mount, no hook, and it was already exercised (§1.4). Sony's own
userland middle layer for exactly this pattern is the Fios overlay API
(`sceFiosOverlayAdd` family; documented lineage on PS4, PS5 presence in this
game's libSceFios2 **UNMEASURED**, probe W3). The Fios variant ranks **3rd** as
a research track: it would give true transparent directory overlay semantics
for loose non-container files without touching the dump, if (and only if) the
API exists in-process and its source path resolves post-closure.

### 2.d PAK injection — put mod containers where the engine already looks

`[UE]` UE's `FPakPlatformFile` at startup enumerates the project's
`Content/Paks` folder (console path: `/app0/<proj>/content/paks/`), mounts every
PAK/IoStore TOC found, and orders them by pak-priority rules (project paks over
engine paks; `_P` suffix = patch priority — the documented Epic patching
mechanism; mount-point `../../../` relative convention). IoStore `.utoc`s in the
same folder are mounted into the IoStore dispatcher under the same ordering, and
same-package-ID entries in a higher-priority TOC override the base containers —
that is precisely how Epic's own patch/DLC pipeline works and why retoc-class
overlay tools exist. Community practice on jailbroken PS4/PS5 confirms the
console behavior: dropping correctly-named containers into the decrypted dump's
paks directory (or repacking the fpkg with them) is the standard UE mod route.

Consequences for us, all evidenced or buildable:
- **Zero new runtime code.** The engine performs discovery, mount, and override
  resolution. Nothing touches the fragile 30 MB composition PRX; the overlay
  works even on launches where the module fails to load (a *feature* while the
  loading regression is open: file-mod progress decouples from module progress).
- The write target is writable via FTP between launches (§1.4 precedent), and
  the add-only owned-file discipline (`original_game_files_overwritten: false`)
  preserves the "originals recoverable" law; uninstall = delete exactly the
  owned names.
- The pipeline primitives are built: PAK v11 writer, IoStore composition, TOC
  merger, transactional staging with readback.
- The open question is honest and singular: this title's engine-mount verdict
  (`engine_mount_or_asset_origin_proved: false`) → probe **W1**.

Ranked **1st — recommended**.

### 2.e Ranking summary

| # | approach | transparency | new code | risk | verdict |
|---|---|---|---|---|---|
| 1 | **Container projection into the dump's paks dir** (2.d) | engine-native mount; "drop-in" UX at the overlay folder | installer verb only (host-side, primitives exist) | low; add-only + rollback; decoupled from module state | **build now** |
| 2 | **Engine platform-file wrapper + late `MountPak`/IoStore mount** via the UE4SS port (2.b-engine) | full in-process overlay incl. loose files | engine-binding units (vtable/IPlatformFile layout, detours) | medium; rides current critical path | build as phase 2 |
| 3 | **Fios overlay API** (`sceFiosOverlayAdd`) pre-main (2.c-userland) | true transparent dir overlay, no dump writes | small PRX call + NID ground truth | medium; PS5 availability UNMEASURED | research track W3 |
| 4 | **libc open/read hook + FileOverrideTable** (2.b-libc) | in-process redirect only | hook layer | medium-high; misses fios-routed IO; C1 still binds the source | fallback only |
| 5 | **Kernel VFS hook** (2.a) | perfect | owned kernel module | panic-class; out of contract | rejected (documented non-goal) |

---

## 3. Unreal-specific mounting research (task item 3)

1. **How UE5 on PS5 discovers PAKs** `[UE]`: `FPakPlatformFile::Initialize`
   enumerates the pak folders (project `Content/Paks`, plus engine folder),
   mounts each `.pak` (legacy) and each `.utoc`+`.ucas` pair (IoStore), in pak
   order. Pak order derives from the path (engine vs project folder) and the
   filename: a `_P` suffix marks a patch pak and raises priority; numbered
   `_P<n>` raise it further. Later/higher-priority mounts win file/package
   lookups, which is the entire basis of the PC-modding `zz_MyMod_P.pak`
   convention. On console the enumerated folder is inside app0 — i.e. inside
   the dump for a shadowmounted title, which is what makes approach 1 work
   with no code. **Title-specific acceptance remains W1's to prove** (a
   shipping build could in principle enumerate a restricted filename set; no
   evidence of that here, and the community practice says otherwise).
2. **`-fileopenlog` and custom mounting points** `[UE]`: `-fileopenlog` wraps
   the platform-file stack with a logging wrapper — diagnostic only, and not
   settable without touching the command line (on PS5 the command line is not
   user-editable; our runtime could set it in-process pre-main, but that is
   extra binding work for a diagnostic). The *useful* idea it points at is the
   wrapper slot itself: the same insertion point is where a phase-2 overlay
   wrapper goes (§2.b-engine). Custom mounting points (`FPakPlatformFile::
   MountPak(path, priority)` and IoStore mounting through the dispatcher) are
   the engine-side APIs a phase-2 late-mount uses once bound; UE4SS upstream
   has **no** built-in file-override system today (its settings expose only
   mod-folder overrides — verified against the upstream `UE4SS-settings.ini`),
   so this is our port's value-add, not a ported feature.
3. **PAK vs IoStore duties on this title**: asset packages must ship as IoStore
   (`utoc/ucas`) to override IoStore-packaged assets (same package IDs — the
   Leon-bundle pattern: 68 asset paths/package IDs matched the installed
   target); loose resources (`.ini`, `.locres`, `.png`, movies, plugins) ride
   legacy PAK v11 (this title's v11 indexes are unencrypted, `None` compression
   — surveyed). Config values in shipping console builds are read from
   containers, not loose files — so `configs/` must be *repacked* into a small
   config container (existing `unreal-config-pak-composition` spike) rather
   than served loose. That is an engine reality, not a tooling choice.

---

## 4. Overlay directory structure (task item 4)

Public drop zone (user/Nexus-app facing), owned by the installer exactly as
`roots/` is; **no runtime reads it directly** (C1). The engine-facing truth is
the *projection* described in §5.

```
/data/modbridge/overlay/PPSA33286/
├── overlay.txt                  # manifest: the overlay's loadlist-equivalent
│                                #   (installer-written; byte-truth of the projection)
├── paks/                        # drop .pak / .utoc+.ucas pairs here (already-cooked)
│   ├── SomeMod_P.pak
│   └── LeonBundle/{pak,utoc,ucas}   # archives may arrive as groups/zips; prep
│                                    # classifies and emits owned projections
├── ~mods/                       # loose Lua mods (UE4SS mods-folder layout)
│   └── <ModName>/scripts/main.lua + mods.txt   (consumed by the existing
│                                # loadlist/lua_mod path → preloaded at module_start)
├── configs/                     # .ini overrides — REPACKED into a config container
│   └── <ModName>/engine.ini     #   (engine reads config only from containers, §3.3)
└── textures/                    # converted texture overrides — packed via the
    └── <ModName>/*.dds          #   existing dds/IoStore pipeline into a container
```

Semantics and rules:

- `overlay.txt` extends the loadlist grammar with roles `overlay_pak`,
  `overlay_utoc`, `overlay_uca`, `overlay_loose`; these stay **listed-not-
  preloaded** exactly like today's `pak` role (never read by the runtime;
  C1-clean by construction). Hashes recorded for every projected file.
- One drop folder per title (`PPSA33286`), mirroring `roots/PPSA33286`.
- `~mods` keeps the UE4SS community naming so a manual PC-style drop "just
  works" semantically — but the delivery path is the existing preload system,
  not engine folder-scan.
- Nothing here is ever read by the game process; uninstall/rollback removes
  exactly the projection's owned files and the folder entries, never originals.

### 4.1 Projection map (overlay folder → engine-visible location)

| overlay subtree | preparation | engine-visible destination |
|---|---|---|
| `paks/` (IoStore groups) | identity/classification (existing selection pipeline) | `<dump>/townfall/content/paks/pakchunk9zz-ModBridge_<uuid>_P.utoc/.ucas` |
| `paks/` (legacy PAK) | verify v11/unencrypted; repack only if naming/mount-point must be normalized | `<dump>/townfall/content/paks/pakchunk9zz-ModBridge_<uuid>_P.pak` |
| `configs/` | pack into one small PAK v11 (mount `../../../`) via config-pak composition | `<dump>/townfall/content/paks/pakchunk9zz-ModBridge_cfg_<uuid>_P.pak` |
| `textures/` | dds → IoStore via existing texture/IoStore pipeline | same as IoStore group above |
| `~mods/` (Lua) | register as `lua_mod` roles in `roots/` loadlist (existing verbs) | in-memory at module_start (unchanged path) |

Naming law (from UE pak-priority rules `[UE]` + the §1.4 precedent):
`pakchunk` + a chunk number block reserved high (999x) + `ModBridge` + owned
UUID + mandatory `_P` patch suffix. The exact suffix digits and chunk-number
band get **calibrated from the dump's own update-container names** (the title's
`_P` vocabulary is observable in its installed paks listing) before the first
multi-mod generation — evidence-driven naming, not a guess. Multi-mod ordering:
the installer assigns strictly increasing priorities per generation so later-
installed mods win, and records the mapping in `overlay.txt`.

---

## 5. Chosen architecture and implementation plan

### 5.0 Layer map

- **L0 drop zone** (`overlay/…`, §4) — UX surface, installer-owned.
- **L1 container projection** (approach 1) — the shipped overlay. Engine-native
  mount; no runtime changes.
- **L2 in-process overlay** (approach 2/3, gated by W1–W3) — late/priority
  mounts and true loose-file serving through the UE4SS port, plus the Fios
  overlay research track.

### 5.1 Phase plan (each phase ends with named evidence; no phase claims success without it)

**P0 — freeze this design + census (host, done by this worker).**
Deliverables: this doc, `EVIDENCE-NOTES.md`, `REPORT.md`. No console action.

**P1 — W1: engine-mount verdict (ONE launch, controller-owned).**
Reuse the already-proven staging channel: re-stage one small *original-only*
container trio under the owned `_P` names into the dump's paks dir
(precedent §1.4; game closed; transactional + readback). Launch once with the
user. Evidence: engine-side proof of mount — from the running runtime's log
once the composition loads (mounted-pak enumeration if/where the port can
observe it), or a user-visible original-asset effect (e.g. a known-texture
substitution), or the diagnostic-render-control instrument repeated with a
decisive payload. Outcomes: (a) mounted → L1 green light; (b) not mounted →
naming-law iteration (one variable) before any L2 spend; (c) crash/refusal →
capture and stop (single-variable law).
Note: P1 does **not** require the 30 MB composition module to load; a plain
launch with the staged files exercises the engine's own discovery — the
decoupling is deliberate (§2.d).

**P2 — L1 productization (host + one console rehearsal).**
- New installer verb `project-overlay` (extends stage-3 install machinery):
  reads `overlay/PPSA33286/`, classifies (IoStore group / legacy PAK / config /
  texture / lua), prepares via existing composition primitives, writes owned
  projections add-only into the dump's paks dir with journal + full SHA-256
  readback + rollback; `overlay.txt` is the commit truth (mirrors loadlist
  law). Uninstall/remove verb deletes exactly the owned names.
- Naming/priority calibration from the dump's own container inventory.
- Multi-mod generation policy: deterministic priority assignment, conflict
  report (two mods claiming the same package ID = last-wins by recorded order,
  surfaced to the UI).
- Acceptance: the STAGED-E2E-COMMANDS-style rehearsal for the new verb
  (host-simulated + one console install/uninstall cycle with the game closed),
  then the P1 launch recipe becomes the standing "install file mod" flow.

**P3 — L2a: engine platform-file wrapper + late mount (build on the engine-binding critical path).**
Once the composition runtime reaches `engine-ready` reliably (current blockers
B1–B3 are the existing ladder), add units: bind `FPlatformFileManager` /
`FPakPlatformFile` vtable slots through the existing layout machinery; wrapper
class serving `FileOverrideTable` lookups; `pread`-backed fd keepalive pool
opened during module_start (generalizes the `runtime-status.txt` trick;
`pread` is one new import row, same admission discipline as PATH_TEST's
`read`). Use cases: guaranteed-override-priority late mounts (mount *after*
engine-ready, independent of naming), hot-reload of small assets, loose-file
serving for files engines read by path. Acceptance: in-process wrapper insert
log line + one redirected loose-file read observed from Lua; host tests first
(mirroring the port's host-test/run.sh pattern).

**P4 — L2b: Fios overlay research (bounded, kill-fast).**
W3 first: from the game process, enumerate `libSceFios2.prx` exports (module
info via the existing module-probe lineage) and check for overlay-family
NIDs; W2 (may ride P3's deferred work or a dedicated mini-module): post-
closure open census of candidate paths (dump paks dir via app0-relative,
sandbox root, pfsmnt). If the API exists and a post-closure-readable source
dir is proven: `sceFiosOverlayAdd(source, target=/app0/townfall/content)`
at module_start gives transparent loose-file overlay with zero dump writes —
implement as a small unit with weak-import fallback (the `optional_imports.h`
R1–R4 pattern) so a missing bind degrades to L1/L2a rather than failing the
load. If W3 says no: record dead, never revisit.

**Explicit non-goals:** kernel VFS hooks (§2.a); PAK crypto work (title is
unencrypted — if a future title encrypts, that title gets its own profile and
the gap is reported, not guessed); recooking PC assets (platform cooking
compatibility stays the preparation pipeline's separate question); any claim
that a staged file loaded without W1-class evidence.

### 5.2 Failure/diagnostic surface (learned from the activation design)

- Every projection write is journaled; interrupted install → `recover` verb
  (same law as STAGED-E2E §4.4); game running → refuse, never live-edit.
- `overlay.txt` records per-file sha256 + assigned priority + destination;
  a later audit verb re-lists the dump dir and diffs against the manifest —
  catches drift exactly like the loadlist row-size audit does for `roots/`.
- The runtime's `runtime-status.txt` stays the session truth for Lua-mod
  state; L1 mounts are engine-internal and are evidenced at W1/P1 time, not
  inferred from file visibility (the §1.4 lesson: "filesystem visibility does
  not establish that Unreal loaded the container").

---

## 6. Honest limits

- **No console contact was made by this worker.** Every console fact cited is
  from retained probes (BOOT-PROTOCOL `probe/`, transport ledger JSONs).
- The engine-mount question for this title is **open** (W1). Community
  practice and engine mechanics say approach 1 should work; this design does
  not claim it works — it designs the one-launch proof.
- UE engine behavior items marked `[UE]` are public-engine knowledge
  (pak-priority/naming, platform-file wrapper pattern, config-from-container)
  and are calibrated against this title at P1/P2 time, not assumed.
- PS5 Fios overlay API availability, post-closure sandbox readability, and
  fios NIDs are **UNMEASURED** on this setup (W2/W3 own them).
- The `/data` readability window (C1) is cited from the retained measured
  contract; if a future launch re-measures it differently, §2 rankings for
  L2 sources change, L1 does not.
- Nothing here modifies the frozen composition, ploader pins, staging scripts,
  or any file outside this worker directory.

## 7. Provenance index (primary citations)

| fact | source (this worktree) |
|---|---|
| /data window contract (C1) | `outputs/2026-10-04-zcode-team/worker-f-r2-fix/composition/vendored/modforge_activation.h:4-27`; PATH_TEST in `worker-minimal-path-test/REPORT.md` |
| loadlist roles, listed-not-preloaded paks | same header + `worker-activation-accessibility/ACCESSIBILITY-ANALYSIS.md` §1 |
| dump layout, paks dir, prior `_P` trio staging, `engine_mount…: false` | `.superpowers/native-observations/console-return-2026-10-01.md`; `transport-progress-before-native-default-detour-2026-10-02.json`; `2026-10-01-townfall-mod-trial.md` |
| sandbox/pfsmnt/app0, autoload (shadowmountplus), boot identity | `worker-boot-protocol/BOOT-PROTOCOL.md` §1 + `probe/` |
| ploader injection, libSceFios2 readiness, import law | `worker-ploader-imports/PLOADER-IMPORT-ANALYSIS.md` |
| UE 5.6 ID, container formats unencrypted | RUNTIME_MAP.txt; `2026-09-30-unreal-container-survey.md`; Leon trial docs |
| IoStore v8 TOC merger, PAK v11 writer, IoStore prep | `.superpowers/native-dependency-spike/iostore-toc-overlay-c-1/README.md`; `2026-10-01-native-pak-writer.md`; `2026-10-02-native-downloaded-iostore-preparation.md` |
| FileOverrideTable (PS5-linked, µs lookup) | `2026-10-01-file-override-selection.md` |
| installer verbs / rollback law | `worker-e2e-staging-final/STAGED-E2E-COMMANDS.md` |
| engine-binding critical path (phase-2 dependency) | `outputs/2026-10-06-claude-cloud/MAC-SESSION-BRIEF.md` |
| UE4SS upstream has no file-override system | upstream `assets/UE4SS-settings.ini` (fetched 2026-10-06; `[Overrides]` = mod folders only) |
| UE pak priority `_P`, console drop practice | Epic patch docs + consolemods/psxhaf practice (see REPORT.md sources) |
