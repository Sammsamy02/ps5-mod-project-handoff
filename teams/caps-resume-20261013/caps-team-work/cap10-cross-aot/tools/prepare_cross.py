from pathlib import Path
import difflib,hashlib,json,os,shutil
H=Path(__file__).resolve().parents[1];W=H/'cross-work';T=H.parent/'cap10-port-closure/target-work'
# Own clone of stable host build; remove copied compilation products before rebuilding.
removed=[]
for root,dirs,files in os.walk(W,followlinks=False):
 for name in list(dirs):
  p=Path(root)/name
  if name in ('.libs','.deps') and not p.is_symlink():
   shutil.rmtree(p);dirs.remove(name);removed.append(str(p.relative_to(W)))
 for name in files:
  p=Path(root)/name
  if p.suffix in ('.o','.lo','.a','.la','.dylib','.so') or name=='mono-sgen':
   p.unlink();removed.append(str(p.relative_to(W)))
changes={
 'config.sub':('\t     | ekkobsd* | freebsd* | riscix* | lynxos* | os400* \\\n','\t     | ekkobsd* | freebsd* | ps5 | riscix* | lynxos* | os400* \\\n'),
 'configure.ac':('   x86_64-ps4-freebsd)\n','   x86_64-sie-ps5)\n\t\t# Public Prospero target: AMD64 SysV/ELF; host compiler stays Darwin.\n\t\tTARGET=AMD64;\n\t\ttarget_mach=no\n\t\ttarget_win32=no\n\t\t;;\n   x86_64-ps4-freebsd)\n')}
rows=[]
for rel,(old,new) in changes.items():
 p=W/rel;b=p.read_bytes();s=b.decode();assert s.count(old)==1,rel
 updated=s.replace(old,new);p.write_text(updated)
 (H/'patches'/f'{rel}.patch').write_text(''.join(difflib.unified_diff(s.splitlines(True),updated.splitlines(True),'a/'+rel,'b/'+rel)))
 rows.append({'path':rel,'original_sha256':hashlib.sha256(b).hexdigest(),'new_sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
for rel in ('config.h','mono/eglib/eglib-config.h'):
 p=H/'target-config'/rel;p.parent.mkdir(parents=True,exist_ok=True);b=(T/rel).read_bytes();p.write_bytes(b)
 rows.append({'target_config':rel,'sha256':hashlib.sha256(b).hexdigest()})
(H/'evidence/PREPARATION.json').write_text(json.dumps({'source_commit':'0cbf0e290c31adb476f9de0fa44b1d8829affa40','clone_source':str(H.parent/'cap10-mono-build/runtime-work'),'removed_compilation_product_count':len(removed),'changes':rows,'target_macro_injection':False},indent=2)+'\n')
print('prepared owned clone;',len(removed),'copied compilation products removed')
