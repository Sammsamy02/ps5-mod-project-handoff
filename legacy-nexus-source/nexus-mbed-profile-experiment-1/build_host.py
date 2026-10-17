"""Fresh libraries and consumer, one matching profile; no old archive/cache invocation."""
import concurrent.futures
import hashlib
import json
import pathlib
import re
import shlex
import subprocess
import tarfile
import threading

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PIN = "068ff080b369adfac81509f9b57b2afabaf82dc5"
SRC = ROOT / ("outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts/"
              "native-038ce86825317d4d74716405/sources/mbedtls/mbedtls-" + PIN)
TAR = HERE.parent / ("mbedtls-" + PIN + ".tar.gz")
OUT = HERE / "host"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    if OUT.exists():
        if (OUT / "tls_client").exists():
            raise RuntimeError("retained successful build must not be overwritten")
        # Preserve the diagnosed first compile failure; no libraries were built.
        if not (OUT / "before-header-fix-commands.json").exists():
            (OUT / "before-header-fix-commands.json").write_bytes((OUT / "commands.json").read_bytes())
            (OUT / "logs/client-before-header-fix.stderr").write_bytes((OUT / "logs/client.stderr").read_bytes())
    else:
        OUT.mkdir()
        (OUT / "logs").mkdir()
    approved = json.loads((ROOT / "outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/native-green-descriptor.json").read_text())["identity"]
    if sha(TAR.read_bytes()) != approved["source_archive_sha256"]:
        raise RuntimeError("pinned source archive mismatch")
    acquisition = json.loads((HERE / "root-acquisition.json").read_text())
    if acquisition["status"] != "authenticated_official_der_acquired":
        raise RuntimeError("official root acquisition prerequisite")
    config_inputs = {n: sha((HERE / n).read_bytes()) for n in ("tls_config.h", "tls_effective.h", "clock_decl.h")}
    config_id = sha(json.dumps({"mbedtls": PIN, "config_inputs": config_inputs}, sort_keys=True).encode())
    (HERE / "experiment_identity.h").write_text('#define EXP_CONFIG_ID "' + config_id + '"\n')
    cmake = (SRC / "library/CMakeLists.txt").read_text()
    families = {k: re.search(r"set\(src_" + k + r"\s+(.*?)\)", cmake, re.S).group(1).split()
                for k in ("crypto", "x509", "tls")}
    sources = [SRC / "library" / n for names in families.values() for n in names]
    with tarfile.open(TAR, "r:gz") as archive:
        for path in [SRC / "library/CMakeLists.txt", *sources]:
            member = archive.extractfile("mbedtls-" + PIN + "/" + path.relative_to(SRC).as_posix())
            if member is None or member.read() != path.read_bytes():
                raise RuntimeError("source/archive correspondence failed: " + str(path))
    flags = ["-std=c11", "-Os", "-fno-common", "-I" + str(HERE), "-I" + str(SRC / "include"),
             "-I" + str(SRC / "library"), '-DMBEDTLS_CONFIG_FILE="tls_config.h"']
    commands = []
    lock = threading.Lock()

    def run(args, label):
        result = subprocess.run(args, cwd=HERE, capture_output=True, text=True, timeout=45)
        (OUT / "logs" / (label + ".stderr")).write_text(result.stderr)
        with lock:
            commands.append({"label": label, "argv": [str(a) for a in args], "exit_code": result.returncode})
        if result.returncode:
            raise RuntimeError(label + ": " + result.stderr[:3000])
        return result.stdout

    try:
        run(["/usr/bin/cc", *flags, "-include", HERE / "tls_effective.h", "-c", HERE / "tls_client.c",
             "-o", OUT / "tls_client.o", "-MD", "-MF", OUT / "tls_client.o.d"], "client")
        macros = run(["/usr/bin/cc", *flags, "-dM", "-E", "-include", HERE / "tls_effective.h", "-x", "c", "/dev/null"], "effective-macros")
        (OUT / "effective-macros.txt").write_text(macros)

        def compile_one(path):
            obj = OUT / (path.stem + ".o")
            run(["/usr/bin/cc", *flags, "-include", SRC / "library/common.h", "-include", HERE / "tls_effective.h",
                 "-c", path, "-o", obj, "-MD", "-MF", str(obj) + ".d"], path.stem)

        with concurrent.futures.ThreadPoolExecutor(max_workers=6) as workers:
            list(workers.map(compile_one, sources))
        for family, names in families.items():
            archive_name = "libmbed" + ("tls" if family == "tls" else family) + ".a"
            run(["/usr/bin/ar", "rcs", OUT / archive_name, *[OUT / (pathlib.Path(n).stem + ".o") for n in names]], "archive-" + family)
        binary = OUT / "tls_client"
        run(["/usr/bin/cc", OUT / "tls_client.o", OUT / "libmbedtls.a", OUT / "libmbedx509.a",
             OUT / "libmbedcrypto.a", "-o", binary], "link")
        (OUT / "undefined-imports.txt").write_text(run(["/usr/bin/nm", "-u", binary], "imports"))
        (OUT / "dynamic-libraries.txt").write_text(run(["/usr/bin/otool", "-L", binary], "dynamic-libraries"))
        closure = set()
        for path in OUT.glob("*.o.d"):
            deptext = path.read_text().replace("\\\n", " ")
            closure.update(pathlib.Path(v) for v in shlex.split(deptext.split(":", 1)[1]))
        correspondence_count = 0
        with tarfile.open(TAR, "r:gz") as archive:
            for path in closure:
                if path.is_relative_to(SRC):
                    member = archive.extractfile("mbedtls-" + PIN + "/" + path.relative_to(SRC).as_posix())
                    if member is None or member.read() != path.read_bytes():
                        raise RuntimeError("actual include/source mismatch: " + str(path))
                    correspondence_count += 1
        (HERE / "notices").mkdir()
        (HERE / "notices/MbedTLS-LICENSE").write_bytes((SRC / "LICENSE").read_bytes())
        summary = {"scope": "fresh host-only real-Mbed TLS profile; not native execution",
                   "mbedtls_commit": PIN, "source_archive_sha256": approved["source_archive_sha256"],
                   "approved_framework_archive_sha256": approved["framework_archive_sha256"],
                   "framework_test_runtime_used": False, "source_path": str(SRC), "source_units": len(sources),
                   "pinned_actual_include_source_correspondences": correspondence_count, "config_id": config_id,
                   "config_inputs": config_inputs, "root_sha256": acquisition["der_sha256"],
                   "compiler": "/usr/bin/cc", "compile_flags": flags, "old_archives_linked": False,
                   "client_sha256": sha((HERE / "tls_client.c").read_bytes()), "binary_bytes": binary.stat().st_size,
                   "native_compiled": False, "native_executed": False}
        (OUT / "build-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
        print(json.dumps(summary))
    finally:
        (OUT / "commands.json").write_text(json.dumps(sorted(commands, key=lambda c: c["label"]), indent=2) + "\n")


if __name__ == "__main__":
    main()
