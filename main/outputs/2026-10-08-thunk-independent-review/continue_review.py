from pathlib import Path
import hashlib, json, os, re, shlex, subprocess
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SEALED = ROOT / 'outputs/2026-10-08-menu-path-audit'
THUNK = SEALED / 'thunk-design'
sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
def save(name, value):
    (HERE / name).write_text(json.dumps(value, indent=2) + '\n')

# Continue the independent symbol capture stage skipped when the first
# publication diagnostic observed no race; never rerun the unchanged controls.
own = (HERE / 'run_review.py').read_text()
symbol_stage = own[own.index("symbols = json.loads"):own.index("print(json.dumps({'frozen_pins'")]
exec(compile(symbol_stage, str(HERE / 'run_review.py') + ':symbol-stage', 'exec'))

target = json.loads((THUNK / 'target/TARGET-RECEIPT.json').read_text())
recipe_file = Path(target['recipe_file'])
assert sha(recipe_file) == target['recipe_sha256']
recipes = json.loads(recipe_file.read_text())['recipes']
target_rows = []
owner = ROOT / 'outputs/2026-10-07-l19-r4-work/dependent-build-pass1/include/Unreal/FField.hpp'
helper = THUNK / 'candidate/ScriptHookThunks.hpp'
original_header = ROOT / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UFunctionStructs.hpp'
for row in target['results']:
    assert row['returncode'] == 0
    command = row['command']
    expected = list(recipes[row['recipe_role']]['pass1_argv'])
    expected[1:1] = ['-I', str(THUNK / 'candidate')]
    for flag in ['-c', '-o', '-MF']:
        expected[expected.index(flag) + 1] = command[command.index(flag) + 1]
    assert expected == command
    assert row['LLVM_CONFIG'] == recipes[row['recipe_role']]['LLVM_CONFIG']
    source = Path(command[command.index('-c') + 1])
    obj = Path(command[command.index('-o') + 1])
    dep = Path(command[command.index('-MF') + 1])
    assert sha(source) == row['source_sha256']
    assert sha(obj) == row['object_sha256']
    assert sha(dep) == row['dependency_sha256']
    tokens = shlex.split(dep.read_text().replace('\\\n', ' '))
    dependencies = set(tokens[1:])
    assert str(source) in dependencies and str(helper) in dependencies and str(owner) in dependencies and str(original_header) in dependencies
    target_rows.append({'source': str(source), 'source_sha256': sha(source), 'object': str(obj), 'object_sha256': sha(obj), 'dependency': str(dep), 'dependency_sha256': sha(dep), 'dependency_count': len(dependencies), 'recipe_flags_exact_except_candidate_include_and_source_output_paths': True, 'recorded_compile_returncode': row['returncode'], 'actual_selected_headers': [{'path': str(p), 'sha256': sha(p)} for p in [helper, owner, original_header]]})
protection = json.loads((SEALED / 'SOURCE-PROTECTION.json').read_text())
source_rows = []
for row in protection['pins']:
    current = sha(row['source'])
    cache = SEALED / 'source-cache' / row['name']
    assert current == row['expected_sha256'] == sha(cache)
    source_rows.append({'source': row['source'], 'current_sha256': current, 'cache': str(cache), 'cache_sha256': sha(cache), 'unchanged': True})
save('TARGET-SOURCE-AUDIT.json', {'pass': True, 'scope': 'Independent hash/recipe/dependency inspection of three recorded successful target compiles; no repeated compiler/link invocation and no native ABI execution claim', 'target_receipt_sha256': sha(THUNK / 'target/TARGET-RECEIPT.json'), 'recipe_sha256': sha(recipe_file), 'target_rows': target_rows, 'protected_source_rows': source_rows})
print(json.dumps({'fresh_nm_captures': len(captures), 'target_tu_recipe_dependency_checks': len(target_rows), 'protected_original_sources': len(source_rows), 'added_standard_symbols': len(added)}))
