from pathlib import Path
import importlib.util,json,copy
r=Path(__file__).resolve().parents[1];spec=importlib.util.spec_from_file_location('generator',r/'profiles/generate_profile.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
p=json.loads((r/'profiles/townfall-build-facts.json').read_text());m.validate_profile(p);cases={}
for name,edit in {
 'schema_bool':lambda p:p.__setitem__('schema',True),
 'unsupported_schema':lambda p:p.__setitem__('schema',2),
 'width_bool':lambda p:p['required_members'][0].__setitem__('width',False),
 'width_negative':lambda p:p['required_members'][0].__setitem__('width',-1),
 'width_string':lambda p:p['required_members'][0].__setitem__('width','8'),
 'source_nonstring':lambda p:p['required_members'][0].__setitem__('source',True),
 'axis_source_nonstring':lambda p:p['axes'].__setitem__('stats_source',True),
 'unknown_editor_flag':lambda p:p['layout'].__setitem__('with_editoronly_data',False),
 'unknown_top_level_flag':lambda p:p.__setitem__('editor',False),
 'missing_stats':lambda p:p['axes'].pop('stats'),
 'bool_stats':lambda p:p['axes'].__setitem__('stats',True),
 'negative_base':lambda p:p['image'].__setitem__('base',-1),
 'missing_pin_source':lambda p:p['required_members'][0].__setitem__('source',''),
 'unknown_stats':lambda p:p['axes'].__setitem__('stats',0),
 'missing_configuration':lambda p:p['axes'].pop('build_configuration'),
 'null_case_preserving':lambda p:p['fname'].__setitem__('case_preserving',None),
 'same_axis_source':lambda p:p['axes'].__setitem__('stats_source',p['axes']['build_source']),
 'missing_runtime_requirements':lambda p:p.__setitem__('required_members',[]),
}.items():
 q=copy.deepcopy(p);edit(q);ok=False
 try:m.validate_profile(q)
 except ValueError:ok=True
 assert ok,name;cases[name]='PASS refused malformed/incomplete profile'
print('PASS profile schema',len(cases),'refusal scenarios')
(r/'reports/PROFILE-SCHEMA-GREEN.json').write_text(json.dumps(cases,indent=2)+'\n')
