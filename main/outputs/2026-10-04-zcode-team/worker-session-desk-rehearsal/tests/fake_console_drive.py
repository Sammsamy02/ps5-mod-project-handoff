#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fake_console_drive.py — FULL-SESSION desk rehearsal (worker-session-desk-rehearsal
private copy; the original lives in worker-console-batch-runner/tests/ and was NOT
edited). Drives the REAL console_batch.py execution engine against a simulated
console. NO network traffic, NO console contact, NO user interaction: curl
invocations and USER gates are intercepted in-process and answered from canned
fixtures (the exact bytes/grammars the plans pin).

Extends the original tool-coverage drive to the RETURN-SESSION shape:

  Phase 0  sanity: Track-8 ploader.ini pre-image fetch + sha (divergence branch
           exercised in the branch walk)
  Phase 1  comp6 one-pass staging with a MOCK build outcome: local stand-in PRX
           (expected name), recorded sha+bytes, simulated verify-only/gate-v2 GO,
           stage upload + readback identity (comp6_one_pass.py itself is NEVER
           executed — it stays a pre-flight skeleton until the initializer lands)
  Phase 2  dll8 (8fam ledger, verdict vs EXPECTED-DELTA-8FAM) -> e2e 15-dispatch
           stage 0..6 cycle -> stage-ridealongs (SC-0..SC-7, registered count 5)
           -> settings v3 restage (freeze-check + lockstep readback of the REAL
           package-v3 bytes) -> IGW-1 exec-write probe staging (REAL armed PRX,
           live on-disk identity) -> fc1 validator staging (MOCK stand-in; no PRX
           is built yet) -> final combined ini registered count == 7
  Phase 3  CP fixtures REGENERATED at the corrected tick 0x33ac8c0 (the
           propagation worker's consequence note: committed s01 fixtures still
           carry the retracted 0x37ac8c0); stale-vs-corrected comparator; real
           post_launch_check.py / rung_check.py --from-dir; full game-evidence
           flow on tick-corrected canned L fixtures
  Branch   CONTROLLER-DECISION-BRIEF walk: each failure shape injected at its
           phase (dll8 import mismatch, e2e NOT CONSUMED, ini drift, CP2 stale
           tick, CP4 lua error, CP7 rung stale, + CP5 shape-a restage proof and
           the Phase-0 pre-image divergence) and the brief's decision +
           next-command verified against what the fake drive actually does.

Run: python3 tests/fake_console_drive.py   (exit 0 = all green)
Receipts: ../receipts/REHEARSAL-RECEIPTS.json + ../receipts/SESSION-GO-NO-GO.json
"""

import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MY = os.path.dirname(HERE)                       # .../worker-session-desk-rehearsal
TEAM = os.path.dirname(MY)                       # .../2026-10-04-zcode-team
TOOL_DIR = os.path.join(TEAM, "worker-console-batch-runner")   # theirs, read-only
SCRATCH = os.path.join(MY, "runs")
RECEIPTS_DIR = os.path.join(MY, "receipts")

sys.path.insert(0, TOOL_DIR)
spec = importlib.util.spec_from_file_location(
    "console_batch", os.path.join(TOOL_DIR, "console_batch.py"))
cb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cb)

FAILURES = []
RECEIPT = {"worker": "worker-session-desk-rehearsal", "phases": {}, "branches": {}, "constants": {}}


def check(name, cond, detail=""):
    print(("  ok   " if cond else "  FAIL ") + name + (("  :: " + detail) if (detail and not cond) else ""))
    if not cond:
        FAILURES.append(name)
    return cond


def sha256_file(p):
    with open(p, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def sha256_bytes(b):
    return hashlib.sha256(b).hexdigest()


# ---------------------------------------------------------------------------
# Canned console content (original drive builders, reused verbatim)
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
    prior = [{"marker": "modbridge.owned-dll", "schema": 1, "build_id": "133c5e9e59f24c2fb57e7e3422c665a1",
              "sequence": i % 4, "phase": [1, 2, 3, 5][i % 4]} for i in range(36)]
    return "\n".join(json.dumps(x) for x in prior + [r0, r1, r2, r3]) + "\n"


def build_8fam_transcript():
    """Canned EXPECTED-DELTA-8FAM PASS shape: 36 prior records, then the 4 new
    c3aba79f records appended at positions 37-40 (dll8-first order). Oracle:
    consumer_imports 90, helper 10, seh64 {enabled:1} from record1,
    memory.last.length 114688 (seq2) -> 163840 (seq3 FINAL)."""
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


def _lines_for(cmd_name):
    return {"stage0-records.json": 2, "stage1-register.json": 2, "stage2a-prepare-archive.json": 4,
            "stage2b-1-register.json": 2, "stage2b-2-register.json": 2, "stage2b-3-prepare.json": 2,
            "stage3-install.json": 3, "stage4-activated.json": 2, "stage5-mods-started.json": 2,
            "stage6-1-remove.json": 3, "stage6-2-remove.json": 3, "stage6-3-remove.json": 2,
            "recover.json": 2}.get(cmd_name, 0)


# Session-scoped ploader.ini end-state (STAGE-PLAN-COMBINED §1 combined layout +
# the ridealong appends this session stages AFTER the tool's combined staging):
TRACK8_INI = ("[PPSA33286]\n"
              "/data/plugins/modbridge_runtime_aligned_entry.prx = 2\n"
              "/data/plugins/modbridge_path_test.prx = 2\n"
              "/data/plugins/modbridge_ingame_facts.prx = 0\n"
              "/data/plugins/modbridge_supplier_composition.prx = 2\n")
SETTINGS_INI_URL = cb.FTP + "/data/modbridge/roots/PPSA33286/ue4ss/UE4SS-settings.ini"
LOADLIST_URL = cb.FTP + "/data/modbridge/roots/PPSA33286/loadlist.txt"
IGW_TARGET = "/data/plugins/modbridge_exec_write_probe_armed.prx"
FC1_TARGET = "/data/plugins/modbridge_fc1_validator.prx"   # placeholder name (no PRX built yet)
COMPOSITION_TARGET = "/data/plugins/modbridge_supplier_composition.prx"

# Session L staging (tick-corrected: built from cb.GAME["tick_line"], which the
# propagation worker's fix already pinned to 0x33ac8c0 in console_batch.py)
L_STAGES = [
    L_GREEN,                                                    # ge.2 baseline
    L_GREEN + "[Lua] [TownfallItemSpawner] 3 spawnable pickups found. Pages=1\n"
              "[Lua] [TownfallItemSpawner] Menu created.\n",     # ge.6 after OPTIONS
    L_GREEN + "[Lua] [TownfallItemSpawner] 3 spawnable pickups found. Pages=1\n"
              "[Lua] [TownfallItemSpawner] Menu created.\n"
              "[Lua] [TownfallItemSpawner] Spawned Antibiotics x1 ID=DeprecatedItem\n",  # ge.9 after CROSS
]
l_fetch_n = {"n": 0}


# ---------------------------------------------------------------------------
# In-process console simulation (original FakeConsole, session-extended)
# ---------------------------------------------------------------------------

class FakeConsole:
    def __init__(self, seed_state=True, ini_preimage=TRACK8_INI,
                 settings_live_pair=(b"LIVE-SETTINGS-778", b"LIVE-LOADLIST-445")):
        self.dispatch_count = 0
        self.uploads = []
        self.e2e_cursor = 0
        self.stor = {}                       # generic STOR registry: url -> bytes(text)
        self.reset_deletes = []
        self.serve_8fam = False
        self.sht_law = False                 # PRX readback channel: False=legacy full-identity,
                                             # True=ftpsrv SHT-strip law channel
        self.prx_stors = []
        self.prx_readback_corruptor = None
        self.ledger_corruptor = None         # fn(transcript_text) -> text (branch injections)
        self.consume_failure = False         # e2e NOT CONSUMED injection
        # seeded "live" console state (Phase 0 / settings freeze-check baseline);
        # core-regression scenarios run UNSEEDED (the legacy qdrv1 create-path
        # expects a console with no prior ploader.ini, exactly like the original drive)
        if seed_state:
            self.stor[cb.FTP + cb.COMBINED_INI["ini"]] = ini_preimage
            self.settings_live = {"ini": settings_live_pair[0], "loadlist": settings_live_pair[1]}
            self.stor[SETTINGS_INI_URL] = self.settings_live["ini"]
            self.stor[LOADLIST_URL] = self.settings_live["loadlist"]
        else:
            self.settings_live = {"ini": b"", "loadlist": b""}
        self.ini_stors = []                  # every ploader.ini STOR (ordering evidence)

    def law_readback(self, d):
        """Simulate the ftpsrv channel: the segment-covered prefix of the bare-sdk
        ELF with e_shnum zeroed in the embedded header (make_fself law)."""
        bare = open(d["bare"], "rb").read()
        rb = bytearray(bare[:d["readback_bytes"]])
        rb[60:62] = b"\x00\x00"
        return bytes(rb)

    def handle_curl(self, argv):
        joined = " ".join(argv)
        out_target = argv[argv.index("-o") + 1] if "-o" in argv else None
        data_target = argv[argv.index("-T") + 1] if "-T" in argv else None

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
            if "ALLFAM8" in joined:
                self.serve_8fam = True
            if "MODBRIDGE_INST_E2E" in joined:
                self.e2e_cursor = min(len(E2E35_LINES), self.e2e_cursor + self._pending)
                self._pending = 0
                if not self.consume_failure:
                    self.cmd_present = False  # payload consumed the cmd file
            return emit("OK")

        # --- owned-dll results transcript ---
        if "/data/modbridge-owned-dll-candidate.jsonl" in joined:
            text = build_8fam_transcript() if self.serve_8fam else build_7fam_transcript()
            if self.ledger_corruptor:
                text = self.ledger_corruptor(text)
            return emit(text)

        # --- payload ELF readbacks ---
        elf_local = None
        if "/data/pldmgr/payloads/ALLFAM7_DLL_TEST/" in joined:
            elf_local = cb.DLL7["elf"]
        elif "/data/pldmgr/payloads/ALLFAM8_DLL_TEST/" in joined:
            elf_local = cb.DLL8["elf"]
        elif "/data/pldmgr/payloads/MODBRIDGE_INST_E2E/MODBRIDGE_INST_E2E.elf" in joined:
            elf_local = cb.E2E["payload"]
        if elf_local is not None:
            with open(elf_local, "rb") as f:
                content = f.read()
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 0, content.decode("utf-8", "replace")

        # --- e2e cmd lifecycle ---
        if "e2e-cmd.json.consumed" in joined:
            # the .consumed twin exists only AFTER the dispatch renamed the cmd file;
            # consume_failure keeps cmd_present True (twin never appears -> NOT CONSUMED)
            return emit(self.consumed_bytes if not self.cmd_present else "")
        if "e2e-cmd.json" in joined:
            if data_target:
                self.consumed_bytes = open(data_target).read()
                self.cmd_present = True
                self._pending = self._pending_next
                return 0, ""
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
            if data_target:
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

        # --- settings v3 restage paths (freeze-check pair + STOR/readback) ---
        if "UE4SS-settings.ini" in joined or joined.rstrip().endswith("/loadlist.txt"):
            url = next((t for t in reversed(argv) if t.startswith("ftp://")), joined.split()[-1])
            if data_target:
                with open(data_target, "rb") as f:
                    self.stor[url] = f.read()
                return 0, ""
            content = self.stor.get(url, b"")
            if out_target:
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 0, content.decode("utf-8", "replace")

        # --- ploader.ini / qdrv1 ---
        if "ploader.ini" in joined:
            url = joined.split()[-1] if (data_target or out_target) else cb.FTP + cb.COMBINED_INI["ini"]
            if data_target:
                with open(data_target, "rb") as f:
                    self.stor[url] = f.read().decode("utf-8", "replace")
                if url.endswith(cb.COMBINED_INI["ini"]):
                    self.ini_stors.append(url)
                return 0, ""
            content = self.stor.get(cb.FTP + cb.COMBINED_INI["ini"], "")
            if out_target:
                if content:
                    with open(out_target, "w") as f:
                        f.write(content)
                    return 0, ""
                return 8, ""  # no prior ini
            return 0, content

        # --- observer / qdrv1 PRX law channels (STAGE-PLAN-COMBINED §3) ---
        if "modbridge_runtime_aligned_observer.prx" in joined:
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

        # --- generic registry: composition / IGW / fc1 PRXs, anything else ---
        url = next((t for t in reversed(argv) if t.startswith("ftp://")), joined.split()[-1])
        if data_target:
            with open(data_target, "rb") as f:
                self.stor[url] = f.read()
            return 0, ""
        content = self.stor.get(url, b"")
        if out_target:
            if content:
                with open(out_target, "wb") as f:
                    f.write(content)
                return 0, ""
            return 8, ""
        return emit(content.decode("utf-8", "replace"))


_ORIG_RUN_CMD = [cb.run_cmd]   # captured ONCE, before any patching


def install_patches(fc):
    """Patch console_batch.run_cmd / user_gate against ONE FakeConsole instance."""
    fc.consumed_bytes = ""
    fc.cmd_present = False
    fc._pending = 0
    fc._pending_next = 0
    _orig_run_cmd = _ORIG_RUN_CMD[0]

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
            return 0, json.dumps({"rung": argv[-1], "verdict": "UNVERIFIED",
                                  "reason": "UNVERIFIED_DRIVER (fixture)"}), ""
        return 0, "", ""

    def fake_run_cmd2(ctx, argv, label, outfile=None, timeout=120, cwd=None):
        # cmd_put cycle wiring: reveal per-cmd ledger lines on the NEXT dispatch
        if argv and argv[0] == "curl" and "-T" in argv:
            src = argv[argv.index("-T") + 1]
            if src.endswith(".json") and "/cmd/" in src:
                fc._pending_next = _lines_for(os.path.basename(src))
        return fake_run_cmd(ctx, argv, label, outfile=outfile, timeout=timeout, cwd=cwd)

    cb.run_cmd = fake_run_cmd2
    cb.user_gate = lambda ctx, prompt: True


def fresh(name):
    d = os.path.join(SCRATCH, name)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d, exist_ok=True)
    return d


def run_flow(builder, run_dir_name):
    ctx = cb.Ctx(run_dir=fresh(run_dir_name), dry=False, assume_yes=True)
    res = builder().run(ctx)
    return ctx, res


def registered_count(ini_text):
    entries, header_count, fatal = cb._parse_combined_ini(ini_text)
    assert fatal is None, fatal
    assert header_count == 1
    return sum(1 for _p, m, _r in entries if m != 0), entries


# ---------------------------------------------------------------------------
# PART 1 — core tool regression (original drive scenarios, MY run dirs)
# ---------------------------------------------------------------------------

def core_regression():
    print("== core: original drive scenarios (regression of the tool engine) ==")
    fc = FakeConsole(seed_state=False)   # unseeded: legacy qdrv1 create-path expects no prior ini
    install_patches(fc)

    ctx, res = run_flow(cb.build_dll7, "core/dll7")
    check("core dll7 all steps PASS", all(s in ("PASS", "WARN") for _i, s in res))
    check("core dll7 verdict PASS", json.load(open(os.path.join(ctx.run_dir, "allfam7-ledger-verdict.json")))["verdict"] == "PASS")

    ctx, res = run_flow(cb.build_e2e, "core/e2e")
    bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
    check("core e2e all steps PASS (rescue SKIP_NOT_NEEDED included)", not bad, str(bad))
    check("core e2e dispatch count == 15 (after dll7's 1)", fc.dispatch_count == 16, str(fc.dispatch_count))
    check("core e2e ledger 35 lines", json.load(open(os.path.join(ctx.run_dir, "e2e-ledger-final.json")))["total_new_lines"] == 35)

    ctx, res = run_flow(cb.build_stage_qdrv1, "core/qdrv1")
    bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
    check("core stage-qdrv1 all steps PASS", not bad, str(bad))

    ctx, res = run_flow(cb.build_game_evidence, "core/game")
    bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
    check("core game-evidence all steps PASS/WARN", not bad, str(bad))
    check("core CP0-CP4 PASS on green fixtures",
          json.load(open(os.path.join(ctx.run_dir, "cp0-cp4-verdict.json")))["verdict"] == "PASS")

    fc.serve_8fam = True
    ctx, res = run_flow(cb.build_dll8, "core/dll8")
    check("core dll8 all steps PASS", all(s in ("PASS", "WARN") for _i, s in res))
    v8 = json.load(open(os.path.join(ctx.run_dir, "allfam8-ledger-verdict.json")))
    check("core dll8 verdict PASS vs EXPECTED-DELTA-8FAM", v8["verdict"] == "PASS")

    # stale-7fam shape halts before the console receipt (original drive check)
    orig_handle = fc.handle_curl

    def stale_7fam_shape(argv, *a, **k):
        joined = " ".join(argv)
        if "/data/modbridge-owned-dll-candidate.jsonl" in joined and "-o" in argv:
            stale = build_8fam_transcript().replace('"consumer_imports": 90', '"consumer_imports": 82')
            with open(argv[argv.index("-o") + 1], "w") as f:
                f.write(stale)
            return (0, "")
        return orig_handle(argv, *a, **k)

    fc.dispatch_count = 0
    fc.handle_curl = stale_7fam_shape
    ctx, res = run_flow(cb.build_dll8, "core/dll8-stale")
    statuses = dict(res)
    check("core dll8 stale-7fam shape FAILS verdict", statuses.get("dll8.7") == "FAIL", statuses.get("dll8.7"))
    check("core dll8 receipt SKIPPED after halt", statuses.get("dll8.8") == "SKIPPED_HALT", statuses.get("dll8.8"))
    check("core stale-shape scenario dispatched exactly once (no re-send)", fc.dispatch_count == 1)
    fc.handle_curl = orig_handle
    RECEIPT["phases"]["core-regression"] = {"verdict": "ran", "failures": [f for f in FAILURES if f.startswith("core")]}


# ---------------------------------------------------------------------------
# PART 2 — the FULL SESSION (one continuous fake console, Phase 0 -> 3)
# ---------------------------------------------------------------------------

IGW_LOCAL = os.path.join(TEAM, "worker-writability-probe-build", "modbridge_exec_write_probe_armed.prx")
IGW_BUILD_RECEIPT = os.path.join(TEAM, "worker-writability-probe-build", "build-receipt-armed.json")
SETTINGS_V3_DIR = os.path.join(TEAM, "worker-settings-v3-package", "package-v3")
SETTINGS_V3_INI = os.path.join(SETTINGS_V3_DIR, "ue4ss", "UE4SS-settings.ini")
SETTINGS_V3_LOADLIST = os.path.join(SETTINGS_V3_DIR, "loadlist.txt")
FIXTURE_GENERATOR = os.path.join(TEAM, "worker-evidence-pipeline-rehearsal", "build_fixtures.py")
COMMITTED_FIXTURES = os.path.join(TEAM, "worker-evidence-pipeline-rehearsal", "fixtures", "s01_full_success")
POST_LAUNCH = os.path.join(TEAM, "worker-post-launch-tools", "post_launch_check.py")
RUNG_CHECK = os.path.join(TEAM, "worker-rung-evidence-reader", "rung_check.py")
SIM_INI = os.path.join(TEAM, "worker-game-session-ridealongs", "simulate_ploader_ini.py")
COMP6_ONE_PASS = os.path.join(TEAM, "..", "2026-10-04-zcode-controller", "comp6_one_pass.py")
COMP6_ONE_PASS = os.path.normpath(COMP6_ONE_PASS)

WRONG_TICK = "GameEngine::Tick address (vtable: 0x37ac8c0; scan: 0x37ac8c0)"


def manual_append_ini(fc, wanted_lines, label):
    """The brief's MANUAL ridealong appends (STAGE-PLAN §3–4 style): plan from the
    live pre-image with the tool's own append-only law, then STOR + readback."""
    rc, pre = fc.handle_curl(["curl", "-sS", cb.FTP + cb.COMBINED_INI["ini"]])
    status, new_text, appended = cb.ridealong_ini_plan(pre, wanted_lines)
    if status != "append":
        return False, "%s: plan status %s (%s)" % (label, status, new_text[:120])
    problems = cb.ridealong_ini_append_only(pre, new_text, appended)
    if problems:
        return False, "%s: append-only law violated: %s" % (label, problems)
    open(os.path.join(SCRATCH, "session", "%s.ini.new" % label), "w").write(new_text)
    fc.handle_curl(["curl", "-sS", "-T", os.path.join(SCRATCH, "session", "%s.ini.new" % label),
                    cb.FTP + cb.COMBINED_INI["ini"]])
    rc, rb = fc.handle_curl(["curl", "-sS", cb.FTP + cb.COMBINED_INI["ini"]])
    if rb != new_text:
        return False, "%s: readback != edited bytes" % label
    return True, "%s: appended %s" % (label, appended)


def stor_and_readback_prx(fc, local_path, target, label):
    """Manual PRX stage: STOR once to its final name + readback identity."""
    rc, _ = fc.handle_curl(["curl", "-sS", "-T", local_path, cb.FTP + target])
    if rc != 0:
        return False, "%s STOR rc=%d" % (label, rc)
    rc, rb = fc.handle_curl(["curl", "-sS", cb.FTP + target, "-o",
                             os.path.join(SCRATCH, "session", "%s.readback.prx" % label)])
    if rc != 0:
        return False, "%s readback rc=%d" % (label, rc)
    rb_bytes = open(os.path.join(SCRATCH, "session", "%s.readback.prx" % label), "rb").read()
    if rb_bytes != open(local_path, "rb").read():
        return False, "%s readback != local bytes" % label
    return True, "%s staged + readback identity OK (%d B)" % (label, len(rb_bytes))


def phase0(fc):
    print("== PHASE 0 — sanity (game CLOSED; Track-8 pre-image) ==")
    p = os.path.join(SCRATCH, "session", "pl.now")
    rc, _ = fc.handle_curl(["curl", "-sS", cb.FTP + "/data/plugins/ploader.ini", "-o", p])
    got = sha256_file(p)
    want = sha256_bytes(fc.stor[cb.FTP + cb.COMBINED_INI["ini"]].encode())
    check("phase0 pre-image sha matches the seeded Track-8 state", got == want)
    entries, hdr, fatal = cb._parse_combined_ini(open(p).read())
    check("phase0 pre-image parses: 1 header + 4 pinned lines",
          hdr == 1 and fatal is None and len(entries) == 4 and
          all(pw == cw and pm == cm for (pw, pm), (cw, cm) in
              zip(((e[0], e[1]) for e in entries), cb.COMBINED_INI["expected_existing"].items())),
          str(entries))
    n_reg, _ = registered_count(open(p).read())
    check("phase0 Track-8 registered count == 3 (entry+path_test+composition; igf mode-0 skipped; "
          "ridealongs not staged yet)", n_reg == 3, str(n_reg))
    RECEIPT["phases"]["phase0"] = {"pre_image_sha12": got[:12], "registered": n_reg, "verdict": "ADVANCE"}


def phase1_mock_comp6(fc):
    print("== PHASE 1 — comp6 one-pass staging (MOCK build outcome) ==")
    d = fresh("session-phase1-comp6")
    # stand-in candidate: expected FINAL name, recorded identity (MOCK — the real
    # comp6 build waits on the initializer; comp6_one_pass.py is NEVER executed here)
    standin = os.path.join(d, "prx-candidate-composition6-1", "modbridge_supplier_composition.prx")
    os.makedirs(os.path.dirname(standin), exist_ok=True)
    with open(standin, "wb") as f:
        f.write(b"MOCK-COMP6-SUPPLIER-COMPOSITION-PRX-REHEARSAL-STANDIN\n" * 64)
    csha, cbytes = sha256_file(standin), os.path.getsize(standin)
    state = {
        "mock": True,
        "note": ("comp6_one_pass.py NOT executed (pre-flight skeleton until the initializer "
                 "lands); this rehearses the OUTCOME: build OK -> gate v2 GO -> staged readback"),
        "verify_only": "MOCK-OK (fixed-script sha / ODR equality / receipt coverage / 14-14 chain symbols)",
        "gate_v2": "MOCK-GO",
        "candidate": {"name": os.path.basename(standin), "sha256": csha, "bytes": cbytes},
        "tool_on_disk_sha12": sha256_file(COMP6_ONE_PASS)[:12] if os.path.exists(COMP6_ONE_PASS) else None,
        "staged_to": COMPOSITION_TARGET,
    }
    json.dump(state, open(os.path.join(d, "comp6-one-pass-state.json"), "w"), indent=2)
    ok, detail = stor_and_readback_prx(fc, standin, COMPOSITION_TARGET, "comp6-composition")
    check("phase1 MOCK candidate staged + readback identity", ok, detail)
    # the staged composition replaced the expected-existing line's FILE, not the ini line
    rc, pre = fc.handle_curl(["curl", "-sS", cb.FTP + cb.COMBINED_INI["ini"]])
    check("phase1 ini untouched by the PRX stage (line still pinned mode 2)",
          "/data/plugins/modbridge_supplier_composition.prx = 2" in pre)
    check("phase1 no JOB FAILED marker in the mock outcome (launch path stays lawful)",
          "JOB FAILED" not in json.dumps(state))
    RECEIPT["phases"]["phase1"] = {"mock": True, "candidate_sha12": csha[:12], "bytes": cbytes,
                                   "staged": ok, "verdict": "GATE->PHASE 2 (gate GO + staged readback)"}


def phase2(fc):
    print("== PHASE 2 — game-closed console batch ==")
    # --- 2.1 dll8 (PREFERRED single DLL run; 8fam ledger) ---
    print("-- 2.1 dll8 --")
    fc.serve_8fam = True
    ctx, res = run_flow(cb.build_dll8, "session/phase2-dll8")
    bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
    check("2.1 dll8 all steps PASS", not bad, str(bad))
    check("2.1 dll8 uploaded ALLFAM8_DLL_TEST.elf exactly once", fc.uploads.count("ALLFAM8_DLL_TEST.elf") == 1)
    v8 = json.load(open(os.path.join(ctx.run_dir, "allfam8-ledger-verdict.json")))
    check("2.1 dll8 verdict PASS vs EXPECTED-DELTA-8FAM (records 37-40, memory 114688->163840)",
          v8["verdict"] == "PASS" and v8["details"]["appended_after_all_existing"] is True)

    # --- 2.2 e2e 15-dispatch cycle ---
    print("-- 2.2 e2e --")
    before = fc.dispatch_count
    ctx, res = run_flow(cb.build_e2e, "session/phase2-e2e")
    bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
    check("2.2 e2e all steps PASS", not bad, str(bad))
    check("2.2 e2e dispatched exactly 15 times", fc.dispatch_count - before == 15, str(fc.dispatch_count - before))
    check("2.2 e2e ledger verbatim (35 lines)", json.load(open(os.path.join(ctx.run_dir, "e2e-ledger-final.json")))["total_new_lines"] == 35)
    check("2.2 e2e §7 reset DELEs stayed inside /data/modbridge/**",
          fc.reset_deletes and all("/data/modbridge/" in d for d in fc.reset_deletes))

    # --- 2.3 stage-ridealongs (SC-0..SC-7; qdrv1 + observer; registered == 5) ---
    print("-- 2.3 stage-ridealongs --")
    fc.sht_law = True  # ftpsrv readback channel per STAGE-PLAN-COMBINED §3
    ctx, res = run_flow(cb.build_stage_ridealongs, "session/phase2-ridealongs")
    bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
    check("2.3 stage-ridealongs all steps PASS (SHT-strip law channel)", not bad, str(bad))
    rc, ini_now = fc.handle_curl(["curl", "-sS", cb.FTP + cb.COMBINED_INI["ini"]])
    n_reg, _entries = registered_count(ini_now)
    check("2.3 combined ini registered count == exactly 5", n_reg == 5, str(n_reg))
    for verdict_file, label in (("qdrv1-readback-verdict.json", "qdrv1"), ("observer-readback-verdict.json", "observer")):
        p = os.path.join(ctx.run_dir, verdict_file)
        check("2.3 %s SHT-strip readback law PASS" % label,
              os.path.exists(p) and json.load(open(p))["ok"] is True)

    # --- 2.4 settings v3 restage (manual; NEW-SETTINGS §3 lockstep law) ---
    print("-- 2.4 settings v3 restage --")
    rc, live_ini = fc.handle_curl(["curl", "-sS", SETTINGS_INI_URL])
    rc, live_ll = fc.handle_curl(["curl", "-sS", LOADLIST_URL])
    if isinstance(live_ini, str):
        live_ini, live_ll = live_ini.encode(), live_ll.encode()
    frozen = (sha256_bytes(live_ini), len(live_ini), sha256_bytes(live_ll), len(live_ll))
    check("2.4 freeze-check: live pair matches the seeded pre-restage identity",
          frozen == (sha256_bytes(fc.settings_live["ini"]), len(fc.settings_live["ini"]),
                     sha256_bytes(fc.settings_live["loadlist"]), len(fc.settings_live["loadlist"])))
    v3_ini_b = open(SETTINGS_V3_INI, "rb").read()
    v3_ll_b = open(SETTINGS_V3_LOADLIST, "rb").read()
    v3_consts = {"ini": {"bytes": len(v3_ini_b), "sha256": sha256_bytes(v3_ini_b)},
                 "loadlist": {"bytes": len(v3_ll_b), "sha256": sha256_bytes(v3_ll_b)}}
    RECEIPT["constants"]["settings_v3"] = v3_consts
    check("2.4 package-v3 constants verified live (ini 1448 B d8faa4ec…, loadlist 447 B 4d98ecd7…)",
          v3_consts["ini"] == {"bytes": 1448, "sha256": "d8faa4eca2ed2f22c02693b6841b7939412b4dd60c218a26438f985152d67038"}
          and v3_consts["loadlist"] == {"bytes": 447, "sha256": "4d98ecd7aba1777028cca50552ef9d9616ecb070a0eb8772b8f3e40a590f5d5d"},
          json.dumps(v3_consts))
    for local, url in ((SETTINGS_V3_INI, SETTINGS_INI_URL), (SETTINGS_V3_LOADLIST, LOADLIST_URL)):
        fc.handle_curl(["curl", "-sS", "-T", local, url])
    rb_ini = fc.stor[SETTINGS_INI_URL]
    rb_ll = fc.stor[LOADLIST_URL]
    if isinstance(rb_ini, str):
        rb_ini, rb_ll = rb_ini.encode(), rb_ll.encode()
    check("2.4 v3 readback byte-exact both files (lockstep law: never launch on partial upload)",
          rb_ini == v3_ini_b and rb_ll == v3_ll_b)
    txt = rb_ini.decode()
    check("2.4 v3 ini carries BOTH A-PI keys at 0 (HookProcessInternal/HookProcessLocalScriptFunction)",
          "HookProcessInternal" in txt and "HookProcessLocalScriptFunction" in txt)
    RECEIPT["phases"]["phase2-settings-v3"] = {"freeze_ok": True, "readback_exact": True,
                                               "rollback": "v2 pair 834 B 9570ead3… + 446 B 3afd10ce…"}

    # --- 2.5 IGW-1 exec-write probe staging (REAL armed artifact, live identity) ---
    print("-- 2.5 IGW probe staging --")
    igw_b = open(IGW_LOCAL, "rb").read()
    igw_sha, igw_bytes = sha256_bytes(igw_b), len(igw_b)
    br = json.load(open(IGW_BUILD_RECEIPT))
    check("2.5 IGW armed artifact matches its build receipt (determinism pair equal)",
          br["determinism"]["final_prx"]["equal"] is True
          and br["run1"]["final_sha256"] == igw_sha and br["run1"]["final_bytes"] == igw_bytes,
          "%s %d" % (igw_sha[:12], igw_bytes))
    RECEIPT["constants"]["igw_armed"] = {"bytes": igw_bytes, "sha256": igw_sha, "build_id": br["build_id"],
                                         "note": "REPORT pin 19277 B/7b9e030c… is STALE (pre-injection); "
                                                 "live on-disk identity is authoritative"}
    ok, detail = stor_and_readback_prx(fc, IGW_LOCAL, IGW_TARGET, "igw-armed")
    check("2.5 IGW armed PRX staged + readback identity", ok, detail)
    ok, detail = manual_append_ini(fc, [IGW_TARGET], "igw")
    check("2.5 IGW =2 line appended (append-only law)", ok, detail)
    rc, ini_now = fc.handle_curl(["curl", "-sS", cb.FTP + cb.COMBINED_INI["ini"]])
    n_reg, _ = registered_count(ini_now)
    check("2.5 registered count now 6", n_reg == 6, str(n_reg))

    # --- 2.6 fc1 validator staging (MOCK stand-in: no PRX built yet) ---
    print("-- 2.6 fc1 validator staging (MOCK) --")
    d = fresh("session-phase2-fc1")
    fc1 = os.path.join(d, "modbridge_fc1_validator.prx")
    with open(fc1, "wb") as f:
        f.write(b"MOCK-FC1-VALIDATOR-PROX-STANDIN (no built artifact in worker-fc1-validator-probe)\n" * 32)
    fsha, fbytes = sha256_file(fc1), os.path.getsize(fc1)
    ok, detail = stor_and_readback_prx(fc, fc1, FC1_TARGET, "fc1")
    check("2.6 fc1 MOCK PRX staged + readback identity", ok, detail)
    ok, detail = manual_append_ini(fc, [FC1_TARGET], "fc1")
    check("2.6 fc1 =2 line appended (append-only law)", ok, detail)
    rc, ini_final = fc.handle_curl(["curl", "-sS", cb.FTP + cb.COMBINED_INI["ini"]])
    n_final, entries_final = registered_count(ini_final)
    check("2.6 FINAL combined ini registered count == 7 (5 tool + IGW + fc1)", n_final == 7, str(n_final))
    RECEIPT["constants"]["fc1"] = {"mock": True, "bytes": fbytes, "sha256": fsha, "target": FC1_TARGET,
                                   "note": "placeholder target name; real identity pins when the PRX builds"}
    RECEIPT["phases"]["phase2"] = {
        "dll8": "PASS", "e2e": "PASS", "ridealongs": "PASS (registered 5)",
        "settings_v3": "PASS (readback exact)", "igw": "staged (registered 6)",
        "fc1": "MOCK staged (registered 7)", "final_registered": n_final,
        "gate": "GATE->PHASE 3: all flows green; readbacks byte-exact"}
    return ini_final, n_final


def regenerate_fixtures():
    print("== PHASE 3 prep — regenerate CP fixtures at the corrected tick 0x33ac8c0 ==")
    d = fresh("phase3-fixtures")
    gen = os.path.join(d, "build_fixtures.py")
    shutil.copyfile(FIXTURE_GENERATOR, gen)   # their file untouched; generator runs in MY scratch
    r = subprocess.run([sys.executable, gen], capture_output=True, text=True, cwd=d)
    check("3.prep fixture generator ran clean (5 scenario trees)", r.returncode == 0, r.stderr[-300:])
    fixdir = os.path.join(d, "fixtures")
    scen = sorted(os.listdir(fixdir)) if os.path.isdir(fixdir) else []
    check("3.prep regenerated 5 scenarios", len(scen) == 5, str(scen))
    # corrected tick present in regenerated s01; stale tick absent
    l_new = open(os.path.join(fixdir, "s01_full_success", "console-tree", "data", "plugins", "UE4SS.log")).read()
    check("3.prep regenerated s01 carries 0x33ac8c0 (cb.GAME['tick_line'])", cb.GAME["tick_line"] in l_new)
    check("3.prep regenerated s01 has ZERO retracted 0x37ac8c0", WRONG_TICK not in l_new)
    # the committed fixture is still stale (the propagation worker's consequence note)
    l_old = open(os.path.join(COMMITTED_FIXTURES, "console-tree", "data", "plugins", "UE4SS.log")).read()
    check("3.prep COMMITTED s01 fixture still stale at 0x37ac8c0 (left untouched)", WRONG_TICK in l_old)
    RECEIPT["constants"]["tick"] = {"correct": cb.GAME["tick_line"], "retracted": WRONG_TICK,
                                    "committed_fixture_stale": WRONG_TICK in l_old}
    return fixdir


def classify_tree(fix_root, scen):
    """Run the tool's own CP0-CP4 classifier over a fixture console-tree."""
    base = os.path.join(fix_root, scen, "console-tree", "data")
    s_text = open(os.path.join(base, "modbridge", "roots", "PPSA33286", "runtime-status.txt")).read()
    l_text = open(os.path.join(base, "plugins", "UE4SS.log")).read()
    d1_text = open(os.path.join(base, "townfall_runtime_aligned_entry.log")).read()
    return cb.cp01234_verdict(cb.cp_parse_status_s(s_text), cb.classify_l_log(l_text),
                              cb.classify_d1_log(d1_text))


def real_tool(argv, cwd=None, timeout=120):
    return subprocess.run([sys.executable] + argv, capture_output=True, text=True, cwd=cwd, timeout=timeout)


def phase3(fixdir, fc):
    print("== PHASE 3 — game-evidence on tick-corrected evidence ==")
    # 3.0 stale-vs-corrected comparator (the honest-refuse guard)
    v_stale = classify_tree(os.path.dirname(COMMITTED_FIXTURES), "s01_full_success")
    verdict_stale, per_stale, _ = v_stale
    check("3.0 stale committed fixture: CP2 is PARTIAL and overall INCOMPLETE (never a PASS)",
          verdict_stale == "INCOMPLETE" and per_stale["CP2"].startswith("PARTIAL"),
          "%s | %s" % (verdict_stale, per_stale["CP2"][:80]))
    v_new = classify_tree(fixdir, "s01_full_success")
    verdict_new, per_new, hint_new = v_new
    check("3.0 regenerated fixture: overall PASS with every cell green (PASS cells may carry notes)",
          verdict_new == "PASS" and all(v.startswith("PASS") for v in per_new.values()),
          json.dumps(per_new))

    from_dir = os.path.join(fixdir, "s01_full_success", "from-dir")
    r = real_tool([POST_LAUNCH, "--from-dir", from_dir, "--json"])
    verdict = "PARSE"
    try:
        verdict = json.loads(r.stdout).get("verdict")
    except Exception:
        pass
    check("3.1 real post_launch_check --from-dir -> MODS_STARTED (exit 0)", r.returncode == 0 and verdict == "MODS_STARTED",
          "rc=%s verdict=%s" % (r.returncode, verdict))

    r = real_tool([RUNG_CHECK, "--from-dir", from_dir, "--json"])
    check("3.2 real rung_check --from-dir runs (honest verdict recorded, never fabricated)",
          r.returncode in (0, 1, 3, 5), "rc=%s %s" % (r.returncode, r.stdout[:120]))
    RECEIPT["phases"]["phase3-rung-fresh"] = {"exit": r.returncode, "stdout_head": r.stdout[:200]}

    # --- 3.3 full game-evidence flow on the fake console (tick-corrected L) ---
    print("-- 3.3 game-evidence flow --")
    l_fetch_n["n"] = 0
    ctx, res = run_flow(cb.build_game_evidence, "session/phase3-game")
    bad = [(i, s) for i, s in res if s not in ("PASS", "WARN")]
    check("3.3 game-evidence all steps PASS/WARN (ge.0..ge.10)", not bad, str(bad))
    cp = json.load(open(os.path.join(ctx.run_dir, "cp0-cp4-verdict.json")))
    check("3.3 CP0-CP4 verdict PASS (session ms fresh + tick 0x33ac8c0)", cp["verdict"] == "PASS", str(cp["per_cp"]))
    cp5 = json.load(open(os.path.join(ctx.run_dir, "cp5-verdict.json")))
    cp6 = json.load(open(os.path.join(ctx.run_dir, "cp6-verdict.json")))
    check("3.3 CP5 PASS (menu created)", str(cp5.get("CP5", "")).startswith("PASS"), str(cp5.get("CP5")))
    check("3.3 CP6 PASS (Spawned line; eyes-confirmation still required by receipt)",
          str(cp6.get("CP6", "")).startswith("PASS"), str(cp6.get("CP6")))
    check("3.3 honesty note present in the session receipt (log lines never certify)",
          "honesty" in json.load(open(os.path.join(ctx.run_dir, "game-evidence-receipt.json"))))
    RECEIPT["phases"]["phase3"] = {"cp01234": cp["verdict"], "cp5": cp5.get("CP5", "")[:40],
                                   "cp6": cp6.get("CP6", "")[:40], "gate": "SESSION EVIDENCE COMPLETE"}


# ---------------------------------------------------------------------------
# PART 3 — CONTROLLER-DECISION-BRIEF branch table walk (failure injections)
# ---------------------------------------------------------------------------

def branch_walk():
    print("== BRANCH WALK — brief decision table, mechanically injected ==")
    notes = RECEIPT["branches"]

    # B0 — Phase 0 pre-image divergence -> STOP + preserve + reconcile (ABORT session launch path)
    fc = FakeConsole(ini_preimage=TRACK8_INI.replace(" = 2\n", " = 0\n", 1))
    p = os.path.join(SCRATCH, "b0", "pl.now")
    os.makedirs(os.path.dirname(p), exist_ok=True)
    fc.handle_curl(["curl", "-sS", cb.FTP + "/data/plugins/ploader.ini", "-o", p])
    divergent = sha256_file(p) != sha256_bytes(TRACK8_INI.encode())
    check("B0 phase0 divergence DETECTED (drifted pre-image sha != expected Track-8)", divergent)
    notes["B0_phase0_preimage_divergence"] = {
        "injected": "entry line flipped to mode 0", "tool_says": "sha mismatch",
        "brief_decision": "STOP; preserve /tmp/pl.now; reconcile vs rollback-runbook §4.3; ABORT session launch path",
        "verified_next_command": "none — reconciliation first", "result": "RESOLVED"}

    # B1 — dll8 import mismatch (consumer_imports 88 = SEH64 attribution shape)
    fc = FakeConsole()
    fc.serve_8fam = True
    fc.ledger_corruptor = lambda t: t.replace('"consumer_imports": 90', '"consumer_imports": 88')
    install_patches(fc)
    ctx, res = run_flow(cb.build_dll8, "b1-dll8-import-mismatch")
    st = dict(res)
    fails = [i for i, s in res if s == "FAIL"]
    check("B1 dll8 verdict step FAILS on consumer_imports 88", st.get("dll8.7") == "FAIL", str(fails))
    check("B1 receipt step SKIPPED after halt (nothing else ran)", st.get("dll8.8") == "SKIPPED_HALT")
    check("B1 exactly ONE dispatch, no auto dll7 fallback", fc.dispatch_count == 1
          and "ALLFAM7" not in fc.uploads, str(fc.uploads))
    vd = json.load(open(os.path.join(ctx.run_dir, "allfam8-ledger-verdict.json")))
    check("B1 divergence names consumer_imports 88 (vs oracle 90)", any("88" in json.dumps(d) for d in vd["details"]["divergences"]),
          json.dumps(vd["details"]["divergences"])[:200])
    notes["B1_dll8_import_mismatch"] = {
        "injected": "consumer_imports 90 -> 88 (seh64 attribution shape per brief §3.1)",
        "tool_says": "verdict FAIL, halt before console receipt, exactly-once dispatch preserved",
        "brief_decision": "STOP + reconcile; OPTIONAL documented fallback: dll7 explicitly, only if "
                          "SEH64-specific and window budget remains — ADAPT, never automatic",
        "verified_next_command": "python3 console_batch.py dll7 (explicit, budget-gated) or ABORT",
        "result": "RESOLVED (tool halts; no automatic fallback exists in the tool — matches brief)"}

    # B2 — e2e NOT CONSUMED (dispatch ambiguity)
    fc = FakeConsole()
    fc.consume_failure = True
    install_patches(fc)
    ctx, res = run_flow(cb.build_e2e, "b2-e2e-not-consumed")
    st = dict(res)
    fail_ids = [i for i, s in res if s == "FAIL"]
    check("B2 e2e halts on a NOT-CONSUMED cmd file (gone/consumed gate)",
          len(fail_ids) >= 1 and fail_ids[0].startswith("e2e.d01"), str(fail_ids[:3]))
    after_fail = [i for i, s in res if s == "SKIPPED_HALT"]
    check("B2 everything after the failed dispatch is SKIPPED_HALT", len(after_fail) >= 10, str(len(after_fail)))
    check("B2 NO re-send: exactly 1 dispatch then stop", fc.dispatch_count == 1, str(fc.dispatch_count))
    notes["B2_e2e_not_consumed"] = {
        "injected": "payload dispatch leaves e2e-cmd.json present + .consumed twin absent",
        "tool_says": "cmd_gone/evid gate FAIL at d01; flow halts; every byte preserved",
        "brief_decision": "NEVER re-send; reconcile via cmd_gone + evid reads; ABORT if ambiguous after reads",
        "verified_next_command": "the tool's reconcile reads only (no dispatch)",
        "result": "RESOLVED (tool never re-sends on its own — brief law holds)"}

    # B3 — ini drift (composition line rolled back to mode 0)
    drifted = TRACK8_INI.replace("/data/plugins/modbridge_supplier_composition.prx = 2",
                                 "/data/plugins/modbridge_supplier_composition.prx = 0")
    fc = FakeConsole(ini_preimage=drifted)
    fc.sht_law = True
    install_patches(fc)
    ctx, res = run_flow(cb.build_stage_ridealongs, "b3-ini-drift")
    st = dict(res)
    fails = [i for i, s in res if s == "FAIL"]
    check("B3 stage-ridealongs FAILS on the drifted pre-image (edit step)", any(i.startswith("ride.6") for i in fails), str(fails))
    dec = json.load(open(os.path.join(ctx.run_dir, "ploader-edit-decision.json")))
    check("B3 edit decision recorded as 'stop' (never blind-edit)", dec["status"] == "stop", json.dumps(dec))
    rc, ini_after = fc.handle_curl(["curl", "-sS", cb.FTP + cb.COMBINED_INI["ini"]])
    check("B3 console ini UNTOUCHED (no STOR after stop)", ini_after == drifted)
    check("B3 ride.7 STOR step SKIPPED (halt)", st.get("ride.7") == "SKIPPED_HALT", st.get("ride.7"))
    check("B3 .bak rollback artifact still written first (ride.2 PASS)", st.get("ride.2") == "PASS")
    notes["B3_ini_drift"] = {
        "injected": "composition line at mode 0 (rolled-back staging shape)",
        "tool_says": "ride.6 edit -> status stop -> FAIL; ride.7 skipped; pre-image + decision preserved in run dir",
        "brief_decision": "ini contract AMBIGUOUS -> STOP, show pre-image, reconcile vs STAGE-PLAN §1/rollback §4.3; never blind-edit",
        "verified_next_command": "none until reconciled (ABORT staging)",
        "result": "RESOLVED"}

    # B4 — CP2 stale tick (retracted 0x37ac8c0 in a FRESH session)
    stale_L = [L_GREEN.replace(cb.GAME["tick_line"], WRONG_TICK),
               L_GREEN.replace(cb.GAME["tick_line"], WRONG_TICK) +
               "[Lua] [TownfallItemSpawner] 3 spawnable pickups found. Pages=1\n[Lua] [TownfallItemSpawner] Menu created.\n",
               L_GREEN]
    orig_L, orig_n = L_STAGES[:], l_fetch_n["n"]
    L_STAGES[:] = stale_L
    fc = FakeConsole()
    install_patches(fc)
    l_fetch_n["n"] = 0
    ctx, res = run_flow(cb.build_game_evidence, "b4-cp2-stale-tick")
    st = dict(res)
    cp = json.load(open(os.path.join(ctx.run_dir, "cp0-cp4-verdict.json")))
    check("B4 ge.3 FAILS with CP2 PARTIAL (engine-ready yes, tick line NOT the corrected address)",
          st.get("ge.3") == "FAIL" and cp["per_cp"]["CP2"].startswith("PARTIAL"), str(cp["per_cp"]["CP2"])[:100])
    check("B4 overall INCOMPLETE — no PASS from the retracted constant", cp["verdict"] == "INCOMPLETE")
    check("B4 CP5 handoff NEVER reached (no pad interaction on a failed CP gate)", st.get("ge.5") == "SKIPPED_HALT")
    L_STAGES[:] = orig_L
    l_fetch_n["n"] = orig_n
    notes["B4_cp2_stale_tick"] = {
        "injected": "L carries the retracted 0x37ac8c0 equality line in a fresh session",
        "tool_says": "CP2 PARTIAL(tick line absent) -> ge.3 FAIL -> halt before CP5 handoff",
        "brief_decision": "STOP, honest result; do NOT re-derive/patch tick addresses; report + reconcile with the director",
        "verified_next_command": "none (director reconciliation; new revision gets distinct ID)",
        "result": "RESOLVED — note: brief CP2 row still cites 0x37ac8c0 as the expected line; one-line fix proposed"}

    # B5 — CP4 lua error (main.lua failed to load -> banner absent, error present)
    lua_body = L_GREEN.replace(cb.GAME["mod_banner"] + "\n", "") + \
        "[Lua] Error loading script: TownfallItemSpawner/Scripts/main.lua: module 'UEHelpers' not found\n"
    lua_L = [lua_body, lua_body, lua_body]
    L_STAGES[:] = lua_L
    fc = FakeConsole()
    install_patches(fc)
    l_fetch_n["n"] = 0
    ctx, res = run_flow(cb.build_game_evidence, "b5-cp4-lua-error")
    st = dict(res)
    cp = json.load(open(os.path.join(ctx.run_dir, "cp0-cp4-verdict.json")))
    check("B5 CP4 FAIL(F9/no banner) names the lua error", st.get("ge.3") == "FAIL"
          and cp["per_cp"]["CP4"].startswith("FAIL(F9/no banner") and "Error loading script" in cp["per_cp"]["CP4"],
          cp["per_cp"]["CP4"][:140])
    check("B5 flow halts before any pad interaction", st.get("ge.5") == "SKIPPED_HALT")
    L_STAGES[:] = orig_L
    notes["B5_cp4_lua_error"] = {
        "injected": "main.lua load error, banner absent (F9 shape), S still reaches mods-started",
        "tool_says": "CP4 FAIL(F9/no banner: [lua_errors]) -> halt; honest result",
        "brief_decision": "Script layer NON-fatal to staging: fix mod/activation CONTENT offline with a "
                          "distinct revision; do NOT re-stage the PRX; new launch for new evidence",
        "verified_next_command": "verify staged shas (f58604… main.lua / 5a8f66… UEHelpers.lua); fix content offline",
        "result": "RESOLVED (halt is honest; branch is ADAPT-content, not ABORT-session)"}

    # B6 — CP7 rung stale (real rung_check O2 gate, exit 5)
    stale_dir = fresh("b6-rung-stale")
    regen_from = os.path.join(SCRATCH, "phase3-fixtures", "fixtures", "s01_full_success", "from-dir")
    shutil.copytree(regen_from, stale_dir, dirs_exist_ok=True)
    sp = os.path.join(stale_dir, "runtime-status.txt")
    lines = open(sp).read().splitlines()
    lines = [ln if not ln.startswith("session\t") else "session\t4241\t8865000" for ln in lines]
    open(sp, "w").write("\n".join(lines) + "\n")
    r = real_tool([RUNG_CHECK, "--from-dir", stale_dir, "--json"])
    body = {}
    try:
        body = json.loads(r.stdout)
    except Exception:
        pass
    check("B6 real rung_check exits 5 INCONCLUSIVE-STALE on a stale session (<= 8866785 ms)",
          r.returncode == 5 and (body.get("verdict") == "INCONCLUSIVE-STALE"
                                 or body.get("overall") == "INCONCLUSIVE-STALE"
                                 or "INCONCLUSIVE-STALE" in r.stdout),
          "rc=%s head=%s" % (r.returncode, r.stdout[:120]))
    notes["B6_cp7_rung_stale"] = {
        "injected": "runtime-status session ms 8865000 <= freshness threshold 8866785 (prior-launch evidence)",
        "tool_says": "exit 5 INCONCLUSIVE-STALE (O2 refuse-to-assert; honest negative, not a failure)",
        "brief_decision": "re-run only in a FRESH window; never re-rate unchanged evidence; absence of rung "
                          "evidence != launch failure (UNVERIFIED_DRIVER honest, G2)",
        "verified_next_command": "wait ~20-30 s, re-run rung_check in the fresh window",
        "result": "RESOLVED"}

    # B7 — CP5 shape (a): EngineTick hook-unavailable after mods-started = restage anomaly
    shapeA_L = [L_GREEN,
                L_GREEN + "[Lua] [TownfallItemSpawner] ExecuteInGameThread: EngineTick method requested "
                          "but EngineTick hook is not available\n",
                L_GREEN]
    L_STAGES[:] = shapeA_L
    fc = FakeConsole()
    install_patches(fc)
    l_fetch_n["n"] = 0
    ctx, res = run_flow(cb.build_game_evidence, "b7-cp5-shape-a")
    st = dict(res)
    cp5 = json.load(open(os.path.join(ctx.run_dir, "cp5-verdict.json")))
    check("B7 CP5 shape (a) classified as UNREACHABLE restage anomaly", st.get("ge.6") == "FAIL"
          and "shape a" in str(cp5.get("CP5", "")) and "restage" in str(cp5.get("CP5", "")),
          str(cp5.get("CP5"))[:140])
    L_STAGES[:] = orig_L
    notes["B7_cp5_shape_a"] = {
        "injected": "EngineTick-unavailable line after mods-started (post-OPTIONS L)",
        "tool_says": "CP5 FAIL(shape a anomaly: … check whether UE4SS-settings.ini was restaged; keep KEEP-EngineTick)",
        "brief_decision": "UNREACHABLE after mods-started => anomaly; reconcile staging provenance (the "
                          "ProcessEvent variant would be restage PROOF via sha leaving 9f3fa127…); never re-press",
        "verified_next_command": "reconcile ini provenance (v3 restage sha d8faa4ec… now; check which pair is live)",
        "result": "RESOLVED — note: the tool's shape-a hint cites only the v2-era sha 9f3fa127…; "
                  "v3 note added to findings"}


# ---------------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------------

def main():
    if os.path.isdir(SCRATCH):
        shutil.rmtree(SCRATCH, ignore_errors=True)
    os.makedirs(SCRATCH, exist_ok=True)
    os.makedirs(os.path.join(SCRATCH, "session"), exist_ok=True)
    os.makedirs(RECEIPTS_DIR, exist_ok=True)
    RECEIPT["constants"]["tool"] = {
        "console_batch_sha12": sha256_file(os.path.join(TOOL_DIR, "console_batch.py"))[:12],
        "tick_line": cb.GAME["tick_line"],
        "dll8": {"sha12": cb.DLL8["sha"][:12], "bytes": cb.DLL8["bytes"]},
        "e2e": {"sha12": cb.E2E["sha"][:12], "bytes": cb.E2E["bytes"]},
        "qdrv1": {"sha12": cb.QDRV1["sha"][:12], "bytes": cb.QDRV1["bytes"]},
        "observer": {"sha12": cb.OBS["sha"][:12], "bytes": cb.OBS["bytes"]},
    }
    check("tool tick constant is the corrected 0x33ac8c0 (no retracted value in the live tool)",
          "33ac8c0" in cb.GAME["tick_line"] and "37ac8c0" not in cb.GAME["tick_line"])

    core_regression()
    fc = FakeConsole()
    install_patches(fc)
    phase0(fc)
    phase1_mock_comp6(fc)
    ini_final, n_final = phase2(fc)
    fixdir = regenerate_fixtures()
    phase3(fixdir, fc)
    branch_walk()

    # final combined ini sanity: exactly one header, no duplicates, 7 registered
    n_check, entries_check = registered_count(ini_final)
    paths = [p for p, _m, _r in entries_check]
    check("final session ini: no duplicate lines (ploader does NOT dedupe)", len(paths) == len(set(paths)))
    check("final session ini registered count still 7 at teardown", n_check == 7, str(n_check))

    go_no_go = {
        "phase0_sanity": "GO (rehearsed; divergence branch aborts correctly)",
        "phase1_comp6": "CONDITIONAL — MOCK ONLY (real one-pass blocked on the initializer landing; "
                        "never run the skeleton as if it built)",
        "phase2_batch": "GO — dll8+e2e+ridealongs tool flows all green on the fake drive; settings v3 "
                        "restage verified against the REAL package-v3 bytes; IGW staged from the REAL "
                        "armed artifact (live identity); fc1 staging is MOCK (no PRX built)",
        "phase3_game_evidence": "GO on corrected fixtures — committed CP fixtures are STALE (0x37ac8c0) "
                                "and MUST be regenerated at 0x33ac8c0 before rating (proven: stale -> "
                                "CP2 PARTIAL/INCOMPLETE, regenerated -> PASS)",
        "branch_table": "6 required shapes + 2 bonus branches exercised; all resolve per the brief with "
                        "3 one-line fixes proposed",
        "session_go": "CONDITIONAL GO" if not FAILURES else "NO-GO (drive checks failed)",
    }
    RECEIPT["go_no_go"] = go_no_go
    RECEIPT["failed_checks"] = FAILURES
    with open(os.path.join(RECEIPTS_DIR, "REHEARSAL-RECEIPTS.json"), "w") as f:
        json.dump(RECEIPT, f, indent=2)
    with open(os.path.join(RECEIPTS_DIR, "SESSION-GO-NO-GO.json"), "w") as f:
        json.dump({"worker": "worker-session-desk-rehearsal", "checklist": go_no_go,
                   "drive_exit": 1 if FAILURES else 0}, f, indent=2)

    print("")
    print("session desk rehearsal summary: %d failed checks" % len(FAILURES))
    for f in FAILURES:
        print("  FAILED: %s" % f)
    sys.exit(1 if FAILURES else 0)


if __name__ == "__main__":
    main()
