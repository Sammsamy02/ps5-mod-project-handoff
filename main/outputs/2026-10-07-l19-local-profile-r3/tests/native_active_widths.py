"""Compatibility entry point for the comprehensive R3 binding width suite."""
from pathlib import Path
import shutil, subprocess, sys
root=Path(__file__).resolve().parents[1]
red=len(sys.argv)>1 and sys.argv[1]=='red'
r=subprocess.run([sys.executable,str(root/'tests/required_binding_widths.py'),*sys.argv[1:]])
source=root/'reports'/('REQUIRED-BINDING-RED.json' if red else 'REQUIRED-BINDING-GREEN.json')
if source.exists():shutil.copyfile(source,root/'reports'/('ACTIVE-WIDTHS-RED.json' if red else 'ACTIVE-WIDTHS-GREEN.json'))
for name in ['NativeLayoutSchema.inc','NativeLayoutEngineIncludes.hpp','controls.cpp']:
 p=root/'tests/required-binding-host'/name
 if p.exists():shutil.copyfile(p,root/'tests/active-width-host'/name)
sys.exit(r.returncode)
