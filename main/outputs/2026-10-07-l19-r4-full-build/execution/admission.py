"""Explicit R4 admission. No subprocess or compiler import occurs in this module."""
import hashlib
import json
import re
import shlex
from pathlib import Path

OWN = Path(__file__).resolve().parents[1]
ROOT = OWN.parents[1]
PENDING = {'program', 'unreal-unrealinitializer', 'native-layoutbinding'}
ROLE = {'program': 'Program', 'unreal-unrealinitializer': 'initializer',
        'native-layoutbinding': 'NativeLayoutBinding.cpp'}


class Refusal(ValueError):
    pass


def require(value, message):
    if not value:
        raise Refusal('HOLD: ' + message)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def resolved(path):
    p = Path(path)
    return (p if p.is_absolute() else ROOT / p).resolve()


def pin(record, label):
    require(isinstance(record, dict), 'missing reviewed pin: ' + label)
    path, digest = record.get('path'), record.get('sha256')
    require(isinstance(path, str) and Path(path).is_absolute(), 'absolute file pin required: ' + label)
    require(isinstance(digest, str) and re.fullmatch('[0-9a-f]{64}', digest), 'complete hash required: ' + label)
    p = Path(path)
    require(p.is_file() and not p.is_symlink(), 'missing regular file: ' + label)
    require(sha(p) == digest, 'hash drift: ' + label)
    return p.resolve()


def dependencies(path):
    text = Path(path).read_text().replace('\\\n', ' ')
    require(':' in text, 'malformed compiler dependency file')
    return {str(resolved(p)) for p in shlex.split(text.split(':', 1)[1])}


def validate_recipe_set(recipes, substitutions):
    rows = recipes.get('recipes', {})
    require(len(rows) == 147 and recipes.get('affected_units') == 147, 'exact147 recipe set required')
    require(recipes.get('selected_units') == 369 and recipes.get('composition_inputs') == 386,
            '369 selected units /386 inputs required')
    require(set(recipes.get('pending_final_units', [])) == PENDING, 'exact three pending recipes required')
    require({n for n, r in rows.items() if not r.get('pass1_reusable')} == PENDING,
            '144 reuse-eligible recipes required')
    require(all(r.get('name') == n and r.get('pass2_must_compile_fresh') is True
                for n, r in rows.items()), 'named fresh pass2 recipe required')
    slots = substitutions.get('input_slots', [])
    require(len(slots) == 386 and [s.get('position') for s in slots] == list(range(386)),
            'exact386 ordered slot positions required')
    require(len({s['old_input']['path'] for s in slots}) == 386, 'duplicate input slot path')
    bound = {s.get('replace_from_affected_recipe'): s for s in slots if s.get('replace_from_affected_recipe')}
    require(len(bound) == 147 and set(bound) == set(rows), 'exact147 input slot substitutions required')
    for n, r in rows.items():
        s = bound[n]
        require(s['unit'] == n and s['position'] == r['composition_input_index']
                and s['old_input'] == r['old_object'], 'recipe/input slot mismatch: ' + n)


def validate_complete_profile(profile):
    require(isinstance(profile, dict) and type(profile.get('schema')) is int and profile['schema'] == 1,
            'missing/unsupported profile schema')
    members, slots = profile.get('required_members'), profile.get('required_vtables')
    require(isinstance(members, list) and len(members) == 69 and isinstance(slots, list) and len(slots) == 7,
            'complete69/7 profile required')
    require(profile.get('unknown_required_static_facts') == [], 'unresolved profile static facts')
    for group in (members, slots):
        require(len({r.get('name') for r in group}) == len(group), 'duplicate profile key')
        require(all(type(r.get('offset')) is int and r['offset'] >= 0 for r in group),
                'unresolved profile offset/total')


def validate_go(go, admission_hash, admission):
    require(go.get('r4_full_build_go') is True and go.get('local_full_build_only') is True
            and go.get('upload_authorized') is False, 'explicit LOCAL R4 build GO required')
    require(go.get('admission_sha256') == admission_hash, 'GO does not bind execution admission')
    for name in ('source_manifest', 'profile_json', 'generated_header'):
        require(go.get(name + '_sha256') == admission.get(name, {}).get('sha256'),
                'GO final pin mismatch: ' + name)


class Admission:
    @classmethod
    def load(cls, path, digest, go_path, go_digest):
        require(go_path is not None and go_digest is not None, 'missing parent R4 GO argument')
        p = pin({'path': str(Path(path).absolute()), 'sha256': digest}, 'execution admission')
        a = read(p)
        gp = pin({'path': str(Path(go_path).absolute()), 'sha256': go_digest}, 'parent R4 GO')
        go = read(gp)
        validate_go(go, digest, a)
        require(a.get('schema') == 1 and type(a['schema']) is int and a.get('readiness') == 'READY',
                'genuine finalized READY admission required')
        self = cls()
        self.path, self.digest, self.go_path, self.go_digest = p, digest, gp, go_digest
        self.data, self.go = a, go
        self.validate()
        return self

    def validate(self):
        a = self.data
        frozen = read(OWN / 'parent-review/FROZEN-PREPARATION.json')
        for rel, digest in frozen['pins'].items():
            pin({'path': str(OWN / rel), 'sha256': digest}, 'frozen preparation/' + rel)
        clone = read(OWN / 'PIPELINE-CLONE-RECEIPT.json')
        for name, r in clone['byte_identical_root_files'].items():
            pin({'path': str(OWN / name), 'sha256': r['sha256']}, 'inherited/' + name)
        required_code = {str(p.resolve()) for p in (OWN / 'execution').glob('*.py')}
        code_pins = a.get('execution_source_pins', {})
        require(required_code and required_code == set(code_pins), 'complete execution hook source pins required')
        for p, h in code_pins.items():
            pin({'path': p, 'sha256': h}, 'reviewed execution hook')
        self.pins = {}
        for name in ('ready_prep', 'edition', 'source_manifest', 'xom_audit', 'profile_json',
                     'profile_header', 'generated_header', 'final_review', 'integration_manifest',
                     'affected_recipes', 'input_substitutions', 'linker', 'nm'):
            self.pins[name] = pin(a.get(name), name)
        require(self.pins['affected_recipes'] == OWN / 'parent-review/AFFECTED-RECIPES.json'
                and self.pins['input_substitutions'] == OWN / 'parent-review/INPUT-SUBSTITUTION-MANIFEST.json',
                'frozen exact147 recipe/slot inputs required')
        self.recipes, self.substitutions = read(self.pins['affected_recipes']), read(self.pins['input_substitutions'])
        validate_recipe_set(self.recipes, self.substitutions)
        self.base = read(pin(self.recipes['base_receipt'], 'R3 base receipt'))
        self.base_inputs = read(pin(self.recipes['base_inputs'], 'R3 base inputs'))
        self.selected = self.base['selected_runtime_units']
        require(len(self.selected) == 369 and len(self.base_inputs) == 386, 'R3 inventory drift')
        require(list(self.base_inputs) == [s['old_input']['path'] for s in self.substitutions['input_slots']],
                'R3 exact ordered input manifest mismatch')
        require(set(self.recipes['recipes']) == {r['name'] for r in read(pin(self.recipes['owner_census'], 'owner census'))['affected']},
                'actual147 consumer census mismatch')
        self.records = read(self.pins['source_manifest']).get('selected_sources', {})
        require(isinstance(self.records, dict), 'selected source roles required')
        for role, record in self.records.items():
            pin(record, 'selected role/' + role)
        for role in ('Program', 'initializer', 'NativeLayoutBinding.cpp', 'NativeLayoutDeclarations.hpp', 'RuntimeBuildProfile.hpp'):
            require(role in self.records, 'missing final source role: ' + role)
        require(self.records['RuntimeBuildProfile.hpp'] == a['profile_header'], 'selected profile header pin mismatch')
        require('FField.hpp' not in self.records, 'owner header requires distinct exact .d admission')
        for name, role in (('runtime_owner', 'runtime_owner'), ('shadow-copy', 'shadow_copy.cpp')):
            old = self.selected[name]
            require(self.records.get(role, {}).get('path') == old['source']
                    and self.records.get(role, {}).get('sha256') == old['source_sha256'],
                    'retained source role must name actual R3 source: ' + role)
        profile = read(self.pins['profile_json'])
        validate_complete_profile(profile)
        digest = a.get('profile_digest')
        require(isinstance(digest, str) and re.fullmatch('[0-9a-f]{64}', digest), 'profile digest required')
        for name in ('profile_header', 'generated_header'):
            require(digest in self.pins[name].read_text(), 'generated profile digest mismatch: ' + name)
        review = read(self.pins['final_review'])
        require(review.get('pass') is True and review.get('findings') == [], 'independent final review must pass')
        require(review.get('source_manifest_sha256') == a['source_manifest']['sha256']
                and review.get('profile_json_sha256') == a['profile_json']['sha256']
                and review.get('generated_header_sha256') == a['generated_header']['sha256'],
                'final review must bind exact source/profile/generated header')
        for name in ('ready_prep', 'edition'):
            require(self.pins[name].parent == self.go_path.parent
                    and self.pins[name].parent.is_relative_to(OWN / 'execution-state'),
                    'READY metadata must reside in separate owned execution-state directory')
        require(self.pins['ready_prep'].name == 'PREP-MANIFEST.json'
                and self.pins['edition'].name == 'EDITIONS-l19.json' and self.go_path.name == 'BUILD-GO.json',
                'inherited READY metadata filenames required')
        prep, edition = read(self.pins['ready_prep']), read(self.pins['edition'])
        require(prep.get('readiness') == 'READY' and prep.get('source_manifest') == a['source_manifest']
                and prep.get('xom_audit_receipt') == a['xom_audit'], 'READY prep final binding mismatch')
        require(self.go.get('parent_reported_xom_audit') is True
                and self.go.get('xom_audit_sha256') == a['xom_audit']['sha256'], 'GO must bind actual XOM review')
        require(edition.get('l19_scope', {}).get('selected_sources') == self.records, 'edition source binding mismatch')
        require(self.pins['integration_manifest'] == OWN / 'integration/change-manifest.json',
                'new owned integration manifest required')
        self.owner_header = pin(self.recipes['recipes']['program']['required_accepted_header'], 'accepted physical owner header')
        self.allowed = {str(resolved(p)): h for p, h in self.base['source_hashes'].items()}
        self.allowed[str(self.owner_header)] = self.recipes['recipes']['program']['required_accepted_header']['sha256']
        owner = pin(self.recipes['owner_source'], 'accepted FField source')
        self.allowed[str(owner)] = self.recipes['owner_source']['sha256']
        for role, r in self.records.items():
            self.allowed[str(resolved(r['path']))] = r['sha256']
        self.allowed[str(self.pins['generated_header'])] = a['generated_header']['sha256']
        for p, h in a.get('additional_source_pins', {}).items():
            pin({'path': p, 'sha256': h}, 'additional final source/evidence')
            require(p not in self.allowed or self.allowed[p] == h, 'conflicting admitted dependency pin')
            self.allowed[p] = h
        self.tool_pins = a.get('tool_pins', {})
        tools = {r['original_argv'][0] for r in self.recipes['recipes'].values()}
        tools |= {r['LLVM_CONFIG'] for r in self.recipes['recipes'].values()}
        require(tools <= self.tool_pins.keys(), 'all actual147 compiler/config tool pins required')
        for p, h in self.tool_pins.items():
            pin({'path': p, 'sha256': h}, 'compiler tool')
        self.validate_tool_aliases()
        for p, h in self.allowed.items():
            pin({'path': p, 'sha256': h}, 'all-source dependency')
        for p, h in self.base_inputs.items():
            pin({'path': str(resolved(p)), 'sha256': h}, 'R3 composition input')
        for name, old in self.selected.items():
            pin({'path': str(resolved(old['source'])), 'sha256': old['source_sha256']}, 'R3 selected source/' + name)
            pin({'path': str(resolved(old['object'])), 'sha256': old['object_sha256']}, 'R3 selected object/' + name)
        for label in ('owner_audit', 'owner_pass1', 'loader_freeze', 'loader_final_receipt'):
            pin(self.recipes[label], label)
        require(read(Path(self.recipes['owner_audit']['path'])).get('pass') is True, 'independent owner audit failed')
        for n, r in self.recipes['recipes'].items():
            if r['pass1_reusable']:
                pin(r['final_source'], 'reuse source/' + n)
                pin(r['pass1_object'], 'reuse object/' + n)
                pin(r['pass1_dependency'], 'reuse dependency/' + n)
                self.check_dependencies(n, r['pass1_dependency']['path'])
            else:
                pin(self.records[ROLE[n]], 'final pending source/' + n)
        # Re-use only exact original 144 argv, not a caller-provided recipe override.
        pass1 = {r['name']: r for r in read(self.recipes['owner_pass1']['path'])['selected_units']}
        for n, r in self.recipes['recipes'].items():
            if r['pass1_reusable']:
                require(r['pass1_argv'] == pass1[n]['argv'], 'accepted144 argv mismatch: ' + n)

    def validate_tool_aliases(self):
        aliases = self.data.get('tool_aliases', {})
        expected = str(self.pins['linker'].parent / 'clang++')
        require(expected in aliases, 'exact delegated clang++ alias mapping required')
        linker_alias = str(self.pins['linker'].parent / 'ld.lld')
        require(self.data.get('linker_invocation') == linker_alias and linker_alias in aliases,
                'exact GNU ld.lld invocation/alias mapping required')
        require(aliases[linker_alias].get('resolved_path') == str(self.pins['linker']),
                'GNU linker invocation must resolve to pinned linker executable')
        for path, record in aliases.items():
            alias = Path(path)
            require(alias.is_absolute() and alias.is_symlink(), 'expected compiler alias missing')
            require(str(alias.readlink()) == record.get('link_target'), 'compiler alias spelling drift')
            target = alias.resolve()
            require(str(target) == record.get('resolved_path'), 'compiler alias target drift')
            require(self.tool_pins.get(str(target)) == record.get('sha256'), 'compiler alias target not tool-pinned')
            pin({'path': str(target), 'sha256': record.get('sha256')}, 'delegated compiler alias target')

    def check_dependencies(self, name, dependency_file):
        actual = dependencies(dependency_file)
        require(str(self.owner_header) in actual, 'accepted FField header absent from actual .d: ' + name)
        for p in actual:
            require(p in self.allowed, 'unqueried/unpinned actual dependency: ' + name + ':' + p)
            pin({'path': p, 'sha256': self.allowed[p]}, 'actual dependency/' + name)
        if name in {'program', 'unreal-unrealinitializer'}:
            require(str(self.pins['profile_header']) in actual, 'selected profile header absent from .d: ' + name)
        if name == 'native-layoutbinding':
            for role in ('NativeLayoutBinding.hpp', 'NativeLayoutActiveWidths.hpp'):
                require(str(resolved(self.records[role]['path'])) in actual,
                        'selected binder helper absent from .d: ' + role)
        if name == 'program':
            require(str(self.pins['generated_header']) in actual
                    and str(resolved(self.records['NativeLayoutDeclarations.hpp']['path'])) in actual,
                    'compiled declaration provider/header absent from Program .d')
        return {p: self.allowed[p] for p in sorted(actual)}
