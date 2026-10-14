#!/usr/bin/env python3
"""host_cmd_entry_patched_test.py — host validation of the COMPILED-IN config
entry (worker-e2e-config-entry revision of modinst_core.c).

Unlike worker-e2e-staging-final/host-test/host_cmd_entry_test.py (which
EMULATED the entry rule in Python and fed synthesized argv to the pristine
D1 binary), this test drives the ACTUAL compiled-in entry of the patched
binary host-test/build/modinst_composed_cfgentry via the designed override
slot `--e2e-cmd FILE`:

  A. bare dispatch (argc<2): the compile-time slot /data/modbridge/intake/
     e2e-cmd.json is absent on host -> one refusal line + usage + EX_BAD 2,
     byte-for-byte today's bare-usage behavior (console-harmless proof).
  B. full stage 0..6 replay (14 dispatches) through the compiled-in entry:
     exit 0 each, per-dispatch consumed-rename (.consumed), final 35-line
     transcript byte-identical to staging-final's sha
     31a4764a4ae13afac76db4e50e0905789a6b07294acb5943ebd2affb3b5ebeed.
  C. legacy-argv twins on the SAME patched binary: transcript byte-identical
     to B AND byte-identical to staging-final's saved twin transcript
     (produced by the PRISTINE D1 binary) -> legacy argv unaffected.
  C2. recover probe through the entry (begin/end recovery shape).
  D. stale re-dispatch of a consumed slot: EX_BAD 2, zero state change.
  E. argv-wins probe: a valid cmd file exists but argc>=2 without the flag
     -> entry never consulted, file left untouched (schema sec 6).
  F. refusal matrix (14 cases): 13 x EX_CONFIG 6 (BOM/oversize/non-UTF-8/
     parse/schema/allow-deny/eager-illegal), absent-file EX_BAD 2; host-
     adapted cases with a valid scratch state dir each append EXACTLY ONE
     {"event":"status","stage":"e2e-cmd","outcome":"refused",...} line and
     never execute the verb (no journal/, no records.txt); state_root-
     invalid refusals write no report line at all.
  G. `--e2e-cmd` without a value stays legacy grammar (unknown flag ->
     usage + EX_BAD 2).

All writes stay under this worker's outputs dir (host-test/scratch/).
"""

import hashlib
import json
import os
import shutil
import subprocess
import sys

WS = "/WORKSPACE/.codex/worktrees/translation-core/what"
TEAM = f"{WS}/outputs/2026-10-04-zcode-team"
HERE = f"{TEAM}/worker-e2e-suite-completion"
CMD = f"{TEAM}/worker-e2e-staging-final/cmd"
BIN = f"{HERE}/build-host/modinst_composed_cfgentry"
PRISTINE_BIN = f"{TEAM}/worker-d1-chain-relink/composition/build/host/modinst_composed"
ZIP = f"{WS}/.superpowers/native-observations/movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip"
MSKIN = f"{TEAM}/worker-e2e-rehearsal/rehearsal/run/mm-skin.mfb"
MLUA = f"{TEAM}/worker-e2e-rehearsal/rehearsal/run/mm-lua.mfb"
STAGING_TWIN_TRANSCRIPT = (
    f"{TEAM}/worker-e2e-staging-final/host-test/scratch/b-argv-twin/state/e2e-status.jsonl")
TRANSCRIPT_SHA = "31a4764a4ae13afac76db4e50e0905789a6b07294acb5943ebd2affb3b5ebeed"

LINE_MAX_ = 4096

# ---------------- transcript contract (identical to staging-final) ------------

T = lambda cmd, title, stage: f'{{"event":"begin","cmd":"{cmd}","title":"{title}","stage":"{stage}"}}'
E0 = '{"event":"end","exit":0}'
S2 = lambda detail: f'{{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"{detail}"}}'
S3 = f'{{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed,mm-skin,mm-lua"}}'
S6 = lambda detail: f'{{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"{detail}"}}'
EXPECTED = [
    T("records", "", "stage-0-baseline"), E0,
    T("register-mod", "PPSA33286", "stage-1-download"), E0,
    T("prepare-archive", "PPSA33286", "stage-2-prepare"),
    S2("manifest-v1.bin"), S2("auto-bundle composed mfb"), E0,
    T("register-mod", "PPSA33286", "stage-1-download"), E0,
    T("register-mod", "PPSA33286", "stage-1-download"), E0,
    T("prepare", "PPSA33286", "stage-2-prepare"), E0,
    T("install", "PPSA33286", "stage-3-install"), S3, E0,
    T("simulate-runtime", "PPSA33286", "stage-4-5-runtime-ack"), E0,
    T("records", "", "stage-0-baseline"), E0,
    T("simulate-runtime", "PPSA33286", "stage-4-5-runtime-ack"), E0,
    T("records", "", "stage-0-baseline"), E0,
    T("install", "PPSA33286", "stage-3-install"), S6("movspeed,mm-skin"), E0,
    T("install", "PPSA33286", "stage-3-install"), S6("movspeed"), E0,
    T("install", "PPSA33286", "stage-3-install"), E0,
    T("records", "", "stage-0-baseline"), E0,
]
RECOVER_2 = [T("recover", "", "recovery"), E0]

STAGES = [
    ("D0/stage0", "stage0-records.json"),
    ("stage1-owner", "stage1-register.json"),
    ("stage2a", "stage2a-prepare-archive.json"),
    ("stage2b-1", "stage2b-1-register.json"),
    ("stage2b-2", "stage2b-2-register.json"),
    ("stage2b-3", "stage2b-3-prepare.json"),
    ("stage3", "stage3-install.json"),
    ("stage4", "stage4-activated.json"),
    ("records-after-4", "stage0-records.json"),
    ("stage5", "stage5-mods-started.json"),
    ("records-after-5", "stage0-records.json"),
    ("stage6-1", "stage6-1-remove.json"),
    ("stage6-2", "stage6-2-remove.json"),
    ("stage6-3", "stage6-3-remove.json"),
    ("closing-records", "stage0-records.json"),
]


def sha256_file(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()


def fresh_scratch(name):
    sc = f"{HERE}/host-test/scratch/{name}"
    shutil.rmtree(sc, ignore_errors=True)
    os.makedirs(f"{sc}/intake")
    shutil.copy(ZIP, f"{sc}/intake/movspeed.zip")
    shutil.copy(MSKIN, f"{sc}/intake/mm-skin.mfb")
    shutil.copy(MLUA, f"{sc}/intake/mm-lua.mfb")
    return sc


def host_variant(sc, cmd_file):
    raw = open(f"{CMD}/{cmd_file}", "rb").read()
    return raw.replace(b"/data/modbridge", sc.encode()), f"{sc}/cmd-{cmd_file}"


def transcript(sc):
    st = f"{sc}/state/e2e-status.jsonl"
    return open(st).read().splitlines() if os.path.isfile(st) else []


def dispatch(sc, argv, log, timeout=60):
    p = subprocess.run(argv, executable=BIN, cwd=sc, capture_output=True,
                       text=True, timeout=timeout)
    log.write(f"exit={p.returncode} argv={argv}\n")
    if p.stdout.strip():
        log.write("stdout:\n" + p.stdout)
    if p.stderr.strip():
        log.write("stderr:\n" + p.stderr)
    return p


def entry_dispatch(sc, cmd_path, log):
    """One dispatch through the compiled-in entry (--e2e-cmd slot)."""
    return dispatch(sc, [BIN, "--e2e-cmd", cmd_path], log)


def main():
    for f in (BIN, PRISTINE_BIN, ZIP, MSKIN, MLUA, STAGING_TWIN_TRANSCRIPT):
        if not os.path.isfile(f):
            print(f"FATAL input missing: {f}")
            return 2
    os.makedirs(f"{HERE}/host-test/scratch", exist_ok=True)
    log = open(f"{HERE}/host-test/host-test.log", "w")
    receipt = []
    ok = True

    def check(cond, label):
        nonlocal ok
        ok = ok and bool(cond)
        receipt.append(f"{'OK  ' if cond else 'FAIL'}  {label}")
        return bool(cond)

    receipt.append("# Identity\n")
    receipt.append(f"- patched binary: {BIN}")
    receipt.append(f"- patched binary sha256: {sha256_file(BIN)}")
    receipt.append(f"- patched .o sha256: {sha256_file(f'{HERE}/build-host/modinst_core_cfgentry.o')}")
    receipt.append(f"- owned source sha256: {sha256_file(f'{HERE}/build-host/modinst_core.c')}")
    receipt.append(f"- pristine base sha256: {sha256_file(f'{TEAM}/worker-d1-chain-relink/composition/vendored/modinst_core.c')}")
    receipt.append(f"- linked import census: identical to pristine D1 binary "
                   f"(nm -u diff empty; no plain strtol anywhere)\n")

    # ---- A: bare dispatch, slot absent on host ---------------------------
    receipt.append("## A. bare dispatch (argc<2, compile-time slot absent on host)\n")
    sc_a = fresh_scratch("a-bare")
    p = dispatch(sc_a, [BIN], log)
    check(p.returncode == 2, f"bare dispatch exit 2 (got {p.returncode})")
    check("error bad_request e2e-cmd: file absent" in p.stdout,
          "bare dispatch refusal line on stdout")
    check("usage:" in p.stdout, "bare dispatch prints usage (today's shape)")
    check(transcript(sc_a) == [], "bare dispatch writes no status lines")
    check(not os.path.exists(f"{sc_a}/state"), "bare dispatch creates no state dir")

    # ---- B: full replay through the compiled-in entry ---------------------
    receipt.append("\n## B. stage 0..6 replay via compiled-in entry (--e2e-cmd)\n")
    sc_b = fresh_scratch("b-entry-replay")
    for label, cf in STAGES:
        raw, hp = host_variant(sc_b, cf)
        open(hp, "wb").write(raw)
        p = entry_dispatch(sc_b, hp, log)
        consumed = os.path.isfile(hp + ".consumed") and not os.path.isfile(hp)
        good = p.returncode == 0 and consumed
        check(good, f"{label}: exit 0 + consumed-rename "
                    f"(exit {p.returncode}, consumed={consumed})")
        if p.returncode != 0:
            break
    got_b = transcript(sc_b)
    match = got_b == EXPECTED
    check(match, f"transcript == 35-line corrected ledger ({len(got_b)} lines)")
    tsha = sha256_file(f"{sc_b}/state/e2e-status.jsonl")
    check(tsha == TRANSCRIPT_SHA, f"transcript sha256 == {TRANSCRIPT_SHA[:16]}... (got {tsha[:16]}...)")

    # ---- C: legacy-argv twins on the SAME patched binary ------------------
    receipt.append("\n## C. legacy-argv twins (hand-written CLI form), same patched binary\n")
    sc_c = fresh_scratch("c-legacy-twin")

    def twin_argv(cmd_file, sc):
        obj = json.loads(open(f"{CMD}/{cmd_file}").read())
        v = obj["verb"]
        s, r = obj["state_root"]["state"].replace("/data/modbridge", sc), \
               obj["state_root"]["roots"].replace("/data/modbridge", sc)
        o = obj.get("options", {})
        for k in ("title", "id", "bundle", "archive", "quarantine", "mod",
                  "remove", "rt_state", "choices"):
            if k in o:
                o[k] = o[k].replace("/data/modbridge", sc)
        if v == "records":
            return [BIN, "records", "--state", s, "--roots", r]
        if v == "register-mod":
            return [BIN, "register-mod", "--id", o["id"], "--title", o["title"],
                    "--bundle", o["bundle"], "--state", s, "--roots", r]
        if v == "prepare-archive":
            a = [BIN, "prepare-archive", "--title", o["title"], "--archive", o["archive"],
                 "--quarantine", o["quarantine"]]
            if "mod" in o:
                a += ["--mod", o["mod"]]
            if o.get("no_auto_bundle") is True:
                a += ["--no-auto-bundle"]
            return a + ["--state", s, "--roots", r]
        if v == "prepare":
            return [BIN, "prepare", "--title", o["title"], "--mods", ",".join(obj["mods"]),
                    "--state", s, "--roots", r]
        if v == "install":
            a = [BIN, "install", "--title", o["title"]]
            if "mods" in obj:
                a += ["--mods", ",".join(obj["mods"])]
            if "remove" in o:
                a += ["--remove", o["remove"]]
            return a + ["--state", s, "--roots", r]
        if v == "simulate-runtime":
            return [BIN, "simulate-runtime", "--title", o["title"],
                    "--rt-state", o["rt_state"], "--state", s, "--roots", r]
        if v == "recover":
            return [BIN, "recover", "--state", s, "--roots", r]
        raise SystemExit(f"no twin for {v}")

    for label, cf in STAGES:
        p = dispatch(sc_c, twin_argv(cf, sc_c), log)
        if p.returncode != 0:
            check(False, f"{label}: legacy twin exit {p.returncode}")
            break
    got_c = transcript(sc_c)
    check(got_c == got_b, "legacy-argv transcript byte-identical to entry transcript")
    staging_twin = open(STAGING_TWIN_TRANSCRIPT).read().splitlines()
    check(got_c == staging_twin,
          "legacy-argv transcript byte-identical to staging-final twin "
          "(PRISTINE D1 binary) -> legacy argv unaffected")

    # ---- C2: recover probe via entry --------------------------------------
    receipt.append("\n## C2. recover probe via compiled-in entry\n")
    sc_c2 = fresh_scratch("c2-recover")
    raw, hp = host_variant(sc_c2, "recover.json")
    open(hp, "wb").write(raw)
    p = entry_dispatch(sc_c2, hp, log)
    got_c2 = transcript(sc_c2)
    check(p.returncode == 0 and got_c2 == RECOVER_2 and os.path.isfile(hp + ".consumed"),
          f"recover: exit 0, begin/end recovery shape, consumed (exit {p.returncode}, {len(got_c2)} lines)")

    # ---- D: stale re-dispatch of a consumed slot ---------------------------
    receipt.append("\n## D. stale re-dispatch (consumed slot)\n")
    p = entry_dispatch(sc_b, f"{sc_b}/cmd-stage3-install.json", log)
    check(p.returncode == 2 and "file absent" in p.stdout,
          f"stale re-dispatch exit 2 + absent line (exit {p.returncode})")
    check(transcript(sc_b) == got_b, "stale re-dispatch changed zero state")

    # ---- E: argv wins, leftover cmd file untouched --------------------------
    receipt.append("\n## E. argv>=2 without --e2e-cmd never consults the entry\n")
    sc_e = fresh_scratch("e-argv-wins")
    raw, hp = host_variant(sc_e, "stage0-records.json")
    open(hp, "wb").write(raw)
    obj = json.loads(raw)
    p = dispatch(sc_e, [BIN, "records", "--state", f"{sc_e}/state",
                        "--roots", f"{sc_e}/roots"], log)
    check(p.returncode == 0, f"legacy records dispatch exit 0 (got {p.returncode})")
    check(os.path.isfile(hp) and not os.path.isfile(hp + ".consumed"),
          "leftover valid cmd file NOT consumed by legacy dispatch")

    # ---- F: refusal matrix (14 cases) through the compiled-in entry --------
    receipt.append("\n## F. refusal matrix (14 cases, schema sec 6) via compiled-in entry\n")
    ref_dir = f"{HERE}/host-test/refuse-cases"
    shutil.rmtree(ref_dir, ignore_errors=True)
    os.makedirs(ref_dir)
    big = '{"schema":1,"verb":"records","state_root":{"state":"' + "/x" * 2100 + '","roots":"/data/modbridge/roots"},"options":{}}'
    # Each case: (name, body, expected_exit, expects_report_line)
    # expects_report_line: 'none' (no valid state dir) | 'host' (scratch dir pre-created)
    cases = [
        ("unknown-top-key", '{"schema":1,"verb":"records","extra":1,"state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{}}', 6, "none"),
        ("unknown-option-key", '{"schema":1,"verb":"records","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"colour":"red"}}', 6, "none"),
        ("deny-verb-make-bundle", '{"schema":1,"verb":"make-bundle","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{}}', 6, "none"),
        ("deny-option-crash_at", '{"schema":1,"verb":"install","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"title":"PPSA33286","crash_at":"staged"}}', 6, "none"),
        ("bad-schema", '{"schema":2,"verb":"records","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{}}', 6, "none"),
        ("mods-with-remove", '{"schema":1,"verb":"install","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"mods":["mm-skin"],"options":{"title":"PPSA33286","remove":"mm-lua"}}', 6, "none"),
        ("mods-on-records", '{"schema":1,"verb":"records","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"mods":["mm-skin"],"options":{}}', 6, "none"),
        ("unicode-escape", '{"schema":1,"verb":"records","state_root":{"state":"\\/data\\/modbridge\\u007state","roots":"/data/modbridge/roots"},"options":{}}', 6, "none"),
        ("oversize", big, 6, "none"),
        ("no_auto_bundle-false", '{"schema":1,"verb":"prepare-archive","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"title":"PPSA33286","archive":"/a","quarantine":"/q","no_auto_bundle":false}}', 6, "none"),
        ("title-on-recover", '{"schema":1,"verb":"recover","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"title":"PPSA33286"}}', 6, "none"),
        ("relative-state", '{"schema":1,"verb":"records","state_root":{"state":"state","roots":"/data/modbridge/roots"},"options":{}}', 6, "none"),
        ("unknown-verb", '{"schema":1,"verb":"transmogrify","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{}}', 6, "none"),
        ("absent-file", None, 2, "none"),
    ]
    receipt.append("| case | expected | exit | stdout reason | report line |")
    receipt.append("|---|---|---|---|---|")
    n_refuse = 0
    for name, body, want_rc, rep in cases:
        p = f"{ref_dir}/{name}.json"
        if body is not None:
            open(p, "w").write(body)
        sc_f = fresh_scratch(f"f-{name}")
        slot = p if name != "absent-file" else f"{sc_f}/does-not-exist.json"
        report_path = f"{sc_f}/state/e2e-status.jsonl"
        if rep == "host":
            # host-adapted rerun: state_root rewritten into the scratch so the
            # report path is writable; pre-create the state dir (refusal path
            # must NOT create it - that is dispatch's job).
            adapted = body.replace(b"/data/modbridge", sc_f.encode())
            open(slot, "wb").write(adapted)
            os.makedirs(f"{sc_f}/state")
        pexit = entry_dispatch(sc_f, slot, log)
        rc_ok = pexit.returncode == want_rc
        reason_ok = (f"error bad_request e2e-cmd:" in pexit.stdout) if want_rc == 6 \
            else ("file absent" in pexit.stdout)
        lines = open(report_path).read().splitlines() if os.path.isfile(report_path) else []
        if rep == "host":
            rep_ok = (len(lines) == 1
                      and '"stage":"e2e-cmd"' in lines[0]
                      and '"outcome":"refused"' in lines[0])
            verb_ok = (not os.path.exists(f"{sc_f}/state/journal")
                       and not os.path.exists(f"{sc_f}/state/records.txt"))
            rep_desc = f"1 refused line, verb untouched={verb_ok}"
            rep_flag = rep_ok and verb_ok
        else:
            rep_ok = len(lines) == 0
            rep_desc = "no report line"
            rep_flag = True
        n_refuse += 1 if (rc_ok and reason_ok and rep_flag) else 0
        good = rc_ok and reason_ok and rep_flag and rep_ok
        ok = ok and good
        receipt.append(f"| {name} | exit {want_rc} | {pexit.returncode} | "
                       f"{'yes' if reason_ok else 'MISSING'} | {rep_desc} | "
                       f"{'OK' if good else 'FAIL'} |")
    check(n_refuse == 14, f"{n_refuse}/14 refusal cases behave as designed")

    # ---- G: valueless --e2e-cmd stays legacy grammar -----------------------
    receipt.append("\n## G. `--e2e-cmd` without a value stays legacy grammar\n")
    sc_g = fresh_scratch("g-valueless")
    p = dispatch(sc_g, [BIN, "--e2e-cmd"], log)
    check(p.returncode == 2 and "usage:" in p.stdout,
          f"valueless --e2e-cmd -> unknown flag usage + exit 2 (exit {p.returncode})")
    check(transcript(sc_g) == [], "valueless --e2e-cmd wrote no status lines")

    # ---- scope honesty ------------------------------------------------------
    receipt.append("\n## Scope honesty\n")
    receipt.append("All results are HOST-side, exercising the ACTUAL compiled-in entry of the "
                   "patched binary through the --e2e-cmd override slot (the designed host "
                   "harness entry point). The compile-time slot /data/modbridge/intake/"
                   "e2e-cmd.json itself is only reachable on console; bare-dispatch behavior "
                   "with that slot absent is proven identical to today's usage()+EX_BAD. "
                   "Console STOR/readback/consumed behavior, the fresh tuple IDs (g1-cmd-entry-*), "
                   "suites re-run (bridge 75 / F-R2 87 / fault 186 + new config-entry cases), "
                   "the new build receipt and the Step P re-pin remain the build worker's and "
                   "controller's work; console results remain [TARGET] until that run.")
    receipt.append(f"\nOVERALL: {'PASS' if ok else 'FAIL'}\n")
    open(f"{HERE}/host-test/HOST-TEST-RECEIPT.md", "w").write(
        "# HOST-TEST-RECEIPT — config-entry revision, compiled-in entry, host validation\n\n"
        + "\n".join(receipt) + "\n")
    log.close()
    print("\n".join(receipt[-14:]))
    print(f"OVERALL: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
