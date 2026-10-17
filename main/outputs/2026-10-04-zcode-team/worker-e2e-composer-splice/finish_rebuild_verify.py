#!/usr/bin/env python3
"""finish_rebuild_verify.py — worker-e2e-composer-splice (2026-10-04).

Completes the interrupted --flavor all rebuild after the spliced driver was
installed (the first attempt died at link_layout.write_linker_script's
exclusive-create against stale D1-era build/console artifacts; vendor() +
patch_modinst() had already produced the promoted core + receipt bytes).

Steps: archive stale build/console aside (no deletion); re-run the tree's own
build (MODBRIDGE_ROOT pinned, --flavor all — exact D1 recipe); AFTER inventory
vs the saved BEFORE inventory; post verification; provenance update.
"""
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys

WHAT = pathlib.Path("/WORKSPACE/.codex/worktrees/translation-core/what")
TEAM = WHAT / "outputs/2026-10-04-zcode-team"
ME = TEAM / "worker-e2e-composer-splice"
TREE = TEAM / "worker-d1-chain-relink"
REC = ME / "receipts"
ARC = ME / "archive"

LOG = []


def log(m):
    print(m)
    LOG.append(m)


def die(m):
    log("finish: FATAL: " + m)
    (REC / "06-post-verification.log").write_text("\n".join(LOG) + "\n")
    sys.exit(2)


def sha_file(p):
    h = hashlib.sha256()
    with pathlib.Path(p).open("rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def load_inv(path):
    inv = {}
    for line in pathlib.Path(path).read_text().splitlines():
        h, _, name = line.partition("  ")
        if h and name:
            inv[name] = h
    return inv


def tree_inventory():
    inv = {}
    for p in sorted(TREE.rglob("*")):
        if p.is_file() and ".git" not in p.parts:
            inv[str(p.relative_to(TREE))] = sha_file(p)
    return inv


def main():
    # ---- archive stale build outputs (D1 51-chain artifacts) ----
    stale = {}
    for flavor in ("console", "host"):
        d = TREE / "composition/build" / flavor
        if d.is_dir():
            for p in sorted(d.iterdir()):
                if p.is_file():
                    stale["build/%s/%s" % (flavor, p.name)] = sha_file(p)
    (ARC / "stale-51chain-build-artifacts-shasums.txt").write_text(
        "".join("%s  %s\n" % (h, k) for k, h in sorted(stale.items())))
    console = TREE / "composition/build/console"
    aside = ARC / "stale-51chain-build-console"
    if console.is_dir():
        if aside.exists():
            shutil.rmtree(aside)
        shutil.move(str(console), str(aside))
        log("ARCHIVED stale build/console aside -> %s (%d files, shas in "
            "stale-51chain-build-artifacts-shasums.txt; D1 ELF e3489297... "
            "remains pinned by D1 receipts)" % (aside.name, len(list(aside.iterdir()))))
    host_bin = TREE / "composition/build/host/modinst_composed"
    if host_bin.is_file():
        log("stale host binary (pre-rebuild): %s" % sha_file(host_bin)[:16])

    # ---- the tree's own build, exact D1 recipe ----
    env = dict(__import__("os").environ, MODBRIDGE_ROOT=str(WHAT))
    try:
        r = subprocess.run([sys.executable, str(TREE / "compose_merged_relink.py"),
                            "--flavor", "all"], env=env, cwd=str(TREE),
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=560)
    except subprocess.TimeoutExpired as e:
        (REC / "03-build-all.log").write_text(
            (e.stdout or b"").decode(errors="replace") + "\nTIMEOUT")
        die("build timed out")
    (REC / "03-build-all.log").write_text(r.stdout.decode(errors="replace"))
    log("BUILD exit=%d (transcript: receipts/03-build-all.log)" % r.returncode)
    if r.returncode:
        die("tree build failed again — see receipts/03-build-all.log")

    # ---- AFTER inventory + diff vs saved BEFORE ----
    before = load_inv(REC / "02-before-inventory.txt")
    after = tree_inventory()
    (REC / "04-after-inventory.txt").write_text(
        "".join("%s  %s\n" % (h, k) for k, h in sorted(after.items())))
    changed = sorted(k for k in set(before) | set(after)
                     if before.get(k) != after.get(k))
    (REC / "05-inventory-diff.txt").write_text(
        "".join("%s: %s -> %s\n" % (k, (before.get(k) or "-")[:16],
                                    (after.get(k) or "-")[:16])
                for k in changed))
    log("AFTER inventory: %d files (before %d), %d changed" %
        (len(after), len(before), len(changed)))

    # ---- post verification ----
    core = TREE / "composition/vendored/modinst_core.c"
    receipt = TREE / "composition/receipts/patches.json"
    prom = TEAM / "worker-e2e-payload-build/patches.json.new"
    owned = TEAM / "worker-e2e-config-entry/owned/modinst_core.c"
    hb = json.loads((TREE / "composition/receipts/host-build.json").read_text())
    prov_json = json.loads(
        (TREE / "composition/receipts/console-import-providers.json").read_text())
    spliced_rec = json.loads(
        (TREE / "receipts/console-bridge-script-patches.json").read_text())
    host_bin = pathlib.Path(hb["binary"])

    checks = [
        ("rebuilt core sha == 43ea66c1 (promoted endpoint)",
         sha_file(core) == "43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0",
         sha_file(core)),
        ("rebuilt receipt sha == 63c28979 (promoted receipt)",
         sha_file(receipt) == "63c28979d21a9a563f29334cd094568f11fd6cc2d56abfe31158ec14a42c80f9",
         sha_file(receipt)),
        ("receipt BYTE-IDENTICAL to promoted patches.json.new",
         receipt.read_bytes() == prom.read_bytes(), ""),
        ("core BYTE-IDENTICAL to owned config-entry core",
         core.read_bytes() == owned.read_bytes(), ""),
        ("host-build receipt: binary sha recorded",
         bool(hb.get("binary_sha256")), hb.get("binary_sha256", "")),
        ("host binary on disk == host-build receipt",
         host_bin.is_file() and sha_file(host_bin) == hb["binary_sha256"],
         hb.get("binary_sha256", "")),
        ("console ELF: imports all resolved (missing=[]) and sha recorded",
         prov_json.get("missing") == [] and bool(prov_json.get("elf_sha256")),
         "%s imports=%d" % (prov_json.get("elf_sha256", "")[:16],
                            len(prov_json.get("imports", {})))),
        ("console ELF on disk == import-providers receipt",
         sha_file(TREE / "composition/build/console/"
                  "modbridge-installer.payload.elf") == prov_json["elf_sha256"],
         prov_json["elf_sha256"]),
        ("tree driver records the 3 g1 splices",
         [s["id"] for s in spliced_rec["splices"]][-3:] ==
         ["g1-block-append", "g1-receipt-promoted-form-def",
          "g1-receipt-promoted-form-seq"],
         str([s["id"] for s in spliced_rec["splices"]][-3:])),
        ("regenerated script receipt records the promoted-form splices",
         any("g1" in s.get("id", "") for s in json.loads(
             (TREE / "e2e-receipts/script-patches.json").read_text())["splices"]),
         ""),
    ]
    ok = True
    for label, passed, detail in checks:
        log("POST %s: %s  %s" % ("PASS" if passed else "FAIL", label, detail))
        ok = ok and passed
    if not ok:
        die("post checks failed")

    # ---- provenance update ----
    prov_path = REC / "g1-splice-provenance.json"
    prov = json.loads(prov_path.read_text())
    inner_sha = sha_file(TREE / "composition_zb_console_inner.py")
    prov["apply"] = {
        "installed_driver_sha256": sha_file(TREE / "compose_merged_relink.py"),
        "rebuilt_core_sha256": sha_file(core),
        "rebuilt_receipt_sha256": sha_file(receipt),
        "receipt_byte_identical_promoted": True,
        "core_byte_identical_owned_config_entry": True,
        "regenerated_inner_sha256": inner_sha,
        "host_binary_sha256": hb["binary_sha256"],
        "host_binary_bytes": host_bin.stat().st_size,
        "console_elf_sha256": prov_json["elf_sha256"],
        "console_elf_bytes": prov_json["elf_bytes"],
        "console_imports": len(prov_json["imports"]),
        "console_imports_missing": prov_json["missing"],
        "stale_51chain_build_archived_aside": sorted(stale),
        "changed_files_vs_before": changed,
        "all_post_checks_pass": True,
        "build_transcript": "receipts/03-build-all.log"}
    prov_path.write_text(json.dumps(prov, indent=2, sort_keys=True))
    (REC / "06-post-verification.log").write_text("\n".join(LOG) + "\n")
    log("RECONCILIATION: promoted receipt (63c28979) == spliced tree "
        "(regenerated receipt byte-identical) == byte-exact rebuild "
        "(core 43ea66c1 == owned config-entry revision) — divergence closed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
