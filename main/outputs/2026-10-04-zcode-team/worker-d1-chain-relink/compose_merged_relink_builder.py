#!/usr/bin/env python3
"""compose_merged_relink_builder.py — worker-d1-chain-relink (2026-10-04).

Builds the canonical 51-tuple merged chain driver from worker-risk-e2e/
D1-PROPAGATION.md section 4, by vendoring worker-zip-bridge-console-link/
compose_console_bridge.py (sha-pinned) and appending two recorded splices to
its MY_SPLICES list:

  d1-uf-block-append : inserts the worker-uninstall-fault-expansion UF_BLOCK
                       (4 uf-* tuples) immediately after zb-usage-bridge in
                       the inner script's EXTRA_PATCHES  (D1 fix FIRST)
  fr2-block-append   : inserts the worker-f-r2-fix R2_BLOCK (2 r2-* tuples)
                       after uf-recover-commit                      (F-R2 LAST)

Resulting apply order: [35 base] + [6 e2e-*] + [4 zb-*] + [4 uf-*] + [2 r2-*]
= 51 tuples. Both block texts are taken VERBATIM from the fix-origin drivers
via AST string-literal extraction (no retyping), and every tuple's old/new
sha256 is verified against the fix-origin workers' patches.json pins before
the merged driver is written.

Writes only under outputs/2026-10-04-zcode-team/worker-d1-chain-relink/.
"""
import ast
import hashlib
import json
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
TEAM = ROOT / "outputs/2026-10-04-zcode-team"
REC = HERE / "receipts"

CONSOLE_DRIVER = TEAM / "worker-zip-bridge-console-link/compose_console_bridge.py"
CONSOLE_DRIVER_SHA = "ab9de3dd53fe208a893d213c36e8db9acd862a902c5330c549b119ef7478dd90"
UF_DRIVER = TEAM / "worker-uninstall-fault-expansion/compose_fault.py"
R2_DRIVER = TEAM / "worker-f-r2-fix/compose_bridge_r2.py"
UF_PINS = TEAM / "worker-uninstall-fault-expansion/composition/receipts/patches.json"
R2_PINS = TEAM / "worker-f-r2-fix/composition/receipts/patches.json"

UF_ANCHOR = '    ("zb-usage-bridge", ZB_USAGE_OLD, ZB_USAGE_NEW, 1),\n]'
R2_ANCHOR = '    ("uf-recover-commit", UF_REC_CMT_OLD, UF_REC_CMT_NEW, 1),\n]'
SPLICE_TAIL_ANCHOR = "     'PATCHES_TAIL_HEAD ='),\n]"

UF_IDS = ["uf-order-commit-a", "uf-order-commit-b",
          "uf-recover-forward", "uf-recover-commit"]
R2_IDS = ["r2-exit-shim", "r2-install-status-ok"]


def digest_bytes(b):
    return hashlib.sha256(b).hexdigest()


def die(msg):
    print("builder: FATAL: " + msg, file=sys.stderr)
    sys.exit(2)


def extract_block(path, name):
    """Pull the exact string-literal value of `name` from a fix-origin driver."""
    tree = ast.parse(path.read_text())
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id == name:
                    return ast.literal_eval(node.value)
    die("block %s not found in %s" % (name, path))


def block_tuples(block_text, label):
    """Exec the block in a sandbox to confirm its tuple ids/order and texts."""
    ns = {"EXTRA_PATCHES": []}
    exec(compile(block_text, "<%s>" % label, "exec"), ns)
    return list(ns["EXTRA_PATCHES"])


def verify_block(block_text, ids, pins_path, label, log):
    pins = json.loads(pins_path.read_text())
    by_id = {p["id"]: p for p in pins["patches"]}
    tuples = block_tuples(block_text, label)
    got_ids = [t[0] for t in tuples]
    if got_ids != ids:
        die("%s tuple order %s != expected %s" % (label, got_ids, ids))
    for pid, old, new, occ in tuples:
        pin = by_id.get(pid)
        if pin is None:
            die("%s tuple %s missing from %s" % (label, pid, pins_path))
        if occ != 1 or pin["occurrences"] != 1:
            die("%s tuple %s occurrence mismatch" % (label, pid))
        if digest_bytes(old.encode()) != pin["old_sha256"]:
            die("%s tuple %s OLD text diverges from pin" % (label, pid))
        if digest_bytes(new.encode()) != pin["new_sha256"]:
            die("%s tuple %s NEW text diverges from pin" % (label, pid))
        log[label].append({"tuple": pid, "old_sha256": pin["old_sha256"],
                    "new_sha256": pin["new_sha256"],
                    "pin_verified": True, "occurrences": 1})
    print("%s: %d tuples verified against %s" % (label, len(ids), pins_path.name))


def main():
    REC.mkdir(parents=True, exist_ok=True)
    drv_bytes = CONSOLE_DRIVER.read_bytes()
    if digest_bytes(drv_bytes) != CONSOLE_DRIVER_SHA:
        die("compose_console_bridge.py diverges from pin %s" % CONSOLE_DRIVER_SHA[:16])

    uf_block = extract_block(UF_DRIVER, "UF_BLOCK")
    r2_block = extract_block(R2_DRIVER, "R2_BLOCK")
    log = {"uf": [], "r2": []}
    verify_block(uf_block, UF_IDS, UF_PINS, "uf", log)
    verify_block(r2_block, R2_IDS, R2_PINS, "r2", log)

    text = drv_bytes.decode()
    n = text.count(SPLICE_TAIL_ANCHOR)
    if n != 1:
        die("MY_SPLICES tail anchor found %d times (want 1)" % n)

    # repr() keeps the embedded block texts byte-exact through the generated
    # driver source (newlines/quotes escaped by Python itself).
    insertion = (
        "\n"
        "    # ---- worker-d1-chain-relink merged-chain splices (D1 fix FIRST,\n"
        "    # F-R2 fix LAST; canonical 51-tuple order per worker-risk-e2e/\n"
        "    # D1-PROPAGATION.md section 4). Block texts verified verbatim\n"
        "    # against both fix-origin patches.json pins by the builder. ----\n"
        "    (\"d1-uf-block-append\",\n"
        "     " + repr(UF_ANCHOR) + ",\n"
        "     " + repr(UF_ANCHOR + uf_block) + "),\n"
        "    (\"fr2-block-append\",\n"
        "     " + repr(R2_ANCHOR) + ",\n"
        "     " + repr(R2_ANCHOR + r2_block) + "),\n"
    )
    # anchor verified unique above; insertion re-closes the MY_SPLICES list
    merged = text.replace(SPLICE_TAIL_ANCHOR,
                          "     'PATCHES_TAIL_HEAD =')," + insertion + "]")
    out = HERE / "compose_merged_relink.py"
    out.write_text(merged)

    receipt = {
        "worker": "worker-d1-chain-relink",
        "design": "worker-risk-e2e/D1-PROPAGATION.md section 4 (canonical order)",
        "vendored_driver": str(CONSOLE_DRIVER),
        "vendored_driver_sha256": CONSOLE_DRIVER_SHA,
        "merge_splices": [
            {"id": "d1-uf-block-append",
             "anchor_sha256": digest_bytes(UF_ANCHOR.encode()),
             "block_source": str(UF_DRIVER),
             "block_text_sha256": digest_bytes(uf_block.encode()),
             "tuples": log["uf"]},
            {"id": "fr2-block-append",
             "anchor_sha256": digest_bytes(R2_ANCHOR.encode()),
             "block_source": str(R2_DRIVER),
             "block_text_sha256": digest_bytes(r2_block.encode()),
             "tuples": log["r2"]},
        ],
        "expected_tuple_order": ["35 base", "6 e2e-*", "4 zb-*",
                                 "4 uf-*", "2 r2-*"],
        "merged_driver": str(out),
        "merged_driver_sha256": digest_bytes(out.read_bytes()),
    }
    (REC / "merged-script-patches.json").write_text(
        json.dumps(receipt, indent=2, sort_keys=True))
    print("merged driver written: %s (%s)" % (out, receipt["merged_driver_sha256"][:16]))

    env = dict(__import__("os").environ, MODBRIDGE_ROOT=str(ROOT))
    r = subprocess.run([sys.executable, str(out), "--flavor", "all"],
                       env=env, cwd=str(HERE))
    sys.exit(r.returncode)


if __name__ == "__main__":
    main()
