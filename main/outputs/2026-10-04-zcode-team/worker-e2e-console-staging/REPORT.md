# REPORT.md — worker-e2e-console-staging

Origin: worker-e2e-console-staging, 2026-10-04 (ZCode team, PS5 mod-manager).
Scope: staging manifest + step-by-step + prereqs + GO/NO-GO for the console
E2E run, stages 0 / 2a (owner-placed zip) / 2b / 3 / 4-5 / 6, with the CURRENT
bridge-linked console payload; stage-1 native marked DEFERRED. Host-side only;
no network; no console contact; no nested subagents; existing files read-only;
all writes under this directory.

## 1. Deliverables

- `CLAIM.json` — read receipt + task contract (written first)
- `STAGING-MANIFEST.md` — 4 files to stage + 2 explicitly NOT staged; every
  sha256 recomputed by this worker, console destinations and purposes
- `PREREQS.md` — C4 environment, identity gates, invocation gate, honesty
  boundaries, sequencing
- `STEP-BY-STEP.md` — exact controller commands (HTTP/FTP patterns from
  CONSOLE-LOG), per-stage expected stdout + e2e-status.jsonl lines, per-step
  rollback, evidence collection
- `GO-NOGO.md` — ordered G0-G6 gates, per-stage verdict table, NO-GO rules

## 2. Inputs read and payload verification

Read as briefed: `outputs/2026-10-04-zcode-controller/ZCODE-FORK-HANDOFF.md`,
`CONSOLE-LOG.md`, `worker-e2e-rehearsal/READINESS.md`. Additional evidence:
`worker-zip-bridge-console-link/REPORT.md` (+ receipts, test suite, link map),
`worker-e2e-final/E2E-RUNBOOK-V2.md`, `worker-installer-ps5io/CONSOLE-E2E-PLAN.md`,
`worker-installer-composition/REPORT.md` §6-7, `worker-e2e-rehearsal/rehearsal/{console.log,e2e-status.jsonl,run_rehearsal.sh}`,
`worker-ps5io-rdwr/NATIVE-DOWNLOAD-LINK.md`, `worker-zip-bridge-console-link/composition/vendored/modinst.c` (entry/argv).

Verified by recomputation (shasum -a 256, 2026-10-04):
- Console payload `worker-zip-bridge-console-link/composition/build/console/modbridge-installer.payload.elf`
  = `cfa96ab5aa408491c74d996f65058b737274a5bb109a7de6e213f9554bbda4f9`,
  681632 B — matches the dispatch brief exactly; import receipt
  `console-imports.stdout` counts exactly 116 `U` lines (wc -l).
- Native download ELF `worker-ps5io-rdwr/native-link/modbridge-installer.download.elf`
  = `fbd355568ccc615f0eaba479a7d1a244a8876226bd0eb2fdb1e9ed640677c8e9`,
  1197880 B — SEPARATE build; NOT staged (stage-1 native deferred per
  READINESS §5).
- `movement-speed-2x.zip` = `3850a2b5…d6ad` (matches zb-pak job.tsv pin);
  `mm-skin.mfb` `7b9888b9…` (311 B); `mm-lua.mfb` `eefc774a…` (337 B);
  `infinite-stamina-v1.zip` `61cd9c81…` (optional alternate).

## 3. Host probes run by this worker (write scope: this directory only)

Using read-only inputs (rehearsal composed binary, then the CURRENT
bridge-linked host binary `worker-zip-bridge-console-link/composition/build/host/modinst_composed`):

- **Probe A** (`probe/owner-zip/`): owner-placed zip entry. `register-mod
  --bundle <RAW ZIP>` -> exit 0, `registered … state=downloaded variants=0`,
  records row artifact = zip path; `prepare-archive --mod` -> exit 0,
  `downloaded -> prepared` durable, NO auto-bundle line (role-less archive).
  Establishes the console stage-1-owner alternative WITHOUT state surgery:
  the download sidecar JSON is not required for this path.
- **Probe B** (`probe/bridge-e2e/`): full sequence on the CURRENT
  bridge-linked binary with `movement-speed-2x.zip`:
  records -> register-mod(zip) -> prepare-archive (auto-bundle composed mfb)
  -> install (generation=1, loadlist sha) -> records (installed,
  running=0) -> simulate-runtime activated (running=0) -> mods-started
  (running=1, SIMULATED) -> install --remove (generation=2, mod prepared).
  Exit 0 at every step; e2e-status lines captured verbatim and embedded in
  STEP-BY-STEP. This is the exact transcript the console run should
  reproduce; console-side truth remains [TARGET-OPEN].

## 4. Key findings that changed the plan vs the rehearsal

1. **F-R1 is closed in the current payload**: role-bearing archives
   auto-bridge to `.mfb` inside `prepare-archive` (`auto-bundle composed
   mfb` status line); `install --mods <id>` then succeeds. The raw-archive
   install refusal only remains for role-less archives (probe A) — stage
   corpus choice matters, hence movement-speed-2x.zip.
2. **Owner-placed zip registration is a one-command path**: `register-mod`
   accepts a raw zip; no sidecar, no records surgery (probe A/B).
3. **NEW hard gate G1 — argv dispatch**: the installer is argv-driven
   (modinst.c:1480-1522) and NO workspace record evidences that pldmgr
   `/loadpayload` forwards arguments (all prior payloads were
   argument-free by design). This is now the top GO gate, with a bounded
   probe specified in STEP-BY-STEP.
4. **NEW gate G3 — FTP write**: intake placement needs FTP STOR on
   `/data/modbridge/intake/`, never evidenced (CONSOLE-LOG shows payload-dir
   HTTP upload + FTP reads only). Bounded probe specified; no silent
   fallback.
5. Stage 4-5 stays honest: simulated ack only (UE4SS + runtime-ack runtime
   deferred); the run report must label `running=1` SIMULATED (READINESS §5).

## 5. Bounds

Tool calls used: 24 of <=35. No subagents; no network/console; originals
untouched; all writes under
`outputs/2026-10-04-zcode-team/worker-e2e-console-staging/` (CLAIM.json, four
docs, REPORT.md, probe/ artifacts). Probes wrote only inside `probe/` with
state/quarantine/bundle paths under it; input zips/bundles were copied, never
modified. Raw provenance: all claims cite worker files by absolute-ish path
from the workspace root; console-side execution is the controller's, per the
staged documents.
