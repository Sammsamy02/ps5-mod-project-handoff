#!/usr/bin/env python3
"""run_api_tests.py — integrated-API host test driver.

Worker-app-backend-integration, 2026-10-04. Compiles the vendored http.c with
the tree's own offline stub-auth discipline (as tests/test_mod_service_read.py
does), points the modmgr runtime at the REAL app_core/modmgr_api.py + REAL
composed installer binary over a fixture tree, drives the full core loop
through raw HTTP frames, and verifies every mutation through the binary's OWN
reader afterwards. No network, no console, no account.
"""
import hashlib
import http.client
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
TEAM = pathlib.Path("/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-app-ui-flows")
VENDORED = TEAM / "vendored" / "repo" / "outputs" / "ps5-mod-bridge"
NATIVE = VENDORED / "native-host"
BIN = TEAM / "vendored" / "bin" / "modinst_composed"
TITLE = "PPSA33286"
SEED_ID = "zb-lua"
API_ID = "api-mod"
CORPUS = pathlib.Path("/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-zip-to-mfb") / "test" / "run" / "zb-lua" / "intake" / "zb-lua.zip"

PASS, FAIL = 0, 0


def check(name, ok, detail=""):
    global PASS, FAIL
    if ok:
        PASS += 1
        print("  ok   %s" % name)
    else:
        FAIL += 1
        print("  FAIL %s %s" % (name, detail))


def sha256(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def seed_fixture(fix):
    """Seed one installed mod through the PROVEN CLI (host core loop)."""
    intake = fix / "intake"
    intake.mkdir(parents=True, exist_ok=True)
    (fix / "state").mkdir(exist_ok=True)
    (fix / "roots").mkdir(exist_ok=True)
    shutil.copy(CORPUS, intake / ("%s.zip" % SEED_ID))
    app = NATIVE / "app_core"
    subprocess.run([sys.executable, str(app / "modmgr.py"),
                    "--binary", str(BIN), "--state", str(fix / "state"),
                    "--roots", str(fix / "roots"),
                    "install", "--title", TITLE,
                    "--source", str(intake / ("%s.zip" % SEED_ID)),
                    "--id", SEED_ID], check=True, capture_output=True)


def compile_harness(work):
    includes = ["-I", NATIVE, "-I", VENDORED / "third_party"]
    objs = []
    cxx = ["c++", "-std=c++20", "-Wall", "-Wextra", "-Werror", *includes,
           "-c", NATIVE / "mod_service/read_bridge.cpp",
           "-o", work / "bridge.o"]
    subprocess.run(cxx, check=True)
    objs.append(work / "bridge.o")
    for name, source, flags in [
            ("http", NATIVE / "http.c",
             ["-DMB_HOST_DOWNLOAD=1", "-DMB_HOST_DOWNLOAD_STORE=1"]),
            ("test", HERE / "modmgr_http_tests.c", []),
            ("assets", NATIVE / "tests" / "web_assets_stub.c", [])]:
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        *includes, *flags, "-c", source,
                        "-o", work / (name + ".o")], check=True)
        objs.append(work / (name + ".o"))
    subprocess.run(["c++", *objs, "-o", work / "http-test"], check=True)
    return work / "http-test"


def run_harness(binary, fix, install_body, outside_body):
    env = os.environ.copy()
    env.update({
        "MB_TEST_STATE": str(fix / "state"),
        "MB_TEST_ROOTS": str(fix / "roots"),
        "MB_TEST_BIN": str(BIN),
        "MB_TEST_SCRIPT": str(NATIVE / "app_core" / "modmgr_api.py"),
        "MB_TEST_INSTALL_BODY": install_body,
        "MB_TEST_OUTSIDE_BODY": outside_body,
        "MB_TEST_SECOND_ID": API_ID,
    })
    proc = subprocess.run([str(binary)], env=env, capture_output=True, text=True)
    if proc.returncode != 0:
        raise AssertionError("harness exited %d\n%s\n%s"
                             % (proc.returncode, proc.stdout, proc.stderr))
    steps = {}
    for line in proc.stdout.splitlines():
        m = re.match(r"STEP (\S+) status=(\d+) body=(.*)", line)
        if m:
            steps[m.group(1)] = (int(m.group(2)), m.group(3))
    return steps


def body_json(raw):
    try:
        return json.loads(raw)
    except ValueError:
        return None


def binary_records(fix):
    return subprocess.run([str(BIN), "--state", str(fix / "state"),
                           "--roots", str(fix / "roots"), "records"],
                          capture_output=True, text=True).stdout


def main():
    assert BIN.exists(), BIN
    assert CORPUS.exists(), CORPUS
    print("== integrated-API host tests (offline; stub auth; real driver+binary) ==")
    with tempfile.TemporaryDirectory(prefix="modmgr-api-") as tmp:
        work = pathlib.Path(tmp)
        fix = work / "fixture"
        seed_fixture(fix)
        binary = compile_harness(work)
        receipt = {"binary": str(BIN), "binary_sha256": sha256(BIN),
                   "corpus_zip_sha256": sha256(CORPUS),
                   "console_executed": False, "provider_contact": False}

        install_body = json.dumps({"title": TITLE, "id": API_ID,
                                   "path": str(fix / "intake" / (API_ID + ".zip"))})
        shutil.copy(CORPUS, fix / "intake" / ("%s.zip" % API_ID))
        outside_body = json.dumps({"title": TITLE, "id": "evil",
                                   "path": "/etc/hosts"})
        steps = run_harness(binary, fix, install_body, outside_body)

        # statuses
        expected = {"S1-installed-disabled": 404, "S2-installed-no-cookie": 401,
                    "S3-installed-empty": 200, "S4-install-no-csrf": 403,
                    "S5-install": 200, "S6-installed-two": 200,
                    "S7-disable": 200, "S8-enable": 200, "S9-uninstall": 200,
                    "S10-uninstall-again": 200,  # idempotent remove: honest 200
                    "S11-install-outside-intake": 400,
                    "S12-installed-post": 405, "S12b-install-get": 405,
                    "S13-capabilities-enabled": 200,
                    "S13b-capabilities-disabled": 200}
        for name, status in sorted(expected.items()):
            got = steps.get(name, (0, ""))[0]
            check(name, got == status, "want %d got %d" % (status, got))

        # S3 runs after seeding: the pre-install list shows the seeded mod.
        s3 = body_json(steps["S3-installed-empty"][1])
        s3items = s3.get("items", []) if s3 else []
        check("S3-seeded-list",
              bool(s3 and s3.get("ok") is True and len(s3items) == 1
                   and s3items[0]["id"] == SEED_ID
                   and s3items[0]["transaction_confirmed"] is True),
              repr(steps["S3-installed-empty"][1])[:160])

        s5 = body_json(steps["S5-install"][1])
        check("S5-ok-envelope", bool(s5 and s5.get("ok") is True))
        check("S5-queue-staged", bool(s5 and len(s5.get("queue", [])) >= 3),
              "queue=%s" % (s5.get("queue") if s5 else None))
        check("S5-verification-converged",
              bool(s5 and s5.get("verification", {}).get("loadlist_diverged") is False))
        check("S5-payload-pin-recorded",
              bool(s5 and s5["queue"] and s5["queue"][0]["payload_identity"]
                   ["sha256"].startswith("a21771f8")))
        check("S5-console-not-executed",
              bool(s5 and all(row["console_executed"] is False for row in s5["queue"])))

        s6 = body_json(steps["S6-installed-two"][1])
        items = s6.get("items", []) if s6 else []
        ids = sorted(i["id"] for i in items)
        check("S6-two-mods", ids == sorted([SEED_ID, API_ID]), repr(ids))
        hexre = re.compile(r"^[a-f0-9]{64}$")
        check("S6-ui-contract",
              all(i.get("transaction_confirmed") is True and hexre.match(i.get("sha256") or "")
                  for i in items), repr(items)[:200])
        check("S6-generation-progressed",
              bool(s6 and s6.get("generation", 0) >= 1), "gen=%s" % (s6 or {}).get("generation"))

        s7 = body_json(steps["S7-disable"][1])
        s7mods = {m["id"]: m["enabled"] for m in s7["verification"]["mods"]} if s7 else {}
        check("S7-disabled-flag", s7mods.get(API_ID) is False, repr(s7mods))
        s8 = body_json(steps["S8-enable"][1])
        s8mods = {m["id"]: m["enabled"] for m in s8["verification"]["mods"]} if s8 else {}
        check("S8-enabled-flag", s8mods.get(API_ID) is True, repr(s8mods))
        check("S7S8-diverged-false",
              bool(s7 and s8 and s7["verification"]["loadlist_diverged"] is False
                   and s8["verification"]["loadlist_diverged"] is False))

        s9 = body_json(steps["S9-uninstall"][1])
        check("S9-ok", bool(s9 and s9.get("ok") is True))

        # binary's own reader (its `records` verb, space-separated print):
        # only the seed mod remains installed, the api mod is back to prepared.
        records = binary_records(fix)
        check("reader-seed-installed",
              ("%s %s installed" % (SEED_ID, TITLE)) in records, records[:200])
        check("reader-api-mod-not-installed",
              ("%s %s installed" % (API_ID, TITLE)) not in records)
        check("reader-api-mod-prepared",
              ("%s %s prepared" % (API_ID, TITLE)) in records, records[:200])
        check("reader-game-row", ("game %s generation=" % TITLE) in records)

        # queue manifest on disk matches the S5 staged rows (sha-bound)
        manifest = fix / "state" / "modmgr" / "console-queue" / "manifest.jsonl"
        rows = [json.loads(x) for x in manifest.read_text().splitlines() if x.strip()]
        check("manifest-rows", len(rows) >= len(s5.get("queue", [])),
              "%d rows" % len(rows))
        ok_shas = all(hashlib.sha256(
            (fix / "state" / "modmgr" / "console-queue" / (row["name"] + ".json"))
            .read_bytes()).hexdigest() == row["command_sha256"] for row in rows)
        check("manifest-sha-readback", ok_shas)
        verbs = [json.loads((fix / "state" / "modmgr" / "console-queue" /
                             (row["name"] + ".json")).read_text())["verb"]
                 for row in rows]
        check("staged-verbs-legal",
              all(v in ("records", "register-mod", "prepare", "prepare-archive",
                        "install", "simulate-runtime", "recover") for v in verbs),
              repr(verbs))
        install_rows = [json.loads((fix / "state" / "modmgr" / "console-queue" /
                                    (row["name"] + ".json")).read_text())
                        for row in rows if row["verb"] == "install"]
        check("disable-staged-recompose",
              install_rows[1].get("mods") == [SEED_ID],
              repr(install_rows[1])[:160])
        check("enable-staged-recompose",
              install_rows[2].get("mods") == sorted([SEED_ID, API_ID]),
              repr(install_rows[2])[:160])

        # stale-command gate: refusing to write over an unconsumed slot.
        # Pin next_slot so the staging target IS the pre-created file.
        queue_dir = fix / "state" / "modmgr" / "console-queue"
        sys.path.insert(0, str(NATIVE / "app_core"))
        try:
            import modmgr_api
            import console_link
            pinned = modmgr_api.next_slot(str(queue_dir)) + 50
            (queue_dir / ("%03d-collision.json" % pinned)).write_bytes(b'{"schema":1}\n')
            original_next_slot = modmgr_api.next_slot
            modmgr_api.next_slot = lambda qdir: pinned
            try:
                doc = console_link.build_command("records", str(fix / "state"),
                                                 str(fix / "roots"))
                modmgr_api.stage_commands(str(fix / "state"), TITLE,
                                          [("collision", doc)])
                check("stale-gate-refuses", False, "staging succeeded over existing slot")
            except modmgr_api.Refused as e:
                check("stale-gate-refuses", e.kind == "stale_command", e.kind)
            finally:
                modmgr_api.next_slot = original_next_slot
        finally:
            sys.path.pop(0)

        # transcript op on an empty transcript file (valid, present-less)
        transcript = subprocess.run(
            [sys.executable, str(NATIVE / "app_core" / "modmgr_api.py"),
             "transcript", "--binary", str(BIN), "--state", str(fix / "state"),
             "--roots", str(fix / "roots")], capture_output=True, text=True)
        tdoc = json.loads(transcript.stdout)
        check("transcript-op-valid", tdoc.get("ok") is True and tdoc.get("valid") is True,
              transcript.stdout[:120])

        # driver-level honest empty state (fresh unseeded store)
        empty_state = work / "empty-state"
        (empty_state / "roots").mkdir(parents=True)
        empty_run = subprocess.run(
            [sys.executable, str(NATIVE / "app_core" / "modmgr_api.py"),
             "list", "--binary", str(BIN), "--state", str(empty_state / "state"),
             "--roots", str(empty_state / "roots")], capture_output=True, text=True)
        edoc = json.loads(empty_run.stdout) if empty_run.returncode == 0 else {}
        check("empty-state-200-items",
              edoc.get("ok") is True and edoc.get("items") == []
              and edoc.get("installed") is False, empty_run.stdout[:160])

        # capabilities honesty in the S13 bodies
        check("S13-installed-records-true",
              '"installed_records":true' in steps["S13-capabilities-enabled"][1],
              steps["S13-capabilities-enabled"][1][:160])
        check("S13b-installed-records-false",
              '"installed_records":false' in steps["S13b-capabilities-disabled"][1],
              steps["S13b-capabilities-disabled"][1][:160])

        receipt["steps"] = {k: v[0] for k, v in sorted(steps.items())}
        receipt["pass"] = PASS
        receipt["fail"] = FAIL
        out = TEAM.parent / "worker-app-payload-repin" / "receipts"
        out.mkdir(exist_ok=True)
        (out / "api-tests.json").write_text(json.dumps(receipt, indent=1) + "\n")
    print("== PASS=%d FAIL=%d ==" % (PASS, FAIL))
    return 1 if FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
