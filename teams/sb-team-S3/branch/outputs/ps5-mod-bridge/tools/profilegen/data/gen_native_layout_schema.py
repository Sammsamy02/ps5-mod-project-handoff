#!/usr/bin/env python3
"""Regenerate native_layout_schema.json from the port's ``NativeLayoutSchema.inc``.

The port refuses a MemberVariableLayout.ini that names a member it does not know (``Unknown native member``), refuses a
duplicate or aliased key, and requires ``UEP_TotalSize`` in every class it is given.  This table is that list of
accepted names -- names only, no numbers -- so the emitter can refuse to write a key the port would reject.  It is
transcribed mechanically; nothing is typed by hand.

    python3 gen_native_layout_schema.py PATH/TO/NativeLayoutSchema.inc > native_layout_schema.json
"""
import hashlib
import json
import re
import sys


def main(path):
    raw = open(path, "rb").read()
    text = raw.decode("utf-8")
    rows = re.findall(r'\{"([^"]+)", "([^"]+)", "([^"]+)", "([^"]+)", (true|false)\}', text)
    classes = {}
    for section, key, target, canonical, bitfield in rows:
        c = classes.setdefault(target, {"keys": [], "aliases": {}, "bitfields": []})
        if key == canonical:
            if canonical not in c["keys"]:
                c["keys"].append(canonical)
        else:
            c["aliases"][key] = canonical
        if bitfield == "true" and canonical not in c["bitfields"]:
            c["bitfields"].append(canonical)
    vt = re.search(r"VirtualTargets\(\)[^{]*\{\s*return\s*\{(.*?)\};\s*\}", text, re.S)
    vtables = re.findall(r'\{"([^"]+)",\s*&Unreal::', vt.group(1)) if vt else []
    out = {"source": "NativeLayoutSchema.inc", "source_sha256": hashlib.sha256(raw).hexdigest(),
           "note": "member names the port's MemberVariableLayout.ini binding accepts, per class; aliases are refused as duplicates of the canonical key",
           "vtable_classes": sorted(vtables),
           "classes": {k: classes[k] for k in sorted(classes)}}
    json.dump(out, sys.stdout, indent=1, sort_keys=False)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main(sys.argv[1])
