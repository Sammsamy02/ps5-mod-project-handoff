"""Recompute every pin Team G owns in the scratch #10 tree, then write the
#10-only patch (after7 -> after10) and its receipt. Re-runnable.

  * INTEGRATION-INPUTS.json: units' source_pin, header_pins, pinned_inputs
    rows for cap10/* files, admitted_imports, include_exemptions sha.
  * typescan-r4/CHANGES.json: the files[] UE4SSProgram.cpp row's
    candidate_sha256 (G2 pin) + the cap10_row_sync record.
  * PATCH/cap10-only-on-R15e+cap07.patch (text only, diff -ruN, -p1 from
    the worktree root) + PATCH/binaries/ (the 4 pinned binaries, copied) +
    PATCH/PATCH-RECEIPT.json.
usage: python3 -I refresh_pins.py
"""
import hashlib, json, shutil, subprocess, sys
from pathlib import Path

G = Path(__file__).resolve().parents[1]
S = G / "scratch"
A7 = S / "r16-base/outputs/2026-10-08-l26-runtime"
A10 = S / "r16-after/outputs/2026-10-08-l26-runtime"
W = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
LIVE = W / "outputs/2026-10-08-l26-runtime"
STAGE = W / "outputs/2026-10-08-l25-prep/intake/gpt-7-10-v2-20261013/unpacked/caps-team-work/cap10-integration-ready/derived/target-ready/outputs/2026-10-08-l26-runtime"
BINARIES = {
    "cap10/lib/libmonosgen-2.0.a": "24eebde5354ccf8ea3061d3a2bc47fd8eec48182934a91b304cd99b366d76a4f",
    "cap10/lib/mscorlib.full-interp.ps5.o": "a9f0a8564ac09739d03b2efd4d7f06fa54c003d24a060f72a7966811251840de",
    "cap10/managed/Probe.dll": "c328dca11447b2c41ab1580582b9576911b0291a4d14d89a09a708e69e1c108d",
    "cap10/runtime-assets/lib/mono/4.5/mscorlib.dll": "86364b7803c92d8c88b590031f015f80b10b00641e9d067e07a529a131d815e4",
}
SIGNAL_PAIR = {
    "cap10/native/MonoOwner.cpp": "03edcc27dced6d2a1fa03162bbe3fbd74345e9d2207653688422a78d086c0bad",
    "cap10/native/MonoOwner.hpp": "8121c4ceb2c8bb6d3172c22abd7e969ecc0643ddd24cdcba1109ccf49bc169ed",
}


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def row(rel):
    p = A10 / rel
    return {"path": rel, "sha256": sha(p), "bytes": p.stat().st_size}


def main():
    for rel, want in {**BINARIES, **SIGNAL_PAIR}.items():
        if sha(A10 / rel) != want:
            raise SystemExit(f"FAIL: pinned input drift {rel}")
    f = A10 / "cap10/INTEGRATION-INPUTS.json"
    d = json.loads(f.read_text())
    for name, spec in d["units"].items():
        spec["source_pin"] = row(spec["source_pin"]["path"])
    d["header_pins"] = [row(r["path"]) for r in d["header_pins"]]
    d["pinned_inputs"] = [row(r["path"]) if not Path(r["path"]).is_absolute() else r for r in d["pinned_inputs"]]
    d["admitted_imports"] = row("cap10/ADMITTED-IMPORTS.json")
    for e in d["include_exemptions"]:
        if sha(e["path"]) != e["sha256"]:
            raise SystemExit("FAIL: include exemption target drift " + e["path"])
    f.write_text(json.dumps(d, indent=2) + "\n")

    ch = A10 / "typescan-r4/CHANGES.json"
    c = json.loads(ch.read_text())
    prog = str(LIVE / "typescan-r4/candidate/src/UE4SSProgram.cpp")
    rows = [r for r in c["files"] if r["candidate"] == prog]
    if len(rows) != 1:
        raise SystemExit("FAIL: UE4SSProgram.cpp files[] row not found exactly once")
    new_prog = sha(A10 / "typescan-r4/candidate/src/UE4SSProgram.cpp")
    rows[0]["candidate_sha256"] = new_prog
    c["cap10_row_sync"] = {
        "date": "2026-10-13",
        "by": "Team G (cap10 integration prep; integrator applies)",
        "content": ("[CAP10] C# runtime (embedded Mono, interpreter + static corlib) for the built-in C# probe. "
                    "LAZY: Mono starts only when the activation snapshot enables a C# mod (mods.txt 'Name : 1' or enabled.txt, the #9 rule) "
                    "whose <mods>/<Name>/csharp/ holds an assembly byte-identical to the built-in probe (Cap10CSharpMods.hpp); otherwise "
                    "init() is the unchanged pre-#10 call (no Mono init, no signal change, no thread, no mapping). SceNet: 7 resolver "
                    "entry points defined fail-closed (ENETDOWN) in Cap10OfflineNet.cpp, 0 SceNet imports. Fixed gates: G7 declared "
                    "raw delta, P7/P11 = 207 + admitted rows (cap10/ADMITTED-IMPORTS.json), P8 240/2, exact include exemption <csignal> "
                    "for cap10-mono-owner. GPT V2 owner/runtime signal pair unchanged (03edcc27 / 24eebde5)."),
        "files": {"src/UE4SSProgram.cpp": new_prog},
        "cap10_inputs": {"INTEGRATION-INPUTS.json": sha(f), "ADMITTED-IMPORTS.json": sha(A10 / "cap10/ADMITTED-IMPORTS.json")},
    }
    ch.write_text(json.dumps(c, indent=2))  # live file has no trailing newline

    out = G / "PATCH"
    out.mkdir(exist_ok=True)
    patch = out / "cap10-only-on-live-R16d-fu.patch"
    # text diff over every file except the 4 binaries (copied separately)
    tmp_a, tmp_b = S / "_pa/outputs", S / "_pb/outputs"
    for t in (S / "_pa", S / "_pb"):
        shutil.rmtree(t, ignore_errors=True)
    shutil.copytree(S / "r16-base/outputs", tmp_a)
    shutil.copytree(S / "r16-after/outputs", tmp_b)
    for rel in BINARIES:
        (tmp_b / "2026-10-08-l26-runtime" / rel).unlink()
    # diff wants a/ b/ prefixes: run inside a temp root with symlinked names
    root = S / "_diffroot"
    shutil.rmtree(root, ignore_errors=True)
    root.mkdir()
    (root / "a").symlink_to(S / "_pa")
    (root / "b").symlink_to(S / "_pb")
    r = subprocess.run(["diff", "-ruN", "a/outputs", "b/outputs"], cwd=root, capture_output=True, text=True)
    if r.returncode not in (0, 1):
        raise SystemExit("FAIL: diff error " + r.stderr[:300])
    patch.write_text(r.stdout)
    shutil.rmtree(root)
    for t in (S / "_pa", S / "_pb"):
        shutil.rmtree(t)
    files = [l[len("+++ b/"):].split("\t")[0] for l in r.stdout.splitlines() if l.startswith("+++ b/")]
    bdir = out / "binaries"
    for rel, want in BINARIES.items():
        dst = bdir / "outputs/2026-10-08-l26-runtime" / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        src = STAGE / rel
        if sha(src) != want:
            raise SystemExit("FAIL: stage binary drift " + rel)
        if not dst.exists() or sha(dst) != want:
            shutil.copyfile(src, dst)
    receipt = {
        "patch": {"path": str(patch), "sha256": sha(patch), "bytes": patch.stat().st_size, "files": len(files), "file_list": files},
        "applies_on": "the CURRENT live tree outputs/2026-10-08-l26-runtime (R16d 3b4c202b + reviewed #7 step 2 31d4839a + r16b/r16c + #11 follow-ups), patch -p1 from the worktree root; base shas in live_base_shas",
        "binaries": {rel: {"sha256": want, "copy_to": "outputs/2026-10-08-l26-runtime/" + rel, "source": str(bdir / "outputs/2026-10-08-l26-runtime" / rel)} for rel, want in BINARIES.items()},
        "signal_pair_unchanged": SIGNAL_PAIR,
        "live_base_shas": {str(p.relative_to(S / "r16-base/outputs")): sha(p) for p in sorted((S / "r16-base/outputs").rglob("*")) if p.is_file()},
        "after10_shas": {str(p.relative_to(S / "r16-after/outputs")): sha(p) for p in sorted((S / "r16-after/outputs").rglob("*")) if p.is_file()},
    }
    (out / "PATCH-RECEIPT-R16d-fu.json").write_text(json.dumps(receipt, indent=1) + "\n")
    print(f"patch {sha(patch)[:16]} files={len(files)} UE4SSProgram={new_prog[:16]} INTEGRATION-INPUTS={sha(f)[:16]}")


if __name__ == "__main__":
    main()
