#!/usr/bin/env python3
"""run_host_checks.py — worker-fname-runtime-probe host verdict assertions.

Parses every JSONL record the FC-1 probe emitted per scenario (via the REAL
lineage record writer) and asserts the FC1-A..D contracts:
  - every line parses as strict JSON, none truncated;
  - scenario verdicts match the host_main C-side expectations;
  - mprotect-first invariants visible in records (read only after success,
    restore recorded, refusals carry no bytes_read_hex).
"""
import json
import pathlib
import sys

FAIL = []


def check(cond, msg):
    if not cond:
        FAIL.append(msg)
        print("FAIL:", msg)
    return cond


def load(path):
    recs = []
    for i, line in enumerate(path.read_text().splitlines()):
        if not line.strip():
            continue
        try:
            obj = json.loads(line)
        except json.JSONDecodeError as e:
            FAIL.append(f"{path.name}:{i}: not JSON: {e}")
            continue
        if obj.get("__trunc"):
            FAIL.append(f"{path.name}:{i}: truncated record")
        # Lineage envelope: probe fields live under "fields" (top level
        # carries marker/schema/build_id/seq/exp/rc/errno). Merge for checks.
        fields = obj.pop("fields", {})
        if isinstance(fields, dict) and "__trunc" in fields:
            FAIL.append(f"{path.name}:{i}: truncated record fields")
        flat = dict(fields)
        flat["seq"] = obj.get("seq")
        flat["exp"] = obj.get("exp")
        flat["rc"] = obj.get("rc")
        flat["errno"] = obj.get("errno")
        recs.append(flat)
    return recs


def by_exp(recs, exp):
    return [r for r in recs if r.get("exp") == exp]


def one(recs, exp, name):
    got = by_exp(recs, exp)
    if not check(len(got) == 1, f"{name}: exactly one {exp} record"):
        return {}
    return got[0]


def sites(recs, name):
    got = by_exp(recs, "fc1_b_site")
    if not check(len(got) == 9, f"{name}: exactly 9 fc1_b_site records"):
        return []
    got.sort(key=lambda r: r["site_index"])
    return got


def mprotect_first_invariants(recs, name, expect_reads):
    for r in recs:
        if "fc1_a" not in r.get("exp", "") and "fc1_b" not in r.get("exp", ""):
            continue
        if r.get("read_performed") == 1:
            check(r.get("mprotect_called") == 1 and r.get("mprotect_rc") == 0,
                  f"{name}: read only after mprotect success ({r['exp']})")
            check(r.get("mprotect_prot_requested") == 5,
                  f"{name}: request adds READ keeping EXEC ({r['exp']})")
        else:
            check("bytes_read_hex" not in r,
                  f"{name}: refused record carries no bytes ({r['exp']})")
            check(r.get("refusal_code") in (-1, -2, -3),
                  f"{name}: refusal code recorded ({r['exp']})")
        if r.get("read_performed") == 1 and expect_reads:
            check(r.get("restore_called") == 1 and r.get("restore_rc") == 0,
                  f"{name}: restore after read ({r['exp']})")


def main(out_dir):
    d = pathlib.Path(out_dir)

    # ---- scenario 1: all_pass -----------------------------------------
    name = "all_pass"
    recs = load(next(d.glob("scenario_1_all_pass.jsonl")))
    mprotect_first_invariants(recs, name, True)
    a = one(recs, "fc1_a_candidate", name)
    check(a.get("match32") == 1 and a.get("match64") == 1,
          f"{name}: FC1-A 32-byte hard gate + 64-byte compare pass")
    check(a.get("first_divergence") == -1, f"{name}: no divergence")
    check(a.get("expected_bytes_hex", "").startswith("554889e553504889f0"),
          f"{name}: expected bytes are the FName candidate prologue")
    for i, s in enumerate(sites(recs, name)):
        check(s.get("site_pass") == 1, f"{name}: site {i} passes")
        check(s.get("decode_status") == "ok", f"{name}: site {i} decode ok")
        check(s.get("t1_e8_ok") == 1 and s.get("t2_rel32_target_ok") == 1,
              f"{name}: site {i} E8+rel32 hits the candidate")
        check(s.get("lea_rsi_target_ok") == 1, f"{name}: site {i} anchor lea")
        check(s.get("rdi_writes") == 1 and s.get("rdx_writes") == 1,
              f"{name}: site {i} single RDI/RDX writes")
        check(s.get("bytes_read_hex", "").startswith("488d35"),
              f"{name}: site {i} window starts 48 8d 35")
        check("ba01000000" in s.get("bytes_read_hex", ""),
              f"{name}: site {i} contains mov edx,1")
        check(s.get("bytes_read_hex", "")[-10:-8] == "e8",
              f"{name}: site {i} window ends with the call")
    dd = one(recs, "fc1_d_anchor_strings", name)
    for i in range(5):
        check(dd.get(f"d{i}_utf16_match") == 1, f"{name}: anchor {i} UTF-16 match")
    check(dd.get("d2_expect_len") == 34,
          f"{name}: AnimGraphRuntime string is 16 chars + NUL in UTF-16")
    s = one(recs, "fc1_summary", name)
    check(s.get("fc1_validated_indicator") == 1, f"{name}: validated")
    check(s.get("fc1_c_fail_closed") == 0, f"{name}: no fail-closed")
    check(s.get("sites_passed") == 9 and s.get("fc1d_matches") == 5,
          f"{name}: 9/9 sites, 5/5 anchors")

    # ---- scenario 2: target drift --------------------------------------
    name = "target_drift"
    recs = load(next(d.glob("scenario_2_target_drift.jsonl")))
    a = one(recs, "fc1_a_candidate", name)
    check(a.get("match32") == 0, f"{name}: hard gate catches byte 10")
    check(a.get("first_divergence") == 10, f"{name}: divergence localized")
    dd = one(recs, "fc1_d_anchor_strings", name)
    check(dd.get("status") == "skipped_a_or_b_failed",
          f"{name}: FC1-D skipped when A fails")
    sm = one(recs, "fc1_summary", name)
    check(sm.get("fc1_validated_indicator") == 0, f"{name}: not validated")
    check(sm.get("fc1_c_fail_closed") == 0,
          f"{name}: A refutation recorded, not site ambiguity")
    check(sm.get("sites_passed") == 9, f"{name}: sites still pass")

    # ---- scenario 3: mov edx,1 corrupted --------------------------------
    name = "site_edx_imm"
    recs = load(next(d.glob("scenario_3_site_edx_imm.jsonl")))
    ss = sites(recs, name)
    check(ss[4].get("decode_status") == "rdx_not_mov_edx_1",
          f"{name}: site 4 flags the corrupted imm")
    check(ss[4].get("site_pass") == 0, f"{name}: site 4 fails")
    sm = one(recs, "fc1_summary", name)
    check(sm.get("sites_passed") == 8, f"{name}: 8/9")
    check(sm.get("fc1_c_fail_closed") == 1, f"{name}: FC1-C fail closed")
    check(sm.get("fc1_validated_indicator") == 0, f"{name}: not validated")
    check(one(recs, "fc1_d_anchor_strings", name).get("status")
          == "skipped_a_or_b_failed", f"{name}: D gated off")

    # ---- scenario 4: mprotect refused -----------------------------------
    name = "mprotect_refused"
    recs = load(next(d.glob("scenario_4_mprotect_refused.jsonl")))
    mprotect_first_invariants(recs, name, False)
    a = one(recs, "fc1_a_candidate", name)
    check(a.get("refusal") == "mprotect_failed_no_read_attempted",
          f"{name}: refusal recorded, no read attempted")
    check(a.get("mprotect_called") == 1 and a.get("read_performed") == 0,
          f"{name}: mprotect attempted, read not performed")
    for i, st in enumerate(sites(recs, name)):
        check(st.get("read_performed") == 0, f"{name}: site {i} not read")
    sm = one(recs, "fc1_summary", name)
    check(sm.get("fc1_validated_indicator") == 0 and
          sm.get("fc1a_read_performed") == 0 and
          sm.get("sites_passed") == 0, f"{name}: nothing validated")
    check(sm.get("fc1a_anomaly") == 0, f"{name}: refusal is not an anomaly")

    # ---- scenario 5: restore anomaly -------------------------------------
    name = "restore_anomaly"
    recs = load(next(d.glob("scenario_5_restore_anomaly.jsonl")))
    a = one(recs, "fc1_a_candidate", name)
    check(a.get("read_performed") == 1 and a.get("match64") == 1,
          f"{name}: read + compare still performed")
    check(a.get("mprotect_restored") == 0 and a.get("anomaly") == 1,
          f"{name}: restore failure recorded as anomaly")
    sm = one(recs, "fc1_summary", name)
    check(sm.get("fc1a_anomaly") == 1, f"{name}: summary carries the anomaly")
    check(sm.get("fc1_validated_indicator") == 1,
          f"{name}: igf5 semantics — anomaly recorded, verdict not gated")

    # ---- scenario 6: map untrusted ---------------------------------------
    name = "map_untrusted"
    recs = load(next(d.glob("scenario_6_map_untrusted.jsonl")))
    a = one(recs, "fc1_a_candidate", name)
    check(a.get("orig_prot_source") == "checkpoint_constant" and
          a.get("orig_prot") == 4,
          f"{name}: LABELED checkpoint fallback used for orig prot")
    dd = one(recs, "fc1_d_anchor_strings", name)
    check(dd.get("status") == "refused_map_untrusted",
          f"{name}: FC1-D refused without a trusted map")
    sm = one(recs, "fc1_summary", name)
    check(sm.get("fc1_validated_indicator") == 1 and
          sm.get("fc1d_performed") == 0,
          f"{name}: code verdict stands, D not performed")

    # ---- scenario 7: call rel32 drift ------------------------------------
    name = "call_rel32_drift"
    recs = load(next(d.glob("scenario_7_call_rel32_drift.jsonl")))
    ss = sites(recs, name)
    check(ss[5].get("t2_rel32_target_ok") == 0,
          f"{name}: site 5 T2 flags the drifted target")
    check(ss[5].get("decode_status") == "call_target_not_candidate",
          f"{name}: site 5 decode flags the drifted target")
    sm = one(recs, "fc1_summary", name)
    check(sm.get("fc1_c_fail_closed") == 1 and
          sm.get("fc1_validated_indicator") == 0,
          f"{name}: fail closed on drift")

    # ---- scenario 8: unsupported encoding --------------------------------
    name = "unsupported_encoding"
    recs = load(next(d.glob("scenario_8_unsupported_encoding.jsonl")))
    ss = sites(recs, name)
    check(ss[2].get("decode_status") == "unsupported_encoding_fail_closed",
          f"{name}: unknown opcode fails closed, never guessed")
    sm = one(recs, "fc1_summary", name)
    check(sm.get("fc1_c_fail_closed") == 1 and
          sm.get("fc1_validated_indicator") == 0, f"{name}: not validated")

    if FAIL:
        print(f"HOST_CHECKS_FAILED ({len(FAIL)} failures)")
        return 1
    print("HOST_CHECKS_PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "tests/out"))
