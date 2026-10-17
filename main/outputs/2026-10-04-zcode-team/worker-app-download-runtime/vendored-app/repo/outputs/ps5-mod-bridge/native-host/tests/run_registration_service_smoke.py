"""Exercise app registration against an explicitly supplied ModForge source tree.

No download, provider, console or game path is used. The backend shell writes
only inside an owned temporary directory. This proves registration, not prepare
or install support. Pass a checkout (or pinned source export) with backend/src
and contract/ directories.
"""
import argparse
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend-root", type=pathlib.Path, required=True)
    args = parser.parse_args()
    backend = args.backend_root.resolve()
    with tempfile.TemporaryDirectory(prefix="modbridge-register-service-") as tmp:
        out = pathlib.Path(tmp).resolve()
        posix = out / "posix.o"
        subprocess.run(["cc", "-std=c11", "-c", str(ROOT / "archive/private/posix_compat.c"),
                        "-o", str(posix)], check=True)
        sources = [backend / "backend/src/service" / (name + ".cpp")
                   for name in ("service", "records", "jobs", "txn", "fsutil")]
        # The pinned service txn.cpp uses unlink without including unistd.h.
        # Supply its declaration without changing the external source tree.
        command = ["c++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-pthread",
                   "-include", "unistd.h",
                   "-I", str(ROOT.parent / "third_party"),
                   "-I", str(backend / "backend/src"), "-I", str(backend / "contract"),
                   str(ROOT / "tests/registration_service_smoke.cpp"),
                   str(ROOT / "mod_service/register_bridge.cpp"),
                   str(ROOT / "download_store/store.cpp"),
                   *map(str, sources), str(posix), "-o", str(out / "smoke")]
        subprocess.run(command, check=True)
        subprocess.run([str(out / "smoke"), str(out)], check=True)
        print("Actual backend shell registration/retry/conflict/reopen smoke passed")


if __name__ == "__main__":
    main()
