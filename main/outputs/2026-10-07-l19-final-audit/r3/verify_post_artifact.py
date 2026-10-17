import hashlib
import json
from pathlib import Path

w = Path.cwd()
p = w / 'outputs/2026-10-07-l19-r3-full-build'
worker = w / 'outputs/2026-10-07-l19-local-profile-r3'
out = Path(__file__).resolve().parent

def load(path):
    return json.loads(Path(path).read_text())

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda: f.read(1048576), b''):
            h.update(b)
    return h.hexdigest()

def pin(path):
    return {'path': str(path), 'sha256': sha(path)}

def save(name, value):
    path = out / name
    assert not path.exists()
    path.write_text(json.dumps(value, indent=2) + '\n')
    return pin(path)

artifact = load(out / 'ARTIFACT-AUDIT.json')
assert artifact['status'] == 'FINAL_ARTIFACT_AUDIT_PASS'
summary = load(p / 'FULL-PIPELINE-RECEIPT.json')
final = load(worker / 'reports/FINAL-RECEIPT.json')
assert summary['unresolved_profile_facts'] == {
    'members': final['remaining_unknown_members'],
    'vtables': final['remaining_unknown_vtables'], 'other_static': []}
assert summary['staged_layout_files_expanded'] is False and summary['mod_and_settings_changed'] is False
assert summary['source_manifest']['sha256'] == artifact['bound_selection_sha256']
delta = load(p / 'parent-review/SOURCE-DELTA.json')
assert [row['file'] for row in delta['changes']] == ['NativeLayoutActiveWidths.hpp', 'RuntimeBuildProfile.hpp']
assert delta['mod_or_settings_edits'] is False and delta['pad_cache_lifetime_deadline_timing_unchanged'] is True
bound = load(p / 'SOURCE-SELECTION.bound.json')
assert len(bound['original_sources']) == 31
for role, item in bound['original_sources'].items():
    assert sha(item['path']) == item['sha256'], role

immutable = []
for name, count, expected in [
    ('2026-10-07-l19-local-profile', 60, '0a6315e616922fab9922c2aff10c98af023654807ae7b2ab52957c05fb5da642'),
    ('2026-10-07-l19-local-profile-r2', 67, '70e26759ff736d5dd167e5cae2d59700ff0dc1eee36ebe39fa0a784ddfc5cdd2')]:
    base = w / 'outputs' / name
    selection = base / 'reports/SOURCE-SELECTION.json'
    assert sha(selection) == expected
    data = load(selection)
    assert len(data['source_files']) == count
    for rel, digest in data['source_files'].items():
        assert sha(base / rel) == digest
    immutable.append({'selection': pin(selection), 'source_pins_exact': count})

dependencies = []
for i in [1, 2]:
    rr = load(p / 'runs' / f'run-l19-{i}' / 'receipt.json')
    selected = rr['selected_runtime_units']['native-layoutbinding']
    dep = Path(selected['dependency_file'])
    assert selected['exit'] == 0 and '-MF' in selected['argv']
    assert Path(selected['argv'][selected['argv'].index('-MF') + 1]) == dep
    assert selected['runtime_replaced'] is True and selected['retained_replaced'] is True
    assert str(p / 'units/NativeLayoutActiveWidths.hpp') in dep.read_text()
    assert sha(selected['source']) == '5714ffd92acb86f0f02efc18414152fecf2a7d7cd7c386b2c3351937a043ec59'
    assert sha(p / 'units/NativeLayoutActiveWidths.hpp') == '619e9dcf9cdd0503becaf8ef03913be9d3bf8f00436c88c19e6a2ce358f22fb6'
    dependencies.append({'run': i, 'dependency_file': pin(dep), 'selected_binding_object': pin(selected['object']),
                         'exact_current_helper_dependency': True, 'fresh_compilation_exit': selected['exit']})

comparator_pin = summary['original_LuaMod_object_comparator']
assert sha(comparator_pin['path']) == comparator_pin['sha256']
comparator = load(comparator_pin['path'])
current = load(p / 'runs/run-l19-2/receipt.json')
baseline = load(w / 'outputs/2026-10-07-l18-full-build/runs/run-l18-2/receipt.json')
current_rows = [(name, row) for name, row in current['selected_runtime_units'].items()
                if row['object'] == comparator['unit_receipt_object']]
assert len(current_rows) == 1
key, row = current_rows[0]
prior = baseline['selected_runtime_units'][key]
assert row['source_sha256'] == prior['source_sha256'] == comparator['source_sha256']
assert row['object_sha256'] == prior['object_sha256'] == comparator['object_sha256'] == comparator['comparator']
assert Path(row['object']).stat().st_size == comparator['bytes'] == 7175336
assert Path(row['object']).read_bytes() == Path(prior['object']).read_bytes()

docs = p / 'inputs/final-report-documents'
fact_source = w / 'outputs/2026-10-07-l19-fact-summary-r3'
for name in ['FACTS.json', 'FACT-TABLE.md', 'VERIFICATION.json']:
    assert (docs / name).read_bytes() == (fact_source / name).read_bytes()
facts, verification = load(docs / 'FACTS.json'), load(docs / 'VERIFICATION.json')
assert facts['profile_file_sha256'] == final['profile_sha256'] and facts['profile_digest'] == final['profile_digest']
counts = facts['counts']
for key, value in {'inventory_fact_rows':125, 'joined_fact_rows':138, 'required_public_width_keys':54,
                   'available_required_public_width_keys':54, 'known_member_pins':67, 'required_member_pins':69,
                   'known_vtable_pins':5, 'required_vtable_pins':7, 'static_instruction_window_checks':1603,
                   'unique_static_instruction_windows':1560, 'saved_local_member_declarations':17,
                   'saved_local_vtable_declarations':1}.items():
    assert counts[key] == value, key
assert len(facts['facts']) == 138 and len({row['id'] for row in facts['facts']}) == 138
assert sum(row['origin'] == 'traced-inventory' for row in facts['facts']) == 125
assert verification['all125_original_rows_preserved'] is True
assert verification['static_instruction_windows_PT_LOAD_bytes_and_single_decode_checked'] == 1603
assert verification['distinct_static_instruction_windows'] == 1560
for rel, digest in facts['input_sha256'].items():
    assert sha(w / rel) == digest, rel
assert len(facts['input_sha256']) == 144
for name, digest in verification['output_sha256'].items():
    assert sha(fact_source / name) == digest, name
timeline = load(docs / 'TIMELINE.json')
timing_inputs = []
for item in timeline['input_hashes']:
    current_bytes = Path(item['path']).read_bytes()
    current_sha = hashlib.sha256(current_bytes).hexdigest()
    if current_sha == item['sha256']:
        assert len(current_bytes) == item['bytes']
        timing_inputs.append({'path':item['path'],'sha256':current_sha,'bytes':len(current_bytes),'matches_current_whole_file':True})
    else:
        assert Path(item['path']).name == 'klog-20261007-205612.log'
        assert len(current_bytes) == 630222 and item['bytes'] == 621320
        prefix, suffix = current_bytes[:item['bytes']], current_bytes[item['bytes']:]
        assert hashlib.sha256(prefix).hexdigest() == item['sha256'] == 'fd02954c5a09a50bfb742c897df9a796c929a45501f94b58938372e6f5ae729b'
        frozen_prefix = out / 'timing-klog-authenticated-prefix.log'
        assert not frozen_prefix.exists()
        frozen_prefix.write_bytes(prefix)
        timing_inputs.append({'path':item['path'],'current_sha256':current_sha,'current_bytes':len(current_bytes),
                              'matches_current_whole_file':False,'matches_exact_frozen_prefix':True,
                              'prefix_bytes':len(prefix),'prefix_sha256':hashlib.sha256(prefix).hexdigest(),
                              'frozen_prefix':pin(frozen_prefix),'appended_bytes':len(suffix),
                              'suffix_sha256':hashlib.sha256(suffix).hexdigest()})
assert sum(row['matches_current_whole_file'] for row in timing_inputs) == 6
timing_addendum = save('TIMING-PROVENANCE-ADDENDUM.json', {
    'status':'TIMING_SAVED_INPUT_AUDIT_PASS_WITH_APPEND_ONLY_LOG_QUALIFICATION','pass':True,
    'frozen_timeline':pin(docs/'TIMELINE.json'),'inputs':timing_inputs,
    'qualification':'Six timing inputs match current whole-file pins. One saved kernel log grew by8902B after the frozen timeline; its original621320B snapshot remains byte-exact and is preserved separately. Appended bytes are excluded from the historical timing analysis. No new console contact or timing change.'})
report = (p / 'BUILD-REPORT.md').read_text()
for text in ['31,024,199', '4565373be6c8d2b3d0dc592e0c8b8ae36744fc9447be3e8ed37abf86d6e2ccc9',
             '01d634d35140aa769dde83c610689362df4da2e46d172d264d714e71644f9d4a',
             '67/69', '5/7', '52 required member declarations and 6 virtual declarations are absent',
             '65 s before PRX entry', '4.082 s', '2.130 s', '2.297 s', '2.355 s',
             'No speedup was applied', 'not ready', 'Upload HOLD', 'future game behavior']:
    assert text in report, text

post = save('POST-ARTIFACT-REPORT-AUDIT.json', {
    'status':'R3_POST_ARTIFACT_REPORT_AND_HELPER_AUDIT_PASS', 'pass':True,
    'summary':pin(p/'FULL-PIPELINE-RECEIPT.json'), 'report':pin(p/'BUILD-REPORT.md'),
    'preserved_worker_freezes':immutable, 'fresh_binding_helper_dependencies':dependencies,
    'original_bound_source_pins_exact':31,
    'original_luamod_byte_comparator':{'receipt':comparator_pin,'current_object':pin(row['object']),
                                      'baseline_object':pin(prior['object']), 'bytes':7175336,'byte_equal':True},
    'joined_fact_document':pin(docs/'FACTS.json'),'fact_table':pin(docs/'FACT-TABLE.md'),
    'joined_report_input_pins_exact':144,'inventory_rows_preserved':125,'joined_rows':138,
    'instruction_window_checks':1603,'distinct_instruction_windows':1560,
    'timing_saved_input_snapshots_authenticated':7,'timing_current_whole_file_pins_exact':6,
    'timing_append_only_provenance_addendum':timing_addendum,
    'source_delta':pin(p/'parent-review/SOURCE-DELTA.json'),
    'report_scope':'Current source/static/host/build claims verified. Historical timing remains saved-log analysis with clock-epoch uncertainty; no speed change. Saved mod/settings copies are local provenance, not live console snapshots.',
    'findings':[], 'qualified_historical_input':'One appended saved log authenticated through its preserved original prefix.',
    'no_console_network_contact':True,'source_pipeline_artifact_mod_settings_mutations':0})
final_audit = save('FINAL-ARTIFACT-AUDIT.json', {
    'status':'FINAL_ARTIFACT_AUDIT_R3_PASS_LOCAL_ONLY','pass':True,'findings':[],
    'artifact_audit':pin(out/'ARTIFACT-AUDIT.json'),'post_artifact_report_audit':post,
    'source_review':pin(out/'REVIEW_FINAL.json'),'full_pipeline_receipt':pin(p/'FULL-PIPELINE-RECEIPT.json'),
    'build_report':pin(p/'BUILD-REPORT.md'),
    'final_prx':artifact['artifacts'][1]['final_prx'],
    'composition':artifact['artifacts'][1]['composed_runtime'],
    'two_artifact_chains_byte_equal':True,'selected_objects_byte_equal':369,'composition_input_count':386,
    'actual_dynamic_import_count':206,'typed_import_identity_count':206,'imports_unchanged_from_final_l18':True,
    'static_checks_per_build':20,'final_gate':'GO (offline)','first_candidate_gate':'D1 initially pending twin; run2 independently establishes determinism.',
    'strong_layout_provider':'Initializer strongU; fresh binding/composition/combined each exactly1strongT and zeroU.',
    'current_binding_helper_dependency_exact_both_builds':True,
    'frozen_source_count':75,'external_evidence_count':36,'current_and_historical_source_drift':artifact['source_drift'],
    'original_luamod_source_and_object_unchanged':True,
    'saved_mod_and_settings_copies':artifact['saved_local_mod_settings_copies'],
    'timing_provenance_addendum':timing_addendum,
    'remaining_unknown_native_facts':summary['unresolved_profile_facts'],
    'saved_layout_declarations':{'members':17,'slots':1,'missing_required_members':52,'missing_required_slots':6,'expanded':False},
    'console_network_contact':False,'upload':'HOLD','engine_menu_or_spawn_proved':False,
    'limits':'Local builds and offline evidence only. No live console/mod/settings snapshot queried. Four required native facts and incomplete declarations still refuse before engine calls; packaging does not establish a working menu or spawn.'})
print(json.dumps({'pass':True,'final_audit':final_audit,'post_report_audit':post,'findings':[]}))
