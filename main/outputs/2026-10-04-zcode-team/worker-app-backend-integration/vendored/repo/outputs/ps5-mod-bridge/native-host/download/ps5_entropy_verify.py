#!/usr/bin/env python3
"""Private host-boundary controls of the actual PS5 source and curl owner.

Only the syscall is injected in private objects. No target binary or console,
entropy quality, TLS, download or runtime readiness is claimed. Original source
and the checkpoint's four-call policy stay immutable. RED uses that retained
reader with its one shared budget; GREEN uses the new stateless checked source.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
DIRECT = ["exact", "stale-errno", "rc-negative", "rc-negative-no-errno",
          "rc-positive", "errno-nonzero", "length-zero", "length-one",
          "length-short", "length-oversized", "length-max", "invalid-context",
          "invalid-output"]
OWNED = ["quota", "source-fault", "outside-window"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--red-legacy", action="store_true")
    args = parser.parse_args()
    profile, out = args.profile.resolve(), args.output.resolve()
    if any(out == p or p in out.parents for p in (profile, HERE)):
        raise RuntimeError("fresh evidence must be outside retained profile/source directories")
    checkpoint = HERE.parent
    cc = profile / "host/cc.py"
    inputs = [checkpoint / name for name in ("entropy.c", "entropy.h", "drbg.c", "drbg.h")]
    inputs.append(checkpoint / "tests/entropy_shim/sys/sysctl.h")
    inputs += [HERE / name for name in ("owner.c", "owner.h", "curl_download.h", "ps5_entropy.h",
                                      "tests/ps5_entropy_control.c", "ps5_entropy_verify.py")]
    inputs += [cc, *[profile / name for name in ("tls_config.h", "tls_effective.h", "psa_config.h", "clock_decl.h")]]
    if not args.red_legacy:
        inputs.append(HERE / "ps5_entropy.c")
    before = {str(path): digest(path) for path in inputs}
    out.mkdir(mode=0o700, parents=True, exist_ok=False)
    commands = []

    def run(command):
        result = subprocess.run([str(arg) for arg in command], capture_output=True,
                                text=True, timeout=30)
        commands.append({"argv": [str(arg) for arg in command], "exit": result.returncode,
                         "stdout": result.stdout, "stderr": result.stderr})
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        return result

    flags = ["-O2", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
             "-fno-omit-frame-pointer", "-I" + str(HERE),
             "-I" + str(checkpoint / "tests/entropy_shim")]
    source_units = [(HERE / "owner.c", []), (HERE / "tests/ps5_entropy_control.c", [])]
    boundary = ["-Dsysctl=mb_ps5_entropy_fixture_sysctl"]
    if args.red_legacy:
        bridge = out / "legacy-four-call-bridge.c"
        bridge.write_text("/* Private RED callback bridge; original four-call reader unchanged. */\n"
                          "#include \"ps5_entropy.h\"\n#include " + json.dumps(str(checkpoint / "entropy.h")) + "\n"
                          "static struct mb_entropy_budget shared;\n"
                          "int mb_curl_ps5_entropy_read32_v1(void *context,uint8_t out[32]) {\n"
                          " (void)context;struct mb_entropy_result metadata;\n"
                          " return mb_entropy_read32(&metadata,out,&shared);\n}\n")
        source_units += [(checkpoint / "entropy.c", ["-DHOST_NATIVE=1", *boundary]), (bridge, [])]
    else:
        source_units.append((HERE / "ps5_entropy.c", boundary))
    objects = []
    for index, (source, extra) in enumerate(source_units):
        obj = out / (str(index) + "-" + source.stem + ".o")
        result = run([cc, *flags, *extra, "-c", source, "-o", obj])
        if result.returncode:
            raise RuntimeError(f"host control compilation failed: {source.name}; see commands.json")
        objects.append(obj)
    binary = out / "ps5-entropy-control"
    result = run([cc, *flags, *objects, "-o", binary])
    if result.returncode:
        raise RuntimeError("host control link failed; see commands.json")
    rows = []
    for name in (["quota"] if args.red_legacy else DIRECT + OWNED):
        result = run([binary, name])
        row = {"case": name, "exit": result.returncode,
               "client": json.loads(result.stdout), "stderr_bytes": len(result.stderr)}
        rows.append(row)
    unchanged = before == {str(path): digest(path) for path in inputs}
    red = (args.red_legacy and unchanged and len(rows) == 1 and rows[0]["exit"] == 1
           and rows[0]["client"]["syscalls"] == 4
           and rows[0]["client"]["accepted_polls"] == 4
           and rows[0]["client"]["terminal"] == 1
           and rows[0]["client"]["boundary_contract"] and not rows[0]["stderr_bytes"])
    passed = (not args.red_legacy and unchanged and
              all(row["exit"] == 0 and row["client"]["passed"] and
                  not row["stderr_bytes"] for row in rows))
    receipt = {"passed": passed, "expected_red_observed": bool(red), "controls": rows,
               "inputs_sha256": before, "inputs_unchanged": unchanged,
               "sysctl_boundary": "explicit private fake; unit fixture bytes only",
               "owner_clocks": "actual ordinary-host monotonic/wall clocks",
               "native_build": False, "console_contact": False,
               "real_entropy_or_TLS_claim": False, "sanitizers": ["address", "undefined"]}
    (out / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({"passed": passed, "expected_red_observed": bool(red),
                      "controls": len(rows), "inputs_unchanged": unchanged,
                      "receipt": str(out / "receipt.json")}))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
