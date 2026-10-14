# agent-HJ-pinpass — offline pak-seam pin pass (report)

Agent HJ (ZCode), 2026-10-10. Assignment: GL's
`PAK-RUNTIME-MOUNT-DESIGN.md` §6.5 missing static pins (GPlatformFileManager
accessor, FPakPlatformFile vtable identity, OpenRead/FileSize slots) +
M-B2 hook-shape addendum. Static-only, read-only on evidence; outputs:
`PAK-PIN-PASS.md` (full doc), `pak-pin-pass-pins.json` (pins),
`phasea-tools/pak_vtable_pins.py` (tool). Image: pinned decrypted Townfall
eboot sha `3b6cfad4…` (re-verified; = HA MANIFEST 1258). T1/T2/T3 snapshot
hashes re-verified against the MANIFEST; **all pins byte-identical on all
four images** (they differ only in the ELF header page + dynamic-metadata
tail). Astro Bot ELF negative control: 0/244 UE markers + 0 anchor hits.

## Pins found (image vaddrs, static confidence)

- **FPlatformFileManager::Get() = 0xb8ed0** (0.95) — guard-checked lazy
  accessor returning `&0x932a428`; used by the mount helper itself
  (0x176f2bc). **GPlatformFileManager static = 0x932a428** (0.9);
  TopPlatformFile at `+0x00`; **GetPlatformFile() = 0xb8a70** (0.95;
  `mov rax,[rdi]; ret` + lazy default build 0x4ca0 → physical instance
  0x9308808, physical vtable 0x7ebc6a0). `[mgr+8]` is likely the engine-wide
  FPakPlatformFile holder (factory installs/releases it there) — flagged for
  a one-read Phase-0 confirm, not relied on.
- **FPakPlatformFile vtable = 0x7fee8c0** (0.97) — ph3 RELRO, 74 slots,
  no RTTI header; proven by ctor vptr store at 0x178aa1c (factory 0x178a9e0,
  sizeof 0xF0, inner = [this+8]) + unique RELA slot for Initialize. Identity
  anchor bytes recorded (ctor-site wildcard pattern: 1 Townfall hit,
  0 Astro).
- **Slot pins** (interface offsets, cross-validated by 17 same-offset
  inner-forwarders): **FileExists +0x80** ✓GL verified; **IterateDirectory
  +0x1c0** ✓GL verified; **FileSize +0x88** (NEW, 0.9: int64/stat return,
  signed-failure use at the .ucas partition query 0x171ba72, exists→size
  pairing); **OpenRead +0xb8** (NEW, 0.93: FPakFile ctor call site 0x17722d2,
  handle → [pak+0x2d0]); **GetLowerLevel +0x68** (bonus, chain-walk proven).
  Unlabeled: +0x40, +0x108, +0x110 (+0x108/+0x110 = where the utoc/ucas
  read seam probably lives → Phase-0 logging).
- **Mount helper ABI refined (corrections to GL's shape)**: RDX is the
  OutError FString (not priority — priority rides args+8; wrapper 0x176b900
  computes via ranker 0x1770140); RCX = TRefCountPtr out; mount record =
  {PakPath@+0, MountPoint@+0x10, Priority@+0x20, guid16@+0x24, int@+0x34}.
  `_P` bump = +100×digit-position. **New: the helper's tail mounts the utoc
  companion** (FileExists +0x80 on `.utoc`, 0xB0 container ctor 0x177fc90 →
  [pak+0x328], registry 0x175a4d0 ← [this+0xd0]) — the A2 container-add seam
  exists statically; late-merge semantics still open.
- **B1 bonus (settles the ~mods question statically)**: the `*.pak`
  FindFiles visitor (vtable 0x7fef0b0, Visit=0x177e260) does NOT recurse
  into subdirectories — `Paks/~mods/` is not scanned on this build; virtual
  names must be direct children of a scanned Paks folder.

## M-B2 hook shape (concrete)

VMT-copy `0x258` bytes (16B header + 74 slots) from `base+0x7fee8c0`;
replace slots **+0x80/+0x88/+0xb8/+0x1c0** with thunks (bool(TCHAR*) /
int64(TCHAR*) / IFileHandle*(TCHAR*,bool) / bool(TCHAR*,visitor&));
publish via ONE aligned vptr store on the instance Phase-0 identifies as the
utoc/ucas-serving layer (pak layer or inner physical); retire = restore
original vptr after ownership check. OpenRead returns our own IFileHandle
(our vtable, our memory); exact-filename dispatch + base-utoc sha guard +
passthrough-default per GL. No code/data/file writes anywhere.

## Honest limits

All static (no console confirmation — live admission per pin law);
`[mgr+8]` hypothesis untraced behind the module vtable; slots
+0x40/+0x108/+0x110 unlabeled; the 0x9131f08 GOT import used as a mount
invariant-guard/record stamp is unidentified (obfuscated; NOT the file
manager); no same-version foreign title available to test anchor transfer.

— agent HJ, 2026-10-10
