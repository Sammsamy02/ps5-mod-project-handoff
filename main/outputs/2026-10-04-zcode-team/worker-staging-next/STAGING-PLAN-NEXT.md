# STAGING-PLAN-NEXT.md — ONE-COMMAND staging plan for the NEXT candidate cycle

Worker: worker-staging-next, 2026-10-05. Bounded delivery: host-side build + structural
verification + this plan + host dry-runs. **No console contact was made by this worker;
the controller executes everything below.** No game claims of any kind.

## The ONE command

```sh
outputs/2026-10-04-zcode-team/worker-staging-next/run_stage_next.sh stage
```

stages phases A+B now. After composition lands, either rerun with the candidate PRX:

```sh
outputs/2026-10-04-zcode-team/worker-staging-next/run_stage_next.sh stage \
  --ue4ss-prx <C6_FSELF_LOCAL_PATH> --ue4ss-sha <C6_FSELF_SHA256>
```

or stage phase C alone:

```sh
outputs/2026-10-04-zcode-team/worker-staging-next/run_stage_next.sh stage-c \
  --ue4ss-prx <C6_FSELF_LOCAL_PATH> --ue4ss-sha <C6_FSELF_SHA256>
```

`plan` (first, anytime, ZERO contact) prints the ordered steps and hash-verifies every
local artifact; `post` (after the user-driven launch) fetches evidence and re-proves the
ini freeze. Reused prior machinery: launch10 wrapper shape, MASTER §0/§B staging law
(Gate A0, pre-images, readback-verify-everything, loadlist LAST), and the retained
`worker-console-batch-runner/console_batch.py` flow `stage-launch10` for phase C
(its BUILD-ID law, `.bak`, SHT-strip readback law, Gate A1 `7f 45 4c 46` magic all
apply unchanged inside that flow).

## Pinned artifacts (verified on disk this session — the script re-verifies before any contact)

| artifact | local path (under `$T=outputs/2026-10-04-zcode-team`) | bytes | sha256 |
|---|---|---|---|
| NEW ploader payload ("ploader.prx" in the task = the payload `ploader.elf`) | `worker-staging-next/ploader-build/ploader.elf` | 144248 | `4c33923521ee8d8b810f8daaa719b935e9e92f6e8892deae43143fb9ec81d292` |
| bundle loadlist (preflight-CLEAN) | `worker-itemspawner-bundle/bundle/loadlist.txt` | 326 | `cbe301368f4a8108ccca16f9739a1c74a8e3242ebc875a9e40b8d21d3846746e` |
| bundle: UE4SS-settings.ini | `worker-itemspawner-bundle/bundle/ue4ss/UE4SS-settings.ini` | 221 | `f6d06cada43d43d0674c2986152b72a4dddecf29331acb9cd85e47172889a37f` |
| bundle: main.lua | `worker-itemspawner-bundle/bundle/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | 21415 | `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658` |
| bundle: enabled.txt | `worker-itemspawner-bundle/bundle/ue4ss/Mods/TownfallItemSpawner/enabled.txt` | 0 | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| bundle: UEHelpers.lua | `worker-itemspawner-bundle/bundle/ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | 9962 | `5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7` |
| UE4SS candidate PRX | `<C6_FSELF_LOCAL_PATH>` — DOES NOT EXIST YET (composition pending) | — | `<C6_FSELF_SHA256>` |

## Ordered steps — each with its verification command and expected output

Console facts (all prior packets): `FTP=ftp://ps5-host-not-configured.invalid:2121` anonymous,
`HTTP=http://ps5-host-not-configured.invalid:8084` pldmgr (arming only). Game CLOSED. `$R=/data/modbridge/roots/PPSA33286`.

| # | step (executed by `stage`) | verification | expected |
|---|---|---|---|
| 0 | host preflight (zero contact): every pinned artifact hash-checked | script exit | `host preflight: all pinned artifacts present and hash-clean` |
| 1 | G0 reachability: `GET $HTTP/list_payloads` | file exists | `g0-list-payloads.json` non-empty |
| 2 | Gate A0 write probe, exactly ONCE: STOR+RETR `/data/modbridge/intake/.writeprobe` | roundtrip body | the 16 bytes `/etc/hostname` starts with; NO roundtrip → STOP (no evidenced write route) |
| 3 | PRE-IMAGEs (RETR only): `$PLOADER_REMOTE`, `$INI_REMOTE`, each bundle target | hashes recorded in run dir | installed payload readable; absence of prior bundle files recorded honestly |
| 4 | console `.bak`: STOR pre-image → `$PLOADER_REMOTE.$TAG.bak` | — | rollback artifact exists BEFORE any overwrite |
| 5 | phase A wave 1: ONE STOR per bundle tree file → `$R/ue4ss/...` | step 7 | — |
| 6 | phase A wave 2: ONE STOR `$R/loadlist.txt` **LAST** (install-commit truth) | step 7 | — |
| 7 | phase B: ONE STOR new payload → `$PLOADER_REMOTE` = `/data/pldmgr/payloads/ploader/ploader.elf` | step 8 | — |
| 8 | READBACK verify EVERY upload: `curl -s "$FTP$R/<file>" | shasum -a 256` | each sha | equals the pinned sha column above; payload expected FULL byte identity (plain ELF, local magic `7f 45 4c 46` — NOT FSELF, no decrypt-oracle gate); the ONLY accepted fallback is the known SHT-strip tail class (len==98332 = max(p_offset+p_filesz), body 64..end identical, magic intact). ANY mismatch → ONE bounded restore of the saved pre-image → refuse (never retry) |
| 9 | ini freeze (twice): `curl -s "$FTP/data/plugins/ploader.ini" | shasum -a 256` | sha equal at start AND end | byte-equal or loud refusal (`ploader.ini.DRIFT.sha256`); **this flow never writes the ini — external drift is the controller's reconciliation** |
| 10 | GO/NO-GO rescue summary → `runs/<tag>-stage-next/go-nogo.json` | file exists | `verdict: GO` (phases A+B) + phase-C status; NO-GO recorded even on halt |
| 11 | phase C (only with `--ue4ss-prx/--ue4ss-sha`): `console_batch.py stage-launch10` | that flow's own run dir | its GO verdict; PRX readback `7f 45 4c 46` (Gate A1); ploader.ini byte-equal enforced there too |

### What must NOT be touched (hard invariants of this plan)

- **Game files**: nothing outside `$R` and `/data/pldmgr/payloads/ploader/ploader.elf` is ever written; no `DELE` anywhere in phases A/B.
- **`/data/plugins/ploader.ini`**: read-only, twice, byte-equal freeze. No edit, no `.bak`, no republish from this flow.
- **`$R/ue4ss/VTableLayout.ini` and `$R/ue4ss/MemberVariableLayout.ini`**: never uploaded, overwritten, or deleted by this plan (the bundle set deliberately contains neither; whatever pair is staged on console stays).
- **`/data/plugins/modbridge_supplier_composition.prx`**: owned ONLY by phase C's launch10 flow (one STOR, its own `.bak`).
- **PS5Pad.ini, UE4SS.log, runtime-status.txt, townfall_supplier_composition.log**: not in the upload set (runtime-created or previously staged); never deleted by phases A/B.

### After staging completes (controller/user, NOT this tool)

1. Game CLOSED, phase C staged (do NOT arm/launch with phase C pending — ONE launch per staged build).
2. Arm ploader: `curl -s "$HTTP/loadpayload:/data/pldmgr/payloads/ploader/ploader.elf"` (or Payload Manager UI).
3. USER launches Townfall. Expected per prior cycles: injection report ~1 min in; the 05r saving shows as a much shorter inter-plugin gap (host-simulated ~62–69 s/boot saving per worker-ploader-speedup; acceptance remains that worker's section-4 criteria, NOT a promise).
4. `run_stage_next.sh post` → evidence dir + SHASUMS + ini re-check. Compare against the phase-C flow's fresh baselines. Negative evidence is evidence; never blind-retry.

### Rollback (game CLOSED, ploader NOT armed)

- Payload: STOR saved pre-image back over `$PLOADER_REMOTE` (hash-verify vs `ploader.elf.pre.sha256`); the console `.bak` is the second artifact.
- Loadlist: STOR saved `loadlist.pre` back (or the controller's prior sha); bundle files that were fresh installs: `DELE` exactly the 5 written paths, only those.
- Phase C rollback: the launch10 flow's own restore path (OLD PRX restored byte-exact from its pre-image).

## Provenance chain of the new payload

pinned upstream v1.1.0 `2b521cd7…` → cleanup-candidate `82f42fdf…` → **05r** (`5b33bc04…`, applied clean 3/3 hunks, result byte-identical to `worker-ploader-speedup/staged/main.c` = `00bec52b…`) → `prospero-clang -Wall -g -O2` (retained ps5-payload-sdk v0.43 + LLVM 21.1.8, upstream Makefile `81ca2d58…`) → **`4c339235…` (144248 B)**. Host controls on the exact built source: 9/9 PASS. Structural class vs the precedent baseline build: identical imports/entry/segments/sections/dynamic; only delta = +0x290 .text, +32 B .rodata, +4 B .bss (`proven_stable_pid`), reworked FAIL string per this lineage. Full receipts: `receipts/BUILD-RECEIPT.json`, `receipts/STRUCTURAL-CHECKS.json`.

## Honest limits

- No console contact, no upload, no execution by this worker; the plan is unproven against the live services until the controller runs it (Gate A0 decides the write route).
- The console's installed ploader.elf has never been hash-matched to any locally compiled ploader (precedent record); the `.bak` + pre-image restore law exists exactly for this.
- The SHT-strip fallback for the payload readback is a strictly-narrowed form of the launch10 content law (identity OR exact-prefix class); on anything else it refuses — safe by construction, but a refusal means staging STOPS for reconciliation. The law must never be widened on a live refusal.
- `-g` debug info embeds build paths: byte-identical rebuilds require this exact directory; stage the pinned file, never a re-build.
- Phase C cannot run until composition lands (worker-preflight-runner fixture reproduction + worker-loop-driver runner gates); its placeholders are deliberate refusals until then.
