#!/usr/bin/env python3
"""host_cmd_entry_test.py — host-side test of the config-file-driven entry DESIGN.

Faithful host emulation of worker-e2e-console-dry/E2E-CMD-SCHEMA.md:
  sec 2/3  top-level schema + options->argv mapping (mechanical, one-to-one)
  sec 5    bounded JSON subset grammar (no \\uXXXX, depth<=2, size<=4096)
  sec 6    refusal matrix (refuse = reason + nonzero, verb NOT executed)
  sec 1    entry rule emulation: file -> synthesized argv -> EXISTING main() parser

The entry itself is NOT compiled into any existing binary (verified: payload
e3489297... and its vendored sources contain no e2e-cmd reader). This test
therefore proves, on host:
  A. every staged console cmd file is inside the grammar (validator);
  B. the exact synthesized argv vectors drive the REAL host binary
     (worker-d1-chain-relink/composition/build/host/modinst_composed) through
     the full stage 0..6 sequence with exit 0 and the verbatim 33-line
     transcript of CONSOLE-E2E-EXECUTION.md;
  C. each dispatch's transcript is byte-identical to its hand-written argv
     twin (schema sec 8 item 3);
  D. every refusal-matrix case refuses with no state change.

All writes stay under this worker's outputs dir (host-test/scratch/).
"""

import hashlib
import os
import shutil
import subprocess
import sys

WS = "/WORKSPACE/.codex/worktrees/translation-core/what"
TEAM = f"{WS}/outputs/2026-10-04-zcode-team"
HERE = f"{TEAM}/worker-e2e-staging-final"
CMD = f"{HERE}/cmd"
BIN = f"{TEAM}/worker-d1-chain-relink/composition/build/host/modinst_composed"
ZIP = f"{WS}/.superpowers/native-observations/movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip"
MSKIN = f"{TEAM}/worker-e2e-rehearsal/rehearsal/run/mm-skin.mfb"
MLUA = f"{TEAM}/worker-e2e-rehearsal/rehearsal/run/mm-lua.mfb"

LINE_MAX_ = 4096
SIZE_MAX = 4096
MODS_MAX = 16
OPTIONS_MAX = 16
VERBS = {"records", "register-mod", "prepare", "prepare-archive",
         "install", "simulate-runtime", "recover"}
DENY_VERBS = {"make-bundle", "download"}  # download is job-file host/host-transport-only
DENY_OPTIONS = {"crash_at", "cancel_at", "cancel_after", "variant",
                "defaults", "state_name", "code", "detail"}
OPT_MAP = {  # key -> (argv flag, legal verbs)   [schema sec 3, verbatim]
    "title":          ("--title",          {"register-mod", "prepare", "prepare-archive", "install", "simulate-runtime"}),
    "id":             ("--id",             {"register-mod"}),
    "bundle":         ("--bundle",         {"register-mod"}),
    "archive":        ("--archive",        {"prepare-archive"}),
    "quarantine":     ("--quarantine",     {"prepare-archive"}),
    "mod":            ("--mod",            {"prepare-archive"}),
    "remove":         ("--remove",         {"install"}),
    "rt_state":       ("--rt-state",       {"simulate-runtime"}),
    "choices":        ("--choices",        {"prepare", "install"}),
    "no_auto_bundle": ("--no-auto-bundle", {"prepare-archive"}),
}


class Refuse(Exception):
    pass


# ---------------- bounded recursive-descent parser (schema sec 5) -------------

def rd_parse(buf: str):
    i = 0
    n = len(buf)

    def ws():
        nonlocal i
        while i < n and buf[i] in " \t\r\n":
            i += 1

    def expect(c):
        nonlocal i
        ws()
        if i >= n or buf[i] != c:
            raise Refuse(f"expected {c!r} at byte {i}")
        i += 1

    def pstring():
        nonlocal i
        expect('"')
        out = []
        while True:
            if i >= n:
                raise Refuse("unterminated string")
            c = buf[i]
            if c == '"':
                i += 1
                return "".join(out)
            if ord(c) < 0x20:
                raise Refuse("control char in string")
            if c == "\\":
                i += 1
                if i >= n:
                    raise Refuse("bad escape")
                e = buf[i]
                if e in '"\\/':
                    out.append(e)
                elif e == "u":
                    raise Refuse("\\uXXXX escape refuses (sec 5)")
                else:
                    raise Refuse(f"bad escape \\{e}")
                i += 1
            else:
                out.append(c)
                i += 1

    def pvalue(depth):
        nonlocal i
        ws()
        if i >= n:
            raise Refuse("unexpected end of input")
        c = buf[i]
        if c == '"':
            return pstring()
        if c == "{":
            if depth >= 2:
                raise Refuse("object depth > 2 refuses (sec 5)")
            i += 1
            obj = {}
            ws()
            if i < n and buf[i] == "}":
                i += 1
                return obj
            while True:
                ws()
                k = pstring()
                if k in obj:
                    raise Refuse(f"duplicate key {k!r}")
                ws()
                expect(":")
                obj[k] = pvalue(depth + 1)
                ws()
                if i < n and buf[i] == ",":
                    i += 1
                    continue
                expect("}")
                return obj
        if c == "[":
            if depth != 1:
                raise Refuse("array only legal as a top-level member: mods (sec 5)")
            i += 1
            arr = []
            ws()
            if i < n and buf[i] == "]":
                i += 1
                return arr
            while True:
                ws()
                arr.append(pvalue(depth + 1))
                ws()
                if i < n and buf[i] == ",":
                    i += 1
                    continue
                expect("]")
                return arr
        if buf.startswith("true", i):
            i += 4
            return True
        if buf.startswith("false", i) or buf.startswith("null", i):
            raise Refuse("only bool true is allowed (sec 5)")
        if c == "-" or c.isdigit():
            j = i + (1 if c == "-" else 0)
            if j >= n or not buf[j].isdigit():
                raise Refuse("bad int")
            while j < n and buf[j].isdigit():
                j += 1
            v = int(buf[i:j])
            i = j
            ws()
            if i < n and (buf[i].isdigit() or buf[i] in ".eE+-"):
                raise Refuse("only plain ints (sec 5)")
            return v
        raise Refuse(f"unexpected byte {c!r}")

    val = pvalue(0)
    ws()
    if i != n:
        raise Refuse("trailing bytes after top-level value")
    return val


# ---------------- validation (schema sec 2, 3, 6) -----------------------------

def validate(path: str):
    """Returns (obj, refuse_reason). refuse_reason None == valid."""
    try:
        if not os.path.isfile(path):
            raise Refuse("file absent (sec 1: usage() + EX_BAD, nothing executes)")
        raw = open(path, "rb").read()
        if raw.startswith(b"\xef\xbb\xbf"):
            raise Refuse("BOM refuses (sec 5)")
        if len(raw) > SIZE_MAX:
            raise Refuse(f"file {len(raw)} B > 4096 refuses before parsing (sec 5)")
        text = raw.decode("utf-8")  # strict
    except Refuse:
        raise
    except UnicodeDecodeError:
        raise Refuse("not valid UTF-8")
    obj = rd_parse(text)
    if not isinstance(obj, dict):
        raise Refuse("top level must be one object (sec 5)")
    for k in obj:
        if k not in {"schema", "verb", "state_root", "mods", "options"}:
            raise Refuse(f"unknown top-level key {k!r} (sec 6)")
    sch = obj.get("schema")
    if isinstance(sch, bool) or sch != 1:
        raise Refuse(f"schema must be exactly 1, got {sch!r} (sec 2)")
    verb = obj.get("verb")
    if not isinstance(verb, str):
        raise Refuse("verb must be string")
    if verb in DENY_VERBS:
        raise Refuse(f"deny-listed verb {verb!r} (sec 2)")
    if verb not in VERBS:
        raise Refuse(f"verb {verb!r} not in allow-list (sec 2)")
    sr = obj.get("state_root")
    if not isinstance(sr, dict):
        raise Refuse("state_root must be object (sec 2)")
    if set(sr) != {"state", "roots"}:
        raise Refuse("state_root keys must be exactly state,roots (sec 2)")
    for k, v in sr.items():
        if not isinstance(v, str):
            raise Refuse(f"state_root.{k} must be string")
        if not v.startswith("/"):
            raise Refuse(f"state_root.{k} must be absolute (sec 2)")
        if len(v) > LINE_MAX_:
            raise Refuse(f"state_root.{k} > LINE_MAX_")
    mods = obj.get("mods")
    if "mods" in obj:
        if not isinstance(mods, list):
            raise Refuse("mods must be array (sec 2)")
        if verb not in ("prepare", "install"):
            raise Refuse(f"mods present with verb {verb!r} refuses (sec 2)")
        if len(mods) > MODS_MAX:
            raise Refuse("mods > 16 entries (sec 3)")
        for m in mods:
            if not isinstance(m, str) or not m:
                raise Refuse("mods entries must be non-empty strings")
            if len(m) > LINE_MAX_:
                raise Refuse("mods entry > LINE_MAX_")
    opts = obj.get("options")
    if "options" in obj:
        if not isinstance(opts, dict):
            raise Refuse("options must be object (sec 2)")
        if len(opts) > OPTIONS_MAX:
            raise Refuse("options > 16 keys (sec 3)")
        for k, v in opts.items():
            if k in DENY_OPTIONS:
                raise Refuse(f"deny-listed option {k!r} (sec 2)")
            if k not in OPT_MAP:
                raise Refuse(f"unknown options key {k!r} (sec 6)")
            flag, legal = OPT_MAP[k]
            if verb not in legal:
                raise Refuse(f"options.{k} illegal for verb {verb!r} (eager refusal, sec 3)")
            if k == "no_auto_bundle":
                if v is not True:
                    raise Refuse("no_auto_bundle accepts bool true only (sec 5)")
            else:
                if not isinstance(v, str):
                    raise Refuse(f"options.{k} must be string")
                if not v:
                    raise Refuse(f"options.{k} empty")
                if len(v) > LINE_MAX_:
                    raise Refuse(f"options.{k} > LINE_MAX_")
    if mods is not None and isinstance(opts, dict) and "remove" in opts:
        raise Refuse("mods together with options.remove refuses (sec 3)")
    return obj


def synth_argv(obj, prog):
    """Sec 1/3: synthesized vector falling into the EXISTING parse loop."""
    argv = [prog, obj["verb"],
            "--state", obj["state_root"]["state"],
            "--roots", obj["state_root"]["roots"]]
    if "mods" in obj:
        argv += ["--mods", ",".join(obj["mods"])]
    opts = obj.get("options", {})
    for k in ("title", "id", "bundle", "archive", "quarantine", "mod",
              "remove", "rt_state", "choices"):
        if k in opts:
            argv += [OPT_MAP[k][0], opts[k]]
    if opts.get("no_auto_bundle") is True:
        argv += ["--no-auto-bundle"]
    return argv


def twin_argv(obj, prog):
    """Hand-written canonical CLI form of the same command (argv twin)."""
    v = obj["verb"]
    s, r = obj["state_root"]["state"], obj["state_root"]["roots"]
    o = obj.get("options", {})
    if v == "records":
        return [prog, "records", "--state", s, "--roots", r]
    if v == "register-mod":
        return [prog, "register-mod", "--id", o["id"], "--title", o["title"],
                "--bundle", o["bundle"], "--state", s, "--roots", r]
    if v == "prepare-archive":
        a = [prog, "prepare-archive", "--title", o["title"], "--archive", o["archive"],
             "--quarantine", o["quarantine"]]
        if "mod" in o:
            a += ["--mod", o["mod"]]
        if o.get("no_auto_bundle") is True:
            a += ["--no-auto-bundle"]
        return a + ["--state", s, "--roots", r]
    if v == "prepare":
        return [prog, "prepare", "--title", o["title"], "--mods", ",".join(obj["mods"]),
                "--state", s, "--roots", r]
    if v == "install":
        a = [prog, "install", "--title", o["title"]]
        if "mods" in obj:
            a += ["--mods", ",".join(obj["mods"])]
        if "remove" in o:
            a += ["--remove", o["remove"]]
        return a + ["--state", s, "--roots", r]
    if v == "simulate-runtime":
        return [prog, "simulate-runtime", "--title", o["title"], "--rt-state", o["rt_state"],
                "--state", s, "--roots", r]
    if v == "recover":
        return [prog, "recover", "--state", s, "--roots", r]
    raise Refuse(f"no twin for {v}")


# ---------------- transcript contract (CONSOLE-E2E-EXECUTION.md, verbatim) ----

T = lambda cmd, title, stage: f'{{"event":"begin","cmd":"{cmd}","title":"{title}","stage":"{stage}"}}'
E0 = '{"event":"end","exit":0}'
S2 = lambda detail: f'{{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"{detail}"}}'
S3 = f'{{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed,mm-skin,mm-lua"}}'
# CORRECTED stage-6 contract (host replay fact, sha 31a4764a... transcript):
# a removal emits the F-R2 status line WHILE MODS REMAIN (detail = remaining mods,
# records order); it is silent only when the removal empties the set (the zb-pak
# transcript's single removal was of the only mod — the EXECUTION doc over-generalized
# that to all removals). Correct all-green ledger = 35 lines, not 33.
S6 = lambda detail: f'{{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"{detail}"}}'
EXPECTED = [
    T("records", "", "stage-0-baseline"), E0,                                   # D0 / stage 0
    T("register-mod", "PPSA33286", "stage-1-download"), E0,                     # stage 1-owner
    T("prepare-archive", "PPSA33286", "stage-2-prepare"),                       # stage 2a
    S2("manifest-v1.bin"), S2("auto-bundle composed mfb"), E0,
    T("register-mod", "PPSA33286", "stage-1-download"), E0,                     # stage 2b (6 lines)
    T("register-mod", "PPSA33286", "stage-1-download"), E0,
    T("prepare", "PPSA33286", "stage-2-prepare"), E0,
    T("install", "PPSA33286", "stage-3-install"), S3, E0,                       # stage 3
    T("simulate-runtime", "PPSA33286", "stage-4-5-runtime-ack"), E0,            # stages 4-5 (8 lines)
    T("records", "", "stage-0-baseline"), E0,
    T("simulate-runtime", "PPSA33286", "stage-4-5-runtime-ack"), E0,
    T("records", "", "stage-0-baseline"), E0,
    T("install", "PPSA33286", "stage-3-install"), S6("movspeed,mm-skin"), E0,   # stage 6 (8 lines)
    T("install", "PPSA33286", "stage-3-install"), S6("movspeed"), E0,
    T("install", "PPSA33286", "stage-3-install"), E0,                           # empty set: silent
    T("records", "", "stage-0-baseline"), E0,                                   # closing records
]
RECOVER_2 = [T("recover", "", "recovery"), E0]

STAGES = [  # (label, cmd file)
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


def dispatch(sc, argv, log):
    p = subprocess.run(argv, executable=BIN, cwd=sc, capture_output=True, text=True, timeout=60)
    log.write(f"exit={p.returncode} argv={argv}\n")
    if p.stdout.strip():
        log.write("stdout:\n" + p.stdout)
    if p.stderr.strip():
        log.write("stderr:\n" + p.stderr)
    return p.returncode


def main():
    assert sha256_file(BIN), "binary missing"
    for f in (ZIP, MSKIN, MLUA):
        if not os.path.isfile(f):
            print(f"FATAL fixture missing: {f}")
            return 2
    os.makedirs(f"{HERE}/host-test", exist_ok=True)
    log = open(f"{HERE}/host-test/host-test.log", "w")
    receipt = []
    ok = True

    # ---- A: staged console bytes must validate --------------------------
    receipt.append("## A. staged console cmd files vs bounded grammar\n")
    receipt.append("| file | verdict |")
    receipt.append("|---|---|")
    for _, cf in STAGES + [("recover", "recover.json")]:
        try:
            validate(f"{CMD}/{cf}")
            receipt.append(f"| {cf} | OK |")
        except Refuse as e:
            ok = False
            receipt.append(f"| {cf} | REFUSE: {e} |")

    # ---- B+C: full replay, config-synth argv vs argv twin ----------------
    receipt.append("\n## B. host replay through the real binary (synth argv)\n")
    sc_a = fresh_scratch("a-config-synth")
    sc_b = fresh_scratch("b-argv-twin")
    lines_a = 0
    for label, cf in STAGES:
        raw, hp = host_variant(sc_a, cf)
        open(hp, "wb").write(raw)
        try:
            obj = validate(hp)
        except Refuse as e:
            ok = False
            receipt.append(f"| {label} | host variant REFUSE: {e} |")
            break
        rc = dispatch(sc_a, synth_argv(obj, "modbridge-installer"), log)
        st = f"{sc_a}/state/e2e-status.jsonl"
        n = sum(1 for _ in open(st)) if os.path.isfile(st) else 0
        good = rc == 0
        receipt.append(f"| {label} | exit {rc} | status lines now {n} | {'OK' if good else 'FAIL'} |")
        ok = ok and good
        lines_a = n
    got = open(f"{sc_a}/state/e2e-status.jsonl").read().splitlines() if os.path.isfile(f"{sc_a}/state/e2e-status.jsonl") else []
    match_ledger = got == EXPECTED
    ok = ok and match_ledger and lines_a == 35
    receipt.append(f"\nTranscript lines: {lines_a} (corrected ledger: 35). Verbatim match: {match_ledger}.\n")

    receipt.append("## C. argv twins (hand-written CLI form), byte-compare\n")
    for label, cf in STAGES:
        raw, hp = host_variant(sc_b, cf)
        open(hp, "wb").write(raw)
        obj = validate(hp)
        rc = dispatch(sc_b, twin_argv(obj, "modbridge-installer"), log)
        if rc != 0:
            ok = False
            receipt.append(f"| {label} | twin exit {rc} | FAIL |")
    got_b = open(f"{sc_b}/state/e2e-status.jsonl").read().splitlines() if os.path.isfile(f"{sc_b}/state/e2e-status.jsonl") else []
    twin_eq = (got == got_b)
    ok = ok and twin_eq
    receipt.append(f"\nTwin transcript byte-identical to synth-argv transcript: {twin_eq}.\n")

    # ---- recover probe on a clean third scratch --------------------------
    receipt.append("## B2. recover probe (interrupted-transaction verb shape)\n")
    sc_c = fresh_scratch("c-recover")
    raw, hp = host_variant(sc_c, "recover.json")
    open(hp, "wb").write(raw)
    obj = validate(hp)
    rc = dispatch(sc_c, synth_argv(obj, "modbridge-installer"), log)
    got_c = open(f"{sc_c}/state/e2e-status.jsonl").read().splitlines() if os.path.isfile(f"{sc_c}/state/e2e-status.jsonl") else []
    rec_ok = rc == 0 and got_c == RECOVER_2
    ok = ok and rec_ok
    receipt.append(f"exit {rc}, lines {got_c!r}, matches begin/end recovery shape: {rec_ok}\n")

    # ---- D: refusal matrix (validator layer; the future entry refuses identically)
    receipt.append("## D. refusal matrix (schema sec 6)\n")
    base = '{"schema":1,"verb":"records","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{}}'
    ref_dir = f"{HERE}/host-test/refuse-cases"
    shutil.rmtree(ref_dir, ignore_errors=True)
    os.makedirs(ref_dir)
    big = '{"schema":1,"verb":"records","state_root":{"state":"' + "/x" * 2100 + '","roots":"/data/modbridge/roots"},"options":{}}'
    cases = {
        "unknown-top-key": '{"schema":1,"verb":"records","extra":1,"state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{}}',
        "unknown-option-key": '{"schema":1,"verb":"records","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"colour":"red"}}',
        "deny-verb-make-bundle": '{"schema":1,"verb":"make-bundle","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{}}',
        "deny-option-crash_at": '{"schema":1,"verb":"install","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"title":"PPSA33286","crash_at":"staged"}}',
        "bad-schema": '{"schema":2,"verb":"records","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{}}',
        "mods-with-remove": '{"schema":1,"verb":"install","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"mods":["mm-skin"],"options":{"title":"PPSA33286","remove":"mm-lua"}}',
        "mods-on-records": '{"schema":1,"verb":"records","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"mods":["mm-skin"],"options":{}}',
        "unicode-escape": '{"schema":1,"verb":"records","state_root":{"state":"\\/data\\/modbridge\\u007state","roots":"/data/modbridge/roots"},"options":{}}',
        "oversize": big,
        "no_auto_bundle-false": '{"schema":1,"verb":"prepare-archive","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"title":"PPSA33286","archive":"/a","quarantine":"/q","no_auto_bundle":false}}',
        "title-on-recover": '{"schema":1,"verb":"recover","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{"title":"PPSA33286"}}',
        "relative-state": '{"schema":1,"verb":"records","state_root":{"state":"state","roots":"/data/modbridge/roots"},"options":{}}',
        "unknown-verb": '{"schema":1,"verb":"transmogrify","state_root":{"state":"/data/modbridge/state","roots":"/data/modbridge/roots"},"options":{}}',
        "absent-file": f"{ref_dir}/does-not-exist.json",
    }
    receipt.append("| case | expected | got |")
    receipt.append("|---|---|---|")
    n_refuse = 0
    for name, body in cases.items():
        p = f"{ref_dir}/{name}.json"
        if name != "absent-file":
            open(p, "w").write(body)
        try:
            validate(p)
            receipt.append(f"| {name} | REFUSE | ACCEPTED | FAIL |")
            ok = False
        except Refuse as e:
            n_refuse += 1
            receipt.append(f"| {name} | REFUSE | REFUSE: {e} | OK |")
    receipt.append(f"\n{n_refuse}/{len(cases)} refusal cases refused.\n")

    # ---- identity receipts ------------------------------------------------
    receipt.append("## Identity\n")
    receipt.append(f"- host binary: {BIN}")
    receipt.append(f"- host binary sha256: {sha256_file(BIN)}")
    receipt.append(f"- console payload (NOT used on host): sha256 e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436, 681528 B")
    receipt.append(f"- movspeed fixture sha256: {sha256_file(ZIP)}")
    receipt.append(f"- mm-skin.mfb sha256: {sha256_file(MSKIN)}")
    receipt.append(f"- mm-lua.mfb sha256: {sha256_file(MLUA)}")
    receipt.append(f"- full transcript sha256: {hashlib.sha256(open(f'{sc_a}/state/e2e-status.jsonl','rb').read()).hexdigest()}")
    receipt.append("\n## Scope honesty\n")
    receipt.append("This is a HOST-SIDE EMULATION of the entry rule (validator + argv synthesis)")
    receipt.append("feeding the REAL host binary parser/dispatch. The console entry itself does not")
    receipt.append("exist in any built payload yet (design only, E2E-CMD-SCHEMA.md). Console results")
    receipt.append("remain [TARGET] until the controller run with the re-pinned config-entry revision.\n")
    receipt.append(f"\nOVERALL: {'PASS' if ok else 'FAIL'}\n")
    open(f"{HERE}/host-test/HOST-TEST-RECEIPT.md", "w").write(
        "# HOST-TEST-RECEIPT — config-file entry design, host emulation\n\n"
        + "\n".join(receipt) + "\n")
    log.close()
    print("\n".join(receipt[-40:]))
    print(f"OVERALL: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
