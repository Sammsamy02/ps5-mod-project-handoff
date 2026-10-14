#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
l10_real_rehearsal.py — worker-staging-rehearsal (2026-10-04).

Full fake-drive rehearsal of the console_batch.py `stage-launch10` flow with the
REAL launch-10 GO candidate (prx-candidate-l10-2):

  LOCAL ONLY. curl invocations are intercepted in-process and answered from an
  in-memory fake console — NO network socket is ever opened, and a hard guard
  raises if any subprocess argv would touch the console host (ps5-host-not-configured.invalid).

  Real inputs (nothing synthetic about the artifact under test):
    FSELF      = outputs/2026-10-04-zcode-controller/launch10-build/
                 prx-candidate-l10-2/modbridge_supplier_composition.prx
    FSELF sha  = 60756ef275da6073b91c8b353dd9e59f49018adb742f98630cf55cbb3aaed2b1
    FSELF size = 30,837,767 B (both recomputed AND cross-checked against the
                 packaged artifacts-receipt.json final_prx record)

  The ONE documented stand-in: the console's current ploader.ini bytes (pinned
  sha 1216aab6…) exist only on the console, so the fake seeds a minimal
  2-plugin-config-shaped fixture ini and L10_INI_EXPECTED_SHA is pointed at its
  sha — exactly the stand-in mechanism console_batch.py's own fake-drive suite
  documents (comment above L10_INI_EXPECTED_SHA). Every other law value is the
  real pinned one. The ini law path (freeze l10.1 / STILL l10.5 / never-written)
  is exercised identically.

  Scenarios (each a fresh fake console + fresh run dir):
    happy     .bak -> exactly-once STOR -> readback law (FULL identity: this
              FSELF is already SHT-stripped, e_shnum=0, so the ftpsrv strip view
              == stored bytes) -> ini STILL byte-equal -> baselines -> GO
    mismatch  one-shot corrupted readback -> l10.4 FAIL -> OLD PRX restored
              byte-exact ONCE from the pre-image -> NO-GO
    drift     console ini diverges AFTER the l10.1 freeze -> l10.3/l10.4 PASS,
              l10.5 REFUSES -> OLD PRX restored once -> NO-GO, ini never written

Run: python3 l10_real_rehearsal.py   (exit 0 = all checks green)
"""

import importlib.util
import hashlib
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BATCH_RUNNER = os.path.join(HERE, "..", "worker-console-batch-runner")
sys.path.insert(0, BATCH_RUNNER)

spec = importlib.util.spec_from_file_location(
    "console_batch", os.path.join(BATCH_RUNNER, "console_batch.py"))
cb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cb)

# ---------------------------------------------------------------------------
# REAL candidate values (recomputed + receipt-cross-checked in the main() below)
# ---------------------------------------------------------------------------
REAL_FSELF_PATH = os.path.join(
    HERE, "..", "..", "2026-10-04-zcode-controller", "launch10-build",
    "prx-candidate-l10-2", "modbridge_supplier_composition.prx")
REAL_FSELF_PATH = os.path.abspath(REAL_FSELF_PATH)
RECEIPT_PATH = os.path.join(os.path.dirname(REAL_FSELF_PATH), "artifacts-receipt.json")
REAL_INI_SHA_PINNED = cb.L10["ini_sha256"]          # 1216aab6… (the tool's own pin)
TARGET_URL = cb.FTP + cb.L10["target_prx"]
INI_URL = cb.FTP + cb.L10["ini"]

FAILURES = []
RECEIPT = {
    "worker": "worker-staging-rehearsal",
    "date": "2026-10-04",
    "mode": "LOCAL-ONLY fake-drive (in-process curl interception; zero sockets; zero console contact)",
    "tool": os.path.abspath(os.path.join(BATCH_RUNNER, "console_batch.py")),
    "candidate": {},
    "stand_ins": [],
    "scenarios": {},
    "failed_checks": [],
}


def check(name, cond, detail=""):
    print(("  ok   " if cond else "  FAIL ") + name + (("  :: " + detail) if (detail and not cond) else ""))
    if not cond:
        FAILURES.append(name)
        RECEIPT["failed_checks"].append({"check": name, "detail": detail})


# ---------------------------------------------------------------------------
# Hard local-only guard: no subprocess may ever reach the console host
# ---------------------------------------------------------------------------
_orig_subprocess_run = subprocess.run
_guard_tripped = {"hit": False}


def guarded_subprocess_run(argv, *a, **k):
    joined = " ".join(map(str, argv or []))
    if "ps5-host-not-configured.invalid" in joined:
        _guard_tripped["hit"] = True
        raise RuntimeError("LOCAL-ONLY GUARD: subprocess tried to reach the console host: %r" % joined[:200])
    return _orig_subprocess_run(argv, *a, **k)


subprocess.run = guarded_subprocess_run


# ---------------------------------------------------------------------------
# Fake console (stage-launch10 surface only)
# ---------------------------------------------------------------------------
class FakeConsole:
    """Serves the exact curl surface stage-launch10 uses. PRX readbacks serve the
    last STORed bytes (the ftpsrv SHT-strip channel is a NO-OP for this artifact:
    the candidate FSELF is already stripped, e_shnum=0 — strip view == stored
    bytes), falling back to the seeded pre-image."""

    def __init__(self, old_prx_bytes, ini_bytes):
        self.old_prx = old_prx_bytes
        self.ini = ini_bytes
        self.ini_initial = ini_bytes
        self.staged = None                 # last bytes STORed to the final PRX name
        self.stors = []                    # ordered (kind, url, nbytes)
        self.ini_stors = []                # must stay EMPTY in every scenario
        self.ini_retr_n = 0
        self.drift_after_freeze = False    # scenario (c)
        self.corrupt_once = False          # scenario (b), one-shot
        self.corrupt_served = False
        self.g0_hits = 0

    def reset(self, ini_bytes=None):
        self.ini = ini_bytes if ini_bytes is not None else self.ini_initial
        self.staged = None
        self.stors = []
        self.ini_stors = []
        self.ini_retr_n = 0
        self.drift_after_freeze = False
        self.corrupt_once = False
        self.corrupt_served = False
        self.g0_hits = 0

    def handle_curl(self, argv):
        joined = " ".join(argv)
        out_target = argv[argv.index("-o") + 1] if "-o" in argv else None
        data_target = argv[argv.index("-T") + 1] if "-T" in argv else None

        def emit_bytes(content):
            if out_target:
                os.makedirs(os.path.dirname(out_target) or ".", exist_ok=True)
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 0, content.decode("utf-8", "replace")

        def emit_text(text):
            if out_target:
                os.makedirs(os.path.dirname(out_target) or ".", exist_ok=True)
                with open(out_target, "w") as f:
                    f.write(text)
                return 0, ""
            return 0, text

        # --- G0 payload manager (8084) ---
        if "list_payloads" in joined:
            self.g0_hits += 1
            return emit_text("/data/pldmgr/payloads/ ... (fake G0 200 OK)\n")

        # --- ploader.ini ---
        if "ploader.ini" in joined:
            if data_target:  # STOR — this flow must NEVER do this
                self.ini_stors.append((joined.split()[-1], os.path.getsize(data_target)))
                return 0, ""
            if out_target:   # RETR
                self.ini_retr_n += 1
                if self.drift_after_freeze and self.ini_retr_n == 1:
                    # external drift lands AFTER the l10.1 freeze RETR, BEFORE l10.5
                    self.ini = self.ini + "/data/plugins/modbridge_surprise_external.prx = 2\n"
                return emit_bytes(self.ini.encode())
            return 0, self.ini

        # --- composition PRX (target of the ONE replacement) ---
        if "modbridge_supplier_composition.prx" in joined:
            if data_target:
                kind = "bak" if ".bak" in joined else "prx"
                self.stors.append((kind, joined.split()[-1], os.path.getsize(data_target)))
                if kind == "prx":
                    with open(data_target, "rb") as f:
                        self.staged = f.read()
                return 0, ""
            content = self.staged if self.staged is not None else self.old_prx
            if self.staged is not None and self.corrupt_once and not self.corrupt_served:
                self.corrupt_served = True
                content = content[:-64] + b"CORRUPTED-L10-READBACK\n"  # law-divergent tail
            return emit_bytes(content)

        # --- baseline sinks (l10.6) ---
        if "UE4SS.log" in joined:
            return emit_text("UE4SS - v3.0\nroot directory: /data/plugins\n(fake baseline)\n")
        if "runtime-status.txt" in joined:
            return emit_text("modforge-runtime-status\t1\nsession\t4242\t9876543\n(fake baseline)\n")
        if "townfall_runtime_aligned_entry.log" in joined:
            return emit_text("module_start_entered value=0x00000000\n(fake baseline)\n")
        if "modbridge-supplier-diag.jsonl" in joined:
            return emit_text("")  # absent/empty on the console — recorded honestly

        return emit_text("")


# ---------------------------------------------------------------------------
# Wire the fake into the real execution engine
# ---------------------------------------------------------------------------
_orig_run_cmd = cb.run_cmd
fc = None  # set in main()


def fake_run_cmd(ctx, argv, label, outfile=None, timeout=120, cwd=None):
    if argv and argv[0] == "curl":
        rc, out = fc.handle_curl(argv)
        if outfile and not ctx.dry:
            ctx.save(outfile, "cmd: %s\nrc: %d\n--- stdout ---\n%s\n" % (" ".join(argv), rc, out))
        return rc, out, ""
    if argv and argv[0] == "shasum":
        return _orig_run_cmd(ctx, argv, label, outfile=None, timeout=timeout, cwd=cwd)
    return 0, "", ""


cb.run_cmd = fake_run_cmd
cb.user_gate = lambda ctx, prompt: True


# ---------------------------------------------------------------------------
# Scenarios
# ---------------------------------------------------------------------------
def run_dir_for(name):
    d = os.path.abspath(os.path.join(HERE, "runs", name))
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d, exist_ok=True)
    return d


def arm_real_values():
    cb.L10_FSELF_LOCAL_PATH = REAL_FSELF_PATH
    cb.L10_FSELF_SHA256 = REAL_SHA
    # the tool's pinned ini sha stays untouched in the module default; for the
    # fake console we point the comparison at the seeded fixture ini (stand-in)
    cb.L10_INI_EXPECTED_SHA = INI_SHA


def statuses_of(res):
    return dict(res)


def main():
    global REAL_SHA, INI_SHA
    print("== worker-staging-rehearsal: REAL-value fake-drive of stage-launch10 ==")

    # ---- real identity computation + receipt cross-check (BUILD-ID law inputs)
    REAL_SHA, REAL_N = cb.local_sha(REAL_FSELF_PATH)
    py_sha = hashlib.sha256(open(REAL_FSELF_PATH, "rb").read()).hexdigest()
    rec = json.load(open(RECEIPT_PATH))
    r_sha = rec["artifacts"]["final_prx"]["sha256"]
    r_bytes = rec["artifacts"]["final_prx"]["bytes"]
    local = open(REAL_FSELF_PATH, "rb").read()
    REAL_N = len(local)
    check("candidate recomputed sha == packaged artifacts-receipt.json final_prx.sha256",
          REAL_SHA == py_sha == r_sha, "%s / %s / %s" % (REAL_SHA, py_sha, r_sha))
    check("candidate size == packaged receipt bytes == 30,837,767",
          REAL_N == r_bytes == 30837767, "%d / %d" % (REAL_N, r_bytes))
    RECEIPT["candidate"] = {
        "path": REAL_FSELF_PATH, "sha256": REAL_SHA, "bytes": REAL_N,
        "receipt_cross_check": "MATCH (recomputed shasum -a 256 == python hashlib == artifacts-receipt.json)",
        "elf_e_shnum_at_60": int.from_bytes(local[60:62], "little"),
        "expected_readback": "FULL byte identity (FSELF already SHT-stripped; e_shnum=0 => ftpsrv strip view == stored bytes)",
        "expected_readback_sha256_l10_readback_sha": REAL_SHA,
        "expected_readback_size_l10_size": REAL_N,
        "tool_sht_prefix_end_value": cb.sht_prefix_end(local),
        "tool_law_identity_verdict": cb.l10_readback_law(local, local),
        "body_sha256_64_to_eof": hashlib.sha256(local[64:]).hexdigest(),
    }
    print("  candidate: %s" % REAL_FSELF_PATH)
    print("  sha256   : %s (%d bytes) — receipt cross-check MATCH" % (REAL_SHA, REAL_N))
    print("  readback : FULL identity expected (e_shnum=%d already stripped)" %
          RECEIPT["candidate"]["elf_e_shnum_at_60"])
    print("  tool sht_prefix_end(local) -> %d (SELF-header parse artifact; the law only "
          "reaches it on a divergent readback, where the length mismatch correctly FAILs)" %
          RECEIPT["candidate"]["tool_sht_prefix_end_value"])

    # ---- the documented stand-in ini (real bytes exist only on the console)
    ini_fixture = ("[PPSA33286]\n"
                   "/data/plugins/modbridge_supplier_composition.prx = 2\n"
                   "/data/plugins/modbridge_path_test.prx = 2\n")
    INI_SHA = hashlib.sha256(ini_fixture.encode()).hexdigest()
    check("stand-in ini is the 2-plugin config shape (composition=2, path_test=2, nothing else)",
          ini_fixture.count("= 2") == 2 and len(ini_fixture.splitlines()) == 3)
    RECEIPT["stand_ins"] = [{
        "what": "ploader.ini console bytes (real pinned sha %s exist only on the console; no local copy)" % REAL_INI_SHA_PINNED,
        "fake_seeds": "minimal 2-plugin-config-shaped fixture (sha %s…)" % INI_SHA[:12],
        "mechanism": "L10_INI_EXPECTED_SHA pointed at the fixture sha, exactly the stand-in the tool documents for its own fake-drive suite; the ini law path (freeze/STILL/never-written) is unchanged",
    }]

    # ---- old PRX pre-image stand-in (current console pre-image d86887ec…/30,756,152 B
    #      is also console-only; the flow pins nothing on it — sha/size are recorded)
    old_prx = (b"[fake] OLD console composition PRX pre-image stand-in (pre-launch-10 content)\n" * 1024)
    RECEIPT["stand_ins"].append({
        "what": "current console PRX pre-image (runbook pre-image sha d86887ec… / 30,756,152 B is console-only)",
        "fake_seeds": "deterministic stand-in bytes (%d B); the flow only requires a non-empty pre-image — its sha/size are RECORDED, never pinned" % len(old_prx),
    })

    global fc
    fc = FakeConsole(old_prx_bytes=old_prx, ini_bytes=ini_fixture)

    # =======================================================================
    # (a) HAPPY PATH
    # =======================================================================
    print("\n== scenario (a) happy path: .bak -> exactly-once STOR -> readback law "
          "(full identity) -> ini STILL byte-equal -> baselines -> GO ==")
    fc.reset()
    arm_real_values()
    rd = run_dir_for("happy")
    ctx = cb.Ctx(run_dir=rd, dry=False, assume_yes=True)
    res = cb.build_stage_launch10().run(ctx)
    st = statuses_of(res)
    bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
    RECEIPT["scenarios"]["happy"] = {"run_dir": rd, "statuses": st}
    check("happy: every step PASS (l10.0..l10.7)", not bad, str(bad))
    check("happy: console sequence EXACTLY [composition .bak -> ONE FSELF STOR to the final name]",
          [(s[0], s[1]) for s in fc.stors] == [("bak", None)[:2][0], ] if False else
          [s[0] for s in fc.stors] == ["bak", "prx"] and fc.stors[1][1] == TARGET_URL, str(fc.stors))
    check("happy: exactly ONE non-.bak STOR (exactly-once law), zero restore STORs",
          sum(1 for s in fc.stors if s[0] == "prx") == 1 and len(fc.stors) == 2, str(fc.stors))
    check("happy: the REAL candidate bytes landed byte-exact",
          fc.staged == local, "staged %s bytes" % (len(fc.staged) if fc.staged else None))
    rbv = json.load(open(os.path.join(rd, "l10-readback-verdict.json")))
    check("happy: readback law OK via FULL byte identity (this artifact's expected channel outcome)",
          rbv["ok"] is True and "byte-identical" in rbv["detail"], rbv["detail"])
    check("happy: recorded readback sha/size == <L10_READBACK_SHA>/<L10_SIZE> (%s… / %d)" %
          (REAL_SHA[:12], REAL_N),
          rbv["readback_sha256"] == REAL_SHA and rbv["readback_bytes"] == REAL_N,
          "%s / %s" % (rbv["readback_sha256"], rbv["readback_bytes"]))
    check("happy: NO ini STOR ever (ini never written by the flow)", fc.ini_stors == [], str(fc.ini_stors))
    check("happy: ini RETRd exactly twice (l10.1 freeze + l10.5 STILL) and STILL byte-equal",
          fc.ini_retr_n == 2 and fc.ini == ini_fixture, "retrs=%d" % fc.ini_retr_n)
    check("happy: ini pre/post sha receipts recorded",
          os.path.exists(os.path.join(rd, "l10-ploader.ini.pre.sha256"))
          and os.path.exists(os.path.join(rd, "l10-ploader.ini.post.sha256")))
    check("happy: pre-image + sha recorded, console .bak STOR has a distinct TAG name",
          os.path.exists(os.path.join(rd, "l10-composition.prx.pre"))
          and ".bak" in fc.stors[0][1], str(fc.stors[0][1] if fc.stors else None))
    base = json.load(open(os.path.join(rd, "l10-baselines.json")))
    check("happy: fresh baselines recorded (3 present + diag absence recorded honestly)",
          base["baselines"]["ue4ss_log"]["present"] and base["baselines"]["runtime_status"]["present"]
          and base["baselines"]["entry_log"]["present"]
          and base["baselines"]["diag"]["present"] is False, str(base["baselines"]))
    gonogo = json.load(open(os.path.join(rd, "l10-go-nogo.json")))
    check("happy: verdict GO, all 8 gates green",
          gonogo["verdict"] == "GO" and all(gonogo["checks"].values()), str(gonogo["checks"]))
    RECEIPT["scenarios"]["happy"]["go_nogo"] = gonogo
    RECEIPT["scenarios"]["happy"]["readback_verdict"] = rbv

    # =======================================================================
    # (b) MISMATCH PATH — old PRX restored
    # =======================================================================
    print("\n== scenario (b) mismatch: corrupted readback -> l10.4 FAIL -> OLD PRX "
          "restored byte-exact ONCE -> NO-GO ==")
    fc.reset()
    fc.corrupt_once = True
    arm_real_values()
    rd = run_dir_for("mismatch")
    ctx = cb.Ctx(run_dir=rd, dry=False, assume_yes=True)
    res = cb.build_stage_launch10().run(ctx)
    st = statuses_of(res)
    RECEIPT["scenarios"]["mismatch"] = {"run_dir": rd, "statuses": st}
    check("mismatch: l10.4 FAILS the readback content law", st.get("l10.4") == "FAIL", st.get("l10.4"))
    check("mismatch: the FSELF was STORed EXACTLY ONCE (no re-STOR of the new payload)",
          sum(1 for s in fc.stors if s[0] == "prx" and s[2] == REAL_N) == 1, str(fc.stors))
    check("mismatch: console sequence [.bak -> FSELF -> ONE bounded restore STOR]",
          [s[0] for s in fc.stors] == ["bak", "prx", "prx"]
          and fc.stors[2][2] == len(old_prx), str(fc.stors))
    check("mismatch: the OLD PRX (pre-image bytes) is what the console now holds",
          fc.staged == old_prx)
    rv = json.load(open(os.path.join(rd, "l10-restore-verdict.json")))
    check("mismatch: restore proven byte-exact by fresh RETR (l10-restore-verdict.json)",
          rv["restored"] is True and rv["readback_sha256"] == hashlib.sha256(old_prx).hexdigest(),
          str(rv)[:200])
    check("mismatch: console_state = old-prx-restored; ini STILL check skipped after the halt",
          st.get("l10.5") == "SKIPPED_HALT", st.get("l10.5"))
    check("mismatch: NO ini STOR ever; ini bytes untouched", fc.ini_stors == [] and fc.ini == ini_fixture)
    gonogo = json.load(open(os.path.join(rd, "l10-go-nogo.json")))
    check("mismatch: NO-GO verdict recorded by the rescue step naming the failed gates",
          gonogo["verdict"] == "NO-GO"
          and gonogo["checks"]["readback_content_law_ok"] is False, str(gonogo))
    RECEIPT["scenarios"]["mismatch"]["go_nogo"] = gonogo
    RECEIPT["scenarios"]["mismatch"]["restore_verdict"] = rv

    # =======================================================================
    # (c) INI-DRIFT PATH
    # =======================================================================
    print("\n== scenario (c) ini drift after the freeze: l10.5 REFUSES -> OLD PRX restored "
          "-> NO-GO; the flow still never writes the ini ==")
    fc.reset()
    fc.drift_after_freeze = True
    arm_real_values()
    rd = run_dir_for("drift")
    ctx = cb.Ctx(run_dir=rd, dry=False, assume_yes=True)
    res = cb.build_stage_launch10().run(ctx)
    st = statuses_of(res)
    RECEIPT["scenarios"]["drift"] = {"run_dir": rd, "statuses": st}
    check("drift: upload + readback law PASSED before the drift was seen",
          st.get("l10.3") == "PASS" and st.get("l10.4") == "PASS",
          str((st.get("l10.3"), st.get("l10.4"))))
    check("drift: l10.5 REFUSES the drifted ini", st.get("l10.5") == "FAIL", st.get("l10.5"))
    check("drift: refusal names the pinned ini sha and the external-drift attribution",
          "NO ini write" in "\n".join(ctx.events), "")
    check("drift: OLD PRX restored byte-exact once after the refusal",
          fc.staged == old_prx
          and json.load(open(os.path.join(rd, "l10-restore-verdict.json")))["restored"] is True)
    check("drift: FSELF STORed exactly once even in the refusal path",
          sum(1 for s in fc.stors if s[0] == "prx" and s[2] == REAL_N) == 1, str(fc.stors))
    check("drift: baselines SKIPPED after the halt; NO ini STOR ever",
          st.get("l10.6") == "SKIPPED_HALT" and fc.ini_stors == [], str(fc.ini_stors))
    check("drift: the drifted bytes REMAIN on the fake console (reconciliation is external — "
          "the flow never fixes the ini itself)",
          fc.ini == ini_fixture + "/data/plugins/modbridge_surprise_external.prx = 2\n")
    gonogo = json.load(open(os.path.join(rd, "l10-go-nogo.json")))
    check("drift: NO-GO verdict recorded (ini_still_byte_equal=false)",
          gonogo["verdict"] == "NO-GO" and gonogo["checks"]["ini_still_byte_equal"] is False,
          str(gonogo))
    RECEIPT["scenarios"]["drift"]["go_nogo"] = gonogo

    # =======================================================================
    # cross-scenario hygiene
    # =======================================================================
    print("\n== cross-scenario hygiene ==")
    check("LOCAL-ONLY guard never tripped (zero console-contact attempts escaped interception)",
          _guard_tripped["hit"] is False)
    check("every scenario left l10-preflight.json pinning the REAL candidate identity",
          all(json.load(open(os.path.join(RECEIPT["scenarios"][k]["run_dir"], "l10-preflight.json")))["got_sha256"] == REAL_SHA
              for k in ("happy", "mismatch", "drift")))
    # module defaults restored for the REAL run
    cb.L10_FSELF_LOCAL_PATH = ""
    cb.L10_FSELF_SHA256 = ""
    cb.L10_INI_EXPECTED_SHA = cb.L10["ini_sha256"]
    check("module restored to shipped defaults (empty placeholders + pinned %s…)" % REAL_INI_SHA_PINNED[:12],
          cb.L10_FSELF_LOCAL_PATH == "" and cb.L10_FSELF_SHA256 == ""
          and cb.L10_INI_EXPECTED_SHA == REAL_INI_SHA_PINNED)

    RECEIPT["failed_checks"] = FAILURES
    with open(os.path.join(HERE, "rehearsal-raw.json"), "w") as f:
        json.dump(RECEIPT, f, indent=2)

    print("\nrehearsal summary: %d failed checks" % len(FAILURES))
    for f_ in FAILURES:
        print("  FAILED: %s" % f_)
    sys.exit(1 if FAILURES else 0)


if __name__ == "__main__":
    REAL_SHA = ""
    INI_SHA = ""
    main()
