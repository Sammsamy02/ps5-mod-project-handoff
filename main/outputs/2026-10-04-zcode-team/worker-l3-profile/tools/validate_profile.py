#!/usr/bin/env python3
"""validate_profile.py — validate the generated game profile against
schema/game-profile.v1.schema.json.

Self-contained mini JSON-Schema (2020-12 subset) validator: type, const,
enum, pattern, required, properties, items, minItems, maxItems, minimum,
$ref (#/$defs/...), $defs. No third-party dependencies (host is offline for
this work). Exit 0 = valid; exit 1 with the error list otherwise.
"""

import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WORKER = HERE.parent


def fail(errors, path, msg):
    errors.append(f"{path or '<root>'}: {msg}")


def validate(inst, schema, root, errors, path="$"):
    if "$ref" in schema:
        ref = schema["$ref"]
        assert ref.startswith("#/"), f"only local refs supported: {ref}"
        target = root
        for part in ref[2:].split("/"):
            target = target[part]
        validate(inst, target, root, errors, path)
        return
    if "const" in schema and inst != schema["const"]:
        fail(errors, path, f"expected const {schema['const']!r}, got {inst!r}")
    if "enum" in schema and inst not in schema["enum"]:
        fail(errors, path, f"{inst!r} not in enum {schema['enum']}")
    t = schema.get("type")
    if t:
        types = t if isinstance(t, list) else [t]
        ok = False
        for tt in types:
            ok |= {
                "object": lambda x: isinstance(x, dict),
                "array": lambda x: isinstance(x, list),
                "string": lambda x: isinstance(x, str),
                "integer": lambda x: isinstance(x, int) and not isinstance(x, bool),
                "boolean": lambda x: isinstance(x, bool),
                "null": lambda x: x is None,
            }[tt](inst)
        if not ok:
            fail(errors, path, f"expected type {t}, got {type(inst).__name__}")
            return
    if isinstance(inst, str) and "pattern" in schema:
        if not re.search(schema["pattern"], inst):
            fail(errors, path, f"{inst!r} does not match pattern {schema['pattern']!r}")
    if isinstance(inst, (int, float)) and not isinstance(inst, bool):
        if "minimum" in schema and inst < schema["minimum"]:
            fail(errors, path, f"{inst} < minimum {schema['minimum']}")
    if isinstance(inst, dict):
        for req in schema.get("required", []):
            if req not in inst:
                fail(errors, path, f"missing required property {req!r}")
        for key, sub in schema.get("properties", {}).items():
            if key in inst:
                validate(inst[key], sub, root, errors, f"{path}.{key}")
    if isinstance(inst, list):
        if "minItems" in schema and len(inst) < schema["minItems"]:
            fail(errors, path, f"array too short ({len(inst)} < {schema['minItems']})")
        if "maxItems" in schema and len(inst) > schema["maxItems"]:
            fail(errors, path, f"array too long ({len(inst)} > {schema['maxItems']})")
        if "items" in schema:
            for i, item in enumerate(inst):
                validate(item, schema["items"], root, errors, f"{path}[{i}]")


def main():
    schema = json.loads((WORKER / "schema/game-profile.v1.schema.json").read_text())
    profile_path = Path(sys.argv[1]) if len(sys.argv) > 1 else WORKER / "game-profile.townfall-ps5-ue56-v1.json"
    profile = json.loads(profile_path.read_text())
    errors = []
    validate(profile, schema, schema, errors)
    if errors:
        print(f"INVALID — {len(errors)} error(s):")
        for e in errors:
            print(f"  - {e}")
        raise SystemExit(1)
    print(f"VALID — {profile_path.name} conforms to game-profile v1 "
          f"({len(profile.get('addresses', {}).get('seed_slots', []))} seed slots, "
          f"{len(profile.get('sources', []))} sources)")


if __name__ == "__main__":
    main()
