#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
tree = root.parents[1]
exe = root / 'tests/preflight-host'
fixture = tree / 'cap13/FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll'
empty_tls = tree / 'cap13/SAMPLE/cap13-mod-keybinds.dll'
real_tls = tree / 'cg-max-ready-20261013-03/cap13/upstream-inputs/ZeroCompanyMandoWardrobe-upstream-main.dll'
cmd = ['clang++', '-arch', 'x86_64', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra',
       '-I' + str(root / 'include'), '-I' + str(root / 'src'),
       str(root / 'tests/test_preflight.cpp'), str(root / 'src/pe_loader.cpp'),
       '-o', str(exe)]
subprocess.run(cmd, check=True, timeout=30)
failed = 0
cases = ['baseline', 'bad_optional', 'missing_reloc', 'empty_tls', 'bad_tls',
         'real_tls', 'bad_import_dir', 'bad_export', 'bad_reloc_target',
         'bad_reloc_odd', 'helper_bad_pe', 'bad_entry', 'empty_reloc',
         'absolute_reloc', 'bad_tls_oversize', 'tls_overlay', 'bad_delay',
         'import_overlay', 'nonexec_export', 'overlap_sections',
         'entry_virtual_tail', 'export_virtual_tail', 'wrong_page_size',
         'tls_zero_size', 'import_zero_size']
for case in cases:
    p = subprocess.run(['arch', '-x86_64', str(exe), case, str(fixture),
                        str(empty_tls), str(real_tls)],
                       text=True, capture_output=True, timeout=8)
    print(p.stdout + p.stderr, end='')
    failed += p.returncode != 0
print(f'cases={len(cases)} failed={failed}')
sys.exit(bool(failed))
