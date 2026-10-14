#!/usr/bin/env python3
"""Independent tester framework — Mac-only, host-tested.

Test harness for adversarial review and integration testing of the
sideproject mod-manager modules. No console contact, no FTP, no payloads.
Everything runs on this Mac against files inside the project tree.

PORT NOTE (2026-10-07): the suites were originally written against the flat
/tmp/sideproject/<module>/ layout. The project has since been consolidated:
  mod-types/detector.py        -> installer/detector.py
  installer-core/installer.py  -> installer/installer.py
  uninstall/{uninstall,conflicts}.py -> installer/{uninstall,conflicts}.py
  compat-checker/{checker.py,sample_catalog.json}
                               -> compat/checker.py + compat/sample_catalog_TEST_FIXTURE.json
All paths are now resolved relative to this file's location (<project
root>/tests/src-tester/framework.py). Set the SIDEPROJECT_ROOT environment
variable to point at a different project checkout; no absolute paths are
hardcoded.

Usage:
    python3 framework.py <suite_file.py> [<suite_file.py> ...]
    python3 framework.py --list <suite_file.py>
"""

import importlib.util
import io
import json
import os
import shutil
import sys
import tempfile
import time
import traceback
import zipfile

_HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.environ.get(
    "SIDEPROJECT_ROOT", os.path.dirname(os.path.dirname(_HERE)))

TESTER = _HERE
RESULTS = os.path.join(TESTER, "results")
CORPUS = os.path.join(TESTER, "corpus")

# Current module locations in the consolidated tree.
INSTALLER_DIR = os.path.join(PROJECT_ROOT, "installer")
DETECTOR = os.path.join(INSTALLER_DIR, "detector.py")
INSTALLER = os.path.join(INSTALLER_DIR, "installer.py")
UNINSTALL = os.path.join(INSTALLER_DIR, "uninstall.py")
CONFLICTS = os.path.join(INSTALLER_DIR, "conflicts.py")
COMPAT_DIR = os.path.join(PROJECT_ROOT, "compat")
CHECKER = os.path.join(COMPAT_DIR, "checker.py")
CATALOG = os.path.join(COMPAT_DIR, "sample_catalog_TEST_FIXTURE.json")

# Legacy module names (original flat layout) -> current directories.
MODULE_DIRS = {
    "catalog-globals": os.path.join(PROJECT_ROOT, "catalog"),
    "catalog-types": os.path.join(PROJECT_ROOT, "catalog"),
    "conformance-static": os.path.join(PROJECT_ROOT, "conformance"),
    "conformance-live": os.path.join(PROJECT_ROOT, "conformance"),
    "installer-core": INSTALLER_DIR,
    "mod-types": INSTALLER_DIR,
    "uninstall": INSTALLER_DIR,
    "compat-checker": COMPAT_DIR,
}
MODULES = list(MODULE_DIRS)


# ---------------------------------------------------------------------------
# Test registry / runner
# ---------------------------------------------------------------------------

class Test:
    def __init__(self, name, fn, category, expect_fail=False):
        self.name = name
        self.fn = fn
        self.category = category
        self.expect_fail = expect_fail  # test asserts that target SHOULD fail


class Assert:
    """Naked assertion API handed to tests."""

    @staticmethod
    def ok(cond, msg=""):
        if not cond:
            raise AssertionError(msg or "condition false")

    @staticmethod
    def eq(a, b, msg=""):
        if a != b:
            raise AssertionError(f"{msg}: {a!r} != {b!r}" if msg else f"{a!r} != {b!r}")

    @staticmethod
    def contains(haystack, needle, msg=""):
        if needle not in haystack:
            raise AssertionError(f"{msg}: {needle!r} not found in {haystack!r}" if msg else f"{needle!r} not in {haystack!r}")

    @staticmethod
    def is_none(v, msg=""):
        if v is not None:
            raise AssertionError(f"{msg}: expected None, got {v!r}" if msg else f"expected None, got {v!r}")

    @staticmethod
    def isinstance(v, cls, msg=""):
        if not isinstance(v, cls):
            raise AssertionError(f"{msg}: {type(v)} not instance of {cls}" if msg else f"{type(v)} not {cls}")

    @staticmethod
    def raises(exc_type, fn, *args, **kwargs):
        try:
            fn(*args, **kwargs)
        except exc_type:
            return
        except Exception as e:  # noqa: BLE001
            raise AssertionError(
                f"expected {exc_type.__name__}, got {type(e).__name__}: {e}"
            )
        raise AssertionError(f"expected {exc_type.__name__}, nothing raised")

    @staticmethod
    def no_crash(fn, msg=""):
        """Run fn; any exception fails the test."""
        try:
            return fn()
        except Exception as e:  # noqa: BLE001
            raise AssertionError(f"{msg}: crashed with {type(e).__name__}: {e}" if msg else f"crashed with {type(e).__name__}: {e}")

    @staticmethod
    def returns_error(fn, msg=""):
        """Target code should fail CONTROLLABLY: it may return an error
        value/tuple or raise a normal Exception, but must not hang, kill the
        process, or corrupt the harness. Returns the outcome."""
        try:
            return fn()
        except KeyboardInterrupt:
            raise
        except BaseException as e:  # noqa: BLE001 — SystemExit/RecursionError matter
            if isinstance(e, (SystemExit, MemoryError)) or "RecursionError" in type(e).__name__:
                raise AssertionError(
                    f"{msg}: uncontrolled failure {type(e).__name__}"
                )
            return ("raised", e)

    @staticmethod
    def exits_cleanly(cmd_runner, msg=""):
        """cmd_runner -> returncode. Must be 0 or a sane small exit code,
        never a signal (negative) or hang."""
        rc = cmd_runner()
        if rc is None or rc < 0 or rc > 255:
            raise AssertionError(f"{msg}: bad exit code {rc}")
        return rc


A = Assert()


class Harness:
    def __init__(self):
        self.tests = []
        self.results = []

    def add(self, name, category="general", expect_fail=False):
        def deco(fn):
            self.tests.append(Test(name, fn, category, expect_fail))
            return fn
        return deco

    def run(self, filter_prefix=None):
        import inspect
        for t in self.tests:
            if filter_prefix and not t.name.startswith(filter_prefix):
                continue
            sys.stdout.flush()
            start = time.time()
            status, detail = "PASS", ""
            sb = None
            try:
                sig = inspect.signature(t.fn)
                if "sb" in sig.parameters:
                    sb = Sandbox(t.name.replace("/", "-"))
                    t.fn(sb=sb)
                else:
                    t.fn()
            except AssertionError as e:
                status = "FAIL" if not t.expect_fail else "PASS-EXPECTED-FAIL"
                detail = str(e)
            except Exception:  # noqa: BLE001
                status = "ERROR"
                detail = traceback.format_exc(limit=6)
            else:
                if t.expect_fail:
                    status = "FAIL"
                    detail = "expected the target to fail this input, but it succeeded"
            finally:
                if sb is not None:
                    sb.cleanup()
            dt = time.time() - start
            self.results.append(
                {"name": t.name, "category": t.category, "status": status,
                 "detail": detail, "seconds": round(dt, 3)}
            )
            print(f"[{status:^22}] {t.name} ({dt:.2f}s)")
            if detail and status in ("FAIL", "ERROR"):
                for line in detail.splitlines()[:8]:
                    print(f"    {line}")
        return self.results

    def summary(self):
        n = {s: sum(1 for r in self.results if r["status"] == s)
             for s in ("PASS", "FAIL", "ERROR", "PASS-EXPECTED-FAIL")}
        return n

    def write_results(self, suite_name):
        os.makedirs(RESULTS, exist_ok=True)
        path = os.path.join(RESULTS, f"{suite_name}.json")
        with open(path, "w") as f:
            json.dump(self.results, f, indent=1)
        return path


# ---------------------------------------------------------------------------
# Sandbox helpers — every test gets a throwaway workspace
# ---------------------------------------------------------------------------

class Sandbox:
    """Temp dir factory. All adversarial fixtures live inside a sandbox so a
    buggy module can never scribble outside tests/src-tester/runs/."""

    ROOT = os.path.join(TESTER, "runs")

    def __init__(self, label):
        os.makedirs(self.ROOT, exist_ok=True)
        self.path = tempfile.mkdtemp(prefix=f"{label}-", dir=self.ROOT)
        self.label = label

    def file(self, rel, content=b""):
        p = os.path.join(self.path, rel)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        mode = "wb" if isinstance(content, (bytes, bytearray)) else "w"
        with open(p, mode) as f:
            f.write(content)
        return p

    def exists(self, rel):
        return os.path.exists(os.path.join(self.path, rel))

    def read(self, rel, binary=False):
        p = os.path.join(self.path, rel)
        with open(p, "rb" if binary else "r") as f:
            return f.read()

    def tree(self):
        out = []
        for root, dirs, files in os.walk(self.path):
            dirs.sort()
            for fn in sorted(files):
                out.append(os.path.relpath(os.path.join(root, fn), self.path))
            for dn in dirs:
                out.append(os.path.relpath(os.path.join(root, dn), self.path) + "/")
        return sorted(out)

    def cleanup(self):
        shutil.rmtree(self.path, ignore_errors=True)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.cleanup()


def sandbox(label):
    def deco(fn):
        def wrapper(*args, **kwargs):
            sb = Sandbox(label)
            kwargs["sb"] = sb
            try:
                return fn(*args, **kwargs)
            finally:
                sb.cleanup()
        wrapper.__name__ = fn.__name__
        return wrapper
    return deco


# ---------------------------------------------------------------------------
# Adversarial zip builders (pure zipfile/python — Mac-only host testing)
# ---------------------------------------------------------------------------

class ZipKit:
    """Builds hostile-but-contained zip fixtures."""

    def __init__(self, base_dir):
        self.base = base_dir

    def normal(self, name, entries):
        """entries: dict relpath -> bytes/str"""
        p = os.path.join(self.base, name)
        with zipfile.ZipFile(p, "w", zipfile.ZIP_DEFLATED) as z:
            for rel, data in entries.items():
                if isinstance(data, str):
                    data = data.encode()
                z.writestr(rel, data)
        return p

    def truncated(self, name, entries):
        """Cut the file short — no central directory."""
        full = os.path.join(self.base, name + ".full")
        self.normal(full, entries)
        p = os.path.join(self.base, name)
        with open(full, "rb") as src, open(p, "wb") as dst:
            dst.write(src.read()[: max(64, os.path.getsize(full) // 2)])
        os.remove(full)
        return p

    def not_a_zip(self, name, content=b"MZ\x90\x00 fake pe header"):
        p = os.path.join(self.base, name)
        with open(p, "wb") as f:
            f.write(content)
        return p

    def empty(self, name):
        p = os.path.join(self.base, name)
        open(p, "wb").close()
        return p

    def bomb(self, name, ratio_target=100, files=50, file_size=1 << 20):
        """~50MB total uncompressed in 50 files; small on disk. Bounded so the
        host itself is never endangered — detects naive full-inflation."""
        p = os.path.join(self.base, name)
        chunk = (b"0123456789abcdef" * (file_size // 16))[:file_size]
        with zipfile.ZipFile(p, "w", zipfile.ZIP_DEFLATED) as z:
            for i in range(files):
                z.writestr(f"bomb/pad_{i:04d}.bin", chunk)
        return p

    def many_files(self, name, count=10000):
        p = os.path.join(self.base, name)
        with zipfile.ZipFile(p, "w", zipfile.ZIP_DEFLATED) as z:
            for i in range(count):
                z.writestr(f"m/{i:05d}.lua", f"-- file {i}\n")
        return p

    def unicode_paths(self, name):
        return self.normal(name, {
            "mods/über-mod/init.lua": "-- unicode dir\n",
            "mods/日本語mod.txt": "unicode name\n",
            "mods/emoji\U0001f600.lua": "-- emoji\n",
            "mods/spaß/init.lua": "-- latin1-ish\n",
        })

    def empty_files(self, name):
        return self.normal(name, {
            "mods/empty/init.lua": "",
            "mods/empty/manifest.json": "",
            "mods/empty/pad.bin": b"",
        })

    def nested_zip(self, name, depth=20):
        """zip-in-zip-in-zip... bounded depth."""
        inner = os.path.join(self.base, name + ".inner")
        self.normal(inner, {"deep/init.lua": "-- deepest\n"})
        cur = inner
        for i in range(depth):
            nxt = os.path.join(self.base, f"{name}.layer{i}")
            with zipfile.ZipFile(nxt, "w") as z:
                z.write(cur, arcname="layer.zip")
            os.remove(cur)
            cur = nxt
        return cur

    def with_symlink(self, name):
        """A zip containing a stored symlink entry pointing outside, plus one
        pointing to a sibling. zipfile writes external_attr for symlinks."""
        p = os.path.join(self.base, name)
        with zipfile.ZipFile(p, "w") as z:
            zi = zipfile.ZipInfo("mods/evil/init.lua")
            zi.external_attr = (0o120777 << 16)  # S_IFLNK | 0777
            z.writestr(zi, "../../etc/passwd")
            zi2 = zipfile.ZipInfo("mods/evil/loop")
            zi2.external_attr = (0o120777 << 16)
            z.writestr(zi2, "loop-target")
            zi3 = zipfile.ZipInfo("mods/evil/loop-target")
            zi3.external_attr = (0o120777 << 16)
            z.writestr(zi3, "loop")  # mutual symlink loop
            z.writestr("mods/evil/real.lua", "-- real\n")
        return p

    def path_traversal(self, name):
        return self.normal(name, {
            "../escape.txt": "escaped\n",
            "mods/ok.lua": "-- ok\n",
            "mods/../../absolute-ish.txt": "hmm\n",
        })

    def dup_names(self, name):
        """Two entries with the SAME name (legal-ish in zips, hostile)."""
        p = os.path.join(self.base, name)
        with zipfile.ZipFile(p, "w") as z:
            z.writestr("mods/dup/init.lua", "-- first version\n")
            z.writestr("mods/dup/init.lua", "-- second version\n")
        return p

    def dir_backslash(self, name):
        return self.normal(name, {
            "mods\\windows\\init.lua": "-- backslash path\n",
        })

    def huge_entry_name(self, name, compname_len=1000):
        p = os.path.join(self.base, name)
        longname = "a/" * (compname_len // 2) + "init.lua"
        with zipfile.ZipFile(p, "w") as z:
            z.writestr(longname[:65535], "-- long\n")
        return p


# ---------------------------------------------------------------------------
# Module discovery / interface probing
# ---------------------------------------------------------------------------

def module_dir(name):
    """Resolve a (legacy) module name to its current directory in the
    consolidated tree; unknown names fall back to <root>/<name>."""
    return MODULE_DIRS.get(name, os.path.join(PROJECT_ROOT, name))


def module_files(name):
    """All files currently present in a module dir (recursive)."""
    d = module_dir(name)
    out = []
    for root, _dirs, files in os.walk(d):
        for fn in files:
            out.append(os.path.relpath(os.path.join(root, fn), d))
    return sorted(out)


def import_any(path):
    """Import a python module from an explicit path, whatever it's called."""
    spec = importlib.util.spec_from_file_location(
        "uut_" + os.path.basename(path).replace(".", "_"), path
    )
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def find_entrypoints(name):
    """Heuristically find runnable entry points of a module."""
    files = module_files(name)
    entry = []
    for f in files:
        if f.endswith(".py") and os.path.basename(f) in (
            "main.py", "cli.py", "__main__.py", "run.py", "app.py"
        ):
            entry.append(("python", os.path.join(module_dir(name), f)))
    return entry, files


# ---------------------------------------------------------------------------

def load_suite(path):
    """Load a suite file; it must define register(h) or TESTS via harness."""
    harness = Harness()
    ns = {"harness": harness, "A": A, "Sandbox": Sandbox, "sandbox": sandbox,
          "ZipKit": ZipKit, "framework": sys.modules[__name__],
          "os": os, "json": json, "zipfile": zipfile, "tempfile": tempfile,
          "shutil": shutil, "subprocess": __import__("subprocess")}
    with open(path) as f:
        code = compile(f.read(), path, "exec")
    ns["__file__"] = os.path.abspath(path)
    exec(code, ns)  # noqa: S102 — sandboxed suite file we authored
    if "register" in ns:
        ns["register"](harness)
    return harness


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    args = list(argv[1:])
    only_list = False
    if args[0] == "--list":
        only_list = True
        args = args[1:]
    total = {"PASS": 0, "FAIL": 0, "ERROR": 0, "PASS-EXPECTED-FAIL": 0}
    for path in args:
        base = os.path.splitext(os.path.basename(path))[0]
        print(f"=== suite: {base} ===")
        h = load_suite(path)
        if only_list:
            for t in h.tests:
                print(f"  [{t.category}] {t.name}")
            continue
        h.run()
        h.write_results(base)
        s = h.summary()
        for k in total:
            total[k] += s[k]
    print("=== totals ===")
    for k, v in total.items():
        print(f"  {k}: {v}")
    return 0 if total["FAIL"] == 0 and total["ERROR"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
