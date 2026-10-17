#!/usr/bin/env python3
"""Check the bounded curl 8.18 backport using retained ordinary-host artifacts.

Copies two upstream translation units and the static archive into a fresh
output directory. Never builds a console binary or modifies retained inputs.
The optional loopback controls use only the existing frozen local TLS fixture.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
SOURCE_HASHES = {
    "mbedtls.c": "8127975c23d0478a98c31d6f184758bc2845f177e2b0932effb537e3336c16d6",
    "vtls.c": "9d2d6bfa917ee33b45065a19d583456abc45ca3ca852917657725265dbba2b29",
}
OBSERVERS = [
    "-Dmbedtls_ctr_drbg_free=test_mbedtls_ctr_drbg_free",
    "-Dmbedtls_entropy_free=test_mbedtls_entropy_free",
    "-Dmbedtls_psa_crypto_free=test_mbedtls_psa_crypto_free",
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(argv, cwd, log, env=None):
    result = subprocess.run([str(x) for x in argv], cwd=cwd,
                            capture_output=True, text=True, timeout=60, env=env)
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}); see {log}")


def client(argv, out):
    result = subprocess.run([str(x) for x in argv], capture_output=True,
                            text=True, timeout=10)
    out.with_suffix(".stdout.json").write_text(result.stdout)
    out.with_suffix(".stderr.log").write_text(result.stderr)
    receipt = {"exit": result.returncode, "stderr_bytes": len(result.stderr)}
    if result.stdout:
        receipt["client"] = json.loads(result.stdout)
    out.with_suffix(".receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--curl-source", type=Path, required=True)
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--loopback", action="store_true")
    args = parser.parse_args()
    source, profile, out = (x.resolve() for x in
                            (args.curl_source, args.profile, args.output))
    if any(out == retained or retained in out.parents for retained in (source, profile)):
        raise RuntimeError("output must be outside both retained input trees")
    out.mkdir(parents=True, exist_ok=False)
    original = {name: source / "lib/vtls" / name for name in SOURCE_HASHES}
    for name, path in original.items():
        if digest(path) != SOURCE_HASHES[name]:
            raise RuntimeError(f"unexpected curl 8.18 source: {name}")
    host = profile / "host"
    archive = host / "curl-build/lib/.libs/libcurl.a"
    immutable = list(original.values()) + [archive, host / "curl-owned-fixture"]
    before = {str(path): digest(path) for path in immutable}
    cc = host / "cc.py"
    includes = ["-DHAVE_CONFIG_H", "-DBUILDING_LIBCURL",
                "-DCURL_STATICLIB", "-DCURL_HIDDEN_SYMBOLS",
                "-I" + str(source / "include"),
                "-I" + str(host / "curl-build/lib"),
                "-I" + str(source / "lib"), "-I" + str(source / "lib/vtls"),
                "-isystem", str(host / "mbed-prefix/include"),
                "-Qunused-arguments", "-Werror-implicit-function-declaration",
                "-O2", "-Werror=partial-availability", "-fvisibility=hidden"]
    objects = [host / "entropy.o", host / "clock_policy.o"]
    libraries = [host / ("lib" + name + ".a")
                 for name in ("mbedtls", "mbedx509", "mbedcrypto")]
    frameworks = ["-framework", "SystemConfiguration", "-framework", "CoreFoundation"]

    # First red: the untouched public consumer must crash before opening a
    # socket when the host-only provider deliberately refuses curl seeding.
    red = client([host / "curl-owned-fixture", "1", "source-curl", "1048576"],
                 out / "original-crash")
    if red["exit"] != -11 or "client" in red:
        raise RuntimeError("original seed-refusal crash was not reproduced")

    def build_variant(name, patched, observed):
        directory = out / name
        directory.mkdir()
        private_source = directory / "lib/vtls"
        private_source.mkdir(parents=True)
        for file, path in original.items():
            shutil.copyfile(path, private_source / file)
        if patched:
            # An ignored experiment inside the worktree is not itself the Git
            # root. Stop repository discovery so apply treats this copy as its
            # working directory instead of silently excluding its patch paths.
            env = dict(os.environ, GIT_CEILING_DIRECTORIES=str(directory.parent))
            command(["git", "apply", "--check", HERE / "curl-8.18-init-failure.patch"],
                    directory, directory / "patch-check.log", env)
            command(["git", "apply", HERE / "curl-8.18-init-failure.patch"],
                    directory, directory / "patch.log", env)
            if any(digest(private_source / file) == SOURCE_HASHES[file] for file in original):
                raise RuntimeError("patch did not change both private translation units")
        else:
            # Isolate the second bug: remove only the invalid NULL logger.
            path = private_source / "mbedtls.c"
            old = '    failf(NULL, " failed\\n  ! mbedtls_ctr_drbg_seed returned -0x%x\\n",\n          (unsigned int)-ret);\n'
            text = path.read_text()
            if text.count(old) != 1:
                raise RuntimeError("unexpected seed-error logger")
            path.write_text(text.replace(old, ""))
        private_archive = directory / "libcurl.a"
        shutil.copyfile(archive, private_archive)
        for file in original:
            obj = directory / ("libcurl_la-" + file[:-2] + ".o")
            flags = OBSERVERS if observed and file == "mbedtls.c" else []
            command([cc, *includes, *flags, "-c", private_source / file, "-o", obj],
                    directory, directory / (file + ".compile.log"))
            command(["ar", "rcs", private_archive, obj], directory,
                    directory / (file + ".archive.log"))
        if observed:
            driver = directory / "test.o"
            command([cc, "-I" + str(source / "include"), "-c",
                     HERE / "curl_init_failure_test.c", "-o", driver],
                    directory, directory / "test.compile.log")
            inputs = [driver, *objects]
            binary = directory / "curl-init-test"
        else:
            inputs = [host / "curl_attempt.o", host / "response_policy.o", *objects]
            binary = directory / "curl-owned-fixture"
        command([cc, *inputs, private_archive, *libraries, *frameworks, "-o", binary],
                directory, directory / "link.log")
        return binary

    intermediate = build_variant("logger-only-red", False, True)
    latch_red = client([intermediate, "refuse"], out / "false-initialized-latch")
    if latch_red["exit"] != 1 or latch_red.get("client", {}).get("first") != 2 or \
            latch_red.get("client", {}).get("second") != 0:
        raise RuntimeError("false initialized latch was not reproduced")

    observed = build_variant("patched-observed", True, True)
    checks = {}
    for mode in ("refuse", "healthy"):
        result = client([observed, mode], out / ("patched-" + mode))
        checks[mode + "_global_init_and_cleanup"] = (
            result["exit"] == 0 and result.get("client", {}).get("passed") is True)
    production = build_variant("patched-consumer", True, False)
    for mode in ("source-curl", "source-psa"):
        result = client([production, "1", mode, "1048576"], out / mode)
        c = result.get("client", {})
        checks[mode + "_terminal_no_transfer"] = (
            result["exit"] == 1 and c.get("source_failed") == 1 and
            c.get("source_closed") == 1 and c.get("committed_bytes") == 0 and
            c.get("application_complete") is False and
            c.get("authentication_success") is False and
            c.get("caller_buffer_cleared") == 1 and c.get("multi_drives") == 0)
        if mode == "source-curl":
            checks["curl_refusal_returns_failed_init"] = (
                c.get("curl_global_rc") == 2 and c.get("curl_global_completed") == 0)
    if args.loopback:
        spec = importlib.util.spec_from_file_location("owned_fixture", profile / "loopback_fixture.py")
        module = importlib.util.module_from_spec(spec)
        old_bytecode_policy = sys.dont_write_bytecode
        try:
            sys.dont_write_bytecode = True
            spec.loader.exec_module(module)
        finally:
            sys.dont_write_bytecode = old_bytecode_policy
        # The module's read-only inputs stay at the retained profile. Only its
        # consumer path moves to the private rebuilt archive/binary.
        (out / "host").mkdir()
        (out / "host/curl-owned-fixture").symlink_to(production)
        module.P = out
        for case in ("fixed", "wrong-root", "wrong-name"):
            result = module.run_case(case, out)
            checks["loopback_" + case] = result["passed"]
    after = {str(path): digest(path) for path in immutable}
    checks["retained_inputs_unchanged"] = before == after
    receipt = {"schema": 1, "checks": checks, "passed": all(checks.values()),
               "original_inputs": before, "patch_sha256": digest(HERE / "curl-8.18-init-failure.patch"),
               "console_executed": False, "native_build": False,
               "dependency_upgrade": False, "production_integration": False}
    (out / "summary.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))
    return 0 if receipt["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
