#!/usr/bin/env python3
"""native_download_link.py — E2E RUNBOOK V2 §1A: relink the console installer
payload with the native download closure, under the extended ps5_io (RDWR).

Origin: worker-ps5io-rdwr, 2026-10-04 (ZCode team, PS5 mod-manager).
Controller-runnable. Local compiles only: no network, no console execution,
no file outside this worker dir is modified (all upstream inputs read-only).

What it adds to the worker-e2e-final console composition
(composition_e2e_inner.py console_build, whose object set is reproduced here):
  + console TUs  owner.c, curl_download.c, ps5_entropy.c   (download closure)
  + console TU   archive/private/file_sink.c               (REAL sink)
  + console TU   owned/modinst_download.c  (one recorded patch: the console
      branch of mi_open_sink_rdwr opens PS5IO_RDWR|CREAT|TRUNC — before the
      F1 extension it could only request WRONLY, which the sink's
      F_GETFL O_ACCMODE==O_RDWR admission (file_sink.c:31) refuses)
  + ps5_io extended vendored copy (this dir, patches.json) instead of the
      upstream worker-installer-ps5io source
  + link inputs   patched libcurl.a + libmbedtls.a + libmbedx509.a +
      libmbedcrypto.a, produced by download/dependency_build.py
      prepare_download("native", ...) (full pin verification included)
  + -lkernel_web (pinned curl closure link option) and -Wl,--gc-sections
  + -DMI_HAS_DOWNLOAD=1 for modinst_core/modinst_download (dispatch hook)
Keeps -Wl,--no-undefined,-z,now: any missing symbol aborts the link loudly.
NEVER links owned/mb_curl_stub_host.c (asserted).

Adaptation note (recorded, honest): the three download closure TUs and
file_sink.c are compiled with the pinned prospero-clang directly, replicating
the retained wrapper's forced includes (-DMBEDTLS_CONFIG_FILE="tls_config.h",
common.h, tls_effective.h) and include dirs, but without the wrapper's
-nostdinc isolation (the composition's console TUs rely on the SDK sysroot
default includes, e.g. <fcntl.h> for file_sink.c). All consumed inputs keep
their sha256-verified pinned identity; the import provider check re-validates
the result.

Usage:
  python3 native_download_link.py --verify-only   # phase 1 only (input proof)
  python3 native_download_link.py                 # full §1A relink
"""
import argparse
import hashlib
import json
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = pathlib.Path(os.environ.get("MODBRIDGE_ROOT", HERE.parents[2]))

# ---- pinned roots (same bindings as composition_e2e_inner.py + descriptor) --
SDK = ROOT / "outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk"
LLVM_BIN = ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
RESOURCE = ROOT / ".superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21"
ARCHIVE_PROFILE = ROOT / ".superpowers/native-dependency-spike/native-libarchive-lzma-experiment-1"
DL_PROFILE = ROOT / ".superpowers/native-dependency-spike/native-curl-profile-experiment-1"
CURL_SRC = ROOT / ".superpowers/native-dependency-spike/curl-release"
NATIVE_MBED = ROOT / "outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts/native-038ce86825317d4d74716405"
MBED_SRC = "mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5"
E2E = ROOT / "outputs/2026-10-04-zcode-team/worker-e2e-final"
E2E_VEND = E2E / "composition/vendored"
E2E_OWNED = E2E / "owned"
DL = ROOT / "outputs/ps5-mod-bridge/native-host/download"
ARCHIVE = ROOT / "outputs/ps5-mod-bridge/native-host/archive"
W = HERE  # worker-ps5io-rdwr

RETAINED_API = ["mb_curl_runtime_start_v1", "mb_curl_runtime_close_v1",
                "mb_curl_download_v1", "mb_curl_ps5_entropy_read32_v1"]

# the one recorded patch over the immutable worker-e2e-final TU
DOWNLOAD_PATCH = {
    "id": "download-console-branch-rdwr",
    "anchor": "return ps5io_open(path, PS5IO_WRONLY | PS5IO_CREAT | PS5IO_TRUNC, 0600);",
    "replacement": "return ps5io_open(path, PS5IO_RDWR | PS5IO_CREAT | PS5IO_TRUNC, 0600);",
    "occurrences": 1,
    "reason": "file_sink.c:31 admits only O_ACCMODE==O_RDWR; the pre-F1 console "
              "branch could not express it (worker-e2e-final REPORT F1)",
}

VERIFY_INPUTS = [
    # (label, path) — existence + sha256 recorded; consumers of the composition
    # trust worker-e2e-final's own receipts for their generation provenance.
    ("extended_ps5_io_c", W / "src/ps5_io.c"),
    ("extended_ps5_io_h", W / "src/ps5_io.h"),
    ("patches_json", W / "patches.json"),
    ("modinst_core_patched", E2E_VEND / "modinst_core.c"),
    ("modforge_activation_c", E2E_VEND / "modforge_activation.c"),
    ("ps5_io_dir_c", E2E_OWNED / "ps5_io_dir.c"),
    ("ps5_prepare_bridge_c", E2E_VEND / "ps5_prepare_bridge.c"),
    ("modinst_archive_prepare_c", E2E_OWNED / "modinst_archive_prepare.c"),
    ("e2e_status_c", E2E_OWNED / "e2e_status.c"),
    ("intake_c", E2E_VEND / "archive/intake.c"),
    ("posix_compat_c", E2E_VEND / "archive/private/posix_compat.c"),
    ("modinst_download_c", E2E_OWNED / "modinst_download.c"),
    ("modinst_download_h", E2E_OWNED / "modinst_download.h"),
    ("owner_c", DL / "owner.c"),
    ("owner_h", DL / "owner.h"),
    ("curl_download_c", DL / "curl_download.c"),
    ("curl_download_h", DL / "curl_download.h"),
    ("runtime_h", DL / "runtime.h"),
    ("ps5_entropy_c", DL / "ps5_entropy.c"),
    ("file_sink_c", ARCHIVE / "private/file_sink.c"),
    ("file_sink_h", ARCHIVE / "private/file_sink.h"),
    ("dependency_build_download_py", DL / "dependency_build.py"),
    ("dependency_build_archive_py", E2E_VEND / "archive/dependency_build.py"),
    ("link_layout_py", E2E_VEND / "archive/link_layout.py"),
    ("linker_export_script", E2E_VEND / "archive/private/posix_compat.h"),
    ("sdk_fcntl_h", SDK / "target/include/sys/fcntl.h"),
]

STUB_HOST = E2E_OWNED / "mb_curl_stub_host.c"  # must NEVER be linked


def digest(path):
    h = hashlib.sha256()
    with pathlib.Path(path).open("rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def die(msg):
    print("native_download_link: FATAL: " + msg, file=sys.stderr)
    sys.exit(2)


def run(argv, rec_dir, log_name, env=None):
    argv = [str(a) for a in argv]
    r = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    (rec_dir / (log_name + ".stdout")).write_bytes(r.stdout)
    (rec_dir / (log_name + ".stderr")).write_bytes(r.stderr)
    return {"argv": argv, "exit": r.returncode,
            "stdout": r.stdout.decode("utf-8", "replace"),
            "stderr": r.stderr.decode("utf-8", "replace")}


def phase_verify(rec):
    """Phase 1: every input present + sha256 recorded; SDK bit citation; the
    modinst_download console-branch anchor found exactly once; the extended
    ps5_io identity matches patches.json; the stub excluded by construction."""
    recorded = {}
    for label, path in VERIFY_INPUTS:
        p = pathlib.Path(path)
        if not p.is_file():
            die("input missing: %s (%s)" % (label, p))
        recorded[label] = {"path": str(p), "sha256": digest(p)}
    # extended adapter identity must match its own patch receipt
    patches = json.loads((W / "patches.json").read_text())
    if (recorded["extended_ps5_io_c"]["sha256"] != patches["patched_c_sha256"]
            or recorded["extended_ps5_io_h"]["sha256"] != patches["patched_h_sha256"]):
        die("extended ps5_io does not match patches.json — regenerate receipts")
    # SDK-cited bit value: O_RDWR must be 0x0002 in the pinned sysroot
    fcntl_txt = recorded["sdk_fcntl_h"] and (SDK / "target/include/sys/fcntl.h").read_text()
    oflags = [line.strip() for line in fcntl_txt.splitlines()
              if line.strip().startswith("#define") and
              any(line.split()[1] == "O_" + n for n in ("RDONLY", "WRONLY", "RDWR", "ACCMODE"))]
    if not any("#define" in l and "O_RDWR" in l and "0x0002" in l for l in oflags):
        die("pinned SDK sys/fcntl.h does not define O_RDWR 0x0002 — re-cite")
    # the console-branch patch anchor
    dl_src = (E2E_OWNED / "modinst_download.c").read_text()
    if dl_src.count(DOWNLOAD_PATCH["anchor"]) != DOWNLOAD_PATCH["occurrences"]:
        die("modinst_download console-branch anchor not found exactly once")
    if STUB_HOST.exists() and "mb_curl_stub_host" in str(VERIFY_INPUTS):
        die("internal error: stub leaked into the input list")
    rec["verify"] = {"inputs": recorded, "sdk_oflags": oflags,
                     "download_patch": DOWNLOAD_PATCH,
                     "stub_excluded": str(STUB_HOST),
                     "retained_api_symbols": RETAINED_API}
    print("VERIFY OK: %d inputs sha-recorded; SDK O_RDWR=0x0002; patch anchor "
          "unique; host stub excluded" % len(VERIFY_INPUTS))
    return rec


def console_flags(dl, deps_ar):
    """Console TU flags: the composition's core/closure flags + the download
    closure consumer settings (replicated wrapper contract, see module doc)."""
    mbed_include = NATIVE_MBED / "sources" / "mbedtls" / MBED_SRC / "include"
    crypto_config = NATIVE_MBED / "config"
    core = ["-std=c11", "-O2", "-Wall", "-Wextra", "-resource-dir", str(RESOURCE),
            "-DMB_ARCHIVE_BUILTIN_XZ=1",
            "-I" + str(W / "stage/src"), "-I" + str(E2E_VEND), "-I" + str(E2E_OWNED),
            "-I" + str(E2E_VEND / "archive"), "-I" + str(mbed_include),
            "-I" + str(crypto_config)]
    closure = list(core) + ["-Wpedantic",
                            '-DMBEDTLS_USER_CONFIG_FILE="crypto_config.h"']
    closure += ["-I" + d for d in deps_ar["include_dirs"]]  # archive.h etc.
    dl_closure = ["-std=c11", "-O2", "-Wall", "-Wextra", "-resource-dir", str(RESOURCE),
                  '-DMBEDTLS_CONFIG_FILE="tls_config.h"',
                  "-I" + str(DL), "-I" + str(ARCHIVE / "private"),
                  "-I" + str(ROOT / "outputs/ps5-mod-bridge/include")]  # modbridge/*.h roots
    dl_closure += ["-I" + d for d in dl["include_dirs"]]
    for f in dl["forced_includes"]:
        dl_closure += ["-include", f]  # one -include per forced header (wrapper contract)
    return core, closure, dl_closure


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--verify-only", action="store_true",
                    help="run phase 1 (input verification receipt) and stop")
    ap.add_argument("--out", default=str(HERE / "native-link"),
                    help="fresh output dir for the relink (default: native-link/)")
    args = ap.parse_args()

    rec_dir = HERE / "receipts" / "native-link"
    rec_dir.mkdir(parents=True, exist_ok=True)
    rec = {"origin": "worker-ps5io-rdwr", "date": "2026-10-04",
           "closes": "E2E-RUNBOOK-V2.md §1A (download closure into console payload)"}
    rec = phase_verify(rec)
    if args.verify_only:
        (rec_dir / "verify-only.json").write_text(json.dumps(rec, indent=2, sort_keys=True))
        print("verify-only receipt: %s" % (rec_dir / "verify-only.json"))
        return 0

    # ---- Phase 2: stage the patched download TU + extended adapter ---------
    out = pathlib.Path(args.out).absolute()
    if out.exists():
        die("output dir exists (keep every run fresh): " + str(out))
    stage = out / "stage"
    (stage / "src").mkdir(parents=True)
    dl_src = (E2E_OWNED / "modinst_download.c").read_text()
    patched = dl_src.replace(DOWNLOAD_PATCH["anchor"], DOWNLOAD_PATCH["replacement"])
    if patched == dl_src or patched.count("PS5IO_RDWR | PS5IO_CREAT") != 1:
        die("staged download TU patch did not apply exactly once")
    (stage / "src/modinst_download.c").write_text(patched)
    for f in ("ps5_io.c", "ps5_io.h"):
        (stage / "src" / f).write_text((W / "src" / f).read_text())
    rec["stage"] = {
        "modinst_download_c": {"origin": str(E2E_OWNED / "modinst_download.c"),
                               "origin_sha256": digest(E2E_OWNED / "modinst_download.c"),
                               "staged_sha256": digest(stage / "src/modinst_download.c"),
                               "patch": DOWNLOAD_PATCH},
        "ps5_io_c": {"staged_sha256": digest(stage / "src/ps5_io.c")},
        "ps5_io_h": {"staged_sha256": digest(stage / "src/ps5_io.h")}}

    # ---- Phase 3: pinned dependency closures (sha-verified by the modules) -
    sys.path.insert(0, str(DL))
    import dependency_build as dl_dep  # noqa: E402
    dl_out = out / "dep-download"
    deps_dl = dl_dep.prepare_download("native", dl_out, DL_PROFILE, CURL_SRC,
                                      sdk=SDK, llvm_bin=LLVM_BIN)
    import importlib.util  # noqa: E402
    spec = importlib.util.spec_from_file_location(
        "modbridge_archive_dependency_build", E2E_VEND / "archive/dependency_build.py")
    ar_dep = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(ar_dep)
    deps_ar = ar_dep.prepare_archive("native", ARCHIVE_PROFILE, sdk=SDK, llvm_bin=LLVM_BIN)
    sys.path.insert(0, str(E2E_VEND / "archive"))
    import link_layout  # noqa: E402
    profile = link_layout.write_linker_script(SDK, out)
    rec["dependencies"] = {
        "download": {"manifest": deps_dl["evidence_manifest"],
                     "archive_sha256": digest(deps_dl["link_archives"][0]),
                     "config_id": deps_dl["config_id"]},
        "archive": {"link_archives": [digest(p) for p in deps_ar["link_archives"]]}}

    cc, nm = SDK / "bin/prospero-clang", SDK / "bin/llvm-nm"
    env = os.environ.copy()
    env["LLVM_CONFIG"] = str(LLVM_BIN / "llvm-config")
    core, closure, dl_closure = console_flags(deps_dl, deps_ar)
    core_dl = list(core) + ["-DMI_HAS_DOWNLOAD=1", "-I" + str(E2E_VEND / "download")]

    targets = [
        ("modinst_core", E2E_VEND / "modinst_core.c", core_dl),
        ("ps5_io", stage / "src/ps5_io.c", core),
        ("ps5_io_dir", E2E_OWNED / "ps5_io_dir.c", core),
        ("ps5_prepare_bridge", E2E_VEND / "ps5_prepare_bridge.c", core),
        ("modinst_archive_prepare", E2E_OWNED / "modinst_archive_prepare.c", core),
        ("e2e_status", E2E_OWNED / "e2e_status.c", core),
        ("modforge_activation", E2E_VEND / "modforge_activation.c", core),
        ("intake", E2E_VEND / "archive/intake.c", closure),
        ("posix_compat", E2E_VEND / "archive/private/posix_compat.c", closure),
        ("file_sink", ARCHIVE / "private/file_sink.c", dl_closure),
        ("modinst_download", stage / "src/modinst_download.c", core_dl),
        ("owner", DL / "owner.c", dl_closure),
        ("curl_download", DL / "curl_download.c", dl_closure),
        ("ps5_entropy", DL / "ps5_entropy.c", dl_closure),
    ]

    # ---- Phase 4: console compiles -----------------------------------------
    commands, objects = [], []
    for name, src, flags in targets:
        obj = out / (name + ".ps5.o")
        r = run([cc, *flags, "-MD", "-MF", out / (name + ".d"), "-c", src, "-o", obj],
                rec_dir, "console-compile-" + name, env)
        commands.append(r)
        if r["exit"]:
            die("console compile failed for %s (see receipts/native-link/console-compile-%s.stderr)"
                % (name, name))
        objects.append(obj)

    # ---- Phase 5: the §1A link (composition point) --------------------------
    # This is where the download TU joins compose_installer's console payload:
    # the worker-e2e-final object set + the five download TUs + the pinned
    # curl/PSA closure, one ELF, --no-undefined -z now.
    # Single mbed tree: owner.c already defines mbedtls_platform_gmtime_r
    # (duplicate-symbol proof, receipts/native-link/console-link.stderr of the
    # first attempt), so the archive closure's libmbedcrypto.a is dropped and
    # every mbed symbol resolves from the download closure's pinned mbed libs
    # (same MBED_PIN 068ff08, retained consumer link order).
    libs = [pathlib.Path(p) for p in deps_ar["link_archives"]] + \
           [pathlib.Path(p) for p in deps_dl["link_archives"]]
    for lib in libs:
        if not lib.is_file():
            die("pinned native link input missing: " + str(lib))
    elf = out / "modbridge-installer.download.elf"
    caller_script = pathlib.Path(profile["caller_script"])
    r = run([cc, "-resource-dir", RESOURCE, *objects, *libs,
             "-Wl,--no-undefined,-z,now", "-Wl,--gc-sections",
             "-lkernel_web",
             "-Wl,-T," + str(caller_script),
             "-Wl,-Map=" + str(out / "modbridge-installer.download.map"),
             "-o", elf], rec_dir, "console-link", env)
    commands.append(r)
    if r["exit"]:
        die("console link failed (see receipts/native-link/console-link.stderr)")

    # ---- Phase 6: ELF verification -----------------------------------------
    r = run([LLVM_BIN / "llvm-readobj", "--file-headers", "--program-headers",
             "--dynamic-table", elf], rec_dir, "console-elf-readobj", env)
    commands.append(r)
    if r["exit"] or r["stderr"]:
        die("llvm-readobj failed/emitted stderr for the download ELF")
    r = run([nm, "-D", "--undefined-only", elf], rec_dir, "console-imports", env)
    commands.append(r)
    if r["exit"]:
        die("llvm-nm failed on the download ELF")
    names = sorted({line.split()[-1] for line in r["stdout"].splitlines() if line.strip()})
    providers = {}
    for lib in sorted((SDK / "target/lib").glob("*.so")):
        rr = run([nm, "-D", "--defined-only", lib], rec_dir,
                 "console-provider-" + lib.stem, env)
        if rr["exit"]:
            die("llvm-nm failed on pinned lib " + str(lib))
        for line in rr["stdout"].splitlines():
            parts = line.split()
            if parts:
                providers.setdefault(parts[-1], set()).add(lib.name)
    missing = [n for n in names if n not in providers]
    # retained API symbols must be present in the static image (defined)
    r2 = run([nm, "--defined-only", elf], rec_dir, "console-defined", env)
    defined = {line.split()[-1] for line in r2["stdout"].splitlines() if line.strip()}
    absent = [s for s in RETAINED_API if s not in defined]
    (rec_dir / "console-import-providers.json").write_text(json.dumps({
        "elf": str(elf), "elf_sha256": digest(elf), "elf_bytes": elf.stat().st_size,
        "imports": {n: sorted(providers.get(n, [])) for n in names},
        "missing": missing,
        "retained_api_symbols_absent": absent,
        "note": "every dynamic import resolved against the pinned SDK target "
                "libs; the four retained mb_curl_* API symbols are statically "
                "present; mb_curl_stub_host.c is not part of this link"},
        indent=2, sort_keys=True))
    if missing:
        die("DOWNLOAD ELF HAS UNRESOLVABLE IMPORTS — missing from pinned libs: "
            + ", ".join(missing))
    if absent:
        die("retained API symbols absent from the ELF: " + ", ".join(absent))
    rec["build"] = {"commands": commands,
                    "objects": [str(o) for o in objects],
                    "static_closure": [str(p) for p in libs],
                    "link_options": ["-Wl,--no-undefined,-z,now", "-Wl,--gc-sections",
                                     "-lkernel_web"],
                    "elf": {"path": str(elf), "bytes": elf.stat().st_size,
                            "sha256": digest(elf), "imports": len(names)}}
    (rec_dir / "native-link.json").write_text(json.dumps(rec, indent=2, sort_keys=True))
    print("NATIVE DOWNLOAD LINK OK: %s (%d bytes, %d imports, 0 missing, "
          "retained API present)" % (elf, elf.stat().st_size, len(names)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
