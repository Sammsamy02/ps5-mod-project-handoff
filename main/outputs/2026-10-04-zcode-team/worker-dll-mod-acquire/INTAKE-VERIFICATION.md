# INTAKE-VERIFICATION — post-download census and retention gates (2026-10-04)

Status: PREPARED, NOT EXECUTED. Companion to EXECUTE-ACQUISITION.md Phase H.
`$W` = `/WORKSPACE/.codex/worktrees/translation-core/what`.
All steps are local, zero-network, and never execute mod bytes.

## 1. Census run (host-side, immediately after Phase G)

Tool (already delivered and baseline-tested by worker-dll-mod-retention):
`$W/outputs/2026-10-04-zcode-team/worker-dll-mod-retention/inspect_dll_census.py`
v1.0.0 — pure stdlib, in-memory zip read, nothing extracted to disk, never
writes outside `-o` (tool docstring + argparse `inputs… -o/--out [required]
[--include-exes]`, verified by read this session).

```
python3 "$W/outputs/2026-10-04-zcode-team/worker-dll-mod-retention/inspect_dll_census.py" \
  "$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/p3rfix/original.zip" \
  -o "$W/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/p3rfix/census.json"
```
The archive is a store-published `.archive` copy named `original.zip` per the
retention convention (ACQUISITION-PATH step 4); the tool accepts zip archives or
raw PE files. Do NOT pass `--include-exes` for this acquisition (target is the
DLL import profile; note its absence in the receipt).

## 2. What the census captures (cited from the tool)

- Every imported symbol: library, name/ordinal, family classification across the
  project taxonomy: crt, heap, tls, loader, clock, workers, critical,
  address-wait, srw, init_once, mutex, fileio, uncategorized.
- Static-TLS detection (TLS data directory: AddressOfIndex, callbacks, raw data).
- CRT-startup flags (`_initterm`/`_onexec`/`_onexit`…), which CRT runtime DLLs
  are imported, static-CRT likelihood heuristic.
- Export census — proxy-DLL forwarding detection (this is the P3RFix
  `dinput8.dll` signature we want on record).
- `tool_version` and per-input sha256 in the output JSON.

## 3. Baseline comparison (services-backlog deltas)

```
python3 - <<'PY'
import json
base = json.load(open("OUTPUTS/2026-10-04-zcode-team/worker-dll-mod-retention/baseline-dll-census.json".replace("OUTPUTS","/WORKSPACE/.codex/worktrees/translation-core/what/outputs")))
new  = json.load(open("/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/dll-mod-retention-2026-10-04-1/p3rfix/census.json"))
def families(doc):
    out = {}
    for mod in doc.get("modules", doc.get("files", [])):
        for fam, n in (mod.get("family_counts") or {}).items():
            out[fam] = out.get(fam, 0) + n
    return out
b, n = families(base), families(new)
for fam in sorted(set(b) | set(n)):
    print(f"{fam}: baseline={b.get(fam,0)} p3rfix={n.get(fam,0)} delta={n.get(fam,0)-b.get(fam,0)}")
PY
```
NOTE (honesty): the aggregation keys above (`modules`/`family_counts`) must be
confirmed against the actual census JSON shape at run time — the controller
should `python3 -m json.tool` the census head once and adapt the two `get()`
key names if the tool uses different field names. Do not edit the tool.

Record the third-party deltas explicitly (ACQUISITION-PATH step 8):
- new import families not present in `baseline-dll-census.json`;
- new uncategorized APIs (these become services-backlog candidates);
- static-TLS findings and CRT-startup findings (crypt013/crt-startup domains);
- proxy-forwarding exports (dinput8 shim shape evidence).

## 4. Retention artifacts checklist (all under `…/dll-mod-retention-2026-10-04-1/p3rfix/`)

| Artifact | Source | Gate |
|---|---|---|
| `original.zip` | byte-copy of `host/store/<job_id>.archive` | sha256 equals store readback AND job-record sha256 |
| `manifest.json` | authored per EXECUTE-ACQUISITION §10 | attributions + inputs{bytes,sha256} + provenance present; license stated or "ambiguous" |
| `receipt.json` | authored | job id, transfer stats, sha256 verified-on-disk, BUILD_ID + artifact sha256, verify.py/requirements_post_verify.py outcome if run |
| `census.json` | inspect_dll_census.py | tool_version present; ≥1 PE module parsed |

Verify.py option: `$W/outputs/ps5-mod-bridge/native-host/download/verify.py`
exercises the real curl-8.18 → file-sink path on owned loopback HTTPS (its own
docstring: no live Nexus account). It is an optional additional transfer-mechanism
check, not required for intake; if the controller runs it, its outcome goes in
`receipt.json`.

## 5. Acceptance gates (binding)

1. **Hash-first**: the artifact counts as retained only after the on-disk sha256
   readback matches the store copy and the job record (ACQUISITION-PATH rule).
2. **Census-only inspection**: the zip is opened only by inspect_dll_census.py
   in memory; nothing is extracted, registered, or executed; nothing is placed
   into any game/engine path by this role.
3. **Attribution preserved**: manifest attributions are never stripped; license
   ambiguity is recorded, keeping retention internal-only (RETENTION-CRITERIA #5).
4. **No >8 claims**: census output is factual evidence for the services backlog;
   it establishes no runtime, installation, or console-loading authority
   (mirrors jobs.h "analysis only" semantics).
5. Backlog update is a separate worker's edit; this role only records deltas
   (see §3 output) into the observation directory.
