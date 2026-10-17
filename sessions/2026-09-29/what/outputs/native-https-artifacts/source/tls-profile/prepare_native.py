"""Fresh native Mbed portable-division profile; actual operation link-only, never run."""
import concurrent.futures
import hashlib
import json
import os
import pathlib
import re
import shlex
import subprocess
import tarfile
import threading

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PRIOR = HERE.parent / "native-https-profile-experiment-1"
OPERATION = HERE.parent / "native-https-integration-experiment-1"
SEED = HERE.parent / "native-https-real-source-experiment-1"
PRODUCT = ROOT / "outputs/ps5-mod-bridge/native-host"
PIN = "068ff080b369adfac81509f9b57b2afabaf82dc5"
SRC = PRODUCT / ("build/crypto/task1-fix1-evidence/artifacts/native-038ce86825317d4d74716405/"
                 "sources/mbedtls/mbedtls-" + PIN)
TAR = HERE.parent / ("mbedtls-" + PIN + ".tar.gz")
SDK = PRODUCT / "build/sdk-v0.43/ps5-payload-sdk"
LLVM = ROOT / "outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
OUT = HERE / "native"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def digest(path):
    return sha(path.read_bytes())


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n")


def main():
    OUT.mkdir()
    (OUT / "logs").mkdir()
    # One upstream-supported arithmetic option. Preserve all module/cipher,
    # clock and entropy policy; do not force 32-bit limbs or disable native
    # multiplication. Upstream itself selects the matching portable paths.
    config = (PRIOR / "tls_config.h").read_text()
    marker = "#define MBEDTLS_BIGNUM_C\n"
    if config.count(marker) != 1 or "MBEDTLS_NO_UDBL_DIVISION" in config:
        raise RuntimeError("unexpected prior arithmetic configuration")
    (HERE / "tls_config.h").write_text(config.replace(marker, marker + "#define MBEDTLS_NO_UDBL_DIVISION\n"))
    (HERE / "clock_decl.h").write_bytes((PRIOR / "clock_decl.h").read_bytes())
    effective = (PRIOR / "tls_effective.h").read_text().replace(
        '#include "mbedtls/version.h"\n', '#include "mbedtls/version.h"\n#include "mbedtls/bignum.h"\n')
    effective = effective[:effective.rfind("#endif")] + '''#if !defined(MBEDTLS_NO_UDBL_DIVISION) || defined(MBEDTLS_HAVE_UDBL)
#error "requires upstream portable half-word division without double-width compiler helper"
#endif
#if !defined(MBEDTLS_HAVE_INT64) || defined(MBEDTLS_HAVE_INT32) || defined(MBEDTLS_NO_64BIT_MULTIPLICATION)
#error "preserve native 64-bit MPI limbs and upstream multiplication policy"
#endif
_Static_assert(sizeof(mbedtls_mpi_uint)==8,"native 64-bit MPI limbs required");
#endif
'''
    if "EXP_DEVELOPMENT_ENTROPY_ONLY" in effective or "deterministic" in effective or "host entropy" in effective:
        raise RuntimeError("effective header must not claim an entropy origin")
    (HERE / "tls_effective.h").write_text(effective)
    config_inputs = {name: digest(HERE / name) for name in ("tls_config.h", "tls_effective.h", "clock_decl.h")}
    config_id = sha(json.dumps({"mbedtls": PIN, "config_inputs": config_inputs}, sort_keys=True).encode())
    prereq_path = PRODUCT / "build/crypto/task1-fix1-evidence/native-green-descriptor.json"
    prereq = json.loads(prereq_path.read_text())
    identity = prereq["identity"]
    pins = identity["tools"]
    resource = pathlib.Path(prereq["native_compile_options"][2])
    ar = resource.parents[2] / "bin/llvm-ar"
    tool_paths = {}
    for name in ("clang", "llvm-config", "llvm-readelf", "llvm-nm"):
        path = LLVM / name
        if digest(path) != pins["tools"][name]:
            raise RuntimeError("LLVM tool mismatch: " + str(path))
        tool_paths[str(path)] = digest(path)
    if digest(ar) != pins["llvm_selected_members"]["bin/llvm-ar"]:
        raise RuntimeError("archiver mismatch")
    tool_paths[str(ar)] = digest(ar)
    lld_pin = json.loads((HERE.parent / "native-clock-build-1/source-identities.json").read_text())["identities"]["llvm_lld_archive_member"]["sha256"]
    if digest(LLVM / "ld.lld") != lld_pin:
        raise RuntimeError("linker mismatch")
    tool_paths[str(LLVM / "ld.lld")] = lld_pin
    for name in ("bin/prospero-clang", "bin/prospero-lld", "bin/prospero-llvm-config", "bin/clang", "bin/ld.lld", "ldscripts/elf_x86_64.x", "target/lib/crt1.o"):
        path = SDK / name
        if digest(path) != pins["sdk_members"][name]:
            raise RuntimeError("SDK input mismatch: " + name)
        tool_paths[str(path)] = digest(path)
    if digest(TAR) != identity["source_archive_sha256"]:
        raise RuntimeError("source archive mismatch")
    cmake = (SRC / "library/CMakeLists.txt").read_text()
    families = {name: re.search(r"set\(src_" + name + r"\s+(.*?)\)", cmake, re.S).group(1).split()
                for name in ("crypto", "x509", "tls")}
    sources = [SRC / "library" / name for names in families.values() for name in names]
    with tarfile.open(TAR, "r:gz") as archive:
        for path in [SRC / "library/CMakeLists.txt", *sources]:
            member = archive.extractfile("mbedtls-" + PIN + "/" + path.relative_to(SRC).as_posix())
            if member is None or member.read() != path.read_bytes():
                raise RuntimeError("source correspondence: " + str(path))
    borrowed = [OPERATION / "tls_attempt.c", OPERATION / "tls_attempt.h",
                OPERATION / "native_link_only.c", SEED / "tls_seed_source.c", SEED / "tls_seed_source.h",
                PRODUCT / "entropy.c", PRODUCT / "entropy.h"]
    borrowed_identities = {str(path): digest(path) for path in borrowed}
    env = {**os.environ, "LLVM_CONFIG": str(LLVM / "llvm-config"), "PYTHONDONTWRITEBYTECODE": "1"}
    cc = SDK / "bin/prospero-clang"
    common = ["-std=c11", "-Os", "-fno-common", "-ffunction-sections", "-fdata-sections",
              '-DMBEDTLS_CONFIG_FILE="tls_config.h"', "-I" + str(HERE),
              "-I" + str(SRC / "include"), "-I" + str(SRC / "library"), *prereq["native_compile_options"]]
    consumer = [*common, "-DHOST_NATIVE=1", "-I" + str(OPERATION), "-I" + str(SEED), "-I" + str(PRODUCT),
                "-include", str(HERE / "tls_effective.h")]
    commands = []
    lock = threading.Lock()

    def run(argv, label):
        args = [str(item) for item in argv]
        result = subprocess.run(args, cwd=ROOT, env=env, capture_output=True, text=True, timeout=45)
        (OUT / "logs" / (label + ".stdout")).write_text(result.stdout)
        (OUT / "logs" / (label + ".stderr")).write_text(result.stderr)
        with lock:
            commands.append({"label": label, "argv": args, "exit_code": result.returncode})
        if result.returncode:
            raise RuntimeError(label + ": " + result.stderr[:3000])
        return result.stdout

    try:
        macros = run([cc, *common, "-include", HERE / "tls_effective.h", "-dM", "-E", "-x", "c", "/dev/null"], "effective-macros")
        (OUT / "effective-macros.txt").write_text(macros)
        defined = set(re.findall(r"^#define ([A-Za-z_][A-Za-z_0-9]*)", macros, re.M))
        if not {"MBEDTLS_NO_UDBL_DIVISION", "MBEDTLS_HAVE_INT64"}.issubset(defined):
            raise RuntimeError("portable division/native limb configuration absent")
        if {"MBEDTLS_HAVE_UDBL", "MBEDTLS_HAVE_INT32", "MBEDTLS_NO_64BIT_MULTIPLICATION", "MBEDTLS_HAVE_ASM"} & defined:
            raise RuntimeError("unexpected arithmetic policy change")

        def compile_one(path):
            obj = OUT / (path.stem + ".o")
            run([cc, *common, "-include", SRC / "library/common.h", "-include", HERE / "tls_effective.h",
                 "-c", path, "-o", obj, "-MD", "-MF", str(obj) + ".d"], "library-" + path.stem)

        with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
            list(pool.map(compile_one, sources))
        archives = {}
        for family, names in families.items():
            path = OUT / ("libmbed" + ("tls" if family == "tls" else family) + ".a")
            run([ar, "rcs", path, *[OUT / (pathlib.Path(name).stem + ".o") for name in names]], "archive-" + family)
            archives[family] = {"path": str(path), "sha256": digest(path), "config_id": config_id}
        objects = []
        for source in (OPERATION / "tls_attempt.c", SEED / "tls_seed_source.c", PRODUCT / "entropy.c", OPERATION / "native_link_only.c"):
            obj = OUT / ("consumer-" + source.stem + ".o")
            run([cc, *consumer, "-c", source, "-o", obj, "-MD", "-MF", str(obj) + ".d"], "consumer-" + source.stem)
            objects.append(obj)
        elf = OUT / "actual-clock-operation-link-only.elf"
        run([cc, *consumer, "-nodefaultlibs", *objects, *[archives[k]["path"] for k in ("tls", "x509", "crypto")],
             "-Wl,--gc-sections", "-Wl,-z,now", "-Wl,-Map," + str(OUT / "link.map"),
             "-lc", "-lkernel_web", "-lSceLibcInternal", "-lSceNet", "-o", elf], "link-only")
        elf_info = run([LLVM / "llvm-readelf", "--file-header", "--program-headers", "--dynamic", "--dyn-syms", elf], "elf")
        if "ELF64" not in elf_info or "Advanced Micro Devices X86-64" not in elf_info or "BIND_NOW" not in elf_info:
            raise RuntimeError("target/BINDNOW link closure")
        library_clocks = run([LLVM / "llvm-nm", "--undefined-only", *[OUT / (name + ".o") for name in ("x509", "x509_crt", "ssl_client", "ssl_tls12_client")]], "library-clock-hooks")
        if library_clocks.count(" U exp_clock_time\n") != 4 or " U time\n" in library_clocks:
            raise RuntimeError("Mbed date readers did not retain caller clock hook")
        if " U mbedtls_platform_gmtime_r\n" not in library_clocks:
            raise RuntimeError("Mbed UTC ALT hook absent")
        operation_imports = run([LLVM / "llvm-nm", "--undefined-only", OUT / "consumer-entropy.o", OUT / "consumer-tls_attempt.o"], "actual-operation-imports")
        for symbol in (" U sysctl", " U clock_gettime", " U time", " U gmtime"):
            if symbol not in operation_imports:
                raise RuntimeError("reviewed native OS boundary absent: " + symbol)
        linked = run([LLVM / "llvm-nm", elf], "linked-symbols")
        bignum_imports = run([LLVM / "llvm-nm", "--undefined-only", OUT / "bignum.o"], "bignum-imports")
        library_imports = run([LLVM / "llvm-nm", "--undefined-only", *[archives[k]["path"] for k in ("tls", "x509", "crypto")]], "all-library-imports")
        def undefined(text):
            return sorted(set(re.findall(r"^\s+U\s+(\S+)\s*$", text, re.M)))
        underscore_imports = {
            label: [name for name in undefined(content) if name.startswith("__")]
            for label, content in (("bignum", bignum_imports), ("all_libraries", library_imports), ("linked_elf", linked))
        }
        # These two are ordinary pinned SDK boundary imports, not compiler
        # arithmetic helpers. Reject every other double-underscore import.
        sdk_boundary = {"__error", "__inet_pton"}
        unexpected = sorted({name for names in underscore_imports.values() for name in names} - sdk_boundary)
        if unexpected or "__udivti3" in linked:
            raise RuntimeError("compiler arithmetic/helper import remains: " + str(unexpected))
        arithmetic_receipt = {
            "no_compiler_arithmetic_helper_import": True,
            "double_underscore_undefined_symbols": underscore_imports,
            "ordinary_sdk_boundary_allowlist": sorted(sdk_boundary),
            "all_library_units_inspected": len(sources),
            "linked_undefined_symbols": undefined(linked),
            "upstream_portable_division": True, "mpi_limb_bits": 64,
            "have_udbl": False, "no_64bit_multiplication": False,
            "upstream_generic_half_word_multiplication_selected": True,
            "runtime_observed": False
        }
        write_json(OUT / "arithmetic-imports.json", arithmetic_receipt)
        for forbidden in ("fixture_sysctl", "tls_attempt_test_cleanup", "getentropy", "mb_random_checkpoint", "mb_drbg_source"):
            if forbidden in linked:
                raise RuntimeError("foreign fixture/host/old checkpoint linkage: " + forbidden)
        closure = set()
        for path in OUT.glob("*.o.d"):
            closure.update(pathlib.Path(part) for part in shlex.split(path.read_text().replace("\\\n", " ").split(":", 1)[1]))
        origins = {"pinned_mbed": 0, "fresh_config": 0, "actual_operation": 0, "actual_seed": 0, "reviewed_entropy": 0, "sdk": 0, "llvm_resource": 0}
        correspondence_rows = []
        with tarfile.open(TAR, "r:gz") as archive:
            for path in sorted(closure):
                value = digest(path)
                if path.is_relative_to(SRC):
                    member = archive.extractfile("mbedtls-" + PIN + "/" + path.relative_to(SRC).as_posix())
                    if member is None or member.read() != path.read_bytes():
                        raise RuntimeError("pinned actual header/source mismatch: " + str(path))
                    origin = "pinned_mbed"
                elif path.is_relative_to(HERE):
                    origin = "fresh_config"
                elif path.is_relative_to(SDK):
                    if pins["sdk_members"][path.relative_to(SDK).as_posix()] != value:
                        raise RuntimeError("SDK header mismatch")
                    origin = "sdk"
                elif path.is_relative_to(resource):
                    if pins["llvm_selected_members"]["lib/clang/21/" + path.relative_to(resource).as_posix()] != value:
                        raise RuntimeError("LLVM resource header mismatch")
                    origin = "llvm_resource"
                elif path.is_relative_to(OPERATION):
                    origin = "actual_operation"
                    if borrowed_identities[str(path)] != value:
                        raise RuntimeError("operation changed while building")
                elif path.is_relative_to(SEED):
                    origin = "actual_seed"
                    if borrowed_identities[str(path)] != value:
                        raise RuntimeError("seed changed while building")
                elif path.is_relative_to(PRODUCT):
                    origin = "reviewed_entropy"
                    if borrowed_identities[str(path)] != value:
                        raise RuntimeError("reviewed collector changed while building")
                else:
                    raise RuntimeError("host/unknown native header fallback: " + str(path))
                origins[origin] += 1
                correspondence_rows.append([str(path), origin, value])
        if any(digest(pathlib.Path(path)) != value for path, value in borrowed_identities.items()):
            raise RuntimeError("borrowed read-only inputs changed")
        (HERE / "notices").mkdir()
        (HERE / "notices/MbedTLS-LICENSE").write_bytes((SRC / "LICENSE").read_bytes())
        descriptor = {"scope": "fresh native portable-division profile; actual clock/seed operation compile/link only",
                      "prior_profile_config_id": json.loads((PRIOR / "native-profile-descriptor.json").read_text())["config_id"],
                      "arithmetic_policy": {"config_option": "MBEDTLS_NO_UDBL_DIVISION", **arithmetic_receipt},
                      "config_id": config_id, "config_inputs": {str(HERE / name): value for name, value in config_inputs.items()},
                      "mbedtls_commit": PIN, "source_path": str(SRC), "source_archive_path": str(TAR),
                      "source_archive_sha256": identity["source_archive_sha256"],
                      "framework_archive_sha256": identity["framework_archive_sha256"], "framework_test_runtime_used": False,
                      "approved_prerequisite_descriptor": str(prereq_path), "archives": archives,
                      "library_source_units": len(sources), "borrowed_library_units": 0,
                      "consumer_compile_options": consumer, "library_compile_options": common,
                      "library_force_includes": [str(SRC / "library/common.h"), str(HERE / "tls_effective.h")],
                      "LLVM_CONFIG": env["LLVM_CONFIG"], "tool_identities": tool_paths,
                      "native_compile_options": prereq["native_compile_options"], "sdk_path": str(SDK),
                      "sdk_commit": pins["sdk_commit"], "sdk_archive_sha256": pins["sdk_archive_sha256"],
                      "llvm_archive_sha256": pins["llvm_archive_sha256"],
                      "actual_include_origins": origins,
                      "verified_actual_correspondence_digest": sha(json.dumps(correspondence_rows, separators=(",", ":")).encode()),
                      "borrowed_read_only_inputs": borrowed_identities,
                      "clock_hooks": {"wall": "time_t exp_clock_time(time_t *out)", "utc": "struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *, struct tm *)", "caller_serialization_required": True},
                      "mbed_library_raw_time_fallback": False, "reviewed_native_sysctl_time_gmtime_clock_gettime_linked": True,
                      "deterministic_or_host_entropy_injection_linked": False,
                      "artifact": str(elf), "artifact_bytes": elf.stat().st_size, "elf_bind_now": True,
                      "operation_policy_still_narrow_requires_adaptation": True,
                      "native_executed": False, "native_clock_runtime_observed": False, "native_tls_runtime_observed": False}
        write_json(HERE / "native-profile-descriptor.json", descriptor)
        print(json.dumps({"native_compiled_linked": True, "config_id": config_id, "source_units": len(sources),
                          "include_origins": origins, "artifact_bytes": elf.stat().st_size,
                          "operation_policy_still_narrow": True, "native_executed": False}))
    finally:
        write_json(OUT / "commands.json", sorted(commands, key=lambda item: item["label"]))


if __name__ == "__main__":
    main()
