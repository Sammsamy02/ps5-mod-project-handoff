"""A local evidence-to-profile acceptance check; no generated source changes."""
from pathlib import Path
import json, sys

evidence_dir = Path(__file__).resolve().parent
profile_path = Path(sys.argv[1])
profile = json.loads(profile_path.read_text())
pins = {pin['name']: pin for pin in profile['required_members']}
checks = []
for filename in ['FIELDC-FACTS.json', 'UCLASS-FACTS.json']:
    facts = json.loads((evidence_dir / filename).read_text())
    assert facts['image']['sha256'] == profile['image']['sha256']
    for fact in facts['facts']:
        name = fact['key'].replace('.', '::', 1)
        if name not in pins:
            assert fact.get('required') is False, name
            continue
        pin = pins[name]
        ok = pin['offset'] == int(fact['value'], 0) and pin['width'] == fact['width_bytes']
        checks.append({'name':name,'expected_offset':fact['value'],'expected_width':fact['width_bytes'],'observed_offset':pin['offset'],'observed_width':pin['width'],'pass':ok})
for name in ['accessor_width.FFieldClass.CastFlags','accessor_width.UClass.ClassCastFlags']:
    checks.append({'name':name,'pass':name not in profile['unknown_required_static_facts']})
result = {'profile':str(profile_path),'checks':checks,'pass':all(c['pass'] for c in checks)}
print(json.dumps(result,indent=2))
sys.exit(0 if result['pass'] else 1)
