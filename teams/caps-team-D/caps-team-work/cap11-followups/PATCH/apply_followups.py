"""Apply the cap11 follow-up patch to the LIVE candidate tree (integrator only).

  python3 -I apply_followups.py            # --check (default): read-only; verifies bases, post-images, CHANGES rows
  python3 -I apply_followups.py --apply    # integrator: backs up, copies the 4 post-images, updates CHANGES.json

--apply refuses unless every live file still has its recorded base sha256 (so it never overwrites other work),
backs up each replaced file as <file>.pre-cap11-fu-20261013 and CHANGES.json as CHANGES.json.pre-cap11-fu-20261013,
copies the post-images, sets the four added_header_hashes values to the post sha256 and adds a
"cap11_fu_row_sync" note. It then re-reads everything and prints items_checked. Exit 0 = OK, 1 = mismatch, 2 = zero items.
"""
import hashlib
import json
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
LIVE = Path("/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4")
CHANGES = LIVE / "CHANGES.json"
TAG = ".pre-cap11-fu-20261013"
FILES = ["include/Platform/NativeGuiHost.hpp", "include/Platform/NativeModHostCore.hpp",
         "candidate/src/Mod/NativeGuiEngineBackend.inc", "candidate/src/Mod/NativeModEngineServices.inc"]


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    apply = "--apply" in sys.argv[1:]
    bases = {r["live_path"]: r for r in json.loads((HERE / "BASES.json").read_text())["changed"]}
    rows = json.loads(CHANGES.read_text())
    hh = rows["added_header_hashes"]
    items, bad = 0, []
    for rel in FILES:
        live, post = LIVE / rel, HERE / "post" / rel
        r = bases[str(live)]
        items += 1
        cur = sha(live)
        state = "base" if cur == r["base_sha256"] else ("ALREADY APPLIED" if cur == r["post_sha256"] else "MOVED")
        if sha(post) != r["post_sha256"]:
            bad.append(f"post-image sha mismatch {rel}")
        if str(live) not in hh:
            bad.append(f"CHANGES added_header_hashes has no row for {rel}")
        print(f"  {rel}: live={cur[:12]} ({state}) post={r['post_sha256'][:12]} CHANGES row={hh.get(str(live), '-')[:12]}")
        if state == "MOVED":
            bad.append(f"live file moved since staging: {rel} (re-stage the patch on the new base)")
    if bad or not apply:
        for b in bad:
            print("  FAIL " + b)
        print(f"apply_followups mode={'apply' if apply else 'check'} items_checked={items} problems={len(bad)}")
        sys.exit(2 if items == 0 else (1 if bad else 0))
    # --apply
    shutil.copy2(CHANGES, str(CHANGES) + TAG)
    for rel in FILES:
        live, post = LIVE / rel, HERE / "post" / rel
        if sha(live) == bases[str(live)]["base_sha256"]:
            shutil.copy2(live, str(live) + TAG)
            shutil.copyfile(post, live)
        hh[str(live)] = sha(live)
    rows["cap11_fu_row_sync"] = {
        "date": "2026-10-13",
        "by": "Team D follow-up (cap11-followups), applied by the integrator",
        "content": "[CAP11-FU] sharp GUI at any DPI (render target = Slate units x viewport scale pixels, cap 4096/side; ImGui "
                   "DisplayFramebufferScale; default font baked at its drawn size and density), m_built partial-build guard + "
                   "2 s failed-build throttle + input released while not shown, GetViewportScale one warning (no refusal), layout "
                   "re-check on open / every 120 GUI frames, C++ mod game-thread frame once per engine frame (Host::game_thread_pump).",
        "files": {rel: hh[str(LIVE / rel)] for rel in FILES},
        "backups": TAG,
    }
    CHANGES.write_text(json.dumps(rows, indent=2))  # same formatting as the file (indent 2, no trailing newline)
    ok = all(sha(LIVE / rel) == bases[str(LIVE / rel)]["post_sha256"] == json.loads(CHANGES.read_text())["added_header_hashes"][str(LIVE / rel)] for rel in FILES)
    print(f"apply_followups mode=apply items_checked={len(FILES)} result={'OK' if ok else 'MISMATCH'}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
