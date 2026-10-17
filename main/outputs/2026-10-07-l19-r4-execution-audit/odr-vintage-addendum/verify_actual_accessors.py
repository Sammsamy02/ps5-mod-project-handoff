"""Execute unchanged accessor verifier with only its output directory isolated."""
from pathlib import Path
import ast,hashlib,json
O=Path(__file__).resolve().parent;W=O.parents[2];P=W/'outputs/2026-10-07-l19-r4-full-build'
source=P/'launch19_one_pass.py';tree=ast.parse(source.read_text())
fn=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='verify_accessors')
out=O/'downstream';(out/'receipts').mkdir(parents=True,exist_ok=True)
alias=out/'actual-first-package'
if alias.exists():assert alias.is_symlink() and alias.resolve()==(P/'prx-candidate-l19-1').resolve()
else:alias.symlink_to(P/'prx-candidate-l19-1',target_is_directory=True)
namespace={'Path':Path,'json':json,'ROOT':W,'MY':out}
exec(compile(ast.Module(body=[fn],type_ignores=[]),str(source),'exec'),namespace)
namespace['verify_accessors'](alias.name)
receipt=out/'receipts/accessor-verification-actual-first-package.json';r=json.loads(receipt.read_text())
assert r['verdict']=='PASS' and not r['errors']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
control={'pass':True,'original_function_AST_unchanged':True,'source':{'path':str(source),'sha256':sha(source)},'receipt':{'path':str(receipt),'sha256':sha(receipt)},'output_isolation':'MY output directory changed only; read-only symlink resolves to exact actual candidate for unchanged containment/hash checks','read_only_tools':['llvm-nm'],'compile_link_package_or_restart':False}
(out/'ACCESSOR-CONTROL.json').write_text(json.dumps(control,indent=2)+'\n')
print(json.dumps({'pass':True,'accessor_receipt_sha256':sha(receipt),'control_sha256':sha(out/'ACCESSOR-CONTROL.json')}))
