# agent-FZ-next-tracks — next-track research report (2026-10-08)

Scope honored: research/design only — no builds, no code changes to existing
trees, no console contact. New files only under `outputs/2026-10-08-l25-prep/`.

## Deliverables

1. `FZ-PHASEB-KSTUFF-LITE.md` — kstuff-lite interface doc + FW/offset matrix +
   kstuffSharp-diff + safety/recovery plan + Phase B use cases.
2. `FZ-GAME2-PROFILE-DRYRUN.md` — second-game profile dry-run report.
3. `FZ-GAME2/` — dry-run artifacts: `astro_fakeself_dryrun.py`,
   `astro-static-facts.json`, `astro-inner-decrypted.elf` (reconstructed).

## Task A — kstuff-lite (Phase B prep)

- **Source NOT on the Mac** (only pinned FULL kstuff main.c
  `.superpowers/native-dependency-spike/ps5-pid-source-shape-1/pinned-kstuff-main.c`,
  in-house kstuffSharp samples, build scripts). Documented from pinned family
  source + console-side evidence (etaHEN startup logs, klog snapshots, pldmgr
  catalog), with evidence tags throughout.
- **Interface**: no dev nodes/ioctl/servers. Call gate = **kekcall**: syscall
  0x27 (getppid) with sub-number in RAX[63:32], via a `getpid+7`-style gadget.
  Known numbers: CHECK `0xffffffff00000027`, READ_DR/WRITE_DR/RDMSR `0x1/2/3
  <<32|0x27`, REMOTE_SYSCALL `0x500000027` (syscall in arbitrary pid). Project's
  own `SharpProspero.Link __sp_kekcall` shim corroborates the convention.
  Resident patches inherited by all payloads: sysentvec native+compat `0xdeb7`,
  crypt XTS/HMAC (why ftpsrv decrypt-on works), `/system_ex` RW remount +
  shellcore patch, automount (disabled via `/data/.kstuff_noautomount`).
- **Verified on this console**: lite v1.11 autoloads first every boot
  (etaHEN chain), resident with `state=1 readable=1 crypt=deb7/deb7` at every
  bootstrap generation, process name `kstuff.elf`; kernel addresses re-randomize
  per boot; no unload path; re-run is a CHECK-guarded no-op (family source).
- **kstuffSharp diff**: crashed 13.60 because its single hardcoded FW-10.01
  offset set (`CryptSingletonArray_1001`) landed writes on arbitrary kernel
  data (coredump 2026-10-06 20:26Z); upstream fails closed on unknown FW and
  keeps maintained 13.60 tables; double-hook conflict with resident kstuff.
- **Safety plan written**: never autoload modified builds / distinct filenames /
  manual per-launch load / kill-switch + rollback / pre-proved FTP recovery
  before any modified-build test / no-rapid-launch law (ShadowMount 15-25 s
  kstuff pause, resume only on clean exit) / NEW-file klog crash-signature
  monitoring / read-before-write ladder / Astro+StellarBlade+Townfall
  regression launches / saves backed up before kernel tests.
- **Open blocker**: fetch EchoStretch/kstuff-lite v1.11 source to confirm lite's
  exact kekcall subset + 0xdeb7-range ABI before writing any Phase B client.

## Task B — second-game profile dry-run

- Decrypted second-game executable WAS on the Mac: **Astro Bot**, verbatim
  inside `astro-eboot-faked.self` (fake-SELF container parsed per make_fself.py
  layout; embedded ELF header at 0x1a0; reconstructed inner image 258,845,524 B
  sha256 `7723d150…`; gaps only PT_SCE_VERSION/metadata).
- Dry-run executed with the real machinery (ref_axes 244-marker engine
  classifier + patternsleuth GUObjectArray/GMalloc/FName anchor probes):
  **0/244 engine markers, 0 anchor hits, no UTF-16 module strings → Astro Bot is
  NOT an Unreal game; profile generation correctly REFUSES.** No pins emitted.
  Machinery validated end-to-end on a never-profiled binary, zero console use.
- The actual UE second game is **Stellar Blade** (PPSA13197; PAK v11/IoStore
  v3/AMPR confirmed; eboot not on Mac). Its decrypt-on acquisition procedure is
  documented (default FTP download + decrypt-off `-Q "SITE SELF"` provenance
  cross-check + never re-upload) and intentionally NOT performed.
- Profile pipeline documented: identity → axes → anchors → two-source static
  layout facts → `generate_profile.py` units; live-base caveat (ET_SCE_EXEC_ASLR)
  recorded.

## Key file paths

- Docs: `outputs/2026-10-08-l25-prep/FZ-PHASEB-KSTUFF-LITE.md`,
  `outputs/2026-10-08-l25-prep/FZ-GAME2-PROFILE-DRYRUN.md`
- Facts: `outputs/2026-10-08-l25-prep/FZ-GAME2/astro-static-facts.json`
- Primary sources cited inside the docs (all local): pinned kstuff main.c,
  POSTMORTEM.md, PS5-MODDING-MISSION.md, boot-protocol logs, l23 harvest,
  make_fself.py, ref_axes.py, offline_anchor_scan.py, engine_version.rs,
  l23/l19 profile trees.
