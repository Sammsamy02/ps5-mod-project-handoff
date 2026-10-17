# EARLY-LOAD-DESIGN — PS5 UE4SS runtime loaded before game initialization

worker-early-load · 2026-10-06 · research + design only. **No console contact was
made for this document.** All analysis is offline against pinned local copies of
the decrypted dump. Nothing here has been booted; every console-behavior claim
below is explicitly labeled measured / observed / inferred / unknown.

## 0. Inputs and provenance

| Artifact | Path | Identity |
|---|---|---|
| Game-root eboot (patch base) | `outputs/2026-10-04-zcode-team/worker-game-version/console-toplevel-eboot.bin` | 194,241,424 B, sha256 `afe0a2a5fc9e1c17b215bdbb3de3c80eb0b3e1aef5c794aea491805899030b7`, sha1 `287c23b09366b52fc67d7112dac9acc707de39fa` |
| Decrypted copy (observations) | `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin` | same size, sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` (matches the task's `3b6cfad4` prefix) |
| Capture receipt | same dir, `capture.json` | remote path `/data/homebrew/Silent Hill Townfall PPSA33286/decrypted/eboot.bin`, complete 194 MB RETR, console_write=false |

The two 194 MB files are **the same ELF**. A byte-exact diff shows 5,386
differing bytes in 1,308 runs, all at file offset ≥ `0xb93c32e`, i.e. entirely
inside the trailing certificate-TLV segment (PH12, type `0x6fffff01`, records
`crt1:`, `crtbegin`, `libScePsr:`, …). The game-root file has that region
zeroed; the observations copy retains it. **Every structure this design touches
(dynamic table `0xb93b278..0xb93c2c8`, strtab `0x930f420..0x9317b61`) is
byte-identical in both files**, so all offsets below apply to either. The
patch script asserts the exact expected bytes before writing and refuses
anything else.

Reference artifacts produced in this directory:

- `patch-apply.py` — reference implementation (proposal-only; applied offline to local copies).
- `patch-verification.txt` — transcript of both patch runs (all before/after bytes).
- `eboot-earlyload-A1.bin` — minimal-variant patched copy (10 bytes changed), sha256 `3374b5ab18f4c5feb58b07a951d845e6462e40dcf7d08bc9b35f6f612b411960`.
- `eboot-earlyload-A2.bin` — full-variant patched copy (31 bytes changed), sha256 `cbb4ee4dd2766e649f597490cf77ea0efa69fc90cc463c7415606b4993255e34`.
- `dynamic-table.txt` — full 261-entry dynamic-table dump with decoded strings.

## 1. Executive summary

**Chosen approach: add one `DT_NEEDED "mb.prx"` entry to the game-root
eboot's dynamic table by recycling four low-value dynamic slots, plus a
7-byte name string in the one genuinely free gap of the packed dynlib
region.** The PS5 dynamic loader then loads and starts our module during
process creation — after libc/libkernel are up (table order) but **before
`DT_INIT` (UE5 static-init walker), before `e_entry`, and before any engine
code** — which is the exact parity point with PC UE4SS's pose-as-system-DLL
trick.

Expected timing: our `module_start` executes at ≈ T+0 of the game process
(kernel dynlib phase, first hundreds of milliseconds after spawn), versus the
measured ~70.7–71.8 s ploader gap today. Net improvement ≈ **70 s → <1 s,
and strictly earlier than every UE constructor** — enabling pre-engine hooks
(GUObjectArray/GEngine construction) that are impossible at 70 s.

Two hardware-verification unknowns remain (kernel's tolerance of a metadata-less
`DT_NEEDED`, and acceptance format for the file itself), handled by a staged
A/B/C variant ladder and a 10/31-byte instant-rollback in §7.

## 2. eboot analysis (all facts below are measured from the pinned bytes)

### 2.1 ELF identity

- ELF64 LE x86-64 (`e_machine 0x3e`), OSABI 9 (FreeBSD — Sony convention).
- `e_type = 0xFE10` (Sony `ET_SCE_DYNAMIC` main-executable flavor). **Plain ELF,
  no SELF/FSELF envelope** — the loader boots it as-is today (observed: the game
  runs from this dump; root file starts `7f 45 4c 46`).
- `e_entry = 0x80` — inside the first LOAD segment (mapped ELF-header page),
  *not* engine code; it is the Sony ERTI entry stub (see 2.4).
- 14 program headers; section-header table points past EOF (`e_shoff
  0x6f48a0a8` > file size) — sections were stripped; everything lives in the
  program headers. The PS5 loader ignores section headers, so this is normal.

### 2.2 Program headers (abridged; full parse in patch-verification transcript)

| # | type | off | vaddr | filesz | notes |
|---|---|---|---|---|---|
| 0 | LOAD R-- | 0x4000 | 0x0 | 0x7479b1c | mapped ELF headers + UE init stubs at vaddr 0x10/0x80 + 122 MB rodata |
| 1 | LOAD --X | 0x7480000 | 0x747c000 | 0xa3e918 | engine text |
| 3 | LOAD RWX | 0x7ec0000 | 0x7ebc000 | 0x127e4e8 | data |
| 6 | SCE_DYNLIBDATA | 0x913e488 | 0x913a488 | 0x60 | UE5 init-func pointer tables (filled by relocations) |
| 7 | LOAD RW- | 0x9140000 | 0x913c000 | 0x1cc5f2 (+bss) | data |
| 8 | LOAD --- | 0x930f420 | 0x9633420 | 0x262cea8 | **the whole dynlib region**: strtab → symtab → jmprel → rela → hash → GNU note → PT_DYNAMIC |
| 10 | DYNAMIC RWX | 0xb93b278 | 0xbc5f278 | 0x1050 | 261 × 16-byte entries |
| 11 | 0x6fffff00 | 0xb93c2d0 | — | 0x5c | "PATHT" table (Jenkins build path string) |
| 12 | 0x6fffff01 | 0xb93c32c | — | 0x201c | certificate TLV records; zeroed in game-root file |
| 13 | NOTE | 0xb93e348 | — | 0x48 | all zeros |

### 2.3 The dynamic table — packed, 261 slots, exactly one terminator

- 261 entries of 16 bytes = `p_filesz` exactly; **the only `DT_NULL` is the very
  last slot [260]**. There is no trailing padding inside the segment and no
  adjacent free file space (next bytes are PH11's "PATHT" table). **A new entry
  cannot be appended; existing slots must be recycled.**
- **57 `DT_NEEDED` entries** in load order: libScePad, libSceNet, libSceNetCtl,
  libSceAudioOut, libSceAjm, libSceSysmodule, libSceVideoOut, libSceUserService,
  libkernel, libSceSsl, libSceHttp(2), libScePlayGo, libSceNpManager,
  libSceAppContent, libSceSaveData(.native/.Dialog), libSceCommonDialog,
  libSceGameLiveStreaming, libSceSystemService, libSceIme(Dialog),
  libSceMsgDialog, libSceNpCommerce, libSceMouse, libSceRandom,
  libSceWebBrowserDialog, libSceAgc(Driver), libSceRtc, libSceShare,
  libSceContentExport, libSceNpGameIntent, libSceVideodec2, libSceAudiodec,
  libSceVoiceQoS, libSceNpAuth, libSceJson2, libSceNpCppWebApi, libSceNpWebApi2,
  libSceNpSessionSignaling, libSceNpEntitlementAccess, libSceSigninDialog,
  libSceNpUniversalDataSystem, libSceNpTrophy2, libSceErrorDialog,
  libSceGameUpdate, libScePlayerInvitationDialog, libSceAudioIn, libSceAcm,
  libScePlayGoDialog, libfmod, libfmodstudio, libScePsml_debug,
  libSceAvPlayer.native, **libc**, libSceAmpr. (Full decode: `dynamic-table.txt`.)
- Each `DT_NEEDED` is followed by a Sony metadata triple:
  `0x61000045` = (module_index << 48 | 0x01 << 40 | 0x01 << 32 | strtab_off of
  the lib short name), `0x61000049` = (lib_index << 48 | 0x0001 << 32 |
  strtab_off), `0x61000019` = per-lib flag (value 9 on system libs, **1 on
  libfmod/libfmodstudio — the app-staged prx/** libs). Module indices 0x1–0x39
  and lib indices 0x0–0x3b are used; **0x3a / 0x3c are the next free**.
- Auxiliary entries: `DT_SONAME`-class `0x61000043` = "Townfall-PS5-Shipping",
  fingerprint `0x61000041` = the Jenkins build path, `DT_RELA` 0x9653430 size
  0x2607ee8 (1,649,812 relative relocs — matches `DT_RELACOUNT` 0x192b94),
  `DT_JMPREL`, `DT_SYMTAB` (2022 × 24 B), `DT_HASH` (2022 buckets), and
  — decisive for §6a — `DT_INIT_ARRAY=0`, `DT_INIT_ARRAYSZ=0`,
  `DT_FINI_ARRAY=0`, `DT_FINI_ARRAYSZ=0`.

### 2.4 Initialization order (what actually runs, in order)

1. Kernel maps PH0–PH8; **loads all 57 `DT_NEEDED` modules in table order**
   (deepest-first through their own dependencies), running each one's
   `module_start`. libc/libkernel are up before the tail of the list; libfmod
   proves app-staged `/app0/prx/*.prx` resolves through this same path (it is
   `DT_NEEDED[52]` and ships only in the game's `prx/` folder — manifest
   evidence).
2. Relocations applied (40 MB `DT_RELA`, including the 1.6 M relative relocs
   that fill UE5's init-pointer tables in `PT_SCE_DYNLIBDATA`).
3. **`DT_INIT` = vaddr 0x10** runs: a hand-written walker (disassembled at file
   0x4010: `lea rbx,[rip+0x913a46a]` → tables at 0x913a48a) that calls every
   relocated function pointer in two tables — this is UE5's static-constructor
   runner. **This binary has no `.init_array`; UE5 on PS5 uses `DT_INIT` +
   relocated tables instead.**
4. `e_entry = 0x80` — the Sony ERTI stub converts the kernel module-entry
   argument block into argc/argv and calls engine `main` (calls into PH1 text at
   0x747c000+), i.e. guarded main → engine init → boot.
5. Today's ploader path: ~50 ms polling + 1.6 s/plugin readiness + attach ⇒ the
   measured 70.7–71.8 s gap (worker-ploader-70s, 4/4 boots) — the delay this
   design removes.

**Insertion point:** a `DT_NEEDED` at recycled slot [236] sits after
libSceAmpr in table order ⇒ loads 58th/last, after libc, but still before
relocation, `DT_INIT`, `e_entry`, and every engine subsystem.

### 2.5 Free space census (why the patch is shaped the way it is)

The dynlib region inside PH8 is **fully packed, byte for byte**:

```
strtab 0x9633420 +0x8741 | 7 FREE ZERO BYTES | symtab 0x963bb68 +0xbd90
| jmprel 0x96478f8 +0xbb38 | rela 0x9653430 +0x2607ee8
| hash 0xbc5b318 +0x3f38 | GNU note 0xb93b250 +0x24 | 4 pad | PT_DYNAMIC 0xbc5f278 +0x1050 = end of PH8
```

The **only** writable free bytes are the 7-byte gap after `.dynstr`
(file `0x9317b61`, strtab offset `0x8741`) — exactly enough for `mb.prx\0`
(6 chars + NUL). There is no room to grow the strtab, symtab, rela, or the
dynamic table in place; hence string-in-gap + slot recycling.

## 3. Feasibility — does the game really load from this directory? (task §4)

Evidence, strongest first:

1. **The whole mod pipeline already depends on it.** The modbridge runtime
   writes/reads game files under `/data/homebrew/Silent Hill Townfall
   PPSA33286/`, and in-game supplier diagnostics report `stat_app0 = 0`
   (success) for game pids 317/355/370/466/492 (worker-game-version
   `modbridge-supplier-diag.jsonl`) — the game's `/app0` is backed by the dump
   directory's content.
2. **mount.lnk** (worker-game-version) contains
   `/data/homebrew/Silent Hill Townfall PPSA33286` — the etaHEN shadow-mount
   link that binds this folder as the title's app content at boot.
3. **capture.json** pulled `decrypted/eboot.bin` from inside that exact folder.
4. **The game-root eboot is a plain ELF 0xFE10** (measured), and the title boots
   under the resident etaHEN kernel state (`kstuff state=1 … crypt=deb7/deb7`,
   etahen-loader-state-2026-10-03-1). The loader therefore already accepts an
   unencrypted, unsigned ELF at the boot target — **no FSELF re-signing step is
   required for the patched eboot.** (The fake-package infra that produced the
   plugin FSELFs remains available as a fallback wrapper if some future loader
   state rejects plain ELFs.)

Residual uncertainty (flagged, with a §7 check): which *filename* the loader
takes at the game root (`eboot.bin`) and that the on-console root file still
matches `afe0a2a5…`. The pre-patch protocol in §7 verifies both by FTP before
anything is staged. FTP law from the regression ledger: RETR of a plain ELF is
byte-identity, and a 194 MB RETR is proven (capture.json, 6.8 s) — so upload +
byte-exact readback verification of the patched eboot is operationally safe.

## 4. The patch (chosen approach A) — exact bytes

Base: game-root `eboot.bin`, sha256 `afe0a2a5…30b7`. Two variants; each also
writes the name string and bumps `DT_STRSZ`. All little-endian.

### 4.1 Common writes (both variants)

| File offset | Len | Before (hex) | After (hex) | Meaning |
|---|---|---|---|---|
| `0x09317b61` | 7 | `00 00 00 00 00 00 00` | `6d 62 2e 70 72 78 00` | write `mb.prx\0` at strtab offset `0x8741` (the verified gap) |
| `0xb93c200` | 8 | `41 87 00 00 00 00 00 00` | `48 87 00 00 00 00 00 00` | dynamic[248] `DT_STRSZ` 0x8741 → 0x8748 (cover the new string) |

### 4.2 Variant A1 — minimal (try first on hardware): 10 bytes total

| File offset | Len | Before | After | Meaning |
|---|---|---|---|---|
| `0xb93c138` | 8 | `15 00 00 00 00 00 00 00` | `01 00 00 00 00 00 00 00` | dynamic[236] tag `DT_DEBUG` → `DT_NEEDED` |
| `0xb93c140` | 8 | `00 00 00 00 00 00 00 00` | `41 87 00 00 00 00 00 00` | value 0 → `0x8741` (⇒ `DT_NEEDED "mb.prx"`) |

Loss: `DT_DEBUG` only (debugger rendezvous slot; unused in normal boot).

### 4.3 Variant A2 — with Sony metadata triple (fallback if A1's bare NEEDED is
ignored or rejected): 31 bytes total = A1 plus

| File offset | Len | Before | After | Meaning |
|---|---|---|---|---|
| `0xb93c178` | 8 | `f9 ff ff 6f 00 00 00 00` | `45 00 00 61 00 00 00 00` | dynamic[240] tag `DT_RELACOUNT`(`0x6ffffff9`) → `0x61000045` |
| `0xb93c180` | 8 | `94 2b 19 00 00 00 00 00` | `41 87 00 00 01 01 3a 00` | value → `0x003A0101_00008741` (module idx 0x3a, str "mb") |
| `0xb93c288` | 8 | `0d 00 00 00 00 00 00 00` | `49 00 00 61 00 00 00 00` | dynamic[257] tag `DT_FINI` → `0x61000049` |
| `0xb93c290` | 8 | `d0 9a 47 07 00 00 00 00` | `41 87 00 00 01 00 3c 00` | value → `0x003C0001_00008741` (lib idx 0x3c, str "mb") |
| `0xb93c298` | 8 | `3f 00 00 61 00 00 00 00` | `19 00 00 61 00 00 00 00` | dynamic[258] tag `0x6100003f` → `0x61000019` |
| `0xb93c2a0` | 8 | `90 bd 00 00 00 00 00 00` | `01 00 00 00 00 00 00 00` | value `0xbd90` → `1` (fmod-style app-lib flag) |

Slot-loss analysis for A2 (ranked by risk):

- `DT_RELACOUNT` — pure optimization hint (count of leading `R_*_RELATIVE`
  relocs). `DT_RELA`/`DT_RELASZ`/`DT_RELAENT` remain, so correctness is
  preserved; worst case is a slower single pass. Low risk.
- `DT_FINI` (0x7479ad0) — exit-time destructors. A game title never exits
  cleanly through them, and PS5 may ignore `DT_FINI` for the main module
  (unknown; harmless either way). Low risk.
- `0x6100003f` (= 0xbd90, the symtab byte-size hint) — **highest-risk
  recycle**: if the kernel dlm uses this tag as the authoritative symbol-table
  size instead of deriving it (e.g. from hash nchain), losing it could break
  symbol binding and abort boot. If A2 fails, use **A2b**: keep [258] as-is and
  ship `NEEDED+0x45+0x49` only (three recycled slots; the `0x61000019` flag is
  plausibly optional — 0x45/0x49 carry the indices). A2b bytes = A2 minus the
  last two rows.
- The `0x61000019` flag value: `1` (fmod/app-lib style) chosen over `9`
  (system style) because our module is app-staged; semantics unverified — a
  one-byte A/B if A2 misbehaves.

Both variants verified offline by full re-parse (see `patch-verification.txt`):
58 `DT_NEEDED`s, `mb.prx` resolves through the strtab, `DT_NULL` still last,
nothing else moved. Patched copies: A1 sha256 `3374b5ab…11960` (10 bytes
changed), A2 sha256 `cbb4ee4d…25e34` (31 bytes).

### 4.4 Creating the loadable module (`mb.prx`)

- **Format**: this console's kernel dlm accepts (a) plain `ET_SCE_DYNAMIC`
  ELF modules — every system lib is one (measured: pulled `libkernel.sprx` =
  plain ELF, `e_type 0xfe18`) — and (b) fake-self containers (magic
  `4f 15 3d 1d`, the format of every plugin prx this project ships). **On this
  console the only *proven-good* user-module load to date was a fake-self
  wrapped prx** (regression ledger: pid 700 ran the FSELF `07ac3e1e`; bare-ELF
  composition builds failed L1–L5, cause not fully isolated). **Rank: stage
  the fake-self wrapper first, plain 0xfe18 ELF as the A/B.** This is the same
  build toolchain the project already uses for plugin prx (native-sdk-build-check /
  ps5-payload-sdk), so no new dependency.
- **Entry**: standard Sony PRX `module_start(SceKernelModuleHandle, reason,
  void*)` returning 0 (KSCE_... OK). Keep `module_stop` returning 0. The
  existing runtime already boots correctly as a ploader plugin, i.e. the
  module-start contract is already implemented and proven — reuse that code
  path, minus ploader's shellcode trampoline.
- **Imports**: restrict to `libkernel` + `libc` NIDs (already resolved earlier
  in table order; the existing plugins import exactly these). An unresolvable
  import in a `DT_NEEDED` module is a boot-abort risk that ploader loading
  never exercised — keep the import surface minimal.
- **Deployment location**: `/app0/prx/mb.prx` (fmod precedent proves the
  `prx/` dir is on the NEEDED search path), plus a byte-copy at
  `/app0/sce_module/mb.prx` as belt-and-braces for the `sce_module` path.
  Note the ledger's ftpsrv law: RETR of a SELF/FSELF returns the
  decrypted+stripped *view*, so verification of the fake-self mb.prx must use
  view-hash receipts (established flows), not raw-byte readback; the patched
  eboot itself is a plain ELF so its readback is byte-exact.
- **What runs**: `module_start` at step 1 of §2.4 — before relocations are
  applied to the *eboot's* tables? No — needed-modules load before the main
  module's relocation pass, so the runtime must not assume eboot globals are
  initialized. The correct early job for `module_start` is: install hooks /
  set up the bridge, and defer engine-dependent work to a thread that waits on
  engine readiness (the existing modbridge worker channel already implements
  readiness polling). This ordering is precisely the value of early load: hook
  `DT_INIT`-time and ctor-time engine state instead of arriving after the
  menu.

### 4.5 Expected patched boot sequence

```
kernel spawns title process
 ├─ map eboot PH0–PH8
 ├─ DT_NEEDED load loop (table order): libScePad … libfmod, libfmodstudio,
 │   libScePsml_debug, libSceAvPlayer, libc, libSceAmpr,
 │   ★ mb.prx (slot 236) → our module_start runs HERE  ← ≈ T+0, before any game code
 ├─ relocations (1.65 M relative)
 ├─ DT_INIT 0x10 → UE5 static-init tables
 ├─ e_entry 0x80 → ERTI → engine main → guarded main → engine boot
 └─ … menu … game …                         (ploader path no longer needed for OUR module)
```

## 5. Timing improvement (evidence-based bounds)

| Path | Our runtime present at | Evidence |
|---|---|---|
| Today (ploader) | ~70–71 s after game start (plugin-to-plugin gap 70.71–71.83 s, 4/4 boots) | measured, worker-ploader-70s |
| Ploader + patches 02+05 (proposal) | ≤ ~8 s | source-derived projection, worker-ploader-70s |
| **This design (DT_NEEDED)** | **kernel dynlib phase, ≈ T+0–0.3 s, before DT_INIT / all UE ctors / e_entry** | structural fact of loader order (§2.4); absolute wall time to be measured once on hardware |

Beyond the ~70 s cut, the qualitative win is ordering, not just latency: the
runtime exists before engine construction, restoring the PC-UE4SS capability
profile (constructor-time hooks, full-lifecycle coverage from first frame).

## 6. Alternatives (task §3) — analysis and why they are not primary

**(a) Patch `.init_array` to call our module first — structurally unavailable
as designed, viable only as a DT_INIT trampoline.** The binary has *no*
init_array (`DT_INIT_ARRAY=SZ=0`, measured); UE5 on PS5 runs its constructors
through `DT_INIT` = 0x10 walking relocation-filled tables in
`PT_SCE_DYNLIBDATA`. Appending to those tables needs new `DT_RELA` slots, and
the rela table ends *exactly* where `.hash` begins (measured) — no space
without moving 40 MB of file. Repointing `DT_INIT` at a trampoline stub in the
first LOAD's code caves (24 zero bytes at vaddr 0x68; CC-padding at 0xd0) could
`call` something, but at `DT_INIT` time no foreign module is loaded, and the
eboot cannot `sceKernelLoadStartModule` without an import stub table we cannot
grow (same packing argument). It would only work *combined with* approach A to
guarantee ordering *within* the needed-module set — unnecessary since slot 236
already orders us last. **Rejected.**

**(b) Replace an existing library with a stub that loads ours then forwards —
viable Plan B, zero eboot bytes.** The best target is
`/app0/sce_module/libSceNpCppWebApi.prx`: app-staged, `DT_NEEDED`-loaded
(position ~35 of 57, after libc — early), and online-only (offline dump). A
replacement stub must export every NID the process imports from it (enumerable
offline from the eboot symtab/hash: 2,022 symbols; the import-lib binding for
index 0x27 "libSceNpCppWebApi" gives the exact set) as error-returning stubs,
and additionally `sceKernelLoadStartModule("/app0/prx/mb.prx")` from its
`module_start`. Costs vs A: no forwarding semantics exist for NEEDED libs (the
game's calls must be stubbed, killing online features), a large NID surface to
author, and it still requires building a loader-accepted replacement module —
the same mb.prx problem. **Held as fallback if the kernel rejects approach A's
table surgery but accepts file replacement.**

**(c) Kernel-level: intercept process creation and inject before any user code
— partially available, complementary.** The console already runs a payload
manager (`/data/pldmgr/`, `loadlist.txt`, ploader at
`/data/pldmgr/payloads/ploader/ploader.elf`). A pldmgr/kernel-notification
trigger at process-create (instead of 50 ms userspace polling + readiness
waits) would land injection at ~T+1–3 s without touching any game file — but
still *after* the dynlib phase and after `DT_INIT` begins, so it can never
restore constructor-time hooking, and it remains an attach-based injection
(readiness gates, attach failures) rather than a first-class loaded module.
The worker-ploader-70s patch series (02 hoist-readiness + 05 wall-clock
deadline → ≤ ~8 s) is the no-risk improvement in this family. **Keep as
mitigation/backup lane; not the early-load goal itself.**

## 7. Rollout, verification, and rollback (one change per boot; proposal-only)

Pre-flight (no game run):
1. FTP LIST game root; RETR root `eboot.bin` → confirm sha256 `afe0a2a5…`
   (plain-ELF RETR is byte-identity; 194 MB RETR proven). If it differs, STOP
   and re-derive offsets from the actual bytes (`patch-apply.py` aborts safely
   on mismatch — it is base-locked).
2. Build mb.prx twice (fake-self wrapper + plain 0xfe18), view-hash receipts.

Staging ladder (each boot = exactly one variable):
- **Boot 0** — control: current files, confirm baseline boot + logs.
- **Boot 1** — stage **A1** eboot + fake-self `mb.prx` in `prx/` (+`sce_module/`).
  Success = our module_start log line timestamped before the earliest engine
  log line; failure modes distinguish loader-vs-format.
- **Boot 2** — if A1 ignored (module absent but game boots): stage **A2**
  (metadata triple present).
- **Boot 3** — if A2 boot-aborts: stage **A2b** (drop the `0x6100003f` recycle,
  keep `0x45/0x49`).
- **Boot 4** — format A/B: swap fake-self ↔ plain-ELF mb.prx.
- Any abort: restore the 10/31 base bytes (rollback = restore saved eboot.bin;
  keep the pristine copy on console before first stage). mb.prx alone is inert
  once the eboot is restored — no residual risk.

Measurement: reuse worker-ploader-70s PATCH 03 steplog discipline — one
timestamped line in module_start and one at first engine contact; acceptance =
module_start strictly earlier than the first engine init marker.

## 8. Honest limits

- Offline facts (headers, table, packing, diff, strtab gap, metadata indices)
  are byte-verified; nothing console-side has run. The kernel dlm's handling of
  a metadata-less `DT_NEEDED`, the meaning of the `0x61000019` flag, the
  authority of `0x6100003f`, and fake-self acceptance *through the
  DT_NEEDED path* (vs ploader's `sceKernelLoadStartModule`) are unknowns with
  an explicit A/B ladder in §7 — not assumptions.
- Which file name the loader takes at the game root and the identity of the
  live root eboot are pre-flight checks, not assumptions (§7 step 1).
- The 70 s figure is measured; the "≈ T+0" placement is a structural
  consequence of loader order, but its absolute wall time is a
  to-be-measured-once quantity.
- No game-quality, runtime, or in-game claims are made here; this document is
  loader-level design only.
