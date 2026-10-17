#!/usr/bin/env python3
"""test_records.py — exercise downloaded/prepared/installed record states for
the Item Spawner bundle per worker-7's model (src/modinst.c):

- durable records are TSV lines `mod <TAB> id <TAB> title <TAB> state <TAB> bundle`
  (modinst.c:241-244, records_save at :340-352);
- the load list is the post-crash/install truth: mods it names are installed;
  installed mods it no longer names drop back to prepared; downloaded mods
  never part of the transaction stay downloaded (records_commit_from_root,
  modinst.c:810-849);
- loadlist.txt is written LAST by the installer (FORMATS.md bundle/installer
  ordering), so its absence means "not installed yet".

Host-only emulation of that model against the staged roots produced by
build_bundle.py. No engine claims.
"""
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
STRICT = HERE / "build" / "stage-root"
WORK = HERE / "build" / "records-fixture"

MODS = {"TownfallItemSpawner": "TownfallItemSpawner-1.0.0.PPSA33286.mfb",
        "AnotherMod": "AnotherMod-1.0.PPSA33286.mfb"}  # second mod: control row


def records_write(states: dict) -> None:
    lines = ["modforge-records\t1"]
    for mod_id, state in states.items():
        lines.append(f"mod\t{mod_id}\tPPSA33286\t{state}\t{MODS[mod_id]}")
    (WORK / "records.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def records_read() -> dict:
    states = {}
    for line in (WORK / "records.txt").read_text(encoding="utf-8").splitlines():
        f = line.split("\t")
        if f and f[0] == "mod" and len(f) >= 5:
            states[f[1]] = f[3]
    return states


def loadlist_mods(root: Path) -> set:
    named = set()
    for line in (root / "loadlist.txt").read_text(encoding="utf-8").splitlines():
        f = line.split("\t")
        if f and f[0] == "mod" and len(f) >= 3:
            named.add(f[1])
    return named


def commit_from_root(states: dict, root: Path) -> dict:
    """Mirror of records_commit_from_root (modinst.c:815-849)."""
    named = loadlist_mods(root) if (root / "loadlist.txt").exists() else set()
    out = {}
    for mod_id, state in states.items():
        if mod_id in named:
            out[mod_id] = "installed"
        elif state == "installed":
            out[mod_id] = "prepared"  # no longer named by the load list
        else:
            out[mod_id] = state
    return out


def stage_partial() -> Path:
    """Files staged but loadlist.txt NOT yet written (pre-commit)."""
    partial = WORK / "root"
    if partial.exists():
        shutil.rmtree(partial)
    shutil.copytree(STRICT, partial)
    (partial / "loadlist.txt").unlink()
    return partial


def check(cond: str, ok: bool) -> bool:
    print(("PASS " if ok else "FAIL ") + cond)
    return ok


def main() -> int:
    if not STRICT.exists():
        sys.exit("run build_bundle.py first")
    WORK.mkdir(parents=True, exist_ok=True)
    ok = True

    # S1 downloaded: bundles known, root not staged
    states = {m: "downloaded" for m in MODS}
    records_write(states)
    ok &= check("S1 records start downloaded for both mods",
                records_read() == {"TownfallItemSpawner": "downloaded", "AnotherMod": "downloaded"})

    # S2 prepared: the transaction's journal step stages the mod's files and
    # marks it prepared (an explicit step, not root-derived — modinst.c only
    # derives installed / installed->prepared from the load list, :810-849);
    # a crash before loadlist-write then leaves prepared untouched on commit
    partial = stage_partial()
    states["TownfallItemSpawner"] = "prepared"  # journal: files staged for install
    states = commit_from_root(states, partial)
    records_write(states)
    ok &= check("S2 crash before loadlist-write keeps prepared (neither installed nor regressed)",
                states["TownfallItemSpawner"] == "prepared")

    # S3 installed: loadlist.txt written last -> named mod becomes installed
    shutil.copy(STRICT / "loadlist.txt", partial / "loadlist.txt")
    states = commit_from_root(states, partial)
    records_write(states)
    ok &= check("S3 loadlist present commits TownfallItemSpawner to installed",
                states["TownfallItemSpawner"] == "installed")
    ok &= check("S3 mod absent from loadlist stays downloaded (never in transaction)",
                states["AnotherMod"] == "downloaded")

    # S4 recovery: a previously installed mod no longer named drops to prepared
    states["AnotherMod"] = "installed"
    states = commit_from_root(states, partial)
    ok &= check("S4 installed-but-unnamed mod drops back to prepared",
                states["AnotherMod"] == "prepared"
                and states["TownfallItemSpawner"] == "installed")

    # S5 the installed root is exactly the activation input the C test consumed
    ok &= check("S5 fixture root still activates (files unchanged by records flow)",
                (partial / "ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua").stat().st_size == 21415)

    print("\nRESULT: " + ("ALL PASS" if ok else "FAILURES"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
