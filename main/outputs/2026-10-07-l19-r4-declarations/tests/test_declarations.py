#!/usr/bin/env python3
"""Behavior tests for the generic profile-only declaration producer."""
import copy, importlib.util, json, pathlib, unittest
ROOT=pathlib.Path(__file__).resolve().parents[3]
OWN=pathlib.Path(__file__).resolve().parents[1]
R3=ROOT/'outputs/2026-10-07-l19-local-profile-r3'
PROFILE=json.loads((R3/'profiles/townfall-build-facts.json').read_text())
SCHEMA=(ROOT/'.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc').read_text()
WIDTHS=json.loads((R3/'reports/ALL-REQUIRED-WIDTHS-GREEN.json').read_text())['observed_widths']

class Declarations(unittest.TestCase):
 def build(self,p=None,w=None,**kw):
  path=OWN/'generate_declarations.py'
  self.assertTrue(path.is_file(),'Missing production profile-derived declaration generator')
  spec=importlib.util.spec_from_file_location('declarations',path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  return m.build(copy.deepcopy(PROFILE) if p is None else p,SCHEMA,copy.deepcopy(WIDTHS) if w is None else w,**kw)
 def refuses(self,mutate,expected,*,widths=False):
  p,w=copy.deepcopy(PROFILE),copy.deepcopy(WIDTHS);mutate(w if widths else p)
  with self.assertRaisesRegex(ValueError,expected): self.build(p,w)
 def test_known_subset_and_four_unknowns(self):
  b=self.build();self.assertEqual(b['manifest']['known_members'],67);self.assertEqual(b['manifest']['known_vtables'],5)
  self.assertEqual(set(b['manifest']['unresolved']),{'FField::UEP_TotalSize','FFieldClass::UEP_TotalSize','UEngine::LoadMap','AGameModeBase::InitGameState'})
  self.assertFalse(b['manifest']['ready']);self.assertEqual(b['manifest']['blocked_member_classes'],['FField','FFieldClass'])
  self.assertIn('Interfaces=0x1d8',b['members']);self.assertIn('Tick=0x310',b['vtables'])
  self.assertNotIn('LoadMap=',b['vtables']);self.assertNotIn('InitGameState=',b['vtables'])
  self.assertNotIn('[FField]',b['stageable_members']);self.assertNotIn('[FFieldClass]',b['stageable_members'])
 def test_require_complete_refuses(self):
  with self.assertRaisesRegex(ValueError,'Incomplete authenticated profile'): self.build(require_complete=True)
 def test_deterministic(self): self.assertEqual(self.build(),self.build())
 def test_unsupported_schema(self): self.refuses(lambda p:p.update(schema=2),'Unsupported profile schema')
 def test_unknown_axis(self): self.refuses(lambda p:p['axes'].update(stats=0),'Independent resolved axes')
 def test_unknown_build_axis(self): self.refuses(lambda p:p['axes'].update(build_configuration=0),'Independent resolved axes')
 def test_bool_axis(self): self.refuses(lambda p:p['axes'].update(stats=True),'Independent resolved axes')
 def test_unsupported_member(self): self.refuses(lambda p:p['required_members'][0].update(name='FUObjectArray::UnprovedSlot'),'Unsupported member')
 def test_duplicate_alias_conflict(self):
  def mutate(p): p['required_members'].append(dict(name='UObjectBase::InternalIndex',offset=16,width=4,source='test'))
  self.refuses(mutate,'Duplicate canonical member')
 def test_known_offset_has_source(self): self.refuses(lambda p:p['required_members'][0].update(source=''),'Known fact requires source')
 def test_unknown_cannot_claim_source(self):
  def mutate(p): next(x for x in p['required_members'] if x['offset'] is None).update(source='claimed')
  self.refuses(mutate,'Unresolved fact cannot claim source')
 def test_bool_offset(self): self.refuses(lambda p:p['required_members'][0].update(offset=False),'Integer offset')
 def test_invalid_width(self): self.refuses(lambda p:p['required_members'][0].update(width=False),'Positive native width')
 def test_missing_compiled_width(self): self.refuses(lambda w:w.pop('UFunction::Func'),'Missing compiled accessor width',widths=True)
 def test_narrow_compiled_width(self): self.refuses(lambda w:w.update({'FField::Owner':7}),'Compiled footprint is narrower',widths=True)
 def test_exact_func_geometry(self):
  def mutate(p): next(x for x in p['required_members'] if x['name']=='UFunction::Func').update(offset=0xd9)
  self.refuses(mutate,'Outside measured class size: UFunction::Func')
 def test_virtual_alignment(self): self.refuses(lambda p:p['required_vtables'][0].update(offset=617),'Aligned virtual byte offset')
 def test_unknown_virtual_class(self): self.refuses(lambda p:p['required_vtables'][0].update(name='NotAClass::ProcessEvent'),'Unsupported virtual class')
 def test_unknown_source_type(self):
  def mutate(p): next(x for x in p['required_members'] if x['offset'] is None).update(source=False)
  self.refuses(mutate,'String fact source required')
 def test_axes_independently_serialized(self):
  p=copy.deepcopy(PROFILE);p['axes'].update(build_configuration=1,stats=2)
  self.assertIn('NonShipping=0\nStats=1\n',self.build(p)['members'])
  p['axes'].update(build_configuration=2,stats=1)
  self.assertIn('NonShipping=1\nStats=0\n',self.build(p)['members'])
 def test_metadata_injection(self): self.refuses(lambda p:p['layout'].update(build_id='bad\n[UClass]'),'ASCII metadata token')

if __name__=='__main__': unittest.main(verbosity=1)
