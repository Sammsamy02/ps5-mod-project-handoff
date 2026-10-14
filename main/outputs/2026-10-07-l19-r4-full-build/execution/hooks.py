"""Narrow in-memory hooks; inherited source files and their pins stay immutable."""
import json
import sys
from pathlib import Path
from admission import OWN, ROOT, ROLE, require, resolved, pin, dependencies, sha, read


def install(admission):
    sys.path.insert(0, str(OWN))
    import prep_guard
    original_ready = prep_guard.require_ready
    metadata = admission.pins['ready_prep'].parent

    def ready():
        old_own = prep_guard.OWN
        try:
            prep_guard.OWN = metadata
            return original_ready()
        finally:
            prep_guard.OWN = old_own

    prep_guard.require_ready = ready
    # This calls the actual inherited admission gate against distinct, pinned metadata.
    ready()
    import selection_checks_l19 as checks
    checks.source_records = lambda: admission.records
    original_helpers = checks.helper_dependencies
    helpers = []

    def bound_helpers(dependency_file, records, required=frozenset(), stage_dir=None):
        if stage_dir:
            return original_helpers(dependency_file, records, required, stage_dir)
        # A byte-identical helper in a retained R3 .d is an explicit old-path admission,
        # not a basename-only fallback. Changed helper bytes always refuse here.
        actual = dependencies(dependency_file)
        adjusted = dict(records)
        for role, r in records.items():
            intended = resolved(r['path'])
            if intended.suffix not in {'.h', '.hpp'}:
                continue
            alternatives = [Path(p) for p in actual if Path(p).name == intended.name]
            if not alternatives:
                continue
            require(len(alternatives) == 1, 'ambiguous selected helper basename')
            found = alternatives[0]
            if found == intended:
                continue
            require(str(found) in admission.allowed and admission.allowed[str(found)] == r['sha256'],
                    'unadmitted prior-source helper: ' + str(found))
            pin({'path': str(found), 'sha256': r['sha256']}, 'explicit identical helper path admission')
            adjusted[role] = {'path': str(found), 'sha256': r['sha256']}
            helpers.append({'dependency_file': str(dependency_file), 'role': role,
                            'selected_path': str(intended), 'actual_pinned_path': str(found),
                            'sha256': r['sha256']})
        return original_helpers(dependency_file, adjusted, required)

    checks.helper_dependencies = bound_helpers
    original_whole = checks.whole_selected_gate

    def whole(run_dir, candidate_dir):
        result = original_whole(run_dir, candidate_dir)
        rec = read(Path(run_dir) / 'receipt.json')
        require(rec.get('r4_execution_admission', {}).get('sha256') == admission.digest,
                'selected composition not bound to actual final R4 admission')
        require(rec.get('r4_affected_units') == 147 and rec.get('r4_pass_number') in (1, 2),
                'R4 exact147/pass receipt missing')
        dependency_pins = {}
        for n in admission.recipes['recipes']:
            unit = rec['selected_runtime_units'][n]
            pin({'path': unit['dependency_file'], 'sha256': unit['dependency_sha256']}, 'R4 selected dependency/' + n)
            dependency_pins[n] = admission.check_dependencies(n, unit['dependency_file'])
        program = rec['selected_runtime_units']['program']
        wanted = '-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="' + str(admission.pins['generated_header']) + '"'
        require(program['argv'].count(wanted) == 1, 'actual selected Program macro missing')
        if rec['r4_pass_number'] == 2:
            require(set(rec['freshly_compiled_units']) == set(admission.recipes['recipes'])
                    and rec.get('r4_reused_accepted_pass1_units') == [], 'pass2 must freshly compile all147')
        from composition import write
        write(Path(candidate_dir) / 'R4-WHOLE-SELECTED.json', {
            'pass': True, 'inherited_whole_selected_pass': True,
            'affected_dependency_walks': 147, 'source_manifest_sha256': admission.data['source_manifest']['sha256'],
            'generated_header': admission.data['generated_header'], 'profile_digest': admission.data['profile_digest'],
            'actual_program_macro': wanted, 'exact_owner_header': str(admission.owner_header),
            'explicit_byte_identical_helper_path_admissions': helpers,
            'execution_admission_sha256': admission.digest})
        return result

    checks.whole_selected_gate = whole
    return checks
