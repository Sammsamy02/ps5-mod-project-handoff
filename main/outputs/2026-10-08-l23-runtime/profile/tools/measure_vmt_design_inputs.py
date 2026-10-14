#!/usr/bin/env python3
"""Read-only metadata context for a design report; no hook or runtime artifact generation."""
from image_lib import *
import json
v=0x8477708;wanted={v-16,v-8,v+0x310};rr={a:[] for a in wanted}
for o in range(off(D[7]),off(D[7])+D[8],D[9]):
 a,info,t=struct.unpack_from('<QQq',B,o)
 if a in rr:rr[a].append({'file_offset':hex(o),'info':hex(info),'addend':hex(t),'bytes':B[o:o+24].hex()})
assert rr[v-16]==rr[v-8]==[] and len(rr[v+0x310])==1
assert rr[v+0x310][0]['info']=='0x8' and rr[v+0x310][0]['addend']=='0x2fac8c0'
rows=[{'index':i,'address':hex(v+8*i),'file_offset':hex(off(v+8*i,8)),'raw_qword':hex(struct.unpack('<Q',read(v+8*i,8))[0]),'bytes':read(v+8*i,8).hex(),'all_dynamic_relocations':rr[v+8*i]} for i in (-2,-1,98)]
pins={}
for f in ('outputs/2026-10-07-l19-worker-engine-slots/ENGINE-TICK-FACTS.json','outputs/2026-10-07-l19-final-audit/r3/TICK-AUDIT.json','outputs/2026-10-08-r5-thread-final-integration/thread/UnrealInitializer.cpp','outputs/2026-10-08-l22-runtime/diagnostics/src/LuaMod.cpp'):
 pins[f]=hashlib.sha256((ROOT/f).read_bytes()).hexdigest()
x={'scope':'Saved-image design inputs only; no live vptr or full vtable bound proven','image_sha256':SHA,'gameengine_address_point':hex(v),'tick_byte_offset':'0x310','entries':rows,'source_pins':pins,'no_runtime_implementation':True}
(Path(__file__).resolve().parents[1]/'evidence/vmt-design-inputs.json').write_text(json.dumps(x,indent=2)+'\n')
print('PASS metadata -2/-1 and Tick98 re-read; all dynamic relocations checked; no full-table boundary inferred')
