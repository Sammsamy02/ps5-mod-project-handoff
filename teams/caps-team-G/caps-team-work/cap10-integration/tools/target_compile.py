"""Mac-only TARGET compile of the #10 units with the R15e build's own compile
command (Team G; modelled on caps-team-D cap09/tests/target_compile_check.py).

For each --unit NAME=SOURCE[:TEMPLATE] (TEMPLATE default 'program', as the
cap10 NEW_UNITS clone it):
  * take R15e's <template>.command.json (the build that produced 53a64553,
    located by its runtime-l26-A.o sha, wherever the pipeline moved it);
  * swap in SOURCE, -o/-MF into --out;
  * replace the build's header overlay with a merged copy in which every
    header changed by the (#7 + #10) scratch tree is redirected to the
    scratch copy (the real integration edits those files in place);
  * apply cap10_integration.compile_argv's rule for cap10 units + 'program':
    MODBRIDGE_CAP10_ADMITTED_MONO_PORT=1, CORLIB/CONFIG dirs from the pinned
    MODBRIDGE_MOD_ROOT, and the three cap10 include dirs;
  * compile with the SDK prospero-clang++ (LLVM_CONFIG = LLVM 21.1.8).
At most --jobs (<=2) compiles at a time; before starting each compile, free
memory (vm_stat free+inactive+purgeable) must be >= 4 GB, else it waits.
Prints items_checked; exit 0 PASS, 1 FAIL, 2 zero items / tool missing.
Read-only on every project tree; writes only under --out.
"""
import argparse, hashlib, json, os, re, subprocess, sys, time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

W = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
BV = W / "outputs/2026-10-08-l26-build-variants"
LIVE = W / "outputs/2026-10-08-l26-runtime"
R15E_RUNTIME_SHA = "84dfccb37c0fb753eed41b48dc77ff2aa9523ffa3c33c20a660a13959104744e"
# The base build can be switched (rebase onto a later build) without editing:
# CAP10_BASE_RUNTIME_SHA / CAP10_BASE_RUNTIME_BYTES (default R15e 53a64553).
BASE_RUNTIME_SHA = os.environ.get("CAP10_BASE_RUNTIME_SHA", R15E_RUNTIME_SHA)
BASE_RUNTIME_BYTES = int(os.environ.get("CAP10_BASE_RUNTIME_BYTES", "127154472"))
LLVM = W / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
ROOT_SOURCE = W / "outputs/2026-10-08-combined-outer-lifecycle/candidate-v2/runtime_module_entry.c"
MIN_FREE = 4 * 1024**3


def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def find_r15e():
    cands = [BV / "l26-A"] + sorted(BV.glob("archive-r6/*/l26-A"), reverse=True)
    for root in cands:
        o = root / "pass1/runtime-l26-A.o"
        if o.exists() and o.stat().st_size == BASE_RUNTIME_BYTES and sha(o) == BASE_RUNTIME_SHA:
            return root
    raise SystemExit("FAIL: base build objects (runtime-l26-A.o sha " + BASE_RUNTIME_SHA[:16] + "...) not found")


def free_bytes():
    out = subprocess.run(["vm_stat"], capture_output=True, text=True).stdout
    page = int(re.search(r"page size of (\d+)", out).group(1))
    tot = 0
    for key in ("Pages free", "Pages inactive", "Pages purgeable"):
        m = re.search(key + r":\s+(\d+)", out)
        tot += int(m.group(1)) if m else 0
    return tot * page


def wait_memory(tag):
    waited = 0
    while free_bytes() < MIN_FREE:
        if waited == 0:
            print(f"  [mem] {tag}: free {free_bytes()/1e9:.2f} GB < 4 GB, pausing (integrator build has priority)", flush=True)
        time.sleep(15)
        waited += 15
        if waited > 3600:
            raise SystemExit("FAIL: memory stayed below 4 GB for an hour")
    return free_bytes()


def merged_overlay(build_overlay, scratch_outputs, out):
    """Build overlay with every changed scratch header redirected."""
    d = json.loads(Path(build_overlay).read_text())
    redirect = {}
    for p in Path(scratch_outputs).rglob("*"):
        if p.is_file() and p.suffix in (".hpp", ".h", ".inc", ".inl"):
            rel = p.relative_to(scratch_outputs)
            live = W / "outputs" / rel
            if not live.exists() or sha(live) != sha(p):
                redirect[str(live)] = str(p)
    def walk(e):
        if e.get("type") == "file" and e.get("external-contents") in redirect:
            e["external-contents"] = redirect[e["external-contents"]]
        for c in e.get("contents", []):
            walk(c)
    for r in d["roots"]:
        walk(r)
    for live, scr in sorted(redirect.items()):
        d["roots"].append({"type": "file", "name": live, "external-contents": scr})
    Path(out).write_text(json.dumps(d, indent=1))
    return redirect


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unit", action="append", required=True, help="NAME=SOURCE[:TEMPLATE]")
    ap.add_argument("--scratch-outputs", required=True, help="scratch <tree>/outputs (the #7+#10 applied copy)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--no-cap10", action="append", default=[], help="unit NAME compiled WITHOUT the cap10 defines (baseline)")
    a = ap.parse_args()
    if a.jobs > 2:
        raise SystemExit("FAIL: at most 2 parallel compiles")
    out = Path(a.out).resolve(); out.mkdir(parents=True, exist_ok=True)
    r15e = find_r15e()
    objdir = r15e / "pass1/objects"
    print(f"  base build objects: {r15e} (runtime-l26-A.o sha {BASE_RUNTIME_SHA[:16]}...)")
    scratch = Path(a.scratch_outputs).resolve()
    redirect = merged_overlay(r15e / "headers.vfs.json", scratch, out / "headers.merged.vfs.json")
    print(f"  merged header overlay: {len(redirect)} scratch header(s) redirected")
    for k, v in sorted(redirect.items()):
        print(f"    {Path(k).relative_to(W)} -> scratch sha {sha(v)[:8]}")
    root_text = ROOT_SOURCE.read_text()
    roots = re.findall(r'^#define\s+MODBRIDGE_MOD_ROOT\s+"([^"\n]+)"', root_text, re.M)
    if len(roots) != 1:
        raise SystemExit("FAIL: MODBRIDGE_MOD_ROOT not found exactly once")
    root = roots[0]
    cap10_dir = scratch / "2026-10-08-l26-runtime/cap10"
    cap10_args = ["-DMODBRIDGE_CAP10_ADMITTED_MONO_PORT=1", f'-DMODBRIDGE_CAP10_CORLIB_DIR="{root}/lib"', f'-DMODBRIDGE_CAP10_CONFIG_DIR="{root}/etc"']
    ii = json.loads((cap10_dir / "INTEGRATION-INPUTS.json").read_text())
    sizes = {}
    for row in ii["runtime_assets"]:
        f = cap10_dir.parent / row["path"]
        if sha(f) != row["sha256"] or f.stat().st_size != row["bytes"]:
            raise SystemExit("FAIL: runtime asset pin drift " + row["path"])
        sizes[row["destination"]] = row["bytes"]
    cap10_args += [f"-DMODBRIDGE_CAP10_CORLIB_BYTES={sizes['lib/mono/4.5/mscorlib.dll']}u", f"-DMODBRIDGE_CAP10_CONFIG_BYTES={sizes['etc/mono/config']}u"]
    for inc in ("native", "mono-headers", "mono-headers/mono/eglib"):
        cap10_args += ["-I", str(cap10_dir / inc)]
    env = dict(os.environ, LLVM_CONFIG=str(LLVM / "llvm-config"))
    jobs = []
    for spec in a.unit:
        name, rest = spec.split("=", 1)
        src, _, tmpl = rest.partition(":")
        tmpl = tmpl or "program"
        cmd_path = objdir / f"{tmpl}.command.json"
        argv = json.loads(cmd_path.read_text())["argv"]
        old_root = str(BV / "l26-A")
        argv = [x.replace(old_root, str(r15e)) if x.startswith(old_root) else x for x in argv]
        srcs = [i for i, x in enumerate(argv) if x.endswith((".cpp", ".cc", ".cxx", ".c")) and not x.startswith("-")]
        if len(srcs) != 1:
            raise SystemExit(f"FAIL: {name}: expected one source in the template command, found {len(srcs)}")
        orig_src = argv[srcs[0]]
        argv[srcs[0]] = str(Path(src).resolve())
        argv[argv.index("-o") + 1] = str(out / f"{name}.o")
        argv[argv.index("-MF") + 1] = str(out / f"{name}.d")
        ov = argv.index("-ivfsoverlay")
        argv[ov + 1] = str(out / "headers.merged.vfs.json")
        extra = [] if name in a.no_cap10 else list(cap10_args)
        extra += ["-I", str(Path(src).resolve().parent)]
        argv = [argv[0]] + extra + argv[1:]
        if any("MODBRIDGE_CAP10_MONO_HOST_VALIDATION" in x for x in argv):
            raise SystemExit("FAIL: host validation define in a target compile")
        (out / f"{name}.command.json").write_text(json.dumps({"argv": argv, "template": str(cmd_path), "template_sha256": sha(cmd_path), "template_source": orig_src}, indent=1))
        jobs.append((name, src, tmpl, argv, cmd_path))

    def run(job):
        name, src, tmpl, argv, cmd_path = job
        fb = wait_memory(name)
        t0 = time.time()
        r = subprocess.run(argv, capture_output=True, text=True, env=env)
        dt = time.time() - t0
        (out / f"{name}.stderr").write_text(r.stderr)
        ok = r.returncode == 0 and (out / f"{name}.o").exists()
        return name, src, tmpl, cmd_path, r, ok, dt, fb

    fails = 0
    with ThreadPoolExecutor(max_workers=a.jobs) as ex:
        for name, src, tmpl, cmd_path, r, ok, dt, fb in ex.map(run, jobs):
            errors = r.stderr.count(" error: "); warns = r.stderr.count(" warning: ")
            print(("  pass " if ok else "  FAIL ") + f"target compile {name}: {Path(src).name} (template {tmpl}, base-build command sha={sha(cmd_path)[:8]}) exit={r.returncode} errors={errors} warnings={warns} {dt:.0f}s free-at-start={fb/1e9:.1f}GB"
                  + (f" obj sha={sha(out / (name + '.o'))[:8]}" if ok else ""))
            if not ok:
                print(r.stderr[-4000:]); fails += 1
    print(f"target_compile items_checked={len(jobs)} failed={fails}")
    if not jobs:
        sys.exit(2)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
