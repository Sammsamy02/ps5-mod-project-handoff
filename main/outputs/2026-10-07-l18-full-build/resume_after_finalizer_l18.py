"""Resume the preserved first build after private finalizer path repair."""
import importlib.util
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from prep_guard import require_ready, OWN

require_ready()
spec = importlib.util.spec_from_file_location("launch18_resume", OWN / "launch18_one_pass.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
runner.verify_units()
first = runner.post_build("prx-candidate-l18-1")
first_hash = runner.determinism(first)
runner.the_pass("prx-candidate-l18-2")
second = runner.post_build("prx-candidate-l18-2")
second_hash = runner.determinism(second)
if first_hash != second_hash:
    runner.die("L18 composed object determinism mismatch")
print("L18 FULL PIPELINE COMPLETE: two fresh compositions/packages, final twin gate GO; composed SHA256=" + first_hash)
