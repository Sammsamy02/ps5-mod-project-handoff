#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
build=root/'build'
build.mkdir(exist_ok=True)
exe=build/'g2h-arity-test'
cmd=['clang++','-arch','x86_64','-std=c++17','-O1','-Wall','-Wextra',
     str(root/'tests/test_g2h.cpp'),str(root/'tests/g2h_entry.S'),
     str(root/'src/win64_bridge.S'),'-o',str(exe)]
p=subprocess.run(cmd,capture_output=True,text=True,timeout=20)
if p.returncode:print(p.stderr,file=sys.stderr);raise SystemExit(p.returncode)
p=subprocess.run(['arch','-x86_64',str(exe)],capture_output=True,text=True,timeout=5)
print(p.stdout,end='')
if p.stderr:print(p.stderr,file=sys.stderr)
raise SystemExit(p.returncode)
