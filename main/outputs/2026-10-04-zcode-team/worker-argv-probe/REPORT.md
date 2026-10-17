# REPORT — worker-argv-probe (team 2026-10-04)

Role: G1 argv probe. Built `ARGV_ECHO.elf`, the bounded payload that settles
whether the pldmgr `GET /loadpayload:<path>` route forwards arguments
(GO-NOGO gate G1, the top-risk hard gate).

## Status

PAYLOAD BUILT, LOCALLY GATED, DETERMINISTIC. Not executed anywhere — not on
host (macOS arm64 cannot run prospero ELFs) and never on console (no
console/network contact by this worker; zero dispatches). The controller
executes `PROBE-PLAN.md`: ≤3 dispatches (2 bare for the determinism pair +
≤1 UI-args variant if the manager exposes an arguments field).

## Key decision — ELF on the /loadpayload lineage, not PRX

The task allowed `.prx` "if the lineage demands". It does not: every
console-proven payload on the `/loadpayload` route (CONSOLE-LOG items 2–4,
8–9: NEXT_DLL_THREADS 6d295f56, TEST_THIS_DLL f72faea5, INITONCE_DLL_TEST
1a637b7) is a standalone ps5-payload-sdk ELF, the same class as the SDK's
`samples/hello_argv`. The v2 7-verbatim-NID import contract is a PRX/ploader
artifact and was NOT ported (porting it would import unproven loader
behavior into a route that has its own three-times-proven link class). From
the v2 lineage this build keeps the discipline: entry record first (the
JSONL write is the payload's ONLY action), bounded /data sink, receipted
build, loud gates, two-clean-build determinism.

## Deliverables (all under outputs/2026-10-04-zcode-team/worker-argv-probe/)

- `src/main.c` — single-file payload; appends exactly one record
  `{"marker":"modbridge.argvprobe","schema":1,"build_id":"argv-…","argc":N,"argv":[…],"argv_capped":b,"str_trunc":b}`
  to `/data/modbridge-argv-probe.jsonl`, then exits 0. argc verbatim; ≤64
  entries, ≤64 B/string, flags on cap; JSON-escaped; no timestamps or
  randomness (two dispatches → byte-identical lines).
- `build_argv_echo.py` / `check_argv_artifact.py` — receipted build + loud
  gate.
- `build-1eb64d8964c6/ARGV_ECHO.elf` — sha256
  `53075bd7cadbebbdfc11df03e08df740a57b81148a270c68652cc5dc81104e6f`,
  111,016 B; twin build byte-identical; `import-proof.txt` (DT_NEEDED:
  libkernel_web/libSceLibcInternal/libSceNet .sprx — the SDK default link;
  undefined dynsym: exactly fopen/fprintf/fputc/fwrite/fclose; forbidden
  dlopen/module-query set ABSENT); `build-receipt.json`.
- `RECEIPT.md` — identity, import proof, gates, honesty terms.
- `PROBE-PLAN.md` — exact controller steps, dispatch budget, interpretation
  matrix, fallback design note.

## Findings during build

1. lld's default GNU build-id hashes the OUTPUT PATH — two clean builds into
   `a/` vs `b/` differed in exactly those 16 bytes. Fixed with
   `-Wl,--build-id=none`; determinism now proven from the files themselves.
   Anyone inheriting the v2 determinism recipe for ELF builds needs this.
2. llvm-readelf warns `invalid PT_DYNAMIC size` on SDK-linked ELFs (same
   link class as the three console-proven payloads) — cosmetic, recorded.
3. The SDK ships `getargv` in libkernel_web (observed in prior worker export
   dumps) and a `hello_argv` sample with a documented `?args=` percent-
   encoded deploy grammar — the prior art the variant test references.

## Interpretation delivered to the controller (PROBE-PLAN.md)

- argc==0 / path-only argv[0] → **no forwarding → G1 NO-GO** for
  argv-driven installers; E2E adopts **config-file-driven entry** (FTP-STOR
  a sha-verified `/data/modbridge/intake/e2e-cmd.json`, bare dispatch,
  installer reads state/roots/verb from it) — chosen over a baked-args
  wrapper (one payload for all stages, no per-command build/upload/alias
  churn, input verified before dispatch, STOR already the gated intake
  route).
- Variant tokens echoed → forwarding works via that exact mechanism; G1 GO
  bound to its verbatim grammar.
- Absent file (verified upload + dispatch OK) = payload-run/write failure,
  explicitly NOT an argv verdict (INITONCE /tmp lesson: sink must be /data).

## Bounds

≤25 tool calls (17 used); CLAIM.json first, REPORT.md last; writes confined
to this worker dir; no nested subagents; no network/console; no dispatch;
existing files elsewhere untouched.
