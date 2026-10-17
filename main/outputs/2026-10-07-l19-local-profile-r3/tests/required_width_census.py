"""Compile all required native member widths using selected target declarations."""
from pathlib import Path
import json, subprocess, hashlib, os, sys, struct

root = Path(__file__).resolve().parents[1]
w = root.parents[1]
red = len(sys.argv) > 1 and sys.argv[1] == 'red'
out = root / 'tests/required-width-target'
out.mkdir(exist_ok=True)
baseline = w / 'outputs/2026-10-07-l19-local-profile-r2'
legacy = w / '.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/src/NativeLayoutWidths.cpp'
profile = json.loads((root / 'profiles/townfall-build-facts.json').read_text())
rows = [x for x in profile['required_members'] if not x['name'].endswith('::UEP_TotalSize')]
assert len(rows) == 54
special = {'UFunction::Func': 'FunctionWidth', 'UStruct::MinAlignment': 'MinAlignmentWidth',
           'UClass::ClassCastFlags': 'ClassCastFlagsWidth', 'FFieldClass::CastFlags': 'FieldCastFlagsWidth'}
if not red:
    special.update({'UStruct::SuperStruct': 'SuperStructWidth', 'FField::Owner': 'FieldOwnerWidth',
                    'FField::Next': 'FieldNextWidth', 'FField::NamePrivate': 'FieldNameWidth',
                    'FProperty::PropertyFlags': 'PropertyFlagsWidth', 'UFunction::FunctionFlags': 'FunctionFlagsWidth'})
expressions = []
assertions = []
for row in rows:
    cls, key = row['name'].split('::')
    if row['name'] in special:
        expression = 'ActiveWidthDetail::' + special[row['name']] + '<Unreal::' + cls + '>()'
    else:
        expression = 'Find(' + json.dumps(cls) + ',' + json.dumps(key) + ')'
    expressions.append(expression + '.value_or(0)')
    minimum = row['width'] or 1
    assertions.append('static_assert(' + expression + '.value_or(0)>=' + str(minimum) + ',' + json.dumps(row['name'] + ' requires available width covering measured field') + ');')
helper = (baseline if red else root) / 'units/NativeLayoutActiveWidths.hpp'
source = out / ('required_census_red.cpp' if red else 'required_census.cpp')
source.write_text('#include ' + json.dumps(str(legacy)) + '\n#include ' + json.dumps(str(helper)) + '\nnamespace RC::Platform::NativeLayouts {\n' + '\n'.join(assertions) + '\nextern "C" [[gnu::used]] const uint32_t AllRequiredWidthFacts[]{\n' + ',\n'.join(expressions) + '\n};}\n')
cmd = list(json.loads((baseline / 'reports/ACTIVE-WIDTHS-TARGET.json').read_text())['argv'])
for i, value in enumerate(cmd):
    if value.endswith('.cpp'):
        cmd[i] = str(source)
cmd[cmd.index('-o') + 1] = str(out / ('census-red.o' if red else 'census.o'))
cmd[cmd.index('-MF') + 1] = str(out / ('census-red.d' if red else 'census.d'))
env = dict(os.environ)
env['LLVM_CONFIG'] = str(w / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')
r = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=120)
receipt = {'scope': 'actual selected-target compilation; all54 required non-total keys available and measured native width minima covered; no getter calls',
           'argv': cmd, 'exit': r.returncode, 'stdout': r.stdout, 'stderr': r.stderr,
           'required_keys': rows, 'assertions': assertions,
           'owner_width_distinction': 'public variant copy footprint16; measured native tagged-pointer storage8 unchanged',
           'source_hashes': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in [source, helper, legacy, root / 'profiles/townfall-build-facts.json']}}
if r.returncode == 0:
    object_bytes = Path(cmd[cmd.index('-o') + 1]).read_bytes()
    assert object_bytes[:6] == b'\x7fELF\x02\x01'
    section_offset = struct.unpack_from('<Q', object_bytes, 40)[0]
    section_size, section_count = struct.unpack_from('<HH', object_bytes, 58)
    assert section_size == 64
    sections = [struct.unpack_from('<IIQQQQIIQQ', object_bytes, section_offset + i * section_size) for i in range(section_count)]
    symbols = []
    for section in sections:
        if section[1] != 2:
            continue
        assert section[9] == 24
        strings = sections[section[6]]
        for pos in range(section[4], section[4] + section[5], 24):
            name_offset, info, other, index, value, size = struct.unpack_from('<IBBHQQ', object_bytes, pos)
            name_start = strings[4] + name_offset
            name = object_bytes[name_start:object_bytes.index(b'\0', name_start)]
            if name == b'AllRequiredWidthFacts':
                symbols.append((index, value, size))
    assert len(symbols) == 1 and symbols[0][2] == 54 * 4
    index, value, size = symbols[0]
    data_offset = sections[index][4] + value
    raw = object_bytes[data_offset:data_offset + size]
    values = struct.unpack('<54I', raw)
    assert all(value >= (row['width'] or 1) for row, value in zip(rows, values))
    receipt.update(object_sha256=hashlib.sha256(object_bytes).hexdigest(), symbol='AllRequiredWidthFacts',
                   symbol_file_offset=data_offset, symbol_bytes=raw.hex(),
                   observed_widths={row['name']: value for row, value in zip(rows, values)},
                   missing_provider=[], narrower_than_measured=[])
(root / 'reports' / ('ALL-REQUIRED-WIDTHS-RED.json' if red else 'ALL-REQUIRED-WIDTHS-GREEN.json')).write_text(json.dumps(receipt, indent=2) + '\n')
print('all54 required target widths', r.returncode)
if r.returncode:
    print(r.stderr[-5000:])
sys.exit(r.returncode)
