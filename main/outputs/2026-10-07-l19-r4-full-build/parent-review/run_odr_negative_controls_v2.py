"""Two deliberate isolated ELF mutations; no compiler/linker/package or source edit."""
import concurrent.futures
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

OWN = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(OWN / 'execution'), str(OWN)]
from admission import sha, require, read
from composition import write


def main():
    spec = importlib.util.spec_from_file_location('original_odr', OWN / 'check_odr_emitters_l19.py')
    original = importlib.util.module_from_spec(spec); spec.loader.exec_module(original)
    root = OWN / 'parent-review/odr-preflight-v2'
    run = OWN / 'runs/run-l19-1'
    source = run / 'coherent-tick-file-witness-runtime.o'
    before = sha(source)
    elf = original.Elf(source)
    symbols = list(elf.defined_symbols())
    shndx = next(i for name, i in symbols if name == original.NMS)
    body_offset = elf.section_of(shndx)[2]
    symoff, count, strings = elf.symtab
    string_offset = None
    for j in range(count):
        st_name = struct.unpack_from('<I', elf.mm, symoff + 24 * j)[0]
        if st_name and elf._cstr(strings, st_name) == original.NMS:
            string_offset = strings + st_name + len(original.NMS) - 1
            break
    require(string_offset is not None, 'real NMS symbol required')
    elf.close()
    generation = root / 'R4-ODR-GENERATION.json'
    controls = {}
    for label, offset in [('bad-body', body_offset), ('bad-symbol', string_offset)]:
        target = root / ('runtime-' + label + '.o')
        require(not target.exists(), 'new isolated negative fixture required')
        shutil.copy2(source, target)
        with target.open('r+b') as f:
            f.seek(offset); prior = f.read(1); f.seek(offset); f.write(bytes([prior[0] ^ 1]))
        controls[label] = {'target': target, 'offset': offset, 'original_byte': prior.hex()}
    def check(label, control):
        result_path = root / ('ODR-' + label.upper() + '.json')
        argv = [sys.executable, str(OWN / 'parent-review/check_odr_emitters_r4_vintage_v2.py'), str(run),
                '--object', str(control['target']), '--require-program-fresh',
                '--generation-receipt', str(generation), '--generation-receipt-sha256', sha(generation),
                '--json', str(result_path)]
        result = subprocess.run(argv, capture_output=True, text=True)
        write(root / ('ODR-' + label.upper() + '.command.json'),
              {'argv': argv, 'exit': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr})
        require(result.returncode == 1, 'mutated real ELF must fail original ODR body/symbol checks')
        report = read(result_path)
        require(report['checks']['C5_nms_composed_single_and_body_equal']['ok'] is False,
                'real NMS composed body/symbol mismatch was not detected')
        return {'pass': True, 'exit': result.returncode, 'expected_failed_check': 'C5_nms_composed_single_and_body_equal',
                'actual_failures': report['failures'], 'mutated_ELF': {'path': str(control['target']), 'sha256': sha(control['target'])},
                'changed_byte_offset': control['offset'], 'original_byte': control['original_byte'],
                'original_check_receipt': {'path': str(result_path), 'sha256': sha(result_path)}}
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        futures = {label: pool.submit(check, label, control) for label, control in controls.items()}
        results = {label: future.result() for label, future in futures.items()}
    require(sha(source) == before, 'real source composition must remain unchanged')
    write(root / 'NEGATIVE-CONTROLS.json', {'pass': True, 'scope': 'two real isolated ELF body/symbol faults rejected by unchanged predicates',
          'controls': results, 'source_composition_sha256_before_and_after': before,
          'compiler_invocations': 0, 'linker_invocations': 0, 'package_invocations': 0, 'console_contact': False})
    print(json.dumps({'pass': True, 'real_fault_controls': len(results), 'receipt_sha256': sha(root / 'NEGATIVE-CONTROLS.json')}))


if __name__ == '__main__': main()
