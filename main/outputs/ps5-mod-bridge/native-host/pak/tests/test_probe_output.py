"""Require the actual host probe to report a failed final stdout flush."""
from pathlib import Path
import argparse
import resource
import signal
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    fixture = Path(__file__).resolve().parent / "fixtures/pack_v11.pak"

    def reject_file_output():
        # Only this child process receives the zero-size output-file limit.
        signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
        resource.setrlimit(resource.RLIMIT_FSIZE, (0, 0))

    with tempfile.TemporaryFile() as output:
        proc = subprocess.run([str(args.probe.resolve()), str(fixture), "0"],
                              stdout=output, stderr=subprocess.PIPE,
                              preexec_fn=reject_file_output, timeout=5)
        output.seek(0)
        if proc.returncode != 2 or output.read() != b"":
            raise RuntimeError("Probe reported success despite rejected output: " + str(proc.returncode))
    print("PASS: actual host probe rejects final output flush failure")


if __name__ == "__main__":
    main()
