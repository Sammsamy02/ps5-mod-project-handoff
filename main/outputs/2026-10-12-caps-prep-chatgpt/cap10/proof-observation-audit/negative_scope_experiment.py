"""One metadata-only counterexample; never load or execute fixture bytes."""
import sys
sys.dont_write_bytecode = True
from pathlib import Path
import hashlib
import importlib.util
import json

CAP10 = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(CAP10))
spec = importlib.util.spec_from_file_location('existing_fixture', CAP10 / 'test_assembly_preflight.py')
fixture_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture_module)
# Existing API: default fixture has correct metadata but trivial managed bodies.
data = fixture_module.fixture()
assert data[0x400:0x403] == b'\x0a\x16\x2a'
assert data[0x410:0x412] == b'\x06\x2a'
result = fixture_module.preflight.inspect(data)
print(json.dumps({'label':'Artificial fixture metadata experiment only; no managed execution',
                  'fixture_sha256':hashlib.sha256(data).hexdigest(), 'fixture_bytes':len(data),
                  'Run_body':{'file_offset':'0x400','bytes':'0a162a','declared_code_size':2,
                              'instructions':['ldc.i4.0','ret'],'contains_import_call':False},
                  'Throw_body':{'file_offset':'0x410','bytes':'062a','declared_code_size':1,
                                'instructions':['ret'],'contains_throw':False},
                  'actual_preflight_result':result,
                  'conclusion':'Metadata gate intentionally admits trivial return0/no-import/no-throw bodies; no IL semantics or execution proof'},indent=2))
