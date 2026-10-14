#!/usr/bin/env python3
"""gen_pins_inc.py - emit PATCH/pak_mount_pins.inc from evidence/pins.json.
The .inc is DATA (per-version, two-source-verified); no offset is hand-written
in a .cpp. Prints how many fields it emitted and fails if zero."""
import json, os, sys
HERE=os.path.dirname(os.path.abspath(__file__))
pins=json.load(open(os.path.join(HERE,"..","evidence","pins.json")))
P={p["id"]:p for p in pins["pins"]}
def v(pid,key="vaddr"): return int(P[pid][key],16)
sha=pins["image"]["sha256"]
fields={
 "image_sha256": f'"{sha}"',
 "pak_vtable": hex(v("P5")),
 "platform_mgr_get": hex(v("P3")),
 "platform_mgr_getpf": hex(v("P4")),
 "slot_get_lower_level": hex(int(P["P6"]["slot_offset"],16)),
 "slot_delegate_execute": hex(int(P["P2"]["slot_offset"],16)),
 "mountpak_delegate": hex(v("P1")),
 "delegate_inst_off": "0",
 "delegate_size_off": "8",
 "handle_mount_pak": hex(v("P7")),
}
out=os.path.join(HERE,"..","PATCH","pak_mount_pins.inc")
with open(out,"w") as f:
    f.write("// GENERATED from evidence/pins.json by tools/gen_pins_inc.py — do not hand-edit.\n")
    f.write("// Per-version engine pin table (UE 5.6.1, image %s).\n"%sha[:12])
    f.write("// Two-source-verified (Epic source + decrypted executable); see pins.json.\n")
    f.write("inline ModBridge::PakMount::EnginePins modbridge_pak_pins_ue561()\n{\n")
    f.write("    ModBridge::PakMount::EnginePins p;\n")
    n=0
    for k,val in fields.items():
        if k=="image_sha256":
            f.write(f'    std::snprintf(p.image_sha256, sizeof p.image_sha256, "%s", {val});\n')
        else:
            f.write(f"    p.{k} = {val};\n"); 
        n+=1
    f.write("    return p;\n}\n")
print(f"emitted {n} pin fields -> {os.path.relpath(out)}")
if n==0: sys.exit(1)
