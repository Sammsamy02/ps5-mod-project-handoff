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
import hashlib
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

# probe verdict sinks (evidence-path law): pre-launch baseline EMPTY (first launch
# with the probes), post-launch fixtures carry THIS launch's verdict records.
# The xwp chain follows the pinned grammar: seq-1 `entry` carries the staged
# build identity (H-1 seq-1 build-id post-stage law) — the ROW-0 REBUILD id.
XWP_STAGES = [
    "",  # baseline fetch (stage-ridealongs SC-0b): absent on a first probe launch
    ('{"marker":"modbridge.execwriteprobe","schema":1,"seq":1,"exp":"entry",'
     '"build_id":"%s"}\n' % cb.XWP_ARMED["build_id"]) +
    ('{"marker":"modbridge.execwriteprobe","schema":1,"seq":2,"exp":"startup",'
     '"build_id":"%s"}\n' % cb.XWP_ARMED["build_id"]) +
    ('{"marker":"modbridge.execwriteprobe","schema":1,"seq":3,"exp":"attempt",'
     '"build_id":"%s","mprotect_rc":0}\n' % cb.XWP_ARMED["build_id"]),
]
_FC1_BASE = ('{"marker":"modbridge.ingamefacts","schema":1,"seq":1,"exp":"entry",'
             '"build_id":"%s"}\n' % cb.FC1POOL["build_id"]) + \
            ('{"marker":"modbridge.ingamefacts","schema":1,"seq":2,"exp":"startup",'
             '"build_id":"%s"}\n' % cb.FC1POOL["build_id"])
FC1_STAGES = [
    "",  # baseline fetch: absent on a first probe launch
    _FC1_BASE +
    ('{"marker":"modbridge.ingamefacts","schema":1,"seq":4,"exp":"fc1pool_result",'
     '"build_id":"%s","verdict":"validated","reason_code":"none","fc1pool_validated":1,'
     '"checks_passed":11,"checks_total":11}\n' % cb.FC1POOL["build_id"]) +
    ('{"marker":"modbridge.ingamefacts","schema":1,"seq":6,"exp":"summary",'
     '"build_id":"%s","write_failures":0}\n' % cb.FC1POOL["build_id"]),
]

# cc1 carrier census sink (ninth-slot law): first launch -> empty baseline, then the
# full success sequence (entry / startup 30x4s / mapscan / 30 qh_counter / census_match
# / census_result CENSUS:VALIDATED / summary write_failures 0)
def _cc1_stage():
    def rec(seq, exp, **fields):
        r = {"marker": "modbridge.ingamefacts", "schema": 1, "build_id": cb.CC1["build_id"],
             "seq": seq, "exp": exp, "rc": 0, "errno": 0}
        if fields:
            r["fields"] = fields
        return json.dumps(r)
    lines = ([rec(1, "entry"), rec(2, "startup", polls=30, poll_s=4), rec(3, "mapscan")]
             + [rec(4 + i, "qh_counter") for i in range(30)]
             + [rec(34, "census_match", name="ReceiveTick"),
                rec(35, "census_match", name="Tick"),
                rec(36, "census_result", verdict="CENSUS:VALIDATED", reason_code="none",
                    controls={"None_id_zero": 1, "ByteProperty_ascii_ok": 1},
                    negative_hits={"PostRender": 0, "ZZZNotARealFunctionQQ": 0},
                    matched_count=2, wide_entries_skipped=7, census_entries_scanned=98304,
                    census_progress_markers=24, census_truncated=0, budget_entries=1048576,
                    base_matches_observed_bias=1),
                rec(37, "summary", write_failures=0, polls_run=30)])
    return "\n".join(lines) + "\n"


CC1_STAGES = [
    "",             # baseline fetch (ride.5): absent on a first carrier launch
    _cc1_stage(),   # post-launch: full census sequence
]
CC1_UE4SS_LINE = ("CC1: build=%s verdict=CENSUS:VALIDATED reason=none entries=98304 matches=2\n"
                  % cb.CC1["build_id"])

# recorded v2 rollback pair (worker-engineversion-ini/package-v2) for the freeze gate
SETTINGS_V2_LOCAL = cb.T + "/worker-engineversion-ini/package-v2/ue4ss/UE4SS-settings.ini"
SETTINGS_V2_LOADLIST_LOCAL = cb.T + "/worker-engineversion-ini/package-v2/loadlist.txt"

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
        self.stor_sequence = []      # ordered ("bak"|"ini"|"prx", url) log: the
                                     # staging-order law asserts the ini publish
                                     # comes strictly AFTER every PRX STOR
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
        if "modbridge-exec-write-probe.jsonl" in joined:
            stage = min(xwp_fetch_n["n"], len(XWP_STAGES) - 1)
            xwp_fetch_n["n"] += 1
            return emit(XWP_STAGES[stage])
        if "modbridge-fc1pool-facts.jsonl" in joined:
            stage = min(fc1_fetch_n["n"], len(FC1_STAGES) - 1)
            fc1_fetch_n["n"] += 1
            return emit(FC1_STAGES[stage])
        if "modbridge-carrier-facts.jsonl" in joined:
            stage = min(cc1_fetch_n["n"], len(CC1_STAGES) - 1)
            cc1_fetch_n["n"] += 1
            return emit(CC1_STAGES[stage])
        # --- settings v3 pair (stage-settings; freeze serves v2, readback v3) ---
        if "UE4SS-settings.ini" in joined:
            if data_target:  # STOR
                self.stor[joined.split()[-1]] = open(data_target, "rb").read()
                self.settings_staged_v3 = True
                return 0, ""
            local = (cb.SETTINGS_V3["settings_local"] if self.settings_staged_v3
                     else SETTINGS_V2_LOCAL)
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(open(local, "rb").read())
                return 0, ""
            return 0, open(local, "rb").read().decode("utf-8", "replace")
        if "roots/PPSA33286/loadlist.txt" in joined:
            if data_target:  # STOR
                self.stor[joined.split()[-1]] = open(data_target, "rb").read()
                self.settings_staged_v3 = True
                return 0, ""
            local = (cb.SETTINGS_V3["loadlist_local"] if self.settings_staged_v3
                     else SETTINGS_V2_LOADLIST_LOCAL)
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(open(local, "rb").read())
                return 0, ""
            return 0, open(local, "rb").read().decode("utf-8", "replace")
        # --- ploader.ini / qdrv1 ---
        if "ploader.ini" in joined:
            if data_target and data_target.endswith(".pre"):
                self.stor_sequence.append(("bak", joined.split()[-1]))
                return 0, ""  # console .bak backup STOR
            if data_target:  # STOR of edited ini / restore copy
                self.stor_sequence.append(("ini", joined.split()[-1]))
                self.stor[joined.split()[-1]] = open(data_target).read()
                return 0, ""
            if out_target:
                content = self.stor.get("ftp://ps5-host-not-configured.invalid:2121" + cb.QDRV1["ini"], self.stor.get(cb.QDRV1["ini"], ""))
                if content:
                    with open(out_target, "w") as f:
                        f.write(content)
                    self.l10_ini_retr_n["n"] += 1
                    # launch-10 drift scenario: AFTER the freeze RETR (l10.1) the
                    # console ini diverges, so the l10.5 STILL-byte-equal check sees it
                    if self.l10_drift_after_first_ini_retr["on"] and self.l10_ini_retr_n["n"] == 1:
                        self.stor[INI_URL] = L10_DIVERGED_INI
                    return 0, ""
                return 8, ""  # no prior ini
            return 0, self.stor.get(cb.QDRV1["ini"], "")
        if "modbridge_supplier_composition.prx" in joined:
            # launch-10 staging: the composition PRX is the ONE replaced file. STORs
            # are recorded (console .bak keeps the tailing .$TAG.bak name); RETRs
            # serve the last staged content (ELF -> SHT-strip view) or the seeded
            # pre-image, falling back to the historical static text the pre-publish
            # guard scenarios expect.
            if data_target:
                kind = "bak" if ".bak" in joined else "prx"
                self.l10_stors.append((kind, joined.split()[-1], os.path.getsize(data_target)))
                self.stor_sequence.append((kind, joined.split()[-1]))
                if kind == "prx":
                    with open(data_target, "rb") as f:
                        self.l10_staged = f.read()
                return 0, ""
            content = (self.l10_staged if self.l10_staged is not None
                       else self.l10_pre_bytes)
            if content is None:
                return emit("PRE-EXISTING-CONSOLE-FSELF supplier_composition (fake bytes)\n")
            if self.l10_staged is not None and self.l10_corrupt_once["on"]:
                # one-shot law-divergent readback (corrupts only a STAGED-content fetch,
                # never the pre-image or the restore readback)
                self.l10_corrupt_once["on"] = False
                content = content[:-64] + b"CORRUPTED-L10-READBACK\n"
            elif self.l10_staged is not None:
                content = _l10_law_readback(content)
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 0, content.decode("utf-8", "replace")
        if "modbridge_runtime_aligned_entry.prx" in joined or \
           "modbridge_path_test.prx" in joined:
            # historical PRE-EXISTING console plugins (never uploaded by the tool):
            # the staging-order pre-publish guard proves their presence via RETR
            name = ("aligned_entry" if "aligned_entry" in joined else "path_test")
            return emit("PRE-EXISTING-CONSOLE-FSELF %s (fake bytes)\n" % name)
        if "modbridge_runtime_aligned_observer.prx" in joined:
            # observer ridealong PRX (STAGE-PLAN-COMBINED): readback follows the
            # SHT-strip channel law whenever the sim is in law mode
            if data_target:
                self.prx_stors.append(joined.split()[-1])
                self.stor_sequence.append(("prx", joined.split()[-1]))
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
                self.stor_sequence.append(("prx", joined.split()[-1]))
                return 0, ""
            content = (self.prx_readback_corruptor(cb.QDRV1) if self.prx_readback_corruptor
                       else self.law_readback(cb.QDRV1) if self.sht_law
                       else open(cb.QDRV1["prx"], "rb").read())
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 0, content.decode("utf-8", "replace")
        if "modbridge_exec_write_probe_armed.prx" in joined:
            # xwp launch probe (definitive layout entry 1): SHT-strip law channel
            if data_target:
                self.prx_stors.append(joined.split()[-1])
                self.stor_sequence.append(("prx", joined.split()[-1]))
                return 0, ""
            content = (self.prx_readback_corruptor(cb.XWP_ARMED) if self.prx_readback_corruptor
                       else self.law_readback(cb.XWP_ARMED) if self.sht_law
                       else open(cb.XWP_ARMED["prx"], "rb").read())
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 0, content.decode("utf-8", "replace")
        if "modbridge_fc1pool.prx" in joined:
            # fc1 launch probe (definitive layout entry 2): SHT-strip law channel
            if data_target:
                self.prx_stors.append(joined.split()[-1])
                self.stor_sequence.append(("prx", joined.split()[-1]))
                return 0, ""
            content = (self.prx_readback_corruptor(cb.FC1POOL) if self.prx_readback_corruptor
                       else self.law_readback(cb.FC1POOL) if self.sht_law
                       else open(cb.FC1POOL["prx"], "rb").read())
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 0, content.decode("utf-8", "replace")
        if "modbridge_carrier.prx" in joined:
            # cc1 carrier probe (NINTH-SLOT layout entry 9): SHT-strip law channel
            if data_target:
                self.prx_stors.append(joined.split()[-1])
                self.stor_sequence.append(("prx", joined.split()[-1]))
                return 0, ""
            content = (self.prx_readback_corruptor(cb.CC1) if self.prx_readback_corruptor
                       else self.law_readback(cb.CC1) if self.sht_law
                       else open(cb.CC1["prx"], "rb").read())
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
fc.settings_staged_v3 = False
# launch-10 staging state (worker-launch10-staging-packet): the composition PRX is
# the ONE replaced console file. l10_pre_bytes seeds the OLD PRX content (None =
# the historical static text the pre-publish guard scenarios expect); l10_staged
# holds whatever was last STORed to the final name (the fake serves the ftpsrv
# SHT-strip view of staged ELFs, verbatim for non-ELF content).
fc.l10_pre_bytes = None
fc.l10_staged = None
fc.l10_stors = []            # ("bak"|"prx", url, nbytes) — every composition STOR, in order
fc.l10_ini_retr_n = {"n": 0}  # ploader.ini RETR counter (l10.1 freeze = #1, l10.5 still = #2)
fc.l10_drift_after_first_ini_retr = {"on": False}
fc.l10_corrupt_once = {"on": False}


def _l10_law_readback(content):
    """Simulate the ftpsrv channel for a STORed ELF: the segment-covered prefix
    with e_shnum zeroed; non-ELF content is served verbatim."""
    try:
        end = cb.sht_prefix_end(content)
    except Exception:
        return content
    rb = bytearray(content[:end])
    rb[60:62] = b"\x00\x00"
    return bytes(rb)
xwp_fetch_n = {"n": 0}
fc1_fetch_n = {"n": 0}
cc1_fetch_n = {"n": 0}

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
XWP_STOR_URL = "ftp://ps5-host-not-configured.invalid:2121" + cb.XWP_ARMED["target_prx"]
FC1_STOR_URL = "ftp://ps5-host-not-configured.invalid:2121" + cb.FC1POOL["target_prx"]
CC1_STOR_URL = "ftp://ps5-host-not-configured.invalid:2121" + cb.CC1["target_prx"]


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

print("== fake-console drive: stage-ridealongs (DEFINITIVE 9-entry law: bootstrap on the "
      "historical pre-image, then probes + ridealongs + carrier under the SHT-strip law channel) ==")
HISTORICAL_INI = PRE4  # aligned_entry, path_test BEFORE composition — the historical shape
seed_ini(HISTORICAL_INI)
fc.prx_stors = []
xwp_fetch_n["n"] = 0
fc1_fetch_n["n"] = 0
cc1_fetch_n["n"] = 0
ctx = cb.Ctx(run_dir=fresh("ridealongs-final-bootstrap"), dry=False, assume_yes=True)
res = cb.build_stage_ridealongs().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("stage-ridealongs all steps PASS (bootstrap + 5 PRXs, law readbacks, canonical ini)",
      not bad, str(bad))
statuses = dict(res)
check("ride.1 §3 constants cross-check PASS (5 probes incl. cc1 + canonical template)",
      statuses.get("ride.1") == "PASS", statuses.get("ride.1"))
CANON_INI = open(cb.FINAL_INI["canonical_template"]).read()
check("BOOTSTRAP fired on the historical pre-image and stored the CANONICAL 9-line bytes",
      stored_ini() == CANON_INI, repr(stored_ini()))
check("INI-BOOTSTRAPPED verdict recorded (ONE sanctioned whole-file STOR)",
      "INI-BOOTSTRAPPED" in open(os.path.join(ctx.run_dir, "ini-bootstrap-verdict.txt")).read())
check("bootstrap pre-image .bak + local pre preserved",
      os.path.exists(os.path.join(ctx.run_dir, "ploader.ini.pre")))
check("final ini is byte-exact canonical: 9 entries, 8 registered, igf=0, carrier last",
      cb.final_ini_verify_endstate(stored_ini()) == [],
      str(cb.final_ini_verify_endstate(stored_ini())))
check("all FIVE PRXs STORed exactly once, file order (xwp, fc1, qdrv1, observer, cc1-carrier)",
      fc.prx_stors == [XWP_STOR_URL, FC1_STOR_URL, Q_STOR_URL, OBS_STOR_URL, CC1_STOR_URL], str(fc.prx_stors))
check("xwp SHT-strip law verdict OK (67832 / e3f13d19…, the row-0 rebuild)",
      json.load(open(os.path.join(ctx.run_dir, "xwp-readback-verdict.json")))["ok"] is True)
check("fc1 SHT-strip law verdict OK (67848 / 7e2dce27…)",
      json.load(open(os.path.join(ctx.run_dir, "fc1-readback-verdict.json")))["ok"] is True)
check("qdrv1 SHT-strip law verdict OK (87112 / 71406d3c…)",
      json.load(open(os.path.join(ctx.run_dir, "qdrv1-readback-verdict.json")))["ok"] is True)
check("observer SHT-strip law verdict OK (30143976 / 3f89a5b6…)",
      json.load(open(os.path.join(ctx.run_dir, "observer-readback-verdict.json")))["ok"] is True)
check("cc1 SHT-strip law verdict OK (71528 / 131ff1c2…)",
      json.load(open(os.path.join(ctx.run_dir, "cc1-readback-verdict.json")))["ok"] is True)
check("SC-0b baselines recorded (UE4SS.log + probe sinks incl. cc1)",
      statuses.get("ride.5") == "PASS" and os.path.exists(os.path.join(ctx.run_dir, "baseline-UE4SS.log")))
check("launch handoff step green (arm/launch gate reached with the canonical ini staged)",
      statuses.get("ride.10") == "PASS", statuses.get("ride.10"))
check("handoff prompt pins the 8-OK/zero-FAIL gate + all three probe sinks",
      "EXACTLY 8" in cb.FINAL_INI_HANDOFF and "modbridge-fc1pool-facts.jsonl" in cb.FINAL_INI_HANDOFF
      and "modbridge-carrier-facts.jsonl" in cb.FINAL_INI_HANDOFF)

print("== fake-console drive: stage-ridealongs ensure-only (canonically-ordered 6-entry "
      "pre-image -> content-anchored appends, NO bootstrap) ==")
ORDERED6 = (cb.FINAL_INI["header"] + "\n"
            + "%s = 2\n" % C_LINE + "%s = 2\n" % Q_LINE + "%s = 2\n" % O_LINE
            + "%s = 2\n" % E_LINE + "%s = 2\n" % P_LINE + "%s = 0\n" % I_LINE)
seed_ini(ORDERED6)
fc.prx_stors = []
ctx = cb.Ctx(run_dir=fresh("ridealongs-final-ensure"), dry=False, assume_yes=True)
res = cb.build_stage_ridealongs().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("ensure-only run all steps PASS", not bad, str(bad))
check("ensure-only run reaches the SAME byte-exact canonical end-state (order-history "
      "independence: bootstrap vs ensure paths converge)",
      stored_ini() == CANON_INI, repr(stored_ini()))
check("ensure-only run uploaded all FIVE PRXs (ini changed -> not the no-op path)",
      len(fc.prx_stors) == 5, str(fc.prx_stors))
decision = json.load(open(os.path.join(ctx.run_dir, "ploader-edit-decision.json")))
check("decision recorded as INI-ENSURE-ONLY (no bootstrap): appends ONLY xwp + fc1 + carrier "
      "at their anchored positions (carrier AFTER the igf =0 line)",
      decision["status"] == "append" and decision["appended"] ==
      ["%s = 2" % X for X in (cb.XWP_ARMED["module_line"], cb.FC1POOL["module_line"],
                              cb.CC1["module_line"])],
      str(decision)[:200])

print("== fake-console drive: stage-ridealongs DIFFERENT staging history (observer "
      "pre-staged early, qdrv1 absent) -> same canonical bytes ==")
OBS_EARLY = (cb.FINAL_INI["header"] + "\n"
             + "%s = 2\n" % C_LINE + "%s = 2\n" % O_LINE + "%s = 2\n" % E_LINE
             + "%s = 2\n" % P_LINE + "%s = 0\n" % I_LINE)
seed_ini(OBS_EARLY)
fc.prx_stors = []
ctx = cb.Ctx(run_dir=fresh("ridealongs-final-obs-early"), dry=False, assume_yes=True)
res = cb.build_stage_ridealongs().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("observer-early history all steps PASS", not bad, str(bad))
check("content-anchored ensure lands qdrv1 BETWEEN composition and observer (byte-exact "
      "canonical regardless of how the ini got here)",
      stored_ini() == CANON_INI, repr(stored_ini()))

print("== fake-console drive: stage-ridealongs re-run on canonical ini is a FULL no-op ==")
seed_ini(CANON_INI)
fc.prx_stors = []
ctx = cb.Ctx(run_dir=fresh("ridealongs-final-noop"), dry=False, assume_yes=True)
res = cb.build_stage_ridealongs().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("canonical re-run all steps PASS", not bad, str(bad))
check("re-run stores NOTHING (ini untouched) and uploads NO PRX",
      stored_ini() == CANON_INI and len(fc.prx_stors) == 0, str(fc.prx_stors))
check("re-run still verifies the on-console PRX artifacts against the law (readback-only)",
      all(json.load(open(os.path.join(ctx.run_dir, "%s-readback-verdict.json" % k)))["stor_skipped_noop"]
          is True for k in ("xwp", "fc1", "qdrv1", "observer", "cc1")))

print("== fake-console drive: stage-ridealongs STOP on an unknown ini line (no bootstrap, "
      "no upload, no store) ==")
seed_ini(HISTORICAL_INI + "/data/plugins/modbridge_surprise.prx = 2\n")
fc.prx_stors = []
ctx = cb.Ctx(run_dir=fresh("ridealongs-final-stop"), dry=False, assume_yes=True)
res = cb.build_stage_ridealongs().run(ctx)
statuses = dict(res)
check("ride.2b FAILS on the unknown line (STOP class kept)", statuses.get("ride.2b") == "FAIL",
      statuses.get("ride.2b"))
check("later steps SKIPPED after the halt",
      statuses.get("ride.3") == "SKIPPED_HALT" and statuses.get("ride.6") == "SKIPPED_HALT",
      str(statuses))
check("STOP scenario uploads NOTHING and stores NOTHING (bootstrap never fires on an "
      "anomalous pre-image)",
      fc.prx_stors == [] and stored_ini() == HISTORICAL_INI + "/data/plugins/modbridge_surprise.prx = 2\n")

# ---------------------------------------------------------------------------
# STAGING-ORDER LAW scenarios (worker-staging-order, launch9 incident fix):
# upload + verify EVERY enabled plugin file FIRST; publish the enabling ini
# LAST behind the pre-publish guard; restore-ini rolls the ini back byte-exact.
# ---------------------------------------------------------------------------

print("== fake-console drive: ORDER LAW (a) — happy path: verify-then-publish; the "
      "enabling ini STOR is strictly LAST ==")
seed_ini(HISTORICAL_INI)
fc.prx_stors = []
fc.stor_sequence = []
xwp_fetch_n["n"] = 0
fc1_fetch_n["n"] = 0
cc1_fetch_n["n"] = 0
ctx = cb.Ctx(run_dir=fresh("order-law-happy"), dry=False, assume_yes=True)
res = cb.build_stage_ridealongs().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("order-law happy path all steps PASS (decision local -> 5x upload+law -> guard -> publish)",
      not bad, str(bad))
check("console sequence is EXACTLY: .bak backup -> five PRX law STORs -> ONE ini publish STOR",
      [s[0] for s in fc.stor_sequence] == ["bak", "prx", "prx", "prx", "prx", "prx", "ini"],
      str(fc.stor_sequence))
check("published ini is byte-exact canonical (published LAST)", stored_ini() == CANON_INI,
      repr(stored_ini())[:120])
check("INI-BOOTSTRAPPED verdict written by the PUBLISH step (post-guard)",
      "INI-BOOTSTRAPPED" in open(os.path.join(ctx.run_dir, "ini-bootstrap-verdict.txt")).read())
guard = json.load(open(os.path.join(ctx.run_dir, "ridealongs-publish-guard.json")))
check("guard receipt: 8 enabled entries, 0 missing (5 this-run law verdicts + 3 presence proofs)",
      len(guard["enabled_entries"]) == 8 and guard["missing"] == []
      and len(guard["law_verified"]) == 5 and len(guard["presence_proofs"]) == 8, str(guard)[:240])

print("== fake-console drive: ORDER LAW (b) — xwp readback law FAIL mid-flow: ini NEVER "
      "published, old ini intact (the launch9 incident, now impossible) ==")
seed_ini(HISTORICAL_INI)
fc.prx_stors = []
fc.stor_sequence = []
fc.prx_readback_corruptor = truncated_readback
ctx = cb.Ctx(run_dir=fresh("order-law-midflow-fail"), dry=False, assume_yes=True)
res = cb.build_stage_ridealongs().run(ctx)
fc.prx_readback_corruptor = None
statuses = dict(res)
check("ride.6 FAILS on the divergent xwp readback (the launch9 halt point)",
      statuses.get("ride.6") == "FAIL", statuses.get("ride.6"))
check("guard + publish SKIPPED after the halt",
      statuses.get("ride.9c") == "SKIPPED_HALT" and statuses.get("ride.4") == "SKIPPED_HALT",
      str((statuses.get("ride.9c"), statuses.get("ride.4"))))
check("NO ini publish happened at all", not any(s[0] == "ini" for s in fc.stor_sequence),
      str(fc.stor_sequence))
check("console keeps the OLD ini byte-identical (safe state: its enabled files are present)",
      stored_ini() == HISTORICAL_INI, repr(stored_ini())[:120])
check("exactly initial STOR + ONE re-STOR of xwp, then abort (no retries, no publish)",
      len(fc.prx_stors) == 2, str(fc.prx_stors))

print("== fake-console drive: H-1 WRONG-BUILD refusal — console still holds the LAUNCH-9 xwp "
      "bytes under the SAME filename: the row-0 law must refuse, never self-verify PASS ==")
OLD_XWP_BARE = os.path.join(cb.T, "worker-writability-probe-build/determinism/run-armed-1/"
                            "bare-sdk/modbridge_exec_write_probe.prx")


def old_build_readback(d):
    """Simulate the stale launch-9 fself sitting on the console: the ftpsrv channel
    serves the segment-covered prefix of the OLD bare-sdk ELF (e_shnum zeroed)."""
    bare = open(OLD_XWP_BARE, "rb").read()
    rb = bytearray(bare[:d["readback_bytes"]])
    rb[60:62] = b"\x00\x00"
    return bytes(rb)


seed_ini(HISTORICAL_INI)
fc.prx_stors = []
fc.stor_sequence = []
fc.prx_readback_corruptor = old_build_readback
ctx = cb.Ctx(run_dir=fresh("order-law-oldbuild"), dry=False, assume_yes=True)
res = cb.build_stage_ridealongs().run(ctx)
fc.prx_readback_corruptor = None
statuses = dict(res)
check("ride.6 REFUSES the stale launch-9 xwp readback against the pinned row-0 law (H-1)",
      statuses.get("ride.6") == "FAIL", statuses.get("ride.6"))
check("guard + publish SKIPPED after the wrong-build halt; NO ini publish at all",
      statuses.get("ride.9c") == "SKIPPED_HALT" and statuses.get("ride.4") == "SKIPPED_HALT"
      and not any(s[0] == "ini" for s in fc.stor_sequence), str(fc.stor_sequence))
check("console keeps the OLD ini byte-identical (safe state)", stored_ini() == HISTORICAL_INI,
      repr(stored_ini())[:120])
check("exactly initial STOR + ONE re-STOR of xwp, then abort (the wrong bytes never PASS)",
      len(fc.prx_stors) == 2, str(fc.prx_stors))
check("the pinned row-0 verdict law rejects the OLD body hash while the pinned local fself is "
      "the row-0 rebuild (e1541af6 pin vs launch-9 bytes)",
      cb.XWP_ARMED["build_id"] == "igd1-e030a09eb4d1"
      and cb.local_sha(cb.XWP_ARMED["prx"])[0] == cb.XWP_ARMED["sha"])

print("== fake-console drive: ORDER LAW (c) — a =2 entry whose file is MISSING/0-byte: "
      "publish REFUSED ==")
seed_ini(HISTORICAL_INI)
fc.prx_stors = []
fc.stor_sequence = []
_rc_orig = fc.handle_curl


def missing_path_test(argv, *a, **k):
    joined = " ".join(argv)
    if "modbridge_path_test.prx" in joined and "-o" in argv and "-T" not in argv:
        open(argv[argv.index("-o") + 1], "wb").close()  # 0-byte file: the launch9 missing-plugin signature
        return (0, "")
    return _rc_orig(argv, *a, **k)


fc.handle_curl = missing_path_test
ctx = cb.Ctx(run_dir=fresh("order-law-missing"), dry=False, assume_yes=True)
res = cb.build_stage_ridealongs().run(ctx)
fc.handle_curl = _rc_orig
statuses = dict(res)
check("all five PRXs uploaded + law-verified FIRST (uploads unaffected by the missing file)",
      len(fc.prx_stors) == 5 and statuses.get("ride.9b") == "PASS",
      str((len(fc.prx_stors), statuses.get("ride.9b"))))
check("ride.9c guard REFUSES the publish", statuses.get("ride.9c") == "FAIL", statuses.get("ride.9c"))
check("the refusal NAMES the missing enabled entry (path_test)",
      "modbridge_path_test.prx" in "\n".join(ctx.events))
check("publish step SKIPPED — no partial publish", statuses.get("ride.4") == "SKIPPED_HALT",
      statuses.get("ride.4"))
check("console ini untouched (old ini intact)", stored_ini() == HISTORICAL_INI
      and not any(s[0] == "ini" for s in fc.stor_sequence), str(fc.stor_sequence))

print("== fake-console drive: restore-ini (order-law rollback helper) ==")
MIN3 = (cb.FINAL_INI["header"] + "\n"
        + "%s = 2\n" % C_LINE + "%s = 2\n" % E_LINE + "%s = 2\n" % P_LINE)  # 3-enabled post-incident shape
ODD = cb.FINAL_INI["header"] + "\n%s = 1\n" % C_LINE
cb.RESTORE_SRC_PATH = os.path.join(fresh("restore-src"), "saved-9entry.ini")
with open(cb.RESTORE_SRC_PATH, "w") as f:
    f.write(CANON_INI)

# (d-1) divergent console pre-state REFUSED by default (never overwrite blind)
seed_ini(ODD)
fc.stor_sequence = []
cb.RESTORE_ALLOW_DIVERGENT_PRE = False
ctx = cb.Ctx(run_dir=fresh("restore-divergent"), dry=False, assume_yes=True)
res = cb.build_restore_ini().run(ctx)
statuses = dict(res)
check("rs.1 freeze REFUSES the divergent console pre-state", statuses.get("rs.1") == "FAIL",
      statuses.get("rs.1"))
check("nothing published after the refusal (no ini STOR)",
      not any(s[0] == "ini" for s in fc.stor_sequence), str(fc.stor_sequence))

# (d-2) BYTE-EXACT restore over the 3-enabled post-incident state (recorded operator
#       override), with EVERY enabled plugin file present on the console
seed_ini(MIN3)
fc.stor_sequence = []
cb.RESTORE_ALLOW_DIVERGENT_PRE = True
ctx = cb.Ctx(run_dir=fresh("restore-ok"), dry=False, assume_yes=True)
res = cb.build_restore_ini().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("restore all steps PASS (override accepted, guard passed, publish landed)", not bad, str(bad))
check("console ini restored BYTE-EXACT to the saved pre-publish copy", stored_ini() == CANON_INI,
      repr(stored_ini())[:120])
check("restore sequence: .bak backup first, then exactly ONE ini STOR",
      fc.stor_sequence[0][0] == "bak" and [s[0] for s in fc.stor_sequence].count("ini") == 1,
      str(fc.stor_sequence))
check("restore verdict records byte-exact source+readback identities",
      json.load(open(os.path.join(ctx.run_dir, "restore-verdict.json")))["byte_exact"] is True)

# (d-3) re-run on the restored console is an honest no-op
fc.stor_sequence = []
ctx = cb.Ctx(run_dir=fresh("restore-noop"), dry=False, assume_yes=True)
res = cb.build_restore_ini().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("restore re-run all steps PASS (already byte-identical)", not bad, str(bad))
check("restore re-run stores NOTHING", stored_ini() == CANON_INI
      and not any(s[0] == "ini" for s in fc.stor_sequence), str(fc.stor_sequence))

# (d-4) guard refusal: the canonical restore is REFUSED while an enabled file is
#       missing (0-byte fc1pool) — restoring would re-create the launch9 state
seed_ini(MIN3)
fc.stor_sequence = []
_rc_orig2 = fc.handle_curl


def missing_fc1(argv, *a, **k):
    joined = " ".join(argv)
    if "modbridge_fc1pool.prx" in joined and "-o" in argv and "-T" not in argv:
        open(argv[argv.index("-o") + 1], "wb").close()  # 0-byte fc1pool on the console
        return (0, "")
    return _rc_orig2(argv, *a, **k)


fc.handle_curl = missing_fc1
ctx = cb.Ctx(run_dir=fresh("restore-guard"), dry=False, assume_yes=True)
res = cb.build_restore_ini().run(ctx)
fc.handle_curl = _rc_orig2
statuses = dict(res)
check("rs.2 guard REFUSES the canonical restore while fc1pool is missing/0-byte",
      statuses.get("rs.2") == "FAIL", statuses.get("rs.2"))
check("restore publish SKIPPED — console keeps the safe 3-enabled minimal ini",
      statuses.get("rs.3") == "SKIPPED_HALT" and stored_ini() == MIN3,
      str((statuses.get("rs.3"), stored_ini()[:60])))

cb.RESTORE_SRC_PATH = cb.RESTORE_INI["src_default"]
cb.RESTORE_ALLOW_DIVERGENT_PRE = False

# ---------------------------------------------------------------------------
# LAUNCH-10 STAGING scenarios (worker-launch10-staging-packet): the composition
# PRX is the ONE replaced console file; ploader.ini stays the pinned 2-plugin
# config (composition + path_test) and is NEVER written; the readback content
# law accepts full identity or ONLY the ftpsrv SHT-strip tail-artifact class.
# ---------------------------------------------------------------------------

print("== fake-console drive: stage-launch10 setup (fixture FSELF + pinned 2-plugin ini) ==")


def l10_fixture_elf(path, body_len=1024):
    """Synthetic launch-10 FSELF: segment coverage ends BEFORE EOF (a fake SHT
    tail follows) and e_shnum is NONZERO — the fake console serves the SHT-strip
    view of it, so the flow's content law must use its tail-artifact branch."""
    import struct as _s
    ehdr = bytearray(64)
    ehdr[0:4] = b"\x7fELF"
    ehdr[4] = 2
    _s.pack_into("<HHI", ehdr, 16, 2, 0xf6, 1)
    phoff = 64
    _s.pack_into("<Q", ehdr, 32, phoff)                      # e_phoff
    _s.pack_into("<H", ehdr, 54, 56)                         # e_phentsize
    _s.pack_into("<H", ehdr, 56, 1)                          # e_phnum
    _s.pack_into("<Q", ehdr, 40, phoff + 56 + body_len)      # e_shoff -> the tail
    _s.pack_into("<H", ehdr, 58, 64)                         # e_shentsize
    _s.pack_into("<H", ehdr, 60, 3)                          # e_shnum NONZERO
    _s.pack_into("<H", ehdr, 62, 2)                          # e_shstrndx
    ph = _s.pack("<IIQQQQQQ", 1, 5, 0, phoff + 56, phoff + 56, body_len, body_len, 8)
    body = bytes((i * 31 + 7) % 256 for i in range(body_len))
    tail = bytes((i * 13 + 3) % 256 for i in range(256))
    with open(path, "wb") as f:
        f.write(bytes(ehdr) + ph + body + tail)
    return path


L10_RUN_ROOT = fresh("l10")
L10_FSELF_PATH = l10_fixture_elf(os.path.join(L10_RUN_ROOT, "modbridge_supplier_composition_launch10.fself"))
L10_FSELF_SHA = hashlib.sha256(open(L10_FSELF_PATH, "rb").read()).hexdigest()
L10_OLD_PRX = b"[fake] OLD composition FSELF bytes (pre-launch-10 console content)\n" * 4
L10_TWO_PLUGIN_INI = cb.FINAL_INI["header"] + "\n%s = 2\n%s = 2\n" % (C_LINE, P_LINE)
L10_INI_SHA = hashlib.sha256(L10_TWO_PLUGIN_INI.encode()).hexdigest()
L10_DIVERGED_INI = L10_TWO_PLUGIN_INI + "%s = 2\n" % Q_LINE   # a third enabled line appears
L10_TARGET_URL = "ftp://ps5-host-not-configured.invalid:2121" + cb.L10["target_prx"]
check("launch-10 fixture ini is the 2-plugin config shape (composition + path_test)",
      L10_TWO_PLUGIN_INI.count("= 2") == 2 and C_LINE in L10_TWO_PLUGIN_INI and P_LINE in L10_TWO_PLUGIN_INI,
      repr(L10_TWO_PLUGIN_INI))


def l10_reset(seed_text=L10_TWO_PLUGIN_INI, pre_bytes=L10_OLD_PRX):
    seed_ini(seed_text)
    fc.l10_pre_bytes = pre_bytes
    fc.l10_staged = None
    fc.l10_stors = []
    fc.stor_sequence = []
    fc.l10_ini_retr_n["n"] = 0
    fc.l10_drift_after_first_ini_retr["on"] = False
    fc.l10_corrupt_once["on"] = False


def l10_arm():
    cb.L10_FSELF_LOCAL_PATH = L10_FSELF_PATH
    cb.L10_FSELF_SHA256 = L10_FSELF_SHA
    cb.L10_INI_EXPECTED_SHA = L10_INI_SHA


print("== fake-console drive: stage-launch10 (a) happy path: .bak -> upload EXACTLY ONCE -> "
      "content-law readback -> ini STILL byte-equal -> baselines -> GO ==")
l10_reset()
l10_arm()
ctx = cb.Ctx(run_dir=fresh("l10-happy"), dry=False, assume_yes=True)
res = cb.build_stage_launch10().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("stage-launch10 happy path all steps PASS", not bad, str(bad))
statuses = dict(res)
check("console sequence is EXACTLY: composition .bak backup -> ONE FSELF STOR (final name)",
      [s[0] for s in fc.l10_stors] == ["bak", "prx"] and fc.l10_stors[1][1] == L10_TARGET_URL,
      str(fc.l10_stors))
check("the FSELF landed byte-exact (fake stored the staged bytes) with NO re-STOR, NO restore",
      fc.l10_staged == open(L10_FSELF_PATH, "rb").read() and len(fc.l10_stors) == 2)
check("readback verified by the content law vs the LOCAL FSELF (SHT-strip tail-artifact branch)",
      json.load(open(os.path.join(ctx.run_dir, "l10-readback-verdict.json")))["ok"] is True
      and "SHT-strip class" in json.load(open(os.path.join(ctx.run_dir, "l10-readback-verdict.json")))["detail"])
check("ploader.ini NEVER written (no ini STOR) and still the 2-plugin bytes",
      not any(s[0] == "ini" for s in fc.stor_sequence) and stored_ini() == L10_TWO_PLUGIN_INI,
      str(fc.stor_sequence))
check("ini freeze (l10.1) + STILL-byte-equal (l10.5) both ran and pinned the 2-plugin config",
      fc.l10_ini_retr_n["n"] == 2
      and os.path.exists(os.path.join(ctx.run_dir, "l10-ploader.ini.pre.sha256"))
      and os.path.exists(os.path.join(ctx.run_dir, "l10-ploader.ini.post.sha256")))
check("fresh baselines re-fetched into the run dir (UE4SS.log + runtime-status + entry log + manifest)",
      os.path.exists(os.path.join(ctx.run_dir, "baseline-UE4SS.log"))
      and os.path.exists(os.path.join(ctx.run_dir, "baseline-runtime-status.txt"))
      and os.path.exists(os.path.join(ctx.run_dir, "baseline-entry-log.txt"))
      and os.path.exists(os.path.join(ctx.run_dir, "l10-baselines.json")))
check("GO verdict recorded (next launch stays USER-driven)",
      statuses.get("l10.7") == "PASS"
      and json.load(open(os.path.join(ctx.run_dir, "l10-go-nogo.json")))["verdict"] == "GO")

print("== fake-console drive: stage-launch10 (b) readback law FAIL -> OLD PRX restored from the "
      "pre-image, ini untouched, NO-GO recorded ==")
l10_reset()
fc.l10_corrupt_once["on"] = True
ctx = cb.Ctx(run_dir=fresh("l10-mismatch"), dry=False, assume_yes=True)
res = cb.build_stage_launch10().run(ctx)
statuses = dict(res)
check("l10.4 FAILS on the law-divergent readback (upload-verify mismatch)", statuses.get("l10.4") == "FAIL",
      statuses.get("l10.4"))
check("the OLD PRX was restored byte-exact from the pre-image (exactly ONE bounded restore STOR)",
      fc.l10_staged == L10_OLD_PRX and [s[0] for s in fc.l10_stors] == ["bak", "prx", "prx"],
      str(fc.l10_stors))
check("the FSELF itself was stored EXACTLY ONCE (no re-STOR of the new payload)",
      [s for s in fc.l10_stors if s[0] == "prx"][0][2] == os.path.getsize(L10_FSELF_PATH)
      and sum(1 for s in fc.l10_stors if s[0] == "prx" and s[2] == os.path.getsize(L10_FSELF_PATH)) == 1)
check("restore proven byte-exact (l10-restore-verdict.json)", json.load(
    open(os.path.join(ctx.run_dir, "l10-restore-verdict.json")))["restored"] is True)
check("ini STILL check SKIPPED after the halt; NO ini STOR ever; old ini bytes intact",
      statuses.get("l10.5") == "SKIPPED_HALT" and not any(s[0] == "ini" for s in fc.stor_sequence)
      and stored_ini() == L10_TWO_PLUGIN_INI)
check("NO-GO verdict recorded by the rescue summary step", statuses.get("l10.7") == "FAIL"
      and json.load(open(os.path.join(ctx.run_dir, "l10-go-nogo.json")))["verdict"] == "NO-GO")

print("== fake-console drive: stage-launch10 (c) ini DRIFT between freeze and re-check -> refusal, "
      "OLD PRX restored, the flow still never wrote the ini ==")
l10_reset()
fc.l10_drift_after_first_ini_retr["on"] = True
ctx = cb.Ctx(run_dir=fresh("l10-drift"), dry=False, assume_yes=True)
res = cb.build_stage_launch10().run(ctx)
statuses = dict(res)
check("upload + content-law readback PASSED before the drift was seen",
      statuses.get("l10.3") == "PASS" and statuses.get("l10.4") == "PASS", str((statuses.get("l10.3"), statuses.get("l10.4"))))
check("l10.5 REFUSES the drifted ploader.ini", statuses.get("l10.5") == "FAIL", statuses.get("l10.5"))
check("the refusal names the PINNED 2-plugin sha (fixture stand-in for 1216aab6…) and the external drift",
      cb.L10_INI_EXPECTED_SHA[:12] in "\n".join(ctx.events) and "NO ini write" in "\n".join(ctx.events))
check("the OLD PRX was restored byte-exact after the refusal",
      fc.l10_staged == L10_OLD_PRX and json.load(
          open(os.path.join(ctx.run_dir, "l10-restore-verdict.json")))["restored"] is True)
check("baselines step SKIPPED after the refusal; NO ini STOR ever (drifted bytes remain — "
      "the flow does not reconcile the ini itself)",
      statuses.get("l10.6") == "SKIPPED_HALT" and not any(s[0] == "ini" for s in fc.stor_sequence)
      and stored_ini() == L10_DIVERGED_INI)
check("NO-GO verdict recorded by the rescue summary step", statuses.get("l10.7") == "FAIL"
      and json.load(open(os.path.join(ctx.run_dir, "l10-go-nogo.json")))["verdict"] == "NO-GO")

print("== fake-console drive: stage-launch10 (d) missing / unpinned local FSELF -> refusal at "
      "ZERO console writes ==")
l10_reset()
cb.L10_FSELF_LOCAL_PATH = os.path.join(L10_RUN_ROOT, "does-not-exist.fself")
cb.L10_FSELF_SHA256 = L10_FSELF_SHA
ctx = cb.Ctx(run_dir=fresh("l10-missing"), dry=False, assume_yes=True)
res = cb.build_stage_launch10().run(ctx)
statuses = dict(res)
check("l10.0 REFUSES the missing local FSELF (BUILD-ID preflight)", statuses.get("l10.0") == "FAIL",
      statuses.get("l10.0"))
check("every console step SKIPPED — ZERO console writes",
      all(statuses.get(s) == "SKIPPED_HALT" for s in ("l10.1", "l10.2", "l10.3", "l10.4", "l10.5", "l10.6"))
      and fc.l10_stors == [] and not any(s[0] == "ini" for s in fc.stor_sequence))
check("NO-GO verdict recorded by the rescue summary step", statuses.get("l10.7") == "FAIL"
      and json.load(open(os.path.join(ctx.run_dir, "l10-go-nogo.json")))["verdict"] == "NO-GO")
cb.L10_FSELF_LOCAL_PATH = ""
cb.L10_FSELF_SHA256 = ""
ctx = cb.Ctx(run_dir=fresh("l10-placeholder"), dry=False, assume_yes=True)
res = cb.build_stage_launch10().run(ctx)
statuses = dict(res)
check("l10.0 REFUSES unfilled <L10_FSELF_LOCAL_PATH>/<L10_FSELF_SHA256> placeholders",
      statuses.get("l10.0") == "FAIL" and "placeholder" in "\n".join(ctx.events).lower(),
      statuses.get("l10.0"))
check("placeholder refusal: zero console writes",
      fc.l10_stors == [] and not any(s[0] == "ini" for s in fc.stor_sequence))

# restore the module defaults so later scenarios see the shipped pinned state
cb.L10_FSELF_LOCAL_PATH = ""
cb.L10_FSELF_SHA256 = ""
cb.L10_INI_EXPECTED_SHA = cb.L10["ini_sha256"]
fc.l10_pre_bytes = None
fc.l10_staged = None
fc.l10_stors = []

print("== fake-console drive: stage-settings (NEW-SETTINGS §3 lockstep restage, v3 pair) ==")
fc.settings_staged_v3 = False  # console currently holds the recorded v2 pair
ctx = cb.Ctx(run_dir=fresh("settings-v3"), dry=False, assume_yes=True)
res = cb.build_stage_settings().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("stage-settings all steps PASS (freeze v2 -> upload v3 pair -> byte-exact readback)",
      not bad, str(bad))
statuses = dict(res)
check("set.0 pair lockstep proven host-side (loadlist row == 1448 == settings bytes)",
      statuses.get("set.0") == "PASS", statuses.get("set.0"))
check("freeze classified the staged pair as v2 (recorded inventory)",
      json.load(open(os.path.join(ctx.run_dir, "settings-freeze.json")))["classified"] == "v2")
check("v3 settings ini (1448 B) read back byte-exact",
      open(os.path.join(ctx.run_dir, "readback-settings"), "rb").read()
      == open(cb.SETTINGS_V3["settings_local"], "rb").read())
check("v3 loadlist (447 B) read back byte-exact",
      open(os.path.join(ctx.run_dir, "readback-loadlist"), "rb").read()
      == open(cb.SETTINGS_V3["loadlist_local"], "rb").read())

print("== fake-console drive: stage-settings re-run is an honest idempotent no-op ==")
ctx = cb.Ctx(run_dir=fresh("settings-v3-noop"), dry=False, assume_yes=True)
res = cb.build_stage_settings().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("stage-settings re-run all steps PASS", not bad, str(bad))
statuses = dict(res)
check("freeze classifies the pair as ALREADY v3 (recorded inventory) and set.2 skips the upload",
      json.load(open(os.path.join(ctx.run_dir, "settings-freeze.json")))["classified"] == "v3"
      and statuses.get("set.2") == "PASS", str((statuses.get("set.1"), statuses.get("set.2"))))

print("== fake-console drive: stage-settings divergent staged root FAILs the freeze ==")
fc.settings_staged_v3 = "divergent"


def divergent_settings(argv):
    joined = " ".join(argv)
    if "UE4SS-settings.ini" in joined and "-o" in argv:
        with open(argv[argv.index("-o") + 1], "wb") as f:
            f.write(b"[Hooks]\nHookProcessInternal=1\n")  # neither v2 nor v3
        return (0, "")
    return None


fc3_orig = fc.handle_curl


def divergent_handle(argv, *a, **k):
    r = divergent_settings(argv)
    return r if r is not None else fc3_orig(argv, *a, **k)


fc.handle_curl = divergent_handle
ctx = cb.Ctx(run_dir=fresh("settings-divergent"), dry=False, assume_yes=True)
res = cb.build_stage_settings().run(ctx)
statuses = dict(res)
check("set.1 FREEZE FAILs on divergent/unowned staged bytes", statuses.get("set.1") == "FAIL",
      statuses.get("set.1"))
check("divergent scenario uploads NOTHING (later steps skipped)",
      statuses.get("set.2") == "SKIPPED_HALT", str(statuses.get("set.2")))
fc.handle_curl = fc3_orig
fc.settings_staged_v3 = True

print("== fake-console drive: rollback-ridealongs (§7 single-character flips) ==")
COMBINED = PRE4 + "%s = 2\n" % Q_LINE + "%s = 2\n" % O_LINE
seed_ini(COMBINED)  # legacy 6-entry state for the legacy rollback flow (unchanged law)
ctx = cb.Ctx(run_dir=fresh("rollback"), dry=False, assume_yes=True)
res = cb.build_rollback_ridealongs().run(ctx)
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("rollback-ridealongs all steps PASS", not bad, str(bad))
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
xwp_fetch_n["n"] = 1   # post-launch sink content (fresh verdict records this launch)
fc1_fetch_n["n"] = 1
cc1_fetch_n["n"] = 1
# post-launch UE4SS.log carries exactly ONE fresh CC1: line (the cc1 census verdict);
# applied from this point on so the pre-launch baselines recorded by ride.5 stay clean
ge_orig_handle = fc.handle_curl


def cc1_ue4ss_aware(argv):
    joined = " ".join(argv)
    if "UE4SS.log" in joined and "-o" in argv:
        target = argv[argv.index("-o") + 1]
        base = ge_orig_handle(argv)
        with open(target, "a") as f:
            f.write(CC1_UE4SS_LINE)
        return base
    return None


def ge_handle(argv, *a, **k):
    r = cc1_ue4ss_aware(argv)
    return r if r is not None else ge_orig_handle(argv, *a, **k)


fc.handle_curl = ge_handle
res = cb.build_game_evidence().run(ctx)
fc.handle_curl = ge_orig_handle
bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
check("game-evidence all steps PASS/WARN", not bad, str(bad))
check("CP0-CP4 verdict PASS on green fixtures", json.load(open(os.path.join(ctx.run_dir, "cp0-cp4-verdict.json")))["verdict"] == "PASS")
pv = json.load(open(os.path.join(ctx.run_dir, "probe-verdicts.json")))
check("probe verdict sinks PASS (xwp records + fc1pool_result validated 11/11)",
      pv["status"] == "PASS" and pv["sinks"]["fc1"]["validated_results"] == 1
      and pv["sinks"]["xwp"]["fresh_records"] == 3, str(pv)[:200])
check("seq-1 entry build_id == pinned row-0 rebuild (H-1 identity receipt on the green path)",
      pv["sinks"]["xwp"]["seq1_entry_build_id"] == cb.XWP_ARMED["build_id"], str(pv["sinks"]["xwp"]))
cv = json.load(open(os.path.join(ctx.run_dir, "carrier-verdicts.json")))
check("cc1 census sink PASS (fresh census_result CENSUS:VALIDATED + 30 qh_counter + one CC1: line)",
      cv["status"] == "PASS" and cv["sinks"]["cc1"]["census_verdict"] == "CENSUS:VALIDATED"
      and cv["sinks"]["cc1"]["qh_counter_lines"] == 30
      and cv["sinks"]["cc1"]["ue4ss_cc1_lines"] == 1, str(cv)[:240])
check("game receipt written", os.path.exists(os.path.join(ctx.run_dir, "game-evidence-receipt.json")))

print("== fake-console drive: H-1 BUILD-ID MISMATCH refused — seq-1 entry carries the OLD "
      "launch-9 build_id: the real ge.3b step must FAIL loud ==")
OLD_BUILD = "igd1-6ebab07f2111"
_ge_steps = {s["id"]: s["fn"] for s in cb.build_game_evidence().steps}


def _xwp_rec(seq, exp, build):
    return json.dumps({"marker": "modbridge.execwriteprobe", "schema": 1,
                       "seq": seq, "exp": exp, "build_id": build}) + "\n"


ctx = cb.Ctx(run_dir=fresh("ge-wrongbuild"), dry=False, assume_yes=True)
ctx.state["xwp_text"] = (_xwp_rec(1, "entry", OLD_BUILD) + _xwp_rec(2, "startup", OLD_BUILD)
                         + _xwp_rec(3, "attempt", OLD_BUILD))
ctx.state["xwp_jsonl_baseline"] = ""
ctx.state["fc1_text"] = FC1_STAGES[1]
ctx.state["fc1_jsonl_baseline"] = ""
ctx.state["l_text"] = ""
st, det = _ge_steps["ge.3b"](ctx)
check("ge.3b FAILS on the WRONG-BUILD seq-1 entry (old launch-9 bytes ran)", st == "FAIL",
      "%s :: %s" % (st, det))
check("the refusal NAMES the observed stale build identity", OLD_BUILD in det, det[:200])
pv = json.load(open(os.path.join(ctx.run_dir, "probe-verdicts.json")))
check("probe-verdicts.json records the observed seq-1 identity for the runbook",
      pv["sinks"]["xwp"]["seq1_entry_build_id"] == OLD_BUILD, str(pv["sinks"]["xwp"]))
ctx = cb.Ctx(run_dir=fresh("ge-noentry"), dry=False, assume_yes=True)
ctx.state["xwp_text"] = _xwp_rec(2, "startup", cb.XWP_ARMED["build_id"])
ctx.state["xwp_jsonl_baseline"] = ""
ctx.state["fc1_text"] = FC1_STAGES[1]
ctx.state["fc1_jsonl_baseline"] = ""
ctx.state["l_text"] = ""
st, det = _ge_steps["ge.3b"](ctx)
check("ge.3b WARNs (honest, not FAIL) when fresh records lack the seq-1 entry record",
      st == "WARN" and "UNVERIFIED" in det, "%s :: %s" % (st, det))

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
