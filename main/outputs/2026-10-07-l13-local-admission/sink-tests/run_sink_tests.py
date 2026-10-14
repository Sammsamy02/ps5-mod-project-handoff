#!/usr/bin/env python3
"""Compile the candidate's verbatim sink region; never touch a console."""
from pathlib import Path
import subprocess, sys
root = Path(__file__).resolve().parents[1]
bridge = root / 'units/runtime_start_bridge_r14_diag_spanpub.cpp'
text = bridge.read_text()
start = text.index('// ---- HOST-TEST-EXTRACT-BEGIN')
end = text.index('// ---- HOST-TEST-EXTRACT-END', start)
(root / 'sink-tests/sink_extract.inc').write_text(text[start:end])
cmd = ['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-O1', '-I', str(root/'units'), str(root/'sink-tests/test_sink.cpp'), '-o', str(root/'sink-tests/test_sink')]
subprocess.run(cmd, check=True)
p = subprocess.run([str(root/'sink-tests/test_sink')], text=True, capture_output=True, timeout=15)
print(p.stdout, end=''); print(p.stderr, end='', file=sys.stderr)
sys.exit(p.returncode)
