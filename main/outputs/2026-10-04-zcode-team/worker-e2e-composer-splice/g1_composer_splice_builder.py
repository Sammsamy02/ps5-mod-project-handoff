#!/usr/bin/env python3
"""g1_composer_splice_builder.py — worker-e2e-composer-splice (2026-10-04).

Executes PROMOTION-PREP.md sec 5 step 4: splices the verified G1 chain
(45c04d08 -> 330d47f2 -> 91a122a8 -> 43ea66c1) into the worker-d1-chain-relink
composer, exactly the way compose_merged_relink_builder.py spliced
UF_BLOCK/R2_BLOCK (AST verbatim extraction, pin verification, recorded
MY_SPLICES entries), so the tree rebuilds byte-reproducibly to the promoted
receipt endpoint 43ea66c1 / promoted receipt 63c28979.

Three recorded MY_SPLICES entries appended to compose_merged_relink.py:
  g1-block-append               : EXTRA_PATCHES += the 3 G1 4-tuples after
                                  r2-install-status-ok (generator source level)
  g1-receipt-promoted-form-def  : defines S_RECEIPT_NEWLINE /
                                  S_G1_PATCHED_LABEL / S_G1_BYTE_COUNTS
  g1-receipt-promoted-form-seq  : applies them in adapt_script's splice loop

The three receipt-form splices make the REGENERATED receipts/patches.json
byte-identical to the promoted receipt (63c28979..., produced by
worker-e2e-payload-build/g1_tuples_build.py): (a) trailing newline after
json.dumps(indent=2); (b) the promoted `patched` label for the config-entry
revision (the rebuilt composition/vendored/modinst_core.c is byte-identical
to it); (c) UTF-8 BYTE lengths for the G1 tuples' new_bytes (consume new =
12862 B — one em-dash — vs 12860 chars), keeping the inherited 51-tuple
prefix's delivered char-length convention untouched. The dry run caught (b)
and (c) BEFORE any tree write: without them a rebuild would emit
content-different receipt bytes and keep the tree divergent.

Full DRY RUN in this worker dir before any tree write: the spliced driver is
exec'd (module level only), the whole adapt_driver -> adapt_script ->
patch_modinst chain is simulated, the 54-tuple core is rebuilt in memory and
the receipt is regenerated in a sandbox — both must hash exactly to the
promoted pins, or the builder dies before touching the tree.

  python3 g1_composer_splice_builder.py           # dry run + provenance receipt
  python3 g1_composer_splice_builder.py --apply   # archive + install + rebuild

Writes (apply mode): tree driver, tree composition/** via the tree's own
build, in-tree .bak archives, and this worker dir only. No console, no
network, no nested subagents.
"""
import ast
import hashlib
import importlib.util
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

DRIVER = TREE / "compose_merged_relink.py"
INNER = TREE / "composition_zb_console_inner.py"
CORE = TREE / "composition/vendored/modinst_core.c"
PROMOTED = TREE / "composition/receipts/patches.json"
BAK51 = TREE / "composition/receipts/patches.json.51.bak"
E2E_SCRIPT = TEAM / "worker-e2e-final/compose_e2e.py"
ORIG_SCRIPT = TEAM / "worker-installer-composition/compose_installer.py"
G1_SRC = TEAM / "worker-e2e-payload-build/g1_block.py"
G1_COPY = TEAM / "worker-e2e-suite-completion/promotion/g1_block.copy.py"
PATCHES_NEW = TEAM / "worker-e2e-payload-build/patches.json.new"
OWNED_CORE = TEAM / "worker-e2e-config-entry/owned/modinst_core.c"
VENDOR_MANIFEST = TREE / "composition/receipts/vendor-manifest.json"

SHA_DRIVER = "aedcd8eaf899bd20f200415728c676fb82505844a6b74b2bd2cf92c16c74492e"
SHA_INNER = "de3a4a3dbaaf7200971f0a85345678e928b378bbab0a592fb6bed3ce69d8988b"
SHA_CORE = "45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d"
SHA_BAK51 = "252ad699d183a1b63cc2118edff51b8f0fbf5cb48a22dfd0f40b1d7341340c3b"
SHA_G1SRC = "9f46b66aeb38a1d4737888da8bf91a1c15437d258d8b71aec68abc6c4bbe85ca"
SHA_PROMOTED = "63c28979d21a9a563f29334cd094568f11fd6cc2d56abfe31158ec14a42c80f9"
SHA_ENDPOINT = "43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0"
REPLAY_PIN = "897cf0eb85ded87c3fc197e64b3bdd80d5f9952d52585c51b95e9200f96e1cde"
G1_IDS = ["g1-cmd-entry-parse", "g1-cmd-entry-allowlist", "g1-cmd-entry-consume"]

LOG_LINES = []


def log(msg):
    print(msg)
    LOG_LINES.append(msg)


def die(msg):
    log("g1-splice-builder: FATAL: " + msg)
    flush_log()
    sys.exit(2)


def flush_log():
    REC.mkdir(parents=True, exist_ok=True)
    (REC / "00-builder-transcript.log").write_text("\n".join(LOG_LINES) + "\n")


def sha_bytes(b):
    return hashlib.sha256(b).hexdigest()


def sha_file(p):
    return sha_bytes(pathlib.Path(p).read_bytes())


def extract_block(path, name):
    """AST verbatim extraction of a module-level assignment value."""
    tree = ast.parse(pathlib.Path(path).read_text())
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id == name:
                    return ast.literal_eval(node.value)
    die("block %s not found in %s" % (name, path))


def exec_module(path, ns_name):
    ns = {"__name__": ns_name, "__file__": str(path)}
    exec(compile(pathlib.Path(path).read_text(), str(path), "exec"), ns)
    return ns


def input_gates():
    checks = [
        (DRIVER, SHA_DRIVER, "tree driver (pristine 51-chain)"),
        (INNER, SHA_INNER, "tree inner (consolidated 51-tuple)"),
        (CORE, SHA_CORE, "tree vendored core (pristine)"),
        (PROMOTED, SHA_PROMOTED, "promoted receipt"),
        (BAK51, SHA_BAK51, "51-tuple receipt backup"),
        (G1_SRC, SHA_G1SRC, "G1_BLOCK source"),
        (PATCHES_NEW, SHA_PROMOTED, "worker-e2e-payload-build/patches.json.new"),
    ]
    for p, want, label in checks:
        got = sha_file(p)
        if got != want:
            die("%s sha %s != pinned %s" % (label, got, want))
        log("GATE ok: %s = %s" % (label, got[:16]))
    if G1_COPY.exists() and sha_file(G1_COPY) != SHA_G1SRC:
        die("g1_block.copy.py diverges from g1_block.py")
    if PROMOTED.read_bytes() != PATCHES_NEW.read_bytes():
        die("promoted receipt != worker-e2e-payload-build/patches.json.new")
    prom = json.loads(PROMOTED.read_text())
    if len(prom["patches"]) != 54 or prom["patched_sha256"] != SHA_ENDPOINT:
        die("promoted receipt shape unexpected")
    if [p["id"] for p in prom["patches"][-3:]] != G1_IDS:
        die("promoted receipt tail ids unexpected")
    # upstream vendored inputs must still match the D1 run's vendor manifest
    vm = json.loads(VENDOR_MANIFEST.read_text())
    for rel, src in (("modinst.c", TEAM / "worker-7-installer/src/modinst.c"),
                     ("ps5_prepare_bridge.c",
                      TEAM / "worker-installer-ps5io/src/ps5_prepare_bridge.c")):
        pin = vm[rel]["sha256"]
        got = sha_file(src)
        if got != pin:
            die("upstream %s sha %s != vendor-manifest pin %s" % (rel, got, pin))
        log("GATE ok: upstream %s matches vendor-manifest pin %s" % (rel, pin[:16]))
    log("INPUT GATES: all pass")


def build_spliced_driver_source():
    """Returns (new_driver_source, provenance dict). No writes."""
    g1 = extract_block(G1_SRC, "G1_BLOCK")
    if len(g1) != 3 or [t[0] for t in g1] != G1_IDS:
        die("G1_BLOCK ids/arity unexpected")
    prom = json.loads(PROMOTED.read_text())
    pins = {p["id"]: p for p in prom["patches"][-3:]}
    for pid, old, new in g1:
        pin = pins[pid]
        if sha_bytes(old.encode()) != pin["old_sha256"]:
            die("G1 tuple %s OLD diverges from promoted pin" % pid)
        if sha_bytes(new.encode()) != pin["new_sha256"]:
            die("G1 tuple %s NEW diverges from promoted pin" % pid)
        if pin["occurrences"] != 1:
            die("G1 tuple %s occurrences != 1" % pid)
        log("PIN-CHECK ok: %s old=%s… new=%s…" % (
            pid, pin["old_sha256"][:12], pin["new_sha256"][:12]))

    drv_mod = exec_module(DRIVER, "d1driver_import")
    zb_splices = drv_mod["ZB_SPLICES"]
    my_splices = drv_mod["MY_SPLICES"]

    # simulate adapt_driver: E2E text + ZB replay (byte-exact gate) + MY
    text = E2E_SCRIPT.read_text()
    for pid, old, new in zb_splices:
        n = text.count(old)
        if n != 1:
            die("replayed splice %s anchor count %d (want 1)" % (pid, n))
        text = text.replace(old, new)
    replay = sha_bytes(text.encode())
    if replay != REPLAY_PIN:
        die("ZB replay sha %s != delivered pin %s" % (replay, REPLAY_PIN))
    log("REPLAY-PROOF ok: intermediate = %s" % replay[:16])
    for pid, old, new in my_splices:
        n = text.count(old)
        if n != 1:
            die("existing MY splice %s anchor count %d (want 1)" % (pid, n))
        text = text.replace(old, new)
    gen_pristine = text  # what adapt_driver writes today (51-chain generator)

    # --- new splice anchors, all asserted unique on the post-splice text ---
    r2_tail = '    ("r2-install-status-ok", R2_STATUS_OLD, R2_STATUS_NEW, 1),\n]'
    loop_tail = '                          S_CONSOLE_TARGETS):'
    def_anchor = '".ps5.o")\')\nPATCHES_TAIL_HEAD = '
    for label, a, t in (("r2-tail", r2_tail, text),
                        ("loop-tail", loop_tail, text),
                        ("def-anchor", def_anchor, text)):
        n = t.count(a)
        if n != 1:
            die("new %s anchor count %d in post-splice generator text (want 1)"
                % (label, n))
    nl_line = '    (REC / "patches.json").write_text(json.dumps(receipt, indent=2))'
    ci = ORIG_SCRIPT.read_text()
    if ci.count(nl_line) != 1:
        die("write_text anchor count %d in compose_installer.py (want 1)"
            % ci.count(nl_line))
    patched_line = '               "patched": "composition/vendored/modinst_core.c",'
    if ci.count(patched_line) != 1:
        die("patched-label anchor count %d in compose_installer.py (want 1)"
            % ci.count(patched_line))
    bytelen_line = '                        "old_bytes": len(old), "new_bytes": len(new)})'
    if ci.count(bytelen_line) != 1:
        die("byte-count anchor count %d in compose_installer.py (want 1)"
            % ci.count(bytelen_line))
    log("ANCHOR-CHECK ok: r2-tail / loop-tail / def-anchor / write_text / "
        "patched-label / byte-count all == 1")

    g1_block_src = (
        "\n"
        "# ---- worker-e2e-config-entry (G1) config-file-driven entry patch\n"
        "# tuples, applied after the F-R2 chain. Promoted receipt tail\n"
        "# (patches.json.new, sha 63c28979...; endpoint 43ea66c1...). Tuples\n"
        "# extracted VERBATIM from worker-e2e-payload-build/g1_block.py via AST\n"
        "# and verified against the promoted pins by\n"
        "# worker-e2e-composer-splice/g1_composer_splice_builder.py. ----\n"
        "EXTRA_PATCHES += [\n"
        + "".join("    (%r, %r, %r, 1),\n" % (pid, old, new)
                  for pid, old, new in g1)
        + "]")
    nl_comment = ("   # worker-e2e-composer-splice: trailing newline matches the"
                  " promoted receipt form (worker-e2e-payload-build/"
                  "g1_tuples_build.py writes json.dumps(pj, indent=2) + newline)")
    nl_literal = '"\\n"'
    nl_line_new = ('    (REC / "patches.json").write_text(json.dumps(receipt,'
                   ' indent=2) + ' + nl_literal + ')' + nl_comment)
    patched_label_new = ('               "patched": "worker-e2e-payload-build/'
                         'src/modinst_core.c (config-entry revision; = '
                         'worker-e2e-config-entry owned/modinst_core.c)",'
                         '   # g1 splice: promoted label; the local'
                         ' composition/vendored/modinst_core.c is byte-identical')
    bytelen_line_new = ('                        "old_bytes": len(old),'
                        ' "new_bytes": len(new.encode("utf-8")) if'
                        ' pid.startswith("g1-cmd-entry-") else len(new)})'
                        '  # g1 splice: promoted tail records UTF-8 BYTE lengths'
                        ' (consume new = 12862 B); inherited prefix keeps the'
                        ' delivered char-length convention')
    s_receipt_newline_def = (
        'S_RECEIPT_NEWLINE = ("g1-promoted-receipt-newline",\n'
        "                     " + repr(nl_line) + ",\n"
        "                     " + repr(nl_line_new) + ")\n"
        'S_G1_PATCHED_LABEL = ("g1-promoted-patched-label",\n'
        "                      " + repr(patched_line) + ",\n"
        "                      " + repr(patched_label_new) + ")\n"
        'S_G1_BYTE_COUNTS = ("g1-promoted-g1-byte-counts",\n'
        "                    " + repr(bytelen_line) + ",\n"
        "                    " + repr(bytelen_line_new) + ")\n")
    loop_tail_new = ('                          S_CONSOLE_TARGETS,\n'
                     '                          S_RECEIPT_NEWLINE,\n'
                     '                          S_G1_PATCHED_LABEL,\n'
                     '                          S_G1_BYTE_COUNTS):')

    # --- extend MY_SPLICES in the driver SOURCE (D1 builder insertion form) --
    def_new = def_anchor.replace(
        "PATCHES_TAIL_HEAD = ",
        s_receipt_newline_def + "PATCHES_TAIL_HEAD = ")
    entries = [
        ("g1-block-append", r2_tail, r2_tail + g1_block_src),
        ("g1-receipt-promoted-form-def", def_anchor, def_new),
        ("g1-receipt-promoted-form-seq", loop_tail, loop_tail_new),
    ]
    drv_text = DRIVER.read_text()
    tail_anchor = 'R2_STATUS_NEW, 1),\\n]\\n\'),\n]'
    if drv_text.count(tail_anchor) != 1:
        die("driver MY_SPLICES tail anchor count %d (want 1)"
            % drv_text.count(tail_anchor))
    insertion = "".join(
        '    ("%s",\n     %s,\n     %s),\n' % (pid, repr(o), repr(nw))
        for pid, o, nw in entries)
    new_driver_source = drv_text.replace(
        tail_anchor,
        tail_anchor[:-len(",\n]")] + ",\n" + insertion + "]")
    compile(new_driver_source, "spliced-driver", "exec")  # syntax gate
    log("SYNTAX ok: spliced driver source compiles")

    # --- execute-level dry run of the FULL chain (no tree writes) -----------
    ns = {"__name__": "d1driver_dry", "__file__": str(DRIVER)}
    exec(compile(new_driver_source, str(DRIVER), "exec"), ns)
    new_my = ns["MY_SPLICES"]
    if [s[0] for s in new_my][-3:] != ["g1-block-append",
                                       "g1-receipt-promoted-form-def",
                                       "g1-receipt-promoted-form-seq"]:
        die("new MY_SPLICES entries not at tail")
    gtext = E2E_SCRIPT.read_text()
    for pid, old, new in ns["ZB_SPLICES"] + list(new_my):
        n = gtext.count(old)
        if n != 1:
            die("dry-run splice %s anchor count %d (want 1)" % (pid, n))
        gtext = gtext.replace(old, new)
    if sha_bytes(gtext.encode()) == sha_bytes(gen_pristine.encode()):
        die("spliced generator text == pristine (splice had no effect)")
    gns = {"__name__": "gen_dry", "__file__": str(TREE / INNER.name)}
    exec(compile(gtext, "generator-dry", "exec"), gns)
    ep = gns["EXTRA_PATCHES"]
    log("DRY-RUN generator EXTRA_PATCHES: %d tuples, tail ids %s" %
        (len(ep), [p[0] for p in ep[-3:]]))
    if [p[0] for p in ep][-3:] != G1_IDS:
        die("generator EXTRA_PATCHES tail != G1 ids")
    if any(len(p) != 4 or p[3] != 1 for p in ep[-3:]):
        die("G1 tuples not 4-tuples with occurrences 1")
    for pid, old, new, _ in ep[-3:]:
        pin = pins[pid]
        if (sha_bytes(old.encode()) != pin["old_sha256"]
                or sha_bytes(new.encode()) != pin["new_sha256"]):
            die("generator G1 tuple %s diverges from pins" % pid)

    # simulate adapt_script (generator): S_* splices + inline EXTRA_PATCHES
    src = gns["ORIG_SCRIPT"].read_text()
    for pid, old, new in (gns["S_DL_DEF"], gns["S_MKDIR"], gns["S_VENDOR"],
                          gns["S_COREFLAGS"], gns["S_TARGETS"],
                          gns["S_CONSOLE_TARGETS"], gns["S_RECEIPT_NEWLINE"],
                          gns["S_G1_PATCHED_LABEL"], gns["S_G1_BYTE_COUNTS"]):
        n = src.count(old)
        if n != 1:
            die("adapt_script splice %s anchor count %d (want 1)" % (pid, n))
        src = src.replace(old, new)
    head = gns["PATCHES_TAIL_HEAD"][:-1]
    extra = "".join("    %r,\n" % (p,) for p in ep)
    src = src.replace(gns["PATCHES_TAIL_HEAD"], head + "\n" + extra + "]")
    cns = {"__name__": "inner_dry", "__file__": str(TREE / INNER.name)}
    exec(compile(src, "inner-dry", "exec"), cns)
    patches = cns["PATCHES"]
    log("DRY-RUN consolidated inner PATCHES: %d tuples, tail ids %s" %
        (len(patches), [p[0] for p in patches[-3:]]))
    if len(patches) != 54:
        die("consolidated PATCHES %d != 54" % len(patches))

    # 54-tuple sequential core rebuild in memory (the exact patch chain)
    core_src = (TEAM / "worker-7-installer/src/modinst.c").read_text()
    chain = [sha_bytes(core_src.encode())]
    for pid, old, new, count in patches:
        n = core_src.count(old)
        if n != count:
            die("dry-run patch %s: found %d occurrences (want %d)"
                % (pid, n, count))
        core_src = core_src.replace(old, new)
        chain.append(sha_bytes(core_src.encode()))
    if chain[-1] != SHA_ENDPOINT:
        die("dry-run core final sha %s != promoted endpoint" % chain[-1])
    if core_src != OWNED_CORE.read_text():
        die("dry-run core != worker-e2e-config-entry owned core (byte cmp)")
    log("DRY-RUN core chain tail: 51-tuple core %s -> %s -> %s -> %s (final)" %
        (chain[51][:8], chain[52][:8], chain[53][:8], chain[54][:8]))
    log("DRY-RUN core == owned config-entry revision: byte-identical")

    # sandbox patch_modinst: the EXACT receipt writer of the spliced inner
    sb = ME / "dryrun"
    if sb.exists():
        shutil.rmtree(sb)
    (sb / "vendored").mkdir(parents=True)
    (sb / "receipts").mkdir(parents=True)
    shutil.copyfile(TEAM / "worker-7-installer/src/modinst.c",
                    sb / "vendored/modinst.c")
    shutil.copyfile(TEAM / "worker-installer-ps5io/src/ps5_prepare_bridge.c",
                    sb / "vendored/ps5_prepare_bridge.c")
    cns["VEND"] = sb / "vendored"
    cns["REC"] = sb / "receipts"
    cns["HERE"] = sb
    cns["patch_modinst"]()
    got_core = sha_file(sb / "vendored/modinst_core.c")
    got_rec = sha_file(sb / "receipts/patches.json")
    if got_core != SHA_ENDPOINT:
        die("sandbox patch_modinst core %s != endpoint" % got_core)
    if got_rec != SHA_PROMOTED:
        die("sandbox patch_modinst receipt %s != promoted 63c28979..." % got_rec)
    if (sb / "receipts/patches.json").read_bytes() != PROMOTED.read_bytes():
        die("sandbox receipt bytes != promoted receipt bytes")
    log("DRY-RUN receipt: sandbox patch_modinst output = %s (byte-identical "
        "to promoted)" % got_rec[:16])
    log("FULL DRY RUN: PASS — tree rebuild will reproduce endpoint + promoted "
        "receipt byte-exactly")

    prov = {
        "worker": "worker-e2e-composer-splice",
        "spec": "worker-e2e-suite-completion/PROMOTION-PREP.md sec 5 step 4",
        "g1_block_source": str(G1_SRC),
        "g1_block_sha256": SHA_G1SRC,
        "g1_tuples": [{"id": pid,
                       "old_sha256": pins[pid]["old_sha256"],
                       "new_sha256": pins[pid]["new_sha256"],
                       "occurrences": 1,
                       "old_bytes": len(old), "new_bytes": len(new),
                       "pin_verified": True} for pid, old, new in g1],
        "driver_before_sha256": SHA_DRIVER,
        "new_my_splices": [
            {"id": "g1-block-append",
             "anchor_sha256": sha_bytes(r2_tail.encode()),
             "appends": "EXTRA_PATCHES += 3 G1 4-tuples after r2-install-status-ok"},
            {"id": "g1-receipt-promoted-form-def",
             "anchor_sha256": sha_bytes(def_anchor.encode()),
             "defines": "S_RECEIPT_NEWLINE, S_G1_PATCHED_LABEL, "
                        "S_G1_BYTE_COUNTS"},
            {"id": "g1-receipt-promoted-form-seq",
             "anchor_sha256": sha_bytes(loop_tail.encode()),
             "applies": "the three S_* splices in adapt_script loop"}],
        "receipt_promoted_form_rationale":
            "promoted receipt (g1_tuples_build.py) form differs from what "
            "patch_modinst regenerates in 3 fields: trailing newline; the "
            "`patched` label (promoted names the config-entry revision the "
            "vendored core is byte-identical to); G1 new_bytes recorded as "
            "UTF-8 byte lengths (consume 12862 B vs 12860 chars). Without "
            "these recorded splices a rebuild would emit a content-different "
            "receipt (sha != 63c28979) and the receipt-tree divergence would "
            "stay open",
        "dry_run": {
            "generator_extra_patches": len(ep),
            "consolidated_patches": len(patches),
            "core_chain": chain[:4],
            "core_final_sha256": chain[-1],
            "core_byte_equal_owned_config_entry": True,
            "sandbox_receipt_sha256": got_rec,
            "sandbox_receipt_byte_equal_promoted": True},
        "expected_driver_after_sha256": sha_bytes(new_driver_source.encode()),
    }
    return new_driver_source, prov


def tree_inventory():
    inv = {}
    for p in sorted(TREE.rglob("*")):
        if p.is_file() and ".git" not in p.parts:
            inv[str(p.relative_to(TREE))] = sha_file(p)
    return inv


def main():
    apply = "--apply" in sys.argv
    log("== g1_composer_splice_builder (apply=%s) ==" % apply)
    input_gates()
    new_driver_source, prov = build_spliced_driver_source()
    prov["mode"] = "apply" if apply else "dry-run"

    if not apply:
        REC.mkdir(parents=True, exist_ok=True)
        (REC / "g1-splice-provenance.json").write_text(
            json.dumps(prov, indent=2, sort_keys=True))
        (REC / "01-dry-run-proof.log").write_text("\n".join(LOG_LINES) + "\n")
        log("dry run complete; provenance + transcript written under %s" % REC)
        flush_log()
        return 0

    # ---- BEFORE archive + inventory (before ANY tree write) ----
    ARC.mkdir(parents=True, exist_ok=True)
    REC.mkdir(parents=True, exist_ok=True)
    before = tree_inventory()
    (REC / "02-before-inventory.txt").write_text(
        "".join("%s  %s\n" % (h, k) for k, h in sorted(before.items())))
    log("BEFORE inventory: %d files" % len(before))
    if before["compose_merged_relink.py"] != SHA_DRIVER:
        die("driver changed between gates and archive (aborting)")
    shutil.copyfile(DRIVER, ARC / ("compose_merged_relink.pristine-%s.py"
                                   % SHA_DRIVER[:8]))
    shutil.copyfile(DRIVER, TREE / "compose_merged_relink.py.51chain.bak")
    shutil.copyfile(INNER, ARC / ("composition_zb_console_inner.pristine-%s.py"
                                  % SHA_INNER[:8]))
    shutil.copyfile(CORE, ARC / ("modinst_core.pristine-%s.c" % SHA_CORE[:8]))
    shutil.copyfile(CORE, TREE / "composition/receipts/"
                    "modinst_core.c.45c04d08.pristine.bak")
    log("ARCHIVED pristine driver/inner/core (worker archive/ + in-tree .bak)")

    # ---- install the spliced driver ----
    DRIVER.write_text(new_driver_source)
    got = sha_file(DRIVER)
    if got != prov["expected_driver_after_sha256"]:
        die("installed driver sha %s != expected" % got)
    log("INSTALLED spliced driver: %s (%s)" % (DRIVER, got[:16]))

    # ---- the tree's own build, exact D1 recipe ----
    env = dict(__import__("os").environ, MODBRIDGE_ROOT=str(WHAT))
    r = subprocess.run([sys.executable, str(DRIVER), "--flavor", "all"],
                       env=env, cwd=str(TREE), stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=590)
    (REC / "03-build-all.log").write_text(r.stdout.decode(errors="replace"))
    log("BUILD exit=%d (transcript: receipts/03-build-all.log)" % r.returncode)
    if r.returncode:
        die("tree build failed — see receipts/03-build-all.log; rollback via "
            "REPORT.md instructions")

    # ---- AFTER inventory + post verification ----
    after = tree_inventory()
    (REC / "04-after-inventory.txt").write_text(
        "".join("%s  %s\n" % (h, k) for k, h in sorted(after.items())))
    changed = sorted(k for k in set(before) | set(after)
                     if before.get(k) != after.get(k))
    (REC / "05-inventory-diff.txt").write_text(
        "".join("%s: %s -> %s\n" % (k, before.get(k, "-")[:16],
                                    after.get(k, "-")[:16]) for k in changed))
    log("AFTER inventory: %d files, %d changed" % (len(after), len(changed)))

    checks = []
    core_sha = sha_file(CORE)
    checks.append(("rebuilt core sha == 43ea66c1...",
                   core_sha == SHA_ENDPOINT, core_sha))
    rec_sha = sha_file(PROMOTED)
    checks.append(("rebuilt receipt sha == 63c28979...",
                   rec_sha == SHA_PROMOTED, rec_sha))
    checks.append(("rebuilt receipt byte-identical to patches.json.new",
                   PROMOTED.read_bytes() == PATCHES_NEW.read_bytes(), ""))
    checks.append(("rebuilt core byte-identical to owned config-entry core",
                   CORE.read_bytes() == OWNED_CORE.read_bytes(), ""))
    hb = json.loads((TREE / "composition/receipts/host-build.json").read_text())
    cb = json.loads((TREE / "composition/receipts/console-build.json").read_text())
    checks.append(("host-build.json present, binary sha recorded",
                   bool(hb.get("binary_sha256")), hb["binary_sha256"]))
    checks.append(("console-build.json present, elf sha recorded",
                   bool(cb.get("elf_sha256") if "elf_sha256" in cb else
                        json.loads((TREE / "composition/receipts/"
                                    "console-import-providers.json")
                                   .read_text())["elf_sha256"]),
                   (TREE / "composition/receipts/console-import-providers.json")
                   .read_text() and json.loads(
                       (TREE / "composition/receipts/"
                        "console-import-providers.json").read_text())["elf_sha256"]))
    host_bin = pathlib.Path(hb["binary"])
    checks.append(("host binary on disk matches host-build.json",
                   host_bin.is_file() and sha_file(host_bin) == hb["binary_sha256"],
                   hb["binary_sha256"]))
    ok = True
    for label, passed, detail in checks:
        log("POST %s: %s %s" % ("PASS" if passed else "FAIL", label, detail))
        ok = ok and passed
    prov["apply"] = {
        "installed_driver_sha256": got,
        "rebuilt_core_sha256": core_sha,
        "rebuilt_receipt_sha256": rec_sha,
        "host_binary_sha256": hb["binary_sha256"],
        "console_elf_sha256": json.loads(
            (TREE / "composition/receipts/console-import-providers.json")
            .read_text())["elf_sha256"],
        "changed_files": changed,
        "all_post_checks_pass": ok}
    (REC / "g1-splice-provenance.json").write_text(
        json.dumps(prov, indent=2, sort_keys=True))
    (REC / "06-post-verification.log").write_text("\n".join(LOG_LINES) + "\n")
    if not ok:
        die("post checks failed")
    flush_log()
    log("SPLICE COMPLETE: receipt-tree divergence closed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
