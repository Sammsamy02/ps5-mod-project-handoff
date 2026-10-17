import ast,hashlib,importlib.util,json,sys,unittest
from pathlib import Path
D=Path(__file__).resolve().parents[1];sys.path.insert(0,str(D));import prep_guard,selection_checks_l19 as gate
class PipelineBindings(unittest.TestCase):
 def test_holds_before_source_freeze(self):
  with self.assertRaisesRegex(ValueError,"HOLD"):prep_guard.require_ready()
 def test_only_new_retained_unit(self):
  def names(path):
   tree=ast.parse(path.read_text())
   node=next(n for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=="RETAINED_REPLACEABLE" for t in n.targets))
   return ast.literal_eval(node.value)
  before=names(D.parent/'2026-10-07-l18-full-build/build_runtime_composition6_fixed_l18.py');after=names(D/'build_runtime_composition6_fixed_l19.py')
  self.assertEqual(after-before,{"native-layoutbinding"});self.assertFalse(before-after)
 def test_selected_header_rejects_identical_old_include(self):
  root=D/'tests/fixtures';root.mkdir(exist_ok=True);new=root/'selected';old=root/'old';new.mkdir(exist_ok=True);old.mkdir(exist_ok=True)
  a=new/'NativeLayoutBinding.hpp';b=old/'NativeLayoutBinding.hpp';a.write_text('selected-header\n');b.write_bytes(a.read_bytes())
  records={'NativeLayoutBinding.hpp':{'path':str(a.resolve()),'sha256':hashlib.sha256(a.read_bytes()).hexdigest()}}
  dep=root/'binding.d';dep.write_text('unit.o: '+str(b.resolve())+'\n')
  with self.assertRaisesRegex(ValueError,'prior-source'):gate.helper_dependencies(dep,records,{'NativeLayoutBinding.hpp'})
  dep.write_text('unit.o: '+str(a.resolve())+'\n');self.assertIn('NativeLayoutBinding.hpp',gate.helper_dependencies(dep,records,{'NativeLayoutBinding.hpp'}))
  a.write_text('drift\n')
  with self.assertRaisesRegex(ValueError,'hash drift'):gate.helper_dependencies(dep,records,{'NativeLayoutBinding.hpp'})
 def test_unchanged_private_tool_pins(self):
  tree=ast.parse((D/'build_runtime_composition6_l19.py').read_text());values={}
  for n in tree.body:
   if isinstance(n,ast.Assign) and len(n.targets)==1 and isinstance(n.targets[0],ast.Name) and n.targets[0].id in {'COMPOSE6_FIXED_SHA','PKG_FIXED_SHA'}:values[n.targets[0].id]=ast.literal_eval(n.value)
  for role,name in [('COMPOSE6_FIXED_SHA','build_runtime_composition6_fixed_l19.py'),('PKG_FIXED_SHA','package_prx_l19.py')]:self.assertEqual(values[role],hashlib.sha256((D/name).read_bytes()).hexdigest())
if __name__=='__main__':unittest.main(verbosity=2)
