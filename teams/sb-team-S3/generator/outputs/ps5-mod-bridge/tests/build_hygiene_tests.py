"""Build hygiene: every test suite in the tree is run by a make target."""
from pathlib import Path
import re
import unittest

PROJECT = Path(__file__).resolve().parents[1]


def recipe(target):
    """The command lines of one Makefile target."""
    lines = (PROJECT / "Makefile").read_text().splitlines()
    start = next(index for index, line in enumerate(lines) if re.match(rf"{re.escape(target)}:", line))
    body = []
    for line in lines[start + 1:]:
        if not line.startswith("\t"):
            break
        body.append(line.strip())
    return body


class BuildHygiene(unittest.TestCase):
    def test_every_playwright_suite_runs_in_test_ui(self):
        commands = " ".join(recipe("test-ui"))
        suites = sorted(path.name for path in (PROJECT / "tests").glob("*.cjs"))
        self.assertTrue(suites)
        missing = [name for name in suites if f"tests/{name}" not in commands]
        self.assertEqual(missing, [], "add these suites to the test-ui recipe in the Makefile")

    def test_python_discovery_covers_every_test_module(self):
        # test-python discovers '*test*.py'; a test module named otherwise would never run.
        self.assertIn("-p '*test*.py'", " ".join(recipe("test-python")))
        for path in (PROJECT / "tests").glob("*.py"):
            if "unittest" in path.read_text() and "class " in path.read_text() and "TestCase" in path.read_text():
                self.assertIn("test", path.name, f"{path.name} defines tests but discovery skips it")


if __name__ == "__main__":
    unittest.main()
