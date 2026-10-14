#!/usr/bin/env python3
"""build_fixtures.py — synthesize five complete fake session evidence trees.

worker-evidence-pipeline-rehearsal, 2026-10-04. NO console contact: every line
is fabricated from the grammars in MASTER-VERIFICATION-RUNBOOK.md plus the real
line shapes already recorded in-tree:
  - 3-line baseline: post_launch_check.py BASELINE_LINES (verbatim, embedded)
  - banner/tick/mod/menu/spawn lines: runbook CP1-CP6 + console_batch.py GAME dict
  - D1 event grammar: runbook §1 + worker-post-launch-tools host-test fixtures
  - PS-scan-timeout detail: ENGINE-READY-PATH.md row 80 ("PS scan timed out" ->
    failed(1,<msg>))
  - lease-gate refusal: transport-ledger launch-8 entry ("Owned read range
    includes unregistered bytes", bridge code=2)
  - C2 NQH1 record shapes: worker-rung-evidence-reader host-test rung1-pass
    fixtures (verbatim field layout, fabricated values)

Layout per scenario:
  fixtures/<scn>/from-dir/     flat files consumed by --from-dir tools
  fixtures/<scn>/console-tree/ same bytes under /data console paths (realism)
Staged L slices for s01 (CP5/CP6 diff rehearsal): l_t1/l_t2/l_t3 UE4SS.log.
"""
import json
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
# tools-final: fixtures regenerated into THIS worker dir only;
# the committed rehearsal fixtures/ tree stays byte-preserved.
FIX = os.path.join(HERE, "fixtures-corrected")

BASELINE = [
    "PATH_TEST: /data/plugins/UE4SS.log writable at 8866785633337 monotonic-ns",
    "ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: "
    "6d6f64666f7267652d6c6f61646c6973",
    "DIAG: module_start reached at 1791155755.879 pid=298 build=sdg2-6f404cee6b94 "
    "variant=imp",
]

ENTRY_BLD = "f6530c7905934e5a8cc201aa9400ebb0"   # aligned-entry channel (real fixture shape)
OBS_BLD = "e9a58a9a8740438ebcb59136252a0556"     # observer channel (runbook §1 grammar)

BANNER = [
    "Native console, GUI, crash dumping and filesystem watching are unavailable",
    "UE4SS - v3.0.1 - Git SHA #a1b2c3d4e5f60718",
    "root directory: /data/plugins",
    "mods directories:",
    "[0] ue4ss/Mods",
    "Setting up mods...",
]
TICK_LINE = "GameEngine::Tick address (vtable: 0x33ac8c0; scan: 0x33ac8c0)"
ENGINE_VER = "Using engine version: 5.6"
MOD_LOAD = ("[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. "
            "F7 opens the menu.")
MENU_LINES = [
    "[Lua] [TownfallItemSpawner] 12 spawnable pickups found. Pages=1",
    "[Lua] [TownfallItemSpawner] Menu created.",
]
SPAWN_LINE = "[Lua] [TownfallItemSpawner] Spawned Antibiotics x1 ID=DeprecatedItem"

STATUS_HDR = "modforge-runtime-status\t1"


def d1(tag, val, bld=ENTRY_BLD):
    return ("[PPSA33286] modbridge-runtime-aligned-entry/v1 build=%s %s "
            "value=0x%s" % (bld, tag, val))


def d1_block(end_tags):
    """module_start -> ctor pairs -> probe -> wiring -> mfa -> end tags."""
    lines = [
        d1("module_start_entered", "00000000"),
        d1("constructor_begin", "00000000"),
        d1("constructor_return", "00000000"),
        d1("constructor_begin", "00000000"),
        d1("constructor_return", "00000000"),
        d1("probe_lifecycle_result", "00000000"),
        d1("runtime_wiring_begin", "00000000"),
        d1("mfa_activated_low", "00008c39"),
        d1("mfa_activated_high", "00007f3c"),
    ]
    for tag, val in end_tags:
        lines.append(d1(tag, val))
    return lines


def status_txt(session, ms, rows):
    out = [STATUS_HDR, "session\t%d\t%d" % (session, ms)]
    out += rows
    return "\n".join(out) + "\n"


def write(scenario, files):
    """files: {basename: content-or-None}. None -> file absent (s03 C2).
    baseline.txt (verbatim embedded-baseline bytes) is staged in every tree so
    the --baseline-file path is exercisable everywhere ("as needed" parity)."""
    files = dict(files)
    files.setdefault("baseline.txt", "\n".join(BASELINE) + "\n")
    fd = os.path.join(FIX, scenario, "from-dir")
    ct = os.path.join(FIX, scenario, "console-tree", "data")
    os.makedirs(fd, exist_ok=True)
    mapping = {
        "UE4SS.log": "plugins/UE4SS.log",
        "runtime-status.txt": "modbridge/roots/PPSA33286/runtime-status.txt",
        "townfall_runtime_aligned_entry.log": "townfall_runtime_aligned_entry.log",
        "townfall_runtime_aligned_observer.log": "townfall_runtime_aligned_observer.log",
        "modbridge-ingame-facts.jsonl": "modbridge-ingame-facts.jsonl",
    }
    for name, content in files.items():
        if content is None:
            continue
        with open(os.path.join(fd, name), "w") as fh:
            fh.write(content if content.endswith("\n") or content == "" else content + "\n")
    for name, rel in mapping.items():
        src = os.path.join(fd, name)
        dst = os.path.join(ct, rel)
        if os.path.isfile(src):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy(src, dst)
        else:
            print("  note: %s/%s absent (mirrors console: file does not exist)"
                  % (scenario, name))


def main():
    if os.path.isdir(FIX):
        shutil.rmtree(FIX)
    os.makedirs(FIX)

    # ---------------------------------------------------------------- s01
    l_full = "\n".join(
        BASELINE + BANNER + [ENGINE_VER, TICK_LINE, MOD_LOAD] + MENU_LINES +
        [SPAWN_LINE]) + "\n"
    c2_snap1 = "\n".join([
        '{"marker":"modbridge.ingamefacts","schema":1,"build_id":"qdrv1-s01",'
        '"seq":3,"exp":"objarray_count","rc":0,"errno":0,"fields":{"count":48152}}',
        '{"record":"native_query_harness","magic":"NQH1","status":"pending",'
        '"engine":"5.6","axes":{"non_shipping":1,"stats":0},"storage":"unknown",'
        '"count_source":"engine_uobject_array",'
        '"game_thread_id_key":"0x0000000000000f3c",'
        '"counts":{"first":0,"second":0,"recorded":0},"delta":0,'
        '"monotonic":"unknown","identity_checks":1,"threshold":10000,'
        '"ticks":{"dispatched":9,"accepted":5,"refusals_between":0},'
        '"gate_source":"PublishStagedNativeLayoutMaps"}',
        '{"refused_wrong_thread":2,"refused_identity_unknown":1}',
    ]) + "\n"
    c2_snap2 = (
        '{"record":"native_query_harness","magic":"NQH1","status":"completed",'
        '"engine":"5.6","axes":{"non_shipping":1,"stats":0},'
        '"storage":"chunked_array","count_source":"engine_uobject_array",'
        '"game_thread_id_key":"0x0000000000000f3c",'
        '"counts":{"first":48152,"second":48152,"recorded":2},"delta":0,'
        '"monotonic":"equal","identity_checks":2,"threshold":10000,'
        '"ticks":{"dispatched":12,"accepted":9,"refusals_between":0},'
        '"gate_source":"PublishStagedNativeLayoutMaps"}') + "\n"
    d2_s01 = "\n".join([
        "[PPSA33286] modbridge-runtime-aligned-observer/v1 build=%s "
        "module_start_entered value=0x00000000" % OBS_BLD,
        "[PPSA33286] modbridge-runtime-aligned-observer/v1 build=%s "
        "module_start_return value=0x00000000" % OBS_BLD,
    ]) + "\n"
    write("s01_full_success", {
        "UE4SS.log": l_full,
        "runtime-status.txt": status_txt(412, 9435000, [
            "state\t1\tactivated\t0\t", "state\t2\tengine-ready\t0\t",
            "state\t3\tmods-started\t0\t"]),
        "townfall_runtime_aligned_entry.log": "\n".join(d1_block([
            ("runtime_start_ok", "00000000"),
            ("module_start_return", "00000000")])) + "\n",
        "townfall_runtime_aligned_observer.log": d2_s01,
        "modbridge-ingame-facts.jsonl": c2_snap1,
        "modbridge-ingame-facts.jsonl.snap2": c2_snap2,
        "baseline.txt": "\n".join(BASELINE) + "\n",
        # staged re-fetch slices for the CP5/CP6 diff rehearsal:
        "l_t1_UE4SS.log": "\n".join(
            BASELINE + BANNER + [ENGINE_VER, TICK_LINE, MOD_LOAD]) + "\n",
        "l_t2_UE4SS.log": "\n".join(
            BASELINE + BANNER + [ENGINE_VER, TICK_LINE, MOD_LOAD] + MENU_LINES) + "\n",
        "l_t3_UE4SS.log": l_full,
    })

    # ---------------------------------------------------------------- s02
    # Configured-roots conversion succeeded (Resolution B); init thread died in
    # the ps_scan loop -> outcome #8, failed(1, "PS scan timed out").
    write("s02_ps_scan_timeout", {
        "UE4SS.log": "\n".join(BASELINE + BANNER + [ENGINE_VER]) + "\n",
        "runtime-status.txt": status_txt(413, 9512000, [
            "state\t1\tactivated\t0\t",
            "state\t2\tfailed\t1\tPS scan timed out"]),
        "townfall_runtime_aligned_entry.log": "\n".join(d1_block([
            ("runtime_start_ok", "00000000"),
            ("module_start_return", "00000000")])) + "\n",
        "townfall_runtime_aligned_observer.log": "",
        "modbridge-ingame-facts.jsonl": "",
    })

    # ---------------------------------------------------------------- s03
    # launch-8 signature: conversion passed, context build started, read-lease
    # threw "Owned read range includes unregistered bytes" (bridge code=2).
    lease_detail = "Owned read range includes unregistered bytes"
    write("s03_lease_gate_refusal", {
        "UE4SS.log": "\n".join(BASELINE + [
            "modbridge module_start failed: runtime_start_failed code=2 detail=%s"
            % lease_detail]) + "\n",
        "runtime-status.txt": status_txt(414, 9534000, [
            "state\t1\tactivated\t0\t",
            "state\t2\tfailed\t2\t%s" % lease_detail]),
        "townfall_runtime_aligned_entry.log": "\n".join(d1_block([
            ("runtime_start_failed", "00000002")])) + "\n",
        "townfall_runtime_aligned_observer.log": "",
        "modbridge-ingame-facts.jsonl": None,   # absent on console: nothing emitted, no driver
        "dirlist-data.txt": "townfall_runtime_aligned_entry.log\n"
                            "townfall_runtime_aligned_observer.log\n"
                            "modbridge\n"
                            "plugins\n",
        "dirlist-data-plugins.txt": "UE4SS.log\nploader.ini\n",
        "dirlist-data-modbridge.txt": "roots\n",
        "dirlist-data-modbridge-roots.txt": "PPSA33286\n",
        "dirlist-data-modbridge-roots-PPSA33286.txt": "runtime-status.txt\nloadlist.txt\n",
    })

    # ---------------------------------------------------------------- s04
    script_err = ("[Lua] Error loading script: "
                  "Mods/TownfallItemSpawner/Scripts/main.lua:712: "
                  "attempt to call a nil value (field 'buildMenu')")
    write("s04_script_error_trap", {
        "UE4SS.log": "\n".join(
            BASELINE + BANNER + [ENGINE_VER, TICK_LINE, script_err]) + "\n",
        "runtime-status.txt": status_txt(415, 9577000, [
            "state\t1\tactivated\t0\t", "state\t2\tengine-ready\t0\t",
            "state\t3\tmods-started\t0\t"]),
        "townfall_runtime_aligned_entry.log": "\n".join(d1_block([
            ("runtime_start_ok", "00000000"),
            ("module_start_return", "00000000")])) + "\n",
        "townfall_runtime_aligned_observer.log": "",
        "modbridge-ingame-facts.jsonl": "\n".join([
            '{"marker":"modbridge.ingamefacts","schema":1,"build_id":"igf6-run1",'
            '"seq":3,"exp":"objarray_count","rc":0,"errno":0,"fields":{"count":48152}}',
            '{"marker":"modbridge.ingamefacts","schema":1,"build_id":"igf6-run1",'
            '"seq":4,"exp":"objarray_count","rc":0,"errno":0,"fields":{"count":48152}}',
        ]) + "\n",
    })

    # ---------------------------------------------------------------- s05
    # Stale launch: repeat oracle lines only; every file still holds the prior
    # session (session ms 8812000 < freshness threshold 8866785).
    stale_probes = [
        "PATH_TEST: /data/plugins/UE4SS.log writable at 9900000000000 monotonic-ns",
        "ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: "
        "deadbeefdeadbeefdeadbeefdeadbeef",
        "DIAG: module_start reached at 1791159999.999 pid=301 "
        "build=sdg2-6f404cee6b94 variant=imp",
    ]
    write("s05_stale_launch", {
        "UE4SS.log": "\n".join(BASELINE + stale_probes) + "\n",
        "runtime-status.txt": status_txt(234, 8812000, [
            "state\t1\tactivated\t0\t", "state\t2\tengine-ready\t0\t",
            "state\t3\tmods-started\t0\t"]),
        "townfall_runtime_aligned_entry.log": "\n".join([
            d1("module_start_entered", "00000000"),
            d1("module_start_entered", "00000000"),
            d1("probe_lifecycle_result", "00000000"),
            d1("module_start_return", "00000000"),
        ]) + "\n",
        "townfall_runtime_aligned_observer.log": "",
        "modbridge-ingame-facts.jsonl": (
            '{"marker":"modbridge.ingamefacts","schema":1,'
            '"build_id":"igf5-old","seq":1,"exp":"objarray_count","rc":0,'
            '"errno":0,"fields":{"count":47900}}') + "\n",
    })

    print("fixtures written under %s" % FIX)
    for scn in sorted(os.listdir(FIX)):
        n = len(os.listdir(os.path.join(FIX, scn, "from-dir")))
        print("  %s: %d from-dir files" % (scn, n))


if __name__ == "__main__":
    main()
