# REPORT.md — worker-pc-acquisition-pack (2026-10-05)

Mission: turn worker-re-pc-ps5-comparison's finding (NO PC Townfall material exists
locally; user may provide PC-side material) into a ready-to-hand-off acquisition pack so
that the moment files drop, comparison work starts with zero friction.

## What was delivered

**1. `ACQUIRE.md` — one-page user-facing checklist**
Distills COMPARISON-PROTOCOL.md §2: exactly which files (main Win64 executable, required;
same-title-update store manifest, strongly preferred; optional pak/utoc/ucas set), where
the installed version lives (Steam `appmanifest_*.acf` buildid — macOS + Windows paths;
Epic `Manifests/*.item` BuildVersion/InstallLocation — per INVENTORY.md search targets),
where to drop them (`inputs/pc-townfall/`), the `capture.json` receipt template in the
PS5 capture shape with every user-fillable field explained (store, store_version,
store_version_source, remote_path, expected_bytes required/warned; sha256 explicitly NOT
the user's job — the verifier computes it from actual bytes), and how to run the gate.

**2. `verify_intake.py` — read-only intake gate** (lief 1.0.0)
- Presence + sizes + sha256 for every staged file (chunked, overlap-guarded scanning).
- PE/ELF class confirmation via lief: the dwmapi fixture parses to PE32+/AMD64 with its
  real section table; the PS5 eboot parses to ELF64 e_type 65040.
- Protocol §2.2 classification, never merging the classes: FOREIGN-PE (not the title —
  honest refusal, no v/p-divergent claim applies), PS5-SIDE-FORMAT (ELF refused as PC
  intake), SAME-UPDATE-PLAUSIBLE (PC UE 5.6 == PS5 5.6, platform-divergent arguable per
  anchor only after manifest confirmation), VERSION-DIVERGENT (delta is first-class
  output), VERSION-UNPROVEN (every mismatch reported version-divergent until pinned).
- Provenance receipt gates per §2.4 (store + remote_path required; store_version
  unfilled → warning already reflected in the classification; expected_bytes mismatch →
  INCOMPLETE-COPY blocker).
- Prints the ready-to-run prioritized 5-op comparison table with PS5 anchors FILLED
  (op 1a GUA: 1,565-lea-ref census baseline, 0x93ba110→0x97ba110; 1b FNamePool:
  0x9365ec0→0x9765ec0 + init/find/add addresses, pool+0x10/0x10040/0x14040, shl-6;
  op 2 FName ctor: 0x23d590→0x63d590, frozen bytes, Win64 sret-in-RCX ABI delta;
  op 3 SCO: 1,918-xref census, 0x490aa0→0x890aa0, consensus targets; op 4 ProcessEvent:
  the widest PS5 gap, PC sets the semantic first; op 5 GMalloc: flavor-first UTF-16
  literal scan + Binned3 fingerprint constants, 0x930add8→0x970add8, 1 store/11 thunks),
  plus the pass-1 structural facts (RELA-vs-reloc/IAT; RELA vtable step has NO PC
  analogue) and the §6 output contract (CORRESPONDENCE.md + PC-PROFILE.json).
- Honest engineering note: PC-side derivation scripts do not exist and are listed as
  next commands for the comparison owner — none can be developed against a nonexistent
  image. The verifier writes nothing into the staging dir (protocol §2.3); optional
  `--json` emits a machine receipt outside it.
- UTF-16 allocator-literal scan at intake gives op 5's flavor question a mechanical
  first pass the moment a real executable lands.

**3. `inputs/pc-townfall/` + README.txt placeholder** — the only path created outside
the output dir, as authorized. Otherwise empty by design.

**4. Tests — 7 scenarios, all pass (TEST-RECEIPT.md)**
- Empty dir → honest NOT-READY (exit 1); missing dir → exit 2; real staging dir with
  only the README → NOT-READY with the README excluded from evidence.
- REAL PE fixture: byte-identical copy of the retained dwmapi.dll (independent shasum
  matched `4e3e5324…4f5c0c9a` on both copies), lief parsed PE32+/AMD64/6 sections, and
  the gate classified it FOREIGN-PE — NOT TOWNFALL. The PE-parse path is proven on a
  real binary; the classification is honest that it is not the game.
- REAL PS5 pinned image (hard link, read-only, re-hashed in-run to `3b6cfad4…`): refused
  as PS5-SIDE-FORMAT — PS5 material cannot masquerade as PC intake through this gate.
- Two clearly-labeled SYNTHETIC probes (MZ magic + Townfall + Release strings) proved
  the SAME-UPDATE-PLAUSIBLE and VERSION-DIVERGENT branches and the FMallocBinned3
  UTF-16 scan; lief honestly refused to structurally parse the fakes and that is shown.
- No scenario reached READY (exit 0): correctly unreachable until genuine material lands.

## Non-claims

No PC Townfall material found or fabricated; the comparison itself did not run; every
PS5-side anchor is restated from the cited deliverables with its existing provenance
class, not re-verified (beyond the T7 sha confirmation); synthetic probes are branch-
logic tests only, not evidence; no quality score, no completion claim, no console or
provider contact, no files touched outside the authorized scope.
