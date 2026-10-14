#!/usr/bin/env python3
import json,sys
from pathlib import Path
p=json.loads(Path(sys.argv[1]).read_text())
allocator=[x for x in p['required_vtables'] if x['name'].startswith('FMalloc::')]
assert len(allocator)==17, f'expected17 FMalloc pins; found{len(allocator)}'
assert next(x for x in allocator if x['name']=='FMalloc::QuantizeSize')['offset']==0x68
print('PASS all17 pinned allocator methods including QuantizeSize0x68')
