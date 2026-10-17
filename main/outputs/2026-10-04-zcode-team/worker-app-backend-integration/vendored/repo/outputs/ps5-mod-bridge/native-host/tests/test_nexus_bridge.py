"""Offline C ABI tests for native-only Nexus metadata decoding."""
import pathlib
import subprocess
import tempfile
import unittest

from build_fixture import ROOT, PROJECT, compile_fixture


class NexusBridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temporary.cleanup)
        folder = pathlib.Path(cls.temporary.name)
        sources = [ROOT / "nexus_bridge.cpp", PROJECT / "src/nexus.cpp"]
        cls.caller = compile_fixture(folder / "caller",
            c_sources=[ROOT / "tests/nexus_caller.c"], cpp_sources=sources)
        cls.allocator = compile_fixture(folder / "allocator",
            cpp_sources=[ROOT / "tests/nexus_failing_allocator.cpp", *sources])

    def test_actual_c_abi_whole_reports_bounds_context_and_secret_omission(self):
        subprocess.run([str(self.caller)], check=True)

    def test_allocation_failure_is_contained_then_next_call_recovers(self):
        subprocess.run([str(self.allocator)], check=True)


if __name__ == "__main__":
    unittest.main()
