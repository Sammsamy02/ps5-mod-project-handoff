# PAK PIN PASS — offline pin pass for the no-file-change mount design

Agent HJ (ZCode), 2026-10-10. Companion to GL's `PAK-RUNTIME-MOUNT-DESIGN.md`
(its §6.5 "Missing static pins" is the assignment). **Static analysis only**:
preserved decrypted eboots, read-only; no builds, no console, no tree edits
beyond this pass's own outputs. Tool: `phasea-tools/pak_vtable_pins.py`
(capstone 5.0.7; phdr map + RELA reconstruction + annotated disasm + vtable
slot classifier). Pins JSON: `pak-pin-pass-pins.json` (GJ per-version-table
field style). Report: `agent-HJ-pinpass.md`.

## 0. Image basis (evidence law)

- Canonical image: `/tmp/eboot-3bc-verify.bin`, sha256
  `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`,
  194,241,424 B — hash re-verified this session; identical to the
  `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
  pin used by worker-kernel-pak and GL's [RE] tags, and to HA's MANIFEST
  entry 1258. All vaddrs below are IMAGE vaddrs of this image; fileoff =
  vaddr + 0x4000 for ph0/ph1/ph3.
- Cross-snapshots: HA MANIFEST class entries T1 (`0a9c60b6…`), T2
  (`c23fb977…`), T3 (`e1538d9c…`) — still present in `/tmp`, hashes
  re-verified against the MANIFEST. **Every pin in this document was
  re-derived byte-identically on all four images** (vtable RELA addends,
  mount-helper code, accessor code, ctor site). The four images differ only
  in the first 4 KiB (ELF/SCE header page) and the dynamic-metadata tail
  (0xb93c000-0xb93e000 versym/verneed area): same build, different SELF
  passes. Live base 0x400000 is the Townfall observation (GJ table
  `observed_live_base`); all pins are image-relative.
- Negative control: reconstructed Astro Bot ELF
  (`FZ-GAME2/astro-inner-decrypted.elf`, sha `7723d150…`) — 0/244 UE engine
  markers (FZ-GAME2 receipt) AND zero hits for both discriminating anchors
  of §5. This is not a UE image; anchors must not be expected to fire there.

Segment map (this image): ph0 text `0x0-0x7479b1c` (X), ph1 rodata
`0x747c000-0x7eba918`, ph3 RELRO data `0x7ebc000-0x913a4e8` (RW on disk),
ph7 RW+BSS `0x913c000-0x9633418`, ph8 `0x9633420-0xbc5f2c8` (fileoff =
vaddr − 0x324000; holds DT_RELA `0x9653430` size `0x2607ee8` and dynsym).
**RTTI is absent** (typeinfo slots are zero; dynsym names Sony-obfuscated) —
class identity below is derived from code shape, not typeinfo.

**Data-word law**: every pointer in ph3 (all vtables) is ZERO in the raw
file; real values exist only as `R_X86_64_RELATIVE` relocations (1,649,556
of them). All vtable reconstruction here follows the GMalloc-pass law
(RELA-reconstructed vtables), same as GJ's fmalloc entry.

## 1. Chain-head singleton (GL's A1 discovery entry) — MEASURED

The mount helper itself performs the discovery (its utoc-companion tail,
0x176f2bc/0x176f2c4): it calls `Get()` then `GetPlatformFile()` and
FileExists-checks the `.utoc` through the chain top. Decoded:

| Pin | vaddr | shape (proved) |
|---|---|---|
| `FPlatformFileManager::Get()` | **0xb8ed0** | `movzx eax,byte[0x932a438]; test; jne+8; lea rax,[0x932a428]; ret`; lazy path wraps `__cxa_guard_acquire`(PLT 0x7471eb0)/`guard_release`(0x7471ec0); returns `&0x932a428`. 157 E8 call sites. |
| GPlatformFileManager static | **0x932a428** (ph7 BSS) | singleton object; guard byte at +0x10 (0x932a438) |
| `TopPlatformFile` member | **[0x932a428 + 0x0]** | active chain head |
| `FPlatformFileManager::GetPlatformFile()` | **0xb8a70** | `mov rax,[rdi]; ret`; on null lazily builds via **0x4ca0** (`GetDefaultPlatformFile`). 135 call sites. |
| default physical instance | **0x9308808** (ph7) | guard 0x9308810; ctor 0x5b80; physical class vtable **0x7ebc6a0** |

A second lazy accessor **0xb8aa0** reads **[0x932a428 + 0x8]** (only 2 call
sites). The pak module factory (§2) installs each freshly constructed
FPakPlatformFile into an `[arg+8]` slot with the previous occupant released
via the deleting-dtor slot (+0x8) — consistent with `[GPlatformFileManager+8]`
being the engine-wide FPakPlatformFile holder, but the factory's caller sits
behind the module vtable and was not traced to its RDI source this pass.
**Pin status: the +0/+0xb8ed0/0xb8a70 recipe is measured; the `[mgr+8]`
pak-file slot is a Phase-0 console confirm** (read `[0x932a428+8]` on console
and check vptr == live FPakPlatformFile vtable). The A1 recipe does not
depend on it: walk from `[0x932a428+0]` following `[obj+8]` links and match
vptrs against the §2 identity.

Chain-walk corroboration (FEngineLoop-ish code at 0x3a15ce0): walks from the
chain top calling slot +0x38 per element and descending via slot **+0x68 =
GetLowerLevel** (impl 0xb3500 = `mov rax,[rdi+8]; ret` — GL's "[this+8]
inner links" re-proved from a second direction; +0x70 is the writer
0xb3510 `mov [rdi+8],rsi`).

## 2. FPakPlatformFile vtable identity — MEASURED

- **Vtable image vaddr: 0x7fee8c0** (ph3 RELRO). 16-byte header
  (offset-to-top 0, typeinfo NULL) + **74 slots (0x248 bytes)**; ends before
  the FPakFile vtable at 0x7feeb10.
- Construction site: module factory **0x178a9e0** (module vtable 0x7feffa0,
  slot2) allocates **0xF0** bytes, calls base ctor 0x84ca0, then the inline
  ctor sequence at **0x178aa1c** `lea rax,[0x7fee8c0]; mov [r14],rax`. The
  same factory registers the DLC/on-demand loop 0x1764de0 as a delegate
  (via 0x1764c60 into `[0x1d21b0()+0x58]`) and registers the "RSA" pak
  signing key FName (ctor 0x23d590).
- Dtor pair: complete **0x17652f0**, deleting **0x17656a0** (self-LEA the
  vtable start at 0x1765308 — the classic vptr-restore signature).

Slot map (offsets from vtable base; `inner` = [this+8]):

| off | impl | identity | evidence |
|---|---|---|---|
| +0x00/+0x08 | 0x17652f0 / 0x17656a0 | dtor pair | ctor/dtor vptr stores |
| +0x10/+0x18 | 0xb290 / 0xe0 | stubs (void / return-false) | not overridden; names unknown |
| +0x20 | 0x1769df0 | **ShouldBeUsed** | −NoPak cmdline check shape |
| +0x28 | 0x1769ee0 | **Initialize** | assert string `FPakPlatformFile::Initialize` |
| +0x30, +0x38 | stubs | (InitAfterIoReaders / InitAfterProjectFilePath-shaped) | +0x38 called by the chain walk |
| +0x40 | 0x1771240 | real, FString-arg — unlabeled | [GAP] |
| +0x50/+0x58 | 0x25340/0x25380 | folded inner-dispatch helpers | ICF-folded shared code |
| +0x68 | 0xb3500 | **GetLowerLevel** `mov rax,[rdi+8]; ret` | chain walk 0x3a15cef |
| +0x70 | 0xb3510 | SetLowerLevel-shaped | writes [this+8] |
| +0x78 | 0x17643d0 | **GetTypeName** → L"PakFile" | string return |
| +0x80 | 0x177c2c0 | **FileExists** | call sites 0x176ec9d (mount helper), 0x176f2f6 (utoc check), 0x1719880; pak-map probe 0x1765d30 then inner-forward; GL's +0x80 **verified** |
| +0x88 | 0x177c360 | **FileSize** | THIS PASS: int64 return (physical impl 0x7810 loads stat size, returns rax); signed-failure `js` usage at the .ucas partition query 0x171ba72; exists(+0x80)→size(+0x88) pairing 0x1719880/0x17198a8 |
| +0x90 | 0x177c460 | bool dir-family (IsDirectory/DirectoryExists) | physical impl 0x7060 `sete al` |
| +0x98 | 0x253d0 | folded | |
| +0xa0/+0xa8/+0xb0 | 0x177c500/0x177c5a0/0x177c650 | dir/move family | self-forward at own offsets |
| +0xb8 | 0x177c6f0 | **OpenRead** | THIS PASS: FPakFile ctor call site 0x17722d2 `call [rax+0xb8]` (filename-only arg), handle stored [pak+0x2d0] |
| +0xc0..+0xd0 | 0x177c7d0/0x177c870/0x177c950 | OpenWrite-class + handle-releasing variants | self-forward |
| +0xd8, +0x118, +0x1f8 | 0xe3240/0xe3ab0/0xe3c10 | pure same-offset forwarders | `mov rdi,[rdi+8]; mov rax,[rdi]; jmp [rax+SAME]` |
| +0x108 | 0x1770e40 | heavily used by IoStore — OpenAsyncRead-class, unlabeled | [GAP→Phase-0] |
| +0x110 | 0x1769800 | used by pak scan + IoStore | |
| +0x1c0 | 0x1769150 | **IterateDirectory** | call site 0x1769ac4 (per-folder scan); GL's +0x1c0 **verified** |
| +0x1c8 | 0x1767e60 | IterateDirectoryRecursively-class | |

Layout cross-validation: **17 pak-module slot impls forward to inner at
their own offset** (+0x80/+0x88/+0x90/+0xa0/+0xa8/+0xb0/+0xb8/+0xc0/+0xc8/
+0xd0/+0xf8/+0x108/+0x110/+0x128/+0x1e8/+0x200/+0x210/+0x218 family) —
the interface offsets are self-consistent across classes (a mis-derived
vtable base would break this agreement).

## 3. Mount helper ABI — refined (two corrections to GL's shape)

`0x176ec30` full call shape, re-measured:

```
RDI  this        FPakPlatformFile*
RSI  &args       struct { TCHAR* PakPath @0; int32 Priority @8;
                          TCHAR* MountPoint @0x10 (null = derive from pak);
                          ...; bool Flag @0x1c }        (built by wrapper 0x176b900)
RDX  OutError    FString* (nullable) — receives "Pak not found" /
                 "Pak is invalid" / "Encryption key not available"
RCX  OutPak      TRefCountPtr<FPakFile>* (nullable) — receives the mounted pak
return: bool success; MountedPaks TArray append is inside the helper
```

Corrections vs GL §1: (1) RDX is the OutError FString, NOT the priority —
the priority rides `args+8` (the wrapper computes it via 0x1770140 when
passed −1); (2) the mount record is `{FString PakPath@+0x00; FString
MountPoint@+0x10; int32 Priority@+0x20; 16-byte GUID@+0x24; int32@+0x34}`
(0x38 bytes) — GL had "filename@+0x10"; +0x10 is the mount point.

Confirmed internals: FileExists via `[this+8]→vt+0x80`; 0x330-byte FPakFile
alloc (0x4896c80) + ctor 0x1771fa0; validation flag `[pak+0x2e1]`;
`_P.pak` Contains (utf16 @0x775a23a) with priority bump `+= 100 × digit
position` (imul ebx, r14d, 0x64 at 0x176f152); MountedPaks at
`[this+0x20]/[this+0x28]/[this+0x2c]` ✓ GL verified.

**New: the helper's tail mounts the utoc companion.** When `[this+0xc0]`
(mount-associated-container flag) is set, it: gets the chain top via
Get()/GetPlatformFile() (0x176f2bc/0x176f2c4); FileExists(+0x80)-checks the
`.utoc` (utf16 @0x7974c24; "utoc not found" @0x7783530); allocates a 0xB0
container object (ctor **0x177fc90**) installed at `[pak+0x328]`; registers
it with the registry at `[this+0xd0]` via **0x175a4d0**. This is the
container-add seam GL marked as A2's GAP: statically, the classic mount
helper DOES carry an associated-container registration path (gated on the
flag and the utoc's presence). Whether a LATE call still merges into an
already-resolved package store remains the open Phase-0/A2 question —
nothing here claims it does.

Invariant guard observation: helper/wrapper/ctor (and ~145k functions
engine-wide) load GOT slot **0x9131f08** (GLOB_DAT, Sony-obfuscated import
symbol 8), save its first qword at entry, and `call abort; ud2` if it
changed; the same 16 bytes are stamped into each mount record (+0x24). This
is an imported runtime object (session-GUID-shaped), **not** the platform
file manager; its exact identity is unresolved statically and is not needed
for any pin in this pass.

## 4. IoStore seam evidence relevant to B2 (partial)

- The `.ucas` partition walk (fn 0x171b9d0) queries each partition file
  through an IPlatformFile held at `[reader+0x10]` via **vt+0x88 (FileSize)**
  with a signed-failure check (0x171ba72) — the container layer sizes its
  partitions through the chain.
- The utoc/ucas READ path shows heavy use of slots +0x108/+0x110
  (async-read class) plus +0xb8 OpenRead sites in the pak module. **The
  exact slot(s) serving the actual utoc/ucas READ handles were not isolated
  this pass** — GL's B2 "seam coverage" GAP stands; Phase 0's observation
  arm (log IterateDirectory/OpenRead-class calls with layer + this-pointer)
  remains the decider. The static facts to check against: +0x80 exists,
  +0x88 sizes, +0xb8 sync-opens, +0x108/+0x110 async-class.
- FindFiles visitor (B1 bonus, settles worker-kernel-pak's open question
  statically): the `*.pak` scan visitor's vtable is **0x7fef0b0**
  (RELA-reconstructed: {0x177e3c0, 0x177e220, 0x120, 0x177e260}); its Visit
  (+0x18 = 0x177e260) wildcard/extension-matches FILES and appends to a
  TArray@visitor+0x10, and its DIRECTORY branch **returns true immediately —
  no recursion**. `Paks/~mods/`-style subfolders are therefore NOT scanned
  by the pak-discovery enumeration on this build; B1's virtual names must
  be direct children of a scanned Paks folder. (Static finding for THIS
  visitor; Phase 0 still observes live enumerations.)

## 5. Per-version anchors (wildcarded; disp32/rel32 fields wildcarded `??`)

Verified hits: Townfall 1 each, Astro 0 each (the generic prologue-only
patterns were tested and are NOT discriminating — 21/2492 Townfall hits and
non-zero Astro hits; do not use them alone):

1. **FPakPlatformFile ctor site** (yields the vtable address from the LEA):
   `48 8D 05 ?? ?? ?? ?? 4D 8D 7E 38 C5 F8 57 C0 4C 89 E7 49 89 06`
   → 1 hit @ vaddr 0x178aa1c.
2. **Mount-helper FileExists→alloc→ctor body**:
   `49 8B 7F 08 4C 89 EE 48 8B 07 FF 90 80 00 00 00 84 C0 0F 84 ?? ?? ??
   ?? BF 30 03 00 00 E8 ?? ?? ?? ?? 49 8B 77 08 41 0F B6 4F 30 45 0F B6
   C6 48 89 C7 4C 89 EA 4C 89 BD`
   → 1 hit @ vaddr 0x176ec93.
3. Get-accessor shape `0F B6 05 ?? ?? ?? ?? 84 C0 74 08 48 8D 05 ?? ?? ??
   ?? C3` — NOT an anchor (2413 Townfall hits, 62 Astro hits); usable only
   as a shape validator once the singleton address is pinned by other means.

Admission chain for a new image of the same version (blade-profile style):
anchor 1 → vtable vaddr from LEA disp → RELA-reconstruct the vtable →
verify slot layout (dtor pair at +0x00/+0x08; Initialize slot's impl
self-identifies via its assert string; ≥10 same-offset inner-forwarders) →
anchor 2 → helper vaddr → derived labels. Live admission still requires the
pin-law prologue-signature + live-base resolution + census-agreement
(vptr match on the walked chain), per GL's fail-closed rules.

## 6. M-B2 hook shape — design addendum (no tree edits)

Mechanics stay GL's proven VMT pump; concretely on this image:

- **Copy size**: the full vtable region `0x258` bytes (16-byte header +
  74 slots) from `live_base + 0x7fee8c0` into our RW page. The header is
  preserved verbatim (offset-to-top = 0, typeinfo = NULL — RTTI is off in
  this build, so the VMT-pump's "RTTI preserved" clause is trivially
  satisfied). A slots-only copy through +0x1c8 (0x1d0 bytes) also covers
  all four hook slots, but the full-region copy is the default because
  the table ends (0x7feeb00/0x7feeb08 NULL qwords) at a known boundary.
- **Replaced slots** (our thunks; all other slots pass through unchanged):
  FileExists **+0x80**, FileSize **+0x88**, OpenRead **+0xb8**,
  IterateDirectory **+0x1c0**. Signatures (SysV): `bool (TCHAR*)`,
  `int64 (TCHAR*)`, `IFileHandle* (TCHAR*, bool)`,
  `bool (TCHAR*, FDirectoryVisitor&)`.
- **Which instance gets the vptr store**: the layer(s) the engine actually
  opens utoc/ucas through — Phase 0 decides; candidates in discovery order:
  the FPakPlatformFile instance (match vptr against the copied-table
  identity), then each `[obj+8]` inner down to the physical file
  (static instance shape 0x9308808-class). Publishing on an inner layer
  intercepts reads that bypass the pak layer; publishing on the pak layer
  only intercepts what dispatches through it. Observation-only arming of
  the same four slots answers this without any substitution.
- **Handle substitution**: our OpenRead/FileSize thunks decide by exact
  filename (`pakchunk0-ps5.utoc` → derived index bytes; `..._s1.ucas` →
  sidecar; else delegate to the original slot target captured at copy
  time). OpenRead returns OUR IFileHandle object (own vtable in our
  memory); FileSize returns our sizes; FileExists may return true for the
  virtual sidecar name; IterateDirectory passthrough unless B1 is also
  armed. Base-utoc sha256 guard and one-shot loud marker per GL B2
  unchanged. Fail-clean default: any unknown state → call through.
- **Thread/atomicity**: publish/retire the vptr only from the game thread
  while the chain is quiescent (same discipline as A1's game-thread drain);
  retire restores the original vptr after the ownership check
  (VmtTickPumpCore law). Nothing here writes the original table, any code
  page, or any file.

## 7. Honest limits / Phase-0 referrals

1. Everything here is static: no pin has runtime confirmation on console;
   live admission still runs the pin-law prologue checks + live base +
   vptr census agreement. Confidences in the JSON reflect static evidence
   density only.
2. `[GPlatformFileManager+8]` as the FPakPlatformFile holder: plausible but
   the factory's caller chain is behind the module vtable — untraced.
   Phase-0 one-read confirm (§1).
3. Slots +0x40 (0x1771240), +0x108 (0x1770e40), +0x110 (0x1769800),
   +0xc8/+0xd0 and stubs +0x10/+0x18 remain unlabeled; +0x108/+0x110 are
   exactly where the utoc/ucas read seam probably lives — Phase-0 logging
   should capture their call sites with layer + this-pointer.
4. The 0x9131f08 imported guard object is unidentified (obfuscated dynsym);
   it is not on any pin path — recorded so nobody chases it as the file
   manager again.
5. Late container registration semantics (A2): the helper's utoc tail
   EXISTS statically, but whether a post-boot call merges into an
   already-resolved package store is unproved and stays with Phase 0/A2.
6. Astro negative control is one non-UE image; a same-version UE 5.6.x
   game was not available locally, so anchor discriminativity ACROSS
   same-version titles is untested (expected: anchors relocate, slot
   layout holds — that is the per-version table's claim to test, not this
   pass's).

— agent HJ (ZCode), 2026-10-10. Read-only on all evidence binaries; the
only files created are this document, `pak-pin-pass-pins.json`,
`agent-HJ-pinpass.md`, and `phasea-tools/pak_vtable_pins.py`.
