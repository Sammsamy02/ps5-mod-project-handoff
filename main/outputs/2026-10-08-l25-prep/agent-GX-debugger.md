# Agent GX — debugger scope (Claude #5 asks i-iii), 2026-10-08

Mac-only: tool edit + tests + docs. No builds, no console, no network.
Full detail: `outputs/2026-10-08-l25-prep/DEBUGGER-SCOPE.md`.

## 1. Sidecar PAIRING — done (ask ii)

`phasea-tools/symbolize_crash.py` gained `--symbols-sidecar FILE` (the
packaging-time PRX symbols sidecar from PHASEA-PLANS §5 BUILD GAP design):

- sidecar embeds `prx_sha256`; with `--prx` the tool hashes the pointed file
  and HARD-REFUSES on mismatch (exit 2, message shows both hashes/paths);
  sidecar without a valid sha refused equally; no `--prx` → output marked
  `pairing: unverified`, never silent. Symbols load as tier SIDECAR-EXACT.
- Game sidecar pairing (`image_sha256_prefix` vs game image) is now REPORTED
  in output (no `--eboot` input exists, so not enforced — stated openly).
- Fixture `phasea-tools/prx-symbols-l26-package-A.json` is an honestly
  labeled STAND-IN derived from the real package-A bare PRX dynsym (true
  sha256 `9ef26a8f…`); the Packager-emitted real sidecar remains the Phase B
  needs-X (verdict unchanged).
- Tests grew honestly 18 → 22 (`phasea-test-receipt.json`, 22 passed /
  0 failed): pairing-verified, pairing-mismatch-refused (real package-B PRX
  as the wrong build), unpairable-refused, unverified-no-prx.

## 2. Game-code addresses (ask iii) — documented policy

Addresses inside the GAME image (eboot base 0x400000, klog module list)
ALWAYS resolve to module + module_offset (+ build-id fingerprint);
named-symbol hits only via the GAME-SIDECAR sparse anchors (22-24 cited
addresses) with `inside_symbol_bounds: null` — nearest-anchor-below, never
claimed as function attribution. Honest cap: shipping eboot is stripped
(Z11: shoff beyond EOF on the decrypted view; dynsym import-only), no
minidumps, no external symbol source. Improvements possible without any
external source: more profile-pinned anchors (one name at a time), callstack
correlation via pins, and Z11's proven call-edge decoding in the decrypted
image (manual, not automated in the tool — stated boundary).

## 3. Live-debug survey (ask i) — verdict DEFERRED

prosper0gdb (local kstuff-lite v1.11 source, re-read this pass): kernel-mode
GDB remote stub — TCP gdbstub binding first free port from 1234, ONE
connection, protocol subset g/G/m/M/c/s/k/qOffsets; kernel-context freeze,
no unload path (reboot-only). Breakpoints: NOT instruction patching — the
GDB `Z` packet is compiled out (`NO_BREAKPOINT_EMULATION`; upstream's
version = trap-flag single-step of every instruction, 64-slot compare);
hardware DR0-DR7 class exists via the kernel-side API only. Other local
tools: etaHEN (not a debugger), kstuffSharp (quarantined Oct-6
counterexample), PHASEB-PLAN has no debugger rung (R0-R4 read ladder).
[TRAINING-KNOWLEDGE]: scene-wide, r0gdb-style kernel gdbstubs + printf
forensics dominate; no user-mode attach for retail titles.

**DEFERRED, not planned, not dropped**: (1) every crash this project
debugged (l24 CE-108255-1 family, 20:58 ghost crash, FName GC-storm, Oct-6
kernel crash) was solved from logs+receipts+Mac static work — zero live-debug
need so far; (2) kernel-stop on the single console with no unload = Oct-6
class session risk; (3) the Phase B kekcall READ ladder covers the
peek-don't-freeze need safely; (4) the debugger's headline feature
(software breakpoints) is disabled in this build anyway. Recorded revisit
trigger: an unattributable crash after the exact sidecar lands, or a
GATE-1 question only step-through answers, plus user-approved kernel-debug
window with reboot drill → then Claude-reviewed use of the pinned binary's
dormant `r0gdb()` (hardware-DR class, zero new kernel code).

## Files

- edited: `phasea-tools/symbolize_crash.py` (pairing + SIDECAR-EXACT tier +
  game-sidecar reporting), `phasea-tools/run_phasea_tests.py` (+4 checks),
  `phasea-tools/phasea-test-receipt.json` (22/22)
- created: `phasea-tools/prx-symbols-l26-package-A.json` (fixture),
  `DEBUGGER-SCOPE.md`, this report
