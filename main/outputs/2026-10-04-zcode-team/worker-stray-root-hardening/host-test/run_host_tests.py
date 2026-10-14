#!/usr/bin/env python3
"""Host tests for find_root_v2 (worker-stray-root-hardening).

LOCAL-ONLY, host-side, stdlib-only. Builds FAKE trees under a tempdir and
asserts the v2 resolution contract; also does read-only real-tree checks
against the frozen packaging module (loaded via importlib, never modified).

Covered cases (task spec):
  T1  clean tree (true root + team dir + worker dir)            -> resolves
  T2  stray bare-.superpowers decoy between team and root       -> LOUD failure
  T3  decoy with native-observations but NO corroboration       -> LOUD failure
  T4  deep nesting beyond 6 ancestors (7 and 8 hops)            -> still resolves
  T5  --root override (rescues T2/T3 ambiguity; bad override rejected)
  T6  multiple strong candidates                                -> LOUD failure
  T7  nesting beyond 8 ancestors (9 hops)                       -> LOUD failure
  T8  drop-in contract: RootResolutionError is a RuntimeError; no-arg-style
      call returns a resolved Path; frozen-parity call from frozen dir
  T9  real-tree equivalence: v2(start=frozen dir) == frozen module's ROOT;
      quarantined stray dir name is NOT treated as a marker; frozen sha
      unchanged before/after the whole run.

Writes PASS-receipt.json + console.log next to this script on success;
exits non-zero with a FAIL receipt on any failure.
"""

from __future__ import annotations

import hashlib
import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import traceback
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
WORKER_DIR = HERE.parent                       # worker-stray-root-hardening/
TEAM = WORKER_DIR.parent                       # outputs/2026-10-04-zcode-team/
V2 = WORKER_DIR / "find_root_v2.py"
FROZEN_PKG = TEAM / "worker-prx-link-gaps" / "package_prx.py"
FROZEN_SHA = "e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313"
WORKSPACE_ROOT = TEAM.parents[1]               # .../what  (audit-verified depth)

sys.path.insert(0, str(WORKER_DIR))
import find_root_v2 as frv2  # noqa: E402

RESULTS = []
LOG_LINES = []


def log(msg: str = "") -> None:
    print(msg)
    LOG_LINES.append(msg)


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def check(case_id: str, description: str, fn) -> None:
    log(f"\n== {case_id}: {description}")
    try:
        detail = fn()
        RESULTS.append({"case": case_id, "description": description,
                        "status": "PASS", "detail": detail})
        log(f"   PASS  {detail}")
    except AssertionError as e:
        RESULTS.append({"case": case_id, "description": description,
                        "status": "FAIL", "detail": str(e)})
        log(f"   FAIL  {e}")
    except Exception as e:  # unexpected — record loudly, keep other cases running
        RESULTS.append({"case": case_id, "description": description,
                        "status": "ERROR", "detail": f"{type(e).__name__}: {e}",
                        "trace": traceback.format_exc()})
        log(f"   ERROR {type(e).__name__}: {e}")


# ------------------------------------------------------------ fake-tree builders
def make_strong_root(root: Path) -> None:
    (root / ".superpowers/native-observations").mkdir(parents=True)
    (root / ".superpowers/native-observations/transport-experiments-progress.json").write_text("{}\n")
    (root / "README-RETURN.md").write_text("workspace root marker\n")


def make_basic_tree(base: Path) -> tuple[Path, Path, Path]:
    """true root / team-01 / worker dir; returns (root, team, worker)."""
    root = base / "fakews"
    team = root / "outputs/team-01"
    worker = team / "worker-x"
    worker.mkdir(parents=True)
    make_strong_root(root)
    return root, team, worker


# ------------------------------------------------------------ test cases
def t1_clean(base: Path) -> str:
    root, _team, worker = make_basic_tree(base / "t1")
    got = frv2.find_root(start=worker)
    assert got == root.resolve(), f"resolved {got}, expected {root.resolve()}"
    assert isinstance(got, Path) and got.is_absolute()
    return f"start={worker.name} -> {got.name}"


def _assert_loud_failure(worker: Path, want_kinds: set[str]) -> str:
    try:
        frv2.find_root(start=worker)
    except frv2.RootResolutionError as e:
        assert isinstance(e, RuntimeError), "RootResolutionError must subclass RuntimeError"
        assert e.kind in want_kinds, f"kind={e.kind}, wanted {want_kinds}"
        assert str(e), "empty error message"
        return f"loud {e.kind}: {str(e).splitlines()[0][:70]}..."
    raise AssertionError("expected RootResolutionError, got clean resolution")


def t2_bare_decoy(base: Path) -> str:
    root, team, worker = make_basic_tree(base / "t2")
    (team / ".superpowers").mkdir()          # the 05:42 stray shape: bare marker
    return _assert_loud_failure(worker, {"weak-decoy"})


def t3_no_corroboration_decoy(base: Path) -> str:
    root, team, worker = make_basic_tree(base / "t3")
    (team / ".superpowers/native-observations").mkdir(parents=True)  # deep marker, no ledger/README/sdd
    return _assert_loud_failure(worker, {"weak-decoy"})


def t4_deep_beyond_6(base: Path) -> str:
    root = base / "t4" / "fakews"
    deep = root / "a/b/c/d/e/f/worker-deep7"        # root is 7 hops above start
    deep.mkdir(parents=True)
    make_strong_root(root)
    got = frv2.find_root(start=deep)
    assert got == root.resolve(), f"7-hop walk resolved {got}"
    deeper = root / "a/b/c/d/e/f/g/worker-deep8"    # 8 hops — exactly the v2 bound
    deeper.mkdir(parents=True)
    got2 = frv2.find_root(start=deeper)
    assert got2 == root.resolve(), f"8-hop walk resolved {got2}"
    return f"resolved at 7 and 8 hops (frozen bound was 6): {got.name}"


def t5_override(base: Path) -> str:
    root, team, worker = make_basic_tree(base / "t5")
    (team / ".superpowers").mkdir()                  # ambiguous again
    got = frv2.find_root(start=worker, override=root)  # override rescues
    assert got == root.resolve()
    # bad override: dir without any .superpowers -> rejected loudly
    innocent = base / "t5" / "innocent"
    innocent.mkdir()
    try:
        frv2.find_root(start=worker, override=innocent)
    except frv2.RootResolutionError as e:
        assert e.kind == "bad-override"
    else:
        raise AssertionError("bad override was not rejected")
    # weak override accepted only with the explicit escape hatch
    got2 = frv2.find_root(start=worker, override=team, allow_weak_override=True)
    assert got2 == team.resolve()
    # CLI honors --root
    proc = subprocess.run(
        [sys.executable, str(V2), "--start", str(worker), "--root", str(root), "--json"],
        capture_output=True, text=True)
    assert proc.returncode == 0, f"CLI --root failed: {proc.stderr}"
    assert json.loads(proc.stdout)["root"] == str(root.resolve())
    return "override rescued ambiguity; bad override rejected; CLI --root honored"


def t6_multiple_strong(base: Path) -> str:
    root, team, worker = make_basic_tree(base / "t6")
    make_strong_root(team)                            # two strong candidates on path
    return _assert_loud_failure(worker, {"ambiguous-multiple", "weak-decoy"})


def t7_beyond_bound(base: Path) -> str:
    root = base / "t7" / "fakews"
    deep = root / "a/b/c/d/e/f/g/h/i/worker-too-deep9"  # 9 hops > 8
    deep.mkdir(parents=True)
    make_strong_root(root)
    return _assert_loud_failure(deep, {"no-root"})


def t8_contract(base: Path) -> str:
    root, _team, worker = make_basic_tree(base / "t8")
    # frozen-parity call: start = frozen file's dir == no-arg mechanism parity
    got = frv2.find_root(start=worker)
    assert got == got.resolve() and isinstance(got, Path)
    # weak_markers=warn keeps resolving, loud on stderr
    import io, contextlib
    team2 = root / "outputs/team-warn"
    w2 = team2 / "worker-warn"
    w2.mkdir(parents=True)
    (team2 / ".superpowers").mkdir()
    buf = io.StringIO()
    with contextlib.redirect_stderr(buf):
        got2 = frv2.find_root(start=w2, weak_markers="warn")
    assert got2 == root.resolve() and "WARNING" in buf.getvalue()
    # invalid policy value rejected
    try:
        frv2.find_root(start=w2, weak_markers="shrug")
    except ValueError:
        pass
    else:
        raise AssertionError("invalid weak_markers value not rejected")
    return "Path/resolved return; warn policy resolves loudly; bad policy rejected"


def t9_real_tree() -> str:
    # frozen sha unchanged before load
    pre = sha256(FROZEN_PKG)
    assert pre == FROZEN_SHA, f"frozen file drift BEFORE tests: {pre}"
    # load the frozen module read-only (module level is constants only; main guarded)
    spec = importlib.util.spec_from_file_location("package_prx_frozen_for_test", FROZEN_PKG)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    # v2 from the frozen dir must equal what the frozen file resolved (post-quarantine)
    v2root = frv2.find_root(start=FROZEN_PKG.parent)
    assert v2root == Path(m.ROOT).resolve(), f"v2 {v2root} != frozen ROOT {m.ROOT}"
    assert v2root == WORKSPACE_ROOT.resolve(), \
        f"v2 {v2root} != expected workspace root {WORKSPACE_ROOT}"
    # quarantined stray name must NOT count as a marker (exact-name matching)
    verdict, _present, _reason = frv2.classify_candidate(TEAM)
    assert verdict is None, f"TEAM classified {verdict} — quarantine leaking as marker?"
    # frozen sha unchanged after load
    post = sha256(FROZEN_PKG)
    assert post == FROZEN_SHA, f"frozen file drift AFTER tests: {post}"
    return (f"v2 == frozen ROOT == {v2root}; TEAM classifies None "
            f"(quarantine inert); frozen sha {pre[:12]}... unchanged")


def main() -> int:
    log(f"find_root_v2 host tests — {datetime.now(timezone.utc).isoformat()}")
    log(f"v2 file : {V2}")
    log(f"v2 sha  : {sha256(V2)}")
    log(f"frozen  : {FROZEN_PKG}")
    log(f"frozen sha: {FROZEN_SHA}")

    tmp = Path(tempfile.mkdtemp(prefix="frv2-tests-"))
    log(f"fake-tree base: {tmp}")

    check("T1", "clean tree resolves (root/team/worker)", lambda: t1_clean(tmp))
    check("T2", "bare .superpowers decoy (the 05:42 stray shape) fails LOUD", lambda: t2_bare_decoy(tmp))
    check("T3", "native-observations-without-corroboration decoy fails LOUD", lambda: t3_no_corroboration_decoy(tmp))
    check("T4", "deep nesting beyond 6 ancestors still resolves", lambda: t4_deep_beyond_6(tmp))
    check("T5", "--root override honored (rescue + rejection + CLI)", lambda: t5_override(tmp))
    check("T6", "multiple strong candidates fail LOUD", lambda: t6_multiple_strong(tmp))
    check("T7", "beyond 8 ancestors fails LOUD (bounded walk)", lambda: t7_beyond_bound(tmp))
    check("T8", "frozen drop-in contract (RuntimeError subclass, Path, policies)", lambda: t8_contract(tmp))
    check("T9", "REAL tree: v2 == frozen ROOT, quarantine inert, sha unchanged", t9_real_tree)

    fails = [r for r in RESULTS if r["status"] != "PASS"]
    passed = [r for r in RESULTS if r["status"] == "PASS"]

    receipt = {
        "receipt": "find_root_v2 host-test",
        "worker": "worker-stray-root-hardening",
        "date": datetime.now(timezone.utc).strftime("%Y-%m-%d"),
        "utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS" if not fails else "FAIL",
        "passed": len(passed),
        "failed": len(fails),
        "frozen_file": str(FROZEN_PKG),
        "frozen_sha256_expected": FROZEN_SHA,
        "frozen_sha256_after_full_run": sha256(FROZEN_PKG),
        "frozen_file_modified": False,
        "v2_file": str(V2),
        "v2_sha256": sha256(V2),
        "fake_tree_base": str(tmp),
        "cases": RESULTS,
        "scope": "host-side only; fake trees under tempdir; real-tree checks read-only",
    }
    (HERE / "PASS-receipt.json" if not fails else HERE / "FAIL-receipt.json").write_text(
        json.dumps(receipt, indent=2) + "\n")
    (HERE / "console.log").write_text("\n".join(LOG_LINES) + "\n")

    log(f"\n{'=' * 60}")
    log(f"RESULT: {receipt['status']}  ({len(passed)} PASS / {len(fails)} FAIL+ERROR)")
    if not fails:
        shutil.rmtree(tmp, ignore_errors=True)
        log("fake-tree tempdir removed; receipt: host-test/PASS-receipt.json")
    else:
        log(f"fake-tree tempdir PRESERVED for triage: {tmp}")
    return 0 if not fails else 1


if __name__ == "__main__":
    raise SystemExit(main())
