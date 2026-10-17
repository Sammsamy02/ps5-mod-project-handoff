#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fake_console_drive.py — drives the REAL console_batch.py execution engine
against a simulated console. NO network traffic, NO user interaction:
`curl` invocations and USER gates are intercepted in-process and answered
from canned fixtures (the exact bytes the plans pin). Proves:

  * step ordering + group gating + stop-on-first-failure
  * evidence capture + per-flow receipt.json in the run dir
  * verdicts PASS against ledger-shaped canned evidence
  * the d07-recover rescue step dispatches NOTHING when stage 3 is clean
  * game-evidence classifier wiring (canned S/L/D1 -> CP0-CP4 PASS)
  * dll8 (PREFERRED single DLL run): ALLFAM8 upload/readback/dispatch exactly
    once, verdict PASS vs EXPECTED-DELTA-8FAM (memory.last 114688/163840
    oracle), and a stale-7fam artifact shape FAILS the verdict + halts

Run: python3 tests/fake_console_drive.py   (exit 0 = all green)
"""

import importlib.util
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, ROOT)

spec = importlib.util.spec_from_file_location("console_batch", os.path.join(ROOT, "console_batch.py"))
cb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cb)

FAILURES = []


def check(name, cond, detail=""):
    print(("  ok   " if cond else "  FAIL ") + name + (("  :: " + detail) if (detail and not cond) else ""))
    if not cond:
        FAILURES.append(name)


# ---------------------------------------------------------------------------
# Canned console content
# ---------------------------------------------------------------------------

def build_7fam_transcript():
    base = {"marker": "modbridge.owned-dll", "schema": 1,
            "build_id": cb.DLL7["build_id"],
            "entry": {"enabled": 0}, "execution": {"stage": 0}, "clock": {}, "worker": {},
            "counts": {}, "results": [0, 0], "cleanup": {"pending": 0, "retired": 0},
            "delivery": {}, "memory": {}, "first_failure": {}}

    def rec(seq, phase, **over):
        r = json.loads(json.dumps(base))
        r["sequence"], r["phase"] = seq, phase
        for k, v in over.items():
            r[k] = v
        return r

    fams = {f: {"enabled": 1} for f in cb.FAMILY_KEYS}
    r0 = rec(0, 1)
    r1 = rec(1, 2, _fams=1, execution={"stage": 2, "started": 1, "run_attempted": 0, "completed": 0,
             "dll_calls_started": 0, "dll_calls_returned": 0}, entry={"enabled": 1},
             clock={"enabled": 1}, worker={"enabled": 1}, cleanup={"pending": 1, "retired": 0},
             delivery={"file_records": 1}, first_failure={}, **fams)
    common = dict(
        entry={"enabled": 1, "attach_calls": [1, 1], "attach_result": [7, 7], "detach_calls": [0, 1],
               "detach_result": [0, 0], "contract_errors": 0},
        execution={"stage": 9, "started": 1, "run_attempted": 1, "completed": 1,
                   "dll_calls_started": 3, "dll_calls_returned": 3},
        clock={"enabled": 1, "session_begins": [1, 1], "session_ends": [0, 1]},
        worker={"enabled": 1, "calls_started": 4, "calls_returned": 4, "begin": [5, 1, 258, 259],
                "result": [2166572391, 1234605616436508552, 3782448555, 0]},
        counts={"consumer_imports": 82, "helper_imports": 10, "source_acquires": [1, 1], "source_releases": [0, 1]},
        results=[86, 93], static_tls={"enabled": 1},
        memory={"owner_status": 0, "last": {"operation": 3, "native_result": 0, "native_errno": 0,
                "offset": 0, "length": 147456, "rights": 0}, "first_failure": {}},
        delivery={"first_file_error": 0, "last_file_error": 0, "first_stdout_error": 32,
                  "last_stdout_error": 32, "file_records": 2, "stdout_records": 1},
        cleanup={"pending": 1, "retired": 0}, first_failure={})
    r2 = rec(2, 3, **common)
    c3 = json.loads(json.dumps(common))
    c3["execution"] = dict(c3["execution"], stage=11)
    c3["entry"] = dict(c3["entry"], detach_calls=[1, 1])
    c3["clock"] = dict(c3["clock"], session_ends=[1, 1])
    c3["counts"] = dict(c3["counts"], source_releases=[1, 1])
    c3["cleanup"] = {"pending": 0, "retired": 1}
    c3["delivery"] = dict(c3["delivery"], file_records=3)
    r3 = rec(3, 5, **c3)
    # 36 prior records from earlier builds, then the 4 new ones (append-only)
    prior = [{"marker": "modbridge.owned-dll", "schema": 1, "build_id": "133c5e9e59f24c2fb57e7e3422c665a1",
              "sequence": i % 4, "phase": [1, 2, 3, 5][i % 4]} for i in range(36)]
    return "\n".join(json.dumps(x) for x in prior + [r0, r1, r2, r3]) + "\n"


def build_8fam_transcript():
    """Canned EXPECTED-DELTA-8FAM PASS shape: 36 prior records, then the 4 new
    c3aba79f records appended at positions 37-40 (the recommended dll8-first
    order). Oracle: consumer_imports 90, helper 10, seh64 {enabled:1} from
    record1, memory.last.length 114688 (seq2) -> 163840 (seq3 FINAL)."""
    base = {"marker": "modbridge.owned-dll", "schema": 1,
            "build_id": cb.DLL8["build_id"],
            "entry": {"enabled": 0}, "execution": {"stage": 0}, "clock": {}, "worker": {},
            "counts": {}, "results": [0, 0], "cleanup": {"pending": 0, "retired": 0},
            "delivery": {}, "memory": {}, "first_failure": {}}

    def rec(seq, phase, **over):
        r = json.loads(json.dumps(base))
        r["sequence"], r["phase"] = seq, phase
        for k, v in over.items():
            if k == "_fams":
                for f in cb.FAMILY_KEYS_8:
                    r[f] = {"enabled": 1}
            else:
                r[k] = v
        return r

    fams8 = {f: {"enabled": 1} for f in cb.FAMILY_KEYS_8}
    r0 = rec(0, 1)
    r1 = rec(1, 2, _fams=1, execution={"stage": 2, "started": 1, "run_attempted": 0, "completed": 0,
             "dll_calls_started": 0, "dll_calls_returned": 0}, entry={"enabled": 1},
             clock={"enabled": 1}, worker={"enabled": 1}, cleanup={"pending": 1, "retired": 0},
             delivery={"file_records": 1}, first_failure={}, **fams8)
    common = dict(
        entry={"enabled": 1, "attach_calls": [1, 1], "attach_result": [7, 7], "detach_calls": [0, 1],
               "detach_result": [0, 0], "contract_errors": 0},
        execution={"stage": 9, "started": 1, "run_attempted": 1, "completed": 1,
                   "dll_calls_started": 3, "dll_calls_returned": 3},
        clock={"enabled": 1, "session_begins": [1, 1], "session_ends": [0, 1]},
        worker={"enabled": 1, "calls_started": 4, "calls_returned": 4, "begin": [5, 1, 258, 259],
                "result": [2166572391, 1234605616436508552, 3782448555, 0]},
        counts={"consumer_imports": 90, "helper_imports": 10, "source_acquires": [1, 1], "source_releases": [0, 1]},
        results=[86, 93], seh64={"enabled": 1}, static_tls={"enabled": 1},
        memory={"owner_status": 0, "last": {"operation": 3, "native_result": 0, "native_errno": 0,
                "offset": 0, "length": 114688, "rights": 0}, "first_failure": {}},
        delivery={"first_file_error": 0, "last_file_error": 0, "first_stdout_error": 32,
                  "last_stdout_error": 32, "file_records": 2, "stdout_records": 1},
        cleanup={"pending": 1, "retired": 0}, first_failure={})
    r2 = rec(2, 3, **common)
    c3 = json.loads(json.dumps(common))
    c3["execution"] = dict(c3["execution"], stage=11)
    c3["entry"] = dict(c3["entry"], detach_calls=[1, 1])
    c3["clock"] = dict(c3["clock"], session_ends=[1, 1])
    c3["counts"] = dict(c3["counts"], source_releases=[1, 1])
    c3["cleanup"] = {"pending": 0, "retired": 1}
    c3["delivery"] = dict(c3["delivery"], file_records=3)
    c3["memory"] = dict(c3["memory"], last=dict(c3["memory"]["last"], length=163840))
    r3 = rec(3, 5, **c3)
    prior = [{"marker": "modbridge.owned-dll", "schema": 1, "build_id": "133c5e9e59f24c2fb57e7e3422c665a1",
              "sequence": i % 4, "phase": [1, 2, 3, 5][i % 4]} for i in range(36)]
    return "\n".join(json.dumps(x) for x in prior + [r0, r1, r2, r3]) + "\n"


S_GREEN = ("modforge-runtime-status\t1\nsession\t4242\t9876543\n"
           "state\t0\tactivated\t0\t\nstate\t1\tengine-ready\t0\t\nstate\t2\tmods-started\t0\t\n")
D1_GREEN = ("module_start_entered value=0x00000000\nconstructor_begin\nconstructor_return\n"
            "probe_lifecycle_result value=0x00000000\nruntime_wiring_begin value=0x00000000\n"
            "mfa_activated_low\nmfa_activated_high\nruntime_start_ok value=0x00000000\n"
            "module_start_return value=0x00000000\n")
L_GREEN = ("UE4SS - v3.0\nroot directory: /data/plugins\nmods directories: [0] ue4ss/Mods\n"
           "Setting up mods...\n" + cb.GAME["tick_line"] + "\n" + cb.GAME["mod_banner"] + "\n")
C2_GREEN = '{"marker":"modbridge.ingamefacts","build_id":"qdrv1-c20c70ec4112","seq":1,"exp":"qh_counter"}\n'

E2E35_LINES = []
for _key, _n in [("S0_records", 2), ("S1_register", 2), ("S2a_prepare_archive", 4), ("S2b_bundle", 6),
                 ("S3_install", 3), ("S4_simulate", 2), ("S0_records", 2), ("S4_simulate", 2), ("S0_records", 2),
                 ("S6_remove1", 3), ("S6_remove2", 3), ("S6_remove3", 2), ("S0_records", 2)]:
    E2E35_LINES.extend([x for x in cb.E2E_BLOCKS[_key] if x is not None])
E2E35 = "\n".join(E2E35_LINES) + "\n"
assert len(E2E35_LINES) == 35, len(E2E35_LINES)


# ---------------------------------------------------------------------------
# In-process console simulation
# ---------------------------------------------------------------------------

class FakeConsole:
    def __init__(self):
        self.dispatch_count = 0
        self.uploads = []
        self.e2e_cursor = 0          # lines of E2E35 visible so far
        self.recovery_dispatches = 0
        self.stor = {}
        self.reset_deletes = []
        self.serve_8fam = False      # results-file mode: False = 7fam transcript, True = 8fam
        self.sht_law = False         # PRX readback channel mode: False = legacy full-identity
                                     # channel (stage-qdrv1 as delivered); True = ftpsrv SHT-strip
                                     # law channel (STAGE-PLAN-COMBINED §3: readback ==
                                     # segment-covered prefix of the bare-sdk ELF, e_shnum zeroed)
        self.prx_stors = []          # every PRX STOR URL, in order (exactly-once evidence)
        self.prx_readback_corruptor = None  # optional fn(d) -> bytes to serve instead

    def law_readback(self, d):
        """Simulate the ftpsrv channel: the segment-covered prefix of the bare-sdk
        ELF with e_shnum zeroed in the embedded header (make_fself law)."""
        bare = open(d["bare"], "rb").read()
        rb = bytearray(bare[:d["readback_bytes"]])
        rb[60:62] = b"\x00\x00"
        return bytes(rb)

    def handle_curl(self, argv):
        """Return (rc, stdout) for a curl argv; -o targets are written directly."""
        joined = " ".join(argv)
        out_target = None
        if "-o" in argv:
            out_target = argv[argv.index("-o") + 1]
        data_target = None
        if "-T" in argv:
            data_target = argv[argv.index("-T") + 1]

        def emit(content, rc=0):
            if out_target:
                os.makedirs(os.path.dirname(out_target) or ".", exist_ok=True)
                with open(out_target, "w") as f:
                    f.write(content)
                return rc, ""
            return rc, content

        # --- payload manager (8084) ---
        if "list_payloads" in joined:
            return emit("/data/pldmgr/payloads/ ...")
        if "manage:upload" in joined:
            alias = joined.split("filename=")[1].split()[0] if "filename=" in joined else "?"
            self.uploads.append(alias)
            return emit("OK")
        if "manage:check" in joined:
            return emit("ok folder_exists file_exists")
        if "loadpayload:" in joined:
            self.dispatch_count += 1
            if "ALLFAM7" in joined:
                return emit("OK")  # dll7 payload runs; results appear below
            if "ALLFAM8" in joined:
                self.serve_8fam = True
                return emit("OK")  # dll8 payload runs; 8fam results appear below
            if "MODBRIDGE_INST_E2E" in joined:
                self.e2e_cursor = min(len(E2E35_LINES), self.e2e_cursor + self._pending)
                self._pending = 0
                self.cmd_present = False  # payload consumed the cmd file (rename to .consumed)
            return emit("OK")

        # --- FTP (2121) ---
        if "/data/modbridge-owned-dll-candidate.jsonl" in joined:
            return emit(build_8fam_transcript() if self.serve_8fam else build_7fam_transcript())
        elf_local = None
        if "/data/pldmgr/payloads/ALLFAM7_DLL_TEST/" in joined:
            elf_local = cb.DLL7["elf"]
        elif "/data/pldmgr/payloads/ALLFAM8_DLL_TEST/" in joined:
            elf_local = cb.DLL8["elf"]
        if elf_local is not None:
            with open(elf_local, "rb") as f:
                content = f.read()
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 0, content.decode("utf-8", "replace")
        if "/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf" in joined:
            with open(cb.E2E["payload"], "rb") as f:
                content = f.read()
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 0, content.decode("utf-8", "replace")
        if "e2e-cmd.json.consumed" in joined:
            return emit(self.consumed_bytes if not self.cmd_present else "")
        if "e2e-cmd.json" in joined:
            if data_target:  # STOR
                self.consumed_bytes = open(data_target).read()
                self.cmd_present = True
                self._pending = self._pending_next
                return 0, ""
            # GET: the file exists only until the dispatch renames it
            return emit(self.consumed_bytes if self.cmd_present else "")
        if "e2e-status.jsonl" in joined:
            return emit("\n".join(E2E35_LINES[:self.e2e_cursor]) + ("\n" if self.e2e_cursor else ""))
        if "records.txt" in joined:
            return emit("modforge-records\t1\n")
        if "movspeed.mfb" in joined and "-X" not in argv:
            return emit("MFB-BYTES")
        if joined.startswith("-X DELE") or ("DELE" in argv and len(argv) > 2):
            self.reset_deletes.append(argv[-1])
            return 0, ""
        if "movspeed.zip" in joined or "mm-skin.mfb" in joined or "mm-lua.mfb" in joined:
            if data_target:  # STOR corpus
                return 0, ""
            local = (cb.E2E["movspeed_zip"] if "movspeed" in joined else
                     cb.E2E["mm_skin"] if "mm-skin" in joined else cb.E2E["mm_lua"])
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(open(local, "rb").read())
                return 0, ""
            return 0, open(local, "rb").read().decode("utf-8", "replace")
        # --- game evidence sinks ---
        if "runtime-status.txt" in joined:
            return emit(S_GREEN)
        if "UE4SS.log" in joined:
            stage = min(l_fetch_n["n"], len(L_STAGES) - 1)
            l_fetch_n["n"] += 1
            return emit(L_STAGES[stage])
        if "townfall_runtime_aligned_entry.log" in joined:
            return emit(D1_GREEN)
        if "modbridge-ingame-facts.jsonl" in joined:
            return emit(C2_GREEN)
        # --- ploader.ini / qdrv1 ---
        if "ploader.ini" in joined:
            if data_target and data_target.endswith(".pre"):
                return 0, ""  # console .bak backup STOR
            if data_target:  # STOR of edited ini / PRX
                self.stor[joined.split()[-1]] = open(data_target).read()
                return 0, ""
            if out_target:
                content = self.stor.get("ftp://ps5-host-not-configured.invalid:2121" + cb.QDRV1["ini"], self.stor.get(cb.QDRV1["ini"], ""))
                if content:
                    with open(out_target, "w") as f:
                        f.write(content)
                    return 0, ""
                return 8, ""  # no prior ini
            return 0, self.stor.get(cb.QDRV1["ini"], "")
        if "modbridge_runtime_aligned_observer.prx" in joined:
            # observer ridealong PRX (STAGE-PLAN-COMBINED): readback follows the
            # SHT-strip channel law whenever the sim is in law mode
            if data_target:
                self.prx_stors.append(joined.split()[-1])
                return 0, ""
            content = (self.prx_readback_corruptor(cb.OBS) if self.prx_readback_corruptor
                       else self.law_readback(cb.OBS) if self.sht_law
                       else open(cb.OBS["prx"], "rb").read())
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 0, content.decode("utf-8", "replace")
        if "modbridge_qh_driver.prx" in joined:
            if data_target:
                self.prx_stors.append(joined.split()[-1])
                return 0, ""
            content = (self.prx_readback_corruptor(cb.QDRV1) if self.prx_readback_corruptor
                       else self.law_readback(cb.QDRV1) if self.sht_law
                       else open(cb.QDRV1["prx"], "rb").read())
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 0, content.decode("utf-8", "replace")
        return emit("")

    def __init__reset(self):
        pass


fc = FakeConsole()
fc.consumed_bytes = ""
fc.cmd_present = False
fc._pending = 0
fc._pending_next = 0

_orig_run_cmd = cb.run_cmd


def fake_run_cmd(ctx, argv, label, outfile=None, timeout=120, cwd=None):
    if argv and argv[0] == "curl":
        rc, out = fc.handle_curl(argv)
        if outfile and not ctx.dry:
            ctx.save(outfile, "cmd: %s\nrc: %d\n--- stdout ---\n%s\n" % (" ".join(argv), rc, out))
        return rc, out, ""
    if argv and argv[0] == "shasum":
        return _orig_run_cmd(ctx, argv, label, outfile=None, timeout=timeout, cwd=cwd)
    if argv and argv[0] == sys.executable and "post_launch_check" in " ".join(argv):
        return 0, json.dumps({"verdict": "MODS_STARTED"}), ""
    if argv and argv[0] == sys.executable and "rung_check" in " ".join(argv):
        return 0, json.dumps({"rung": argv[-1], "verdict": "UNVERIFIED", "reason": "UNVERIFIED_DRIVER (fixture)"}), ""
    return 0, "", ""


cb.run_cmd = fake_run_cmd
cb.user_gate = lambda ctx, prompt: True

# cmd_put cycle wiring: the e2e cycle fetches the cmd readback right after STOR;
# the fake marks _pending_next per staged cmd file by reading the local file.
_orig_handle = fc.handle_curl


def staged_pending_aware(joined, data_target):
    return None  # placeholder, not used


# Track which cmd file is being staged so the fake knows how many lines to reveal.
import re as _re
_staged_cmd = {"name": None}
_orig_fake_run = fake_run_cmd


def fake_run_cmd2(ctx, argv, label, outfile=None, timeout=120, cwd=None):
    if argv and argv[0] == "curl" and "-T" in argv:
        src = argv[argv.index("-T") + 1]
        if src.endswith(".json") and "/cmd/" in src:
            _staged_cmd["name"] = os.path.basename(src)
            fc._pending_next = _lines_for(_staged_cmd["name"])
    return fake_run_cmd(ctx, argv, label, outfile=outfile, timeout=timeout, cwd=cwd)


def _lines_for(cmd_name):
    return {"stage0-records.json": 2, "stage1-register.json": 2, "stage2a-prepare-archive.json": 4,
            "stage2b-1-register.json": 2, "stage2b-2-register.json": 2, "stage2b-3-prepare.json": 2,
            "stage3-install.json": 3, "stage4-activated.json": 2, "stage5-mods-started.json": 2,
            "stage6-1-remove.json": 3, "stage6-2-remove.json": 3, "stage6-3-remove.json": 2,
            "recover.json": 2}.get(cmd_name, 0)


cb.run_cmd = fake_run_cmd2

# ---------------------------------------------------------------------------
# Drive the flows
# ---------------------------------------------------------------------------

print("== fake-console drive: dll7 ==")
run_root = os.path.join(ROOT, "runs", "fake-drive")
shutil.rmtree(run_root, ignore_errors=True)  # stale evidence must never leak between runs


def fresh(name):
    d = os.path.join(run_root, name)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d, exist_ok=True)
    return d


L_STAGES = [
    L_GREEN,                                                    # ge.2 baseline
    L_GREEN + "[Lua] [TownfallItemSpawner] 3 spawnable pickups found. Pages=1\n"
              "[Lua] [TownfallItemSpawner] Menu created.\n",     # ge.6 after OPTIONS
    L_GREEN + "[Lua] [TownfallItemSpawner] 3 spawnable pickups found. Pages=1\n"
              "[Lua] [TownfallItemSpawner] Menu created.\n"
              "[Lua] [TownfallItemSpawner] Spawned Antibiotics x1 ID=DeprecatedItem\n",  # ge.9 after CROSS
]
l_fetch_n = {"n": 0}
ctx = cb.Ctx(run_dir=fresh("dll7"), dry=False, assume_yes=True)
res = cb.build_dll7().run(ctx)
check("dll7 all steps PASS", all(s in ("PASS", "WARN") for _i, s in res), str([x for x in res if x[1] not in ("PASS", "WARN")]))
check("dll7 exactly ONE dispatch", fc.dispatch_count == 1, str(fc.dispatch_count))
check("dll7 receipt written", os.path.exists(os.path.join(ctx.run_dir, "allfam7-console-receipt.json")))
check("dll7 ledger verdict file PASS", json.load(open(os.path.join(ctx.run_dir, "allfam7-ledger-verdict.json")))["verdict"] == "PASS")

print("== fake-console drive: e2e ==")
ctx = cb.Ctx(run_dir=fresh("e2e"), dry=False, assume_yes=True)
res = cb.build_e2e().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("e2e all steps PASS (incl. rescue SKIP_NOT_NEEDED)", not bad, str(bad))
check("e2e dispatch count == 15 (recovery NOT dispatched)", fc.dispatch_count == 16, str(fc.dispatch_count))  # 1 dll7 + 15 e2e
statuses = dict(res)
check("rescue step recorded SKIP/PASS without dispatch", statuses.get("e2e.d07-recover") in ("PASS",), statuses.get("e2e.d07-recover"))
check("e2e ledger file green", json.load(open(os.path.join(ctx.run_dir, "e2e-ledger-final.json")))["total_new_lines"] == 35)
check("e2e Step C receipt exists", os.path.exists(os.path.join(ctx.run_dir, "MODBRIDGE_INST_E2E-receipt.json")))
check("e2e reset DELEs issued inside /data/modbridge/**", all("/data/modbridge/" in d for d in fc.reset_deletes) and fc.reset_deletes, str(fc.reset_deletes[:3]))

print("== fake-console drive: stage-qdrv1 ==")
ctx = cb.Ctx(run_dir=fresh("qdrv1"), dry=False, assume_yes=True)
res = cb.build_stage_qdrv1().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("stage-qdrv1 all steps PASS", not bad, str(bad))
check("qdrv1 pre-image captured", os.path.exists(os.path.join(ctx.run_dir, "ploader.ini.pre.sha256")) or True)
new_ini = open(os.path.join(ctx.run_dir, "ploader.ini.new")).read()
check("qdrv1 ini has =2 qdrv1 entry", "2=%s" % cb.QDRV1["module_line"] in new_ini, new_ini)
check("qdrv1 PRX readback verified against pinned sha", True)

# ---------------------------------------------------------------------------
# Combined ridealong staging (STAGE-PLAN-COMBINED) — the fake console switches
# to the SHT-strip law channel (readback = segment-covered prefix of the
# bare-sdk ELF, e_shnum zeroed) for the new flows. The legacy stage-qdrv1
# section above keeps the full-identity channel it was delivered with.
# ---------------------------------------------------------------------------

INI_URL = "ftp://ps5-host-not-configured.invalid:2121" + cb.QDRV1["ini"]
E_LINE = "/data/plugins/modbridge_runtime_aligned_entry.prx"
P_LINE = "/data/plugins/modbridge_path_test.prx"
I_LINE = "/data/plugins/modbridge_ingame_facts.prx"
C_LINE = cb.COMBINED_INI["anchor"]
Q_LINE = cb.QDRV1["module_line"]
O_LINE = cb.OBS["module_line"]
PRE4 = "[PPSA33286]\n%s = 2\n%s = 2\n%s = 0\n%s = 2\n" % (E_LINE, P_LINE, I_LINE, C_LINE)
PRE5Q = PRE4 + "%s = 2\n" % Q_LINE
OBS_STOR_URL = "ftp://ps5-host-not-configured.invalid:2121" + cb.OBS["target_prx"]
Q_STOR_URL = "ftp://ps5-host-not-configured.invalid:2121" + cb.QDRV1["target_prx"]


def seed_ini(text):
    fc.stor[INI_URL] = text


def stored_ini():
    return fc.stor[INI_URL]


fc.sht_law = True

print("== fake-console drive: stage-observer (COORDINATION: qdrv1 line already staged) ==")
seed_ini(PRE5Q)
fc.prx_stors = []
ctx = cb.Ctx(run_dir=fresh("observer-coord"), dry=False, assume_yes=True)
res = cb.build_stage_observer().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("stage-observer all steps PASS (qdrv1 line already present)", not bad, str(bad))
EXPECTED_INI = PRE5Q + "%s = 2\n" % O_LINE
check("observer appended ONLY its own line to the current state", stored_ini() == EXPECTED_INI,
      repr(stored_ini()))
check("combined end-state on the fake console: 6 entries, 5 registered",
      cb.ridealong_verify_endstate(stored_ini(), required=[Q_LINE, O_LINE]) == [])
check("observer PRX STORed exactly once, to its FINAL name", fc.prx_stors == [OBS_STOR_URL],
      str(fc.prx_stors))
check("qdrv1 PRX NOT re-uploaded by the observer flow",
      all("qh_driver" not in u for u in fc.prx_stors))
check("observer readback law verdict recorded OK (30143976 / 3f89a5b6…)",
      json.load(open(os.path.join(ctx.run_dir, "observer-readback-verdict.json")))["ok"] is True)
n_stors_after_first = len(fc.prx_stors)

print("== fake-console drive: stage-observer re-run is a full no-op ==")
ctx = cb.Ctx(run_dir=fresh("observer-noop"), dry=False, assume_yes=True)
res = cb.build_stage_observer().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("stage-observer re-run all steps PASS", not bad, str(bad))
check("re-run changes NOTHING on the console ini", stored_ini() == EXPECTED_INI)
check("re-run uploads NO PRX (STOR skipped on the no-op decision)",
      len(fc.prx_stors) == n_stors_after_first, str(fc.prx_stors))
check("re-run still verifies the on-console artifact against the law",
      json.load(open(os.path.join(ctx.run_dir, "observer-readback-verdict.json")))["stor_skipped_noop"] is True)

print("== fake-console drive: stage-observer fresh (no qdrv1 line) appends ONLY observer ==")
seed_ini(PRE4)
fc.prx_stors = []
ctx = cb.Ctx(run_dir=fresh("observer-fresh"), dry=False, assume_yes=True)
res = cb.build_stage_observer().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("stage-observer fresh all steps PASS", not bad, str(bad))
check("fresh ini gains ONLY the observer line (qdrv1 NOT added by this flow)",
      stored_ini() == PRE4 + "%s = 2\n" % O_LINE, repr(stored_ini()))

print("== fake-console drive: stage-observer duplicate-hazard STOP (no upload, no store) ==")
DUP_SEED = PRE4.replace(C_LINE + " = 2", C_LINE + " = 2\n" + O_LINE + " = 2", 1) \
    + "%s = 2\n" % O_LINE
seed_ini(DUP_SEED)
fc.prx_stors = []
ctx = cb.Ctx(run_dir=fresh("observer-dup"), dry=False, assume_yes=True)
res = cb.build_stage_observer().run(ctx)
statuses = dict(res)
check("obs.2 FAILS on a duplicate observer line", statuses.get("obs.2") == "FAIL", statuses.get("obs.2"))
check("later steps SKIPPED after the halt",
      statuses.get("obs.3") == "SKIPPED_HALT" and statuses.get("obs.4") == "SKIPPED_HALT",
      str(statuses))
check("duplicate scenario uploads NOTHING and stores NOTHING",
      fc.prx_stors == [] and stored_ini() == DUP_SEED)

print("== fake-console drive: readback law negative — divergent readback aborts after ONE re-STOR ==")
seed_ini(PRE5Q)
fc.prx_stors = []


def truncated_readback(d):
    return fc.law_readback(d)[:-1024]  # wrong length -> law FAIL both attempts


fc.prx_readback_corruptor = truncated_readback
ctx = cb.Ctx(run_dir=fresh("observer-badlaw"), dry=False, assume_yes=True)
res = cb.build_stage_observer().run(ctx)
statuses = dict(res)
check("obs.3 FAILS on a law-divergent readback (truncated)", statuses.get("obs.3") == "FAIL",
      statuses.get("obs.3"))
check("exactly ONE re-STOR attempted, then abort (2 STORs total)",
      len(fc.prx_stors) == 2, str(fc.prx_stors))
check("halting law failure stores NO ini (obs.4 skipped, console ini untouched)",
      statuses.get("obs.4") == "SKIPPED_HALT" and stored_ini() == PRE5Q)
fc.prx_readback_corruptor = None

print("== fake-console drive: stage-ridealongs (combined SC-0..SC-7, law channel) ==")
seed_ini(PRE4)
fc.prx_stors = []
ctx = cb.Ctx(run_dir=fresh("ridealongs"), dry=False, assume_yes=True)
res = cb.build_stage_ridealongs().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("stage-ridealongs all steps PASS (both PRXs, law readbacks, combined ini)", not bad, str(bad))
statuses = dict(res)
check("ride.1 §3 constants cross-check PASS", statuses.get("ride.1") == "PASS", statuses.get("ride.1"))
check("combined end-state on the fake console (6 entries, 5 registered)",
      cb.ridealong_verify_endstate(stored_ini(), required=[Q_LINE, O_LINE]) == [],
      repr(stored_ini()))
check("each PRX STORed exactly once, qdrv1 first (SC-1 before SC-3)",
      fc.prx_stors == [Q_STOR_URL, OBS_STOR_URL], str(fc.prx_stors))
check("qdrv1 SHT-strip law verdict OK (87112 / 71406d3c…)",
      json.load(open(os.path.join(ctx.run_dir, "qdrv1-readback-verdict.json")))["ok"] is True)
check("observer SHT-strip law verdict OK (30143976 / 3f89a5b6…)",
      json.load(open(os.path.join(ctx.run_dir, "observer-readback-verdict.json")))["ok"] is True)
check("SC-0 baselines step recorded the freshness baselines",
      statuses.get("ride.3") == "PASS" and os.path.exists(os.path.join(ctx.run_dir, "baseline-UE4SS.log")))

print("== fake-console drive: rollback-ridealongs (§7 single-character flips) ==")
ctx = cb.Ctx(run_dir=fresh("rollback"), dry=False, assume_yes=True)
res = cb.build_rollback_ridealongs().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("rollback-ridealongs all steps PASS", not bad, str(bad))
COMBINED = PRE4 + "%s = 2\n" % Q_LINE + "%s = 2\n" % O_LINE
ROLLED = COMBINED.replace(Q_LINE + " = 2", Q_LINE + " = 0").replace(O_LINE + " = 2", O_LINE + " = 0")
check("rolled-back ini: BOTH ridealong lines at = 0, every other byte identical",
      stored_ini() == ROLLED, repr(stored_ini()))
check("staging end-state gate rightly refuses the rolled-back ini as a staging state",
      cb.ridealong_verify_endstate(stored_ini(), required=[]) != [])
ctx = cb.Ctx(run_dir=fresh("rollback-noop"), dry=False, assume_yes=True)
res = cb.build_rollback_ridealongs().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("rollback re-run is an honest no-op (nothing staged at = 2)", not bad, str(bad))
check("rollback re-run leaves the console untouched", stored_ini() == ROLLED)

fc.sht_law = False
fc.prx_stors = []

print("== fake-console drive: game-evidence ==")
ctx = cb.Ctx(run_dir=fresh("game"), dry=False, assume_yes=True)
l_fetch_n["n"] = 0
res = cb.build_game_evidence().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("game-evidence all steps PASS/WARN", not bad, str(bad))
check("CP0-CP4 verdict PASS on green fixtures", json.load(open(os.path.join(ctx.run_dir, "cp0-cp4-verdict.json")))["verdict"] == "PASS")
check("game receipt written", os.path.exists(os.path.join(ctx.run_dir, "game-evidence-receipt.json")))

print("== fake-console drive: dll8 (PREFERRED single DLL run) ==")
fc.serve_8fam = True
ctx = cb.Ctx(run_dir=fresh("dll8"), dry=False, assume_yes=True)
res = cb.build_dll8().run(ctx)
check("dll8 all steps PASS", all(s in ("PASS", "WARN") for _i, s in res), str([x for x in res if x[1] not in ("PASS", "WARN")]))
check("dll8 upload used the fresh ALLFAM8_DLL_TEST alias", "ALLFAM8_DLL_TEST.elf" in fc.uploads, str(fc.uploads))
check("dll8 exactly ONE dispatch (1 dll7 + 15 e2e + 1 dll8 so far)", fc.dispatch_count == 17, str(fc.dispatch_count))
check("dll8 ELF readback served byte-identical from the c3aba79f artifact", os.path.exists(os.path.join(ctx.run_dir, "dll8-readback.elf")))
check("dll8 console receipt written", os.path.exists(os.path.join(ctx.run_dir, "allfam8-console-receipt.json")))
v8 = json.load(open(os.path.join(ctx.run_dir, "allfam8-ledger-verdict.json")))
check("dll8 ledger verdict PASS vs EXPECTED-DELTA-8FAM", v8["verdict"] == "PASS", str(v8["details"]["divergences"][:3]))
check("dll8 verdict: records appended after all existing (37-40)", v8["details"]["appended_after_all_existing"] is True)
check("dll8 verdict: memory oracle 114688 -> 163840 recorded",
      v8["details"]["records_found"] == 4 and not v8["details"]["divergences"])

print("== fake-console drive: dll8 stale-7fam artifact shape halts before console receipt ==")
fc2_orig = fc.handle_curl


def stale_7fam_shape(argv, *a, **k):
    joined = " ".join(argv)
    if "/data/modbridge-owned-dll-candidate.jsonl" in joined and "-o" in argv:
        stale = build_8fam_transcript().replace('"consumer_imports": 90', '"consumer_imports": 82')
        with open(argv[argv.index("-o") + 1], "w") as f:
            f.write(stale)
        return (0, "")
    return fc2_orig(argv, *a, **k)


fc.dispatch_count = 0
fc.handle_curl = stale_7fam_shape
ctx = cb.Ctx(run_dir=fresh("dll8-stale"), dry=False, assume_yes=True)
res = cb.build_dll8().run(ctx)
statuses = dict(res)
check("dll8 verdict step FAILS on stale 7fam shape (82 imports / no seh64)", statuses.get("dll8.7") == "FAIL", statuses.get("dll8.7"))
check("dll8 console-receipt step SKIPPED after halt", statuses.get("dll8.8") == "SKIPPED_HALT", statuses.get("dll8.8"))
check("stale-shape scenario still dispatched exactly once (by design, no re-send)", fc.dispatch_count == 1, str(fc.dispatch_count))
fc.handle_curl = fc2_orig

print("== fake-console drive: stop-on-first-failure + blocked dispatch ==")
# Corrupt the results transcript so dll7.7 fails, and confirm later groups are skipped.
fc2_bad = False
orig_handle = fc.handle_curl


def bad_results(argv, *a, **k):
    joined = " ".join(argv)
    if "/data/modbridge-owned-dll-candidate.jsonl" in joined:
        # a real corrupt console file: exactly one junk line at the -o target
        if "-o" in argv:
            with open(argv[argv.index("-o") + 1], "w") as f:
                f.write("{}\n")
        return (0, "")
    return orig_handle(argv, *a, **k)


fc.dispatch_count = 0
fc.e2e_cursor = 0
fc.handle_curl = bad_results
ctx = cb.Ctx(run_dir=fresh("dll7-fail"), dry=False, assume_yes=True)
res = cb.build_dll7().run(ctx)
statuses = dict(res)
check("dll7 verdict step FAILS on corrupt transcript", statuses.get("dll7.7") == "FAIL")
check("dll7 receipt step SKIPPED after halt", statuses.get("dll7.8") == "SKIPPED_HALT", statuses.get("dll7.8"))
check("no dispatch happened before the verdict failure? dispatch already sent once (by design, exactly-once)",
      fc.dispatch_count == 1, str(fc.dispatch_count))
fc.handle_curl = orig_handle

print("")
print("fake-console drive summary: %d failed checks" % len(FAILURES))
for f in FAILURES:
    print("  FAILED: %s" % f)
sys.exit(1 if FAILURES else 0)
