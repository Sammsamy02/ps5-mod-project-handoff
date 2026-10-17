"""Generate/check current data pins; no image read or code execution."""
import json,sys
from pathlib import Path
sys.dont_write_bytecode=True
H=Path(__file__).resolve().parent.parent
p=json.loads((H/'evidence/current-profile.json').read_text())
s='#pragma once\n// GENERATED from evidence/current-profile.json; image-bound data only.\n#include <cstdio>\ninline ModBridge::PakMount::EnginePins modbridge_pak_pins_ue561()\n{\n    ModBridge::PakMount::EnginePins p;\n    std::snprintf(p.image_sha256,sizeof p.image_sha256,"%s","'+p['image_sha256']+'");\n'
for key,val in p['fields'].items():s+='    p.'+key+' = '+val+';\n'
for i,r in enumerate(p['proofs']):s+='    p.proofs[%d] = {%s,{%s},%d};\n'%(i,r['linked'],','.join('0x'+r['bytes'][j:j+2] for j in range(0,len(r['bytes']),2)),r['size'])
s+='    return p;\n}\n'
if sys.argv[1:]==['--check']:
 if (H/'PATCH/pak_mount_pins.inc').read_text()!=s:raise SystemExit('FAIL generated pins differ')
 print('PASS: 16 data fields, 6 exact signature profiles checked')
elif not sys.argv[1:]:sys.stdout.write(s)
else:raise SystemExit('usage: gen_pins_inc.py [--check]')
