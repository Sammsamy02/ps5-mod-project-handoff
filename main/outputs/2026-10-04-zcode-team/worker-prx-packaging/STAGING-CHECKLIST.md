# STAGING-CHECKLIST: ploader staging of the composed-supplier PRX candidate

Source of the loader contract: `outputs/ZCODE-HANDOFF-2026-10-04/CONSOLE_RUNBOOK.txt`
section PLOADER (lines 70-80). Nothing here extrapolates beyond it.

## 1. Candidate artifact

- Built by: `python3 package_prx.py --link` (fresh `prx-candidate-*/` dir; the
  script refuses to reuse or overwrite any existing directory).
- Wrapped candidate: `modbridge_supplier_composition.prx` (fake-self, magic
  `4f153d1d`; bare ELF magic `7f454c46`).
- Its sha256/bytes are recorded in `build-receipt.json` -> `commands` ->
  candidate stage and in `verification-evidence/static-checks.json`.
- Do NOT stage a candidate whose run produced `GAPS.json`; reconcile first.

## 2. ploader.ini content

Config path on console: `/data/plugins/ploader.ini`

```ini
[PPSA33286]
/data/plugins/modbridge_supplier_composition.prx = 2
```

- Section name is the title id: `[PPSA33286]`.
- Entry syntax: `/data/plugins/<module>.prx = 2`.
- Mode `2` is LATE loading.
- Precedent: the frozen aligned-entry module used exactly this shape with its
  own filename; the SDK stamp 0x11000000 is what turned the earlier
  ESDKVERSION 0x80020063 failure into a real OK. The candidate carries the same
  stamp (verified statically by the packaging script).

## 3. Placement

- Module path: `/data/plugins/modbridge_supplier_composition.prx`.
- Log path (expected by the candidate profile): `/data/townfall_supplier_composition.log`.
- Never overwrite the frozen `modbridge_runtime_aligned_entry.prx` or any prior
  module; place under a NEW filename only (the candidate name above).
- Preserve any prior module, its log and the existing 88-byte worker input;
  the frozen lineage's placement receipt shows the expected readback flow:
  place, read back, compare hash, only then rely on it.

## 4. Per-launch re-send requirement

- Mode 2 payload entries must be RE-SENT for each game launch, according to
  ploader's lifecycle (runbook line 77). A module sent for launch N is not
  present for launch N+1 without re-send.
- After sending, verify the module actually loaded from its log evidence, not
  from the send command's return alone.

## 5. What the module must NOT do until in-game facts land

- No engine calls beyond the probe path: the composed runtime's supplier
  adapters (lmf-loader-adapter / lmf-sce-ops / lmf-live-module-facts) run in
  degraded dl-retention mode; they may observe and report, but must not
  dereference any reported address and must not attempt handle retention.
- No UE4SS initialization, no Lua VM start, no mod loading, no hook
  registration, no memory writes: until `/data/modbridge-ingame-facts.jsonl`
  exists on console with real measurements (G1-G7 per the checkpoint's "next"
  field), the module is a facts collector only.
- No promotion of "no crash" or empty logs to success: a supplier that cannot
  retain yields ZERO facts by design; absence of facts is not evidence of
  correctness.
- Never re-send/queue duplicate payloads; never replay a failed registration.

## 6. Order of operations when facts land

1. Controller runs `package_prx.py --link`; requires rc=0 and no GAPS.json.
2. Independent review of `static-checks.json` (SDK stamp, import table equals
   the 196-name extended contract, `_DYNAMIC` not imported, explicit
   DT_INIT/DT_FINI, init/fini slots recorded, module_start entry-alignment
   byte evidence present).
3. Stage per sections 2-4 with user confirmation, fresh log file.
4. Collect the in-game facts file; only then decide the next milestone
   (harmless-query rung per checkpoint `zcode_supplier_composition_1.next`).
