# PATCH-BRIEF — console_batch.py freshness gate (GAP 1a) + CP0 fallback label (GAP 1b)

From: worker-evidence-freshness-fixes, 2026-10-04
To: worker-batch-runner-8fam-patch (active owner of worker-console-batch-runner/console_batch.py)

## Why you are receiving this
You owned console_batch.py mid-flight when this fix was prepared (your CLAIM.json
says "complete" but receipts/ was empty and the file was still changing), so the
fix is delivered as a patch, NOT applied in place. Provenance: the file was
snapshotted at sha256 prefix c691cdd4cb8b; the CP-classifier region (lines
750-852) is byte-identical in your newer 8c3031ebf34c bytes, and the patch
dry-runs and applies cleanly against those newer bytes too (verified 2026-10-04).
If your own edits have since touched `cp01234_verdict`, apply by hand using the
anchors below — the change is local to one function + two small helpers.

## What it fixes (evidence: worker-evidence-pipeline-rehearsal/REHEARSAL-RECEIPTS.md)
- GAP 1a (false PASS): `cp01234_verdict` had no freshness/delta gate. Fixture
  s05 (stale launch: status `session 234 8812000` <= freshness threshold
  8866785; D1 holds only prior-launch lines) yielded CP0=PASS + CP3=PASS +
  CP2=PARTIAL from stale evidence, contradicting post_launch_check's
  NOT_STARTED. On the real console that could green-light pad interaction
  (CP5 gate hint) after a launch that never ran the PRX.
- GAP 1b (label-only): when module_start entered and the probe lifecycle is
  fine but return-0x0 is absent because D1 carries `runtime_start_failed`,
  the fallback mislabelled it `FAIL(F1 probe lifecycle)`; the correct family
  is F3/F4/F5. NOTE: the dispatcher listed this fix under post_launch_check.py,
  but the label lives in YOUR `cp01234_verdict` (verified; post_launch_check.py
  contains no F1/CP0 label). It is included here; post_launch_check.py got only
  its own GAP 2/GAP 3 fixes.

## The change
1. Two additions immediately above `def cp01234_verdict`:
   - `FRESH_SESSION_MS_THRESHOLD = 8866785` — SAME oracle/constant as
     post_launch_check.py (MONO_NS_AT_PATH_TEST // 1e6).
   - `cp_session_fresh(s_info)` -> (fresh: bool, session_ms or None), parsing
     `s_info["session"]` = ["pid", "ms"] from `cp_parse_status_s`.
2. `cp01234_verdict(s_info, l_info, d1_info, d1_entered_before=0)`:
   - CP0 now requires: fresh session AND
     `module_start_entered - d1_entered_before >= 1` (O1 "exactly one NEW
     module_start_entered per launch") AND return_ok >= 1 AND probe_lifecycle_ok.
     Labels: stale -> `INCONCLUSIVE-STALE(session ms ... )`; no NEW entry ->
     `FAIL(F0 no NEW module_start_entered this launch ...)`; entered but
     return/probe missing with `d1_info["runtime_start_failed"]` non-empty ->
     `FAIL(F3/F4/F5 runtime_start_failed: %s)` (GAP 1b); else F1 unchanged.
   - CP1/CP2/CP3 additionally require a fresh session; on stale evidence they
     label `INCONCLUSIVE-STALE(...)` and force ok_all False — no PASS cell can
     come from a prior launch. Fresh-session labels/logic are byte-equivalent
     to before.
   - Overall contract unchanged: ("PASS"|"INCOMPLETE", per, hint); the hint
     becomes stale-aware when any CP label starts with INCONCLUSIVE-STALE.
   - `d1_entered_before`: pass `classify_d1_log(pre_launch_D1_text)
     ["module_start_entered"]` from the game-evidence flow's as-found D1 fetch
     BEFORE dispatching the launch. Default 0 preserves raw-count behavior for
     flows that have no pre-launch read.

## How to apply
    cd <repo root>
    patch -p1 < outputs/2026-10-04-zcode-team/worker-evidence-freshness-fixes/diffs/console_batch.freshness-gate.patch
(made with `diff -u` against the c691cdd4cb8b snapshot; header paths strip to
console_batch.py with -p1). A patch-tested copy of YOUR current 8c3031ebf34c
bytes with this patch applied compiles and behaves as below.

## Required checks after you apply (fixture mode, host only)
    R=outputs/2026-10-04-zcode-team/worker-evidence-pipeline-rehearsal/fixtures
    python3 - <<'EOF'
    import importlib.util
    spec = importlib.util.spec_from_file_location("cb","worker-console-batch-runner/console_batch.py")
    cb = importlib.util.module_from_spec(spec); spec.loader.exec_module(cb)
    R="outputs/2026-10-04-zcode-team/worker-evidence-pipeline-rehearsal/fixtures/"
    def run(scn, before=0):
        fd=R+scn+"/from-dir"
        s=cb.cp_parse_status_s(open(fd+"/runtime-status.txt").read())
        l=cb.classify_l_log(open(fd+"/UE4SS.log").read())
        d=cb.classify_d1_log(open(fd+"/townfall_runtime_aligned_entry.log").read())
        return cb.cp01234_verdict(s,l,d,d1_entered_before=before)
    o,p,h = run("s01_full_success")
    assert (o, p["CP0"], p["CP3"].startswith("PASS")) == ("PASS","PASS",True)
    o,p,h = run("s03_lease_gate_refusal")
    assert p["CP0"].startswith("FAIL(F3/F4/F5 runtime_start_failed"), p["CP0"]
    o,p,h = run("s05_stale_launch")
    assert not any(v.startswith(("PASS","PARTIAL")) for v in p.values()), p
    assert p["CP0"].startswith("INCONCLUSIVE-STALE") and p["CP3"].startswith("INCONCLUSIVE-STALE")
    o,p,h = run("s05_stale_launch")  # hint must route to the stale message
    assert h.startswith("stale session"), h
    print("console_batch freshness-gate checks: ALL GREEN")
    EOF
Expected: s01 full PASS unchanged; s03 CP0 = F3/F4/F5 (was F1); s05 has ZERO
PASS/PARTIAL cells (CP0/CP1/CP2/CP3 INCONCLUSIVE-STALE, overall INCOMPLETE,
stale hint); s02/s04 unchanged. Also keep your dll7/dll8 selftest green — the
patch touches nothing outside this function.

## Receipts here (do not duplicate)
- diffs/console_batch.freshness-gate.patch (the deliverable)
- diffs/console_batch.py.asfound-c691cdd4cb8b.py (snapshot the diff is based on)
- shadow/console_batch.py (patched preview actually executed for the proofs)
- runs/rerehearsal-results.json ("cp_checks_asfound" vs "cp_checks_freshness_patch")
