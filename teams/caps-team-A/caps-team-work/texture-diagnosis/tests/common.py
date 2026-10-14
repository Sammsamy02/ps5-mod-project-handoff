"""Shared paths + item counters for the texture-diagnosis host tests (Mac-only).

All inputs are read-only local copies of the game's own files and the
downloaded mod; tests write only under tests/tmp and tests/out.
"""
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TOOLS = os.path.join(ROOT, "tools")
sys.path.insert(0, TOOLS)

WHAT = os.environ.get("WHAT_ROOT", "/WORKSPACE/.codex/worktrees/translation-core/what")
OBS = os.path.join(WHAT, ".superpowers", "native-observations")
SPIKE = os.path.join(WHAT, ".superpowers", "native-dependency-spike")
MOD2K_UTOC = os.path.join(OBS, "leon-real-mod-intake-2026-10-01", "extracted", "zzz_Leon_Townfall_2K_P.utoc")
MOD2K_UCAS = os.path.join(OBS, "leon-real-mod-intake-2026-10-01", "extracted", "zzz_Leon_Townfall_2K_P.ucas")
MOD2K_PAK = os.path.join(OBS, "leon-real-mod-intake-2026-10-01", "extracted", "zzz_Leon_Townfall_2K_P.pak")
MOD4K_UTOC = os.path.join(SPIKE, "leon-iostore-adapter-4k-trial-1", "source", "zzz_Leon_Townfall_4K_P.utoc")
MOD4K_UCAS = os.path.join(SPIKE, "leon-iostore-adapter-4k-trial-1", "source", "zzz_Leon_Townfall_4K_P.ucas")
BASE_UTOCS = [os.path.join(OBS, "townfall-main-utoc-2026-10-01", "pakchunk%d-ps5.utoc" % n) for n in (0, 1, 10, 11, 12)]
BASE_HEADER = os.path.join(OBS, "townfall-original-container-header-1", "container-header.bin")
GLOBAL_UTOC = os.path.join(OBS, "townfall-manifest-capture-2026-10-01", "global.utoc")
GLOBAL_UCAS = os.path.join(OBS, "townfall-global-scriptobjects-1", "global.ucas")
BASE_PAK_LISTING = os.path.join(OBS, "leon-real-mod-intake-2026-10-01", "base-pak-metadata", "repak-list.stdout.txt")
RETOC = os.path.join(SPIKE, "retoc-v0.1.5-raw-roundtrip-1", "tool", "retoc_cli-aarch64-apple-darwin", "retoc")
OVERLAY_PATCHED = os.path.join(ROOT, "PATCH", "new")

TMP = os.path.join(HERE, "tmp")
OUT = os.path.join(HERE, "out")
COUNTS = os.path.join(OUT, "counts.json")


def fresh_tmp(name: str) -> str:
    p = os.path.join(TMP, name)
    shutil.rmtree(p, ignore_errors=True)
    os.makedirs(p)
    return p


def record(key: str, n: int) -> None:
    """Record how many items a test checked (run_tests.sh fails on any zero)."""
    os.makedirs(OUT, exist_ok=True)
    d = {}
    if os.path.exists(COUNTS):
        with open(COUNTS) as f:
            d = json.load(f)
    d[key] = d.get(key, 0) + int(n)
    with open(COUNTS, "w") as f:
        json.dump(d, f, indent=1, sort_keys=True)


def require_inputs(testcase, paths):
    missing = [p for p in paths if not os.path.exists(p)]
    if missing:
        testcase.fail("required read-only input missing (a test that checks nothing is a failed test): %s" % missing)
