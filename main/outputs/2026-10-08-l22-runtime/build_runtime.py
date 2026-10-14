#!/usr/bin/env python3
"""Two local incremental compiles with complete links and explicit header invalidation."""
import concurrent.futures,copy,hashlib,json,os,shlex,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
BASE=ROOT/'outputs/2026-10-08-menu-complete-build'
OUT=ROOT/'outputs/2026-10-08-l22-build-r3'
LLVM=ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):p=Path(p).resolve();return {'path':str(p),'sha256':sha(p),'size':p.stat().st_size}
def write(p,d):p.write_text(json.dumps(d,indent=2)+'\n')
def deps(p):return [Path(x).absolute() for x in shlex.split(Path(p).read_text().replace('\\\n',' '))[1:]]
def rewrite_compile(argv,original_source,source,obj,dep):
    argv=list(argv)
    # -c is a boolean flag; retained recipes put the filename either before
    # or after it. Replace the pinned filename, never the token following -c.
    assert argv.count(str(original_source))==1
    argv[argv.index(str(original_source))]=str(source)
    for flag,value in [('-o',obj),('-MF',dep)]:
        assert argv.count(flag)==1
        argv[argv.index(flag)+1]=str(value)
    assert argv.count('-c')==1
    return argv
def run(cmd,out,label,env):
    r=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=env,capture_output=True,text=True,timeout=600)
    (out/(label+'.stdout')).write_text(r.stdout);(out/(label+'.stderr')).write_text(r.stderr)
    write(out/(label+'.command.json'),{'argv':list(map(str,cmd)),'exit':r.returncode})
    if r.returncode:raise RuntimeError(label+' failed: '+str(out/(label+'.stderr')))
    return r.stdout

def main():
    OUT.mkdir(exist_ok=False)
    oldsel=json.loads((BASE/'SOURCE-SELECTION.json').read_text())
    approved_header_identities={(Path(p['path']).name,p['sha256']) for p in oldsel['source_pins'] if Path(p['path']).suffix in ['.h','.hpp','.inc','.inl']}
    for key in ('source_pins','receipt_pins','artifact_pins'):
        for p in oldsel[key]:assert sha(p['path'])==p['sha256'],p['path']
    changes=[];added=[];declared_include_dirs=[]
    for part in ['profile','abi','diagnostics']:
        d=json.loads((HERE/part/'CHANGES.json').read_text());changes.extend(d['files']);added.extend(d.get('added_headers',[]));declared_include_dirs.extend(d.get('include_directories',[]))
    oldpad=ROOT/'outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/port/PS5PadSource.cpp'
    pad=HERE/'candidate/PS5PadSource.cpp'
    changes.append({'original':str(oldpad),'candidate':str(pad),'original_sha256':sha(oldpad),'candidate_sha256':sha(pad)})
    replacements={};header_specs=[]
    for row in changes:
        original=Path(row['original']).resolve();candidate=Path(row['candidate']).resolve()
        assert sha(original)==row['original_sha256'];assert sha(candidate)==row['candidate_sha256']
        assert candidate.is_relative_to(HERE)
        assert str(original) not in replacements or replacements[str(original)]==str(candidate)
        replacements[str(original)]=str(candidate)
        if original.suffix in ['.hpp','.h','.inc','.inl']:header_specs.append((original.name,row['original_sha256'],str(candidate)))
    base=json.loads((BASE/'pass1/COMPOSITION-RECEIPT.json').read_text())
    universe=set();unitdeps={}
    for u in base['selected_units']:
        assert sha(u['dependency'])==u['dependency_sha256'],u['name']
        ds=deps(u['dependency']);unitdeps[u['name']]=ds;universe.update(ds)
    # Map every emitted alias with identical original contents, including frozen
    # include overlays; no historical header or source is edited in place.
    aliases={}
    for p in universe:
        for name,digest,candidate in header_specs:
            if p.name==name and sha(p)==digest:
                aliases[str(p)]=candidate;aliases[str(p.resolve())]=candidate
    for original,candidate in replacements.items():
        if Path(original).suffix in ['.hpp','.h','.inc','.inl']:aliases[original]=candidate
    overlay=OUT/'headers.vfs.json'
    write(overlay,{'version':0,'roots':[{'type':'file','name':k,'external-contents':v} for k,v in sorted(aliases.items())]})
    affected=[]
    for u in base['selected_units']:
        if str(Path(u['source']).resolve()) in replacements or any(str(p) in aliases or str(p.resolve()) in aliases for p in unitdeps[u['name']]):affected.append(u['name'])
    write(OUT/'CHANGE-SELECTION.json',{'files':changes,'added_headers':added,'aliases':aliases,'affected_units':affected,'retained_units':len(base['selected_units'])-len(affected),'no_console_contact':True})
    print(json.dumps({'affected_units':len(affected),'retained_units':374-len(affected),'header_aliases':len(aliases)}),flush=True)
    env=dict(os.environ,LLVM_CONFIG=str(LLVM/'llvm-config'),CCACHE_DISABLE='1',PYTHONDONTWRITEBYTECODE='1')
    include_dirs=sorted(set(declared_include_dirs)|{str(Path(p).parent) for p in added}|{str(Path(x['candidate']).parent) for x in changes if Path(x['candidate']).suffix in ['.hpp','.h']})
    results=[];actual_pins={}
    for passno in [1,2]:
        parent=json.loads((BASE/f'pass{passno}/COMPOSITION-RECEIPT.json').read_text())
        units=copy.deepcopy(parent['selected_units']);out=OUT/f'pass{passno}';out.mkdir();objects=out/'objects';objects.mkdir()
        def compile_unit(u):
            assert sha(u['source'])==u['source_sha256'];assert sha(u['object'])==u['object_sha256']
            if u['name'] not in affected:return u,None
            new=copy.deepcopy(u);source=Path(replacements.get(str(Path(u['source']).resolve()),u['source']))
            obj=objects/(u['name']+'.o');dep=objects/(u['name']+'.d');argv=rewrite_compile(u['argv'],u['source'],source,obj,dep)
            argv[1:1]=['-ivfsoverlay',str(overlay),'-I',str(Path(u['source']).parent),*[a for p in include_dirs for a in ['-I',p]]]
            run(argv,objects,u['name'],env)
            ds=deps(dep);oldset={str(p.resolve()) for p in unitdeps[u['name']]}
            # An already pinned identical header may become a new dependency
            # through a different frozen include alias. Record its actual path;
            # never admit an unknown file solely because it is under the repo.
            extra=[str(p) for p in ds if str(p.resolve()) not in oldset and not p.resolve().is_relative_to(HERE) and (p.name,sha(p)) not in approved_header_identities]
            if extra:raise RuntimeError('unapproved include graph additions '+u['name']+': '+repr(extra))
            new.update(source=str(source),source_sha256=sha(source),object=str(obj),object_sha256=sha(obj),dependency=str(dep),dependency_sha256=sha(dep),argv=argv,selection='l22_fresh_selected_source_or_header_dependent',LLVM_CONFIG=str(LLVM/'llvm-config'))
            return new,ds
        done={};count=0
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            futures=[pool.submit(compile_unit,u) for u in units]
            for f in concurrent.futures.as_completed(futures):
                try:u,ds=f.result()
                except Exception:
                    for pending in futures:pending.cancel()
                    raise
                done[u['name']]=u
                if ds is not None:
                    count+=1
                    for p in ds:actual_pins[str(p.resolve())]=pin(p)
                    if count%25==0:print(json.dumps({'pass':passno,'compiled':count,'total':len(affected)}),flush=True)
        units=[done[u['name']] for u in units]
        link=json.loads(Path(parent['link_command']['path']).read_text());old_to_new={a['object']:b['object'] for a,b in zip(parent['selected_units'],units)}
        cmd=[old_to_new.get(a,a) for a in link['argv']];runtime=out/'runtime-l22.o';cmd[cmd.index('-o')+1]=str(runtime)
        cmd=['-Map='+str(out/'runtime.map') if a.startswith('-Map=') else a for a in cmd]
        input_paths=[a for i,a in enumerate(cmd) if a.endswith(('.o','.a')) and i!=cmd.index('-o')+1]
        assert len(input_paths)==len(set(input_paths))==391
        for u in units:assert input_paths.count(u['object'])==1
        for p in link['fixed_inputs']:assert sha(p['path'])==p['sha256']
        run(cmd,out,'link',env)
        after=run([LLVM/'llvm-nm','-u',runtime],out,'undefined',env)
        before=(BASE/f'pass{passno}/undefined.stdout').read_text()
        symbols=lambda s:{tuple(line.split()[-2:]) for line in s.splitlines() if line.split()}
        assert symbols(after)==symbols(before),'raw import delta must be declared before packaging'
        write(out/'LINK-COMMAND.json',{'argv':cmd,'ordered_inputs':input_paths,'fixed_inputs':link['fixed_inputs']})
        write(out/'COMPILE-RECEIPT.json',{'fresh_units':[{'name':u['name'],'command':pin(objects/(u['name']+'.command.json')),'object':pin(u['object'])} for u in units if u['name'] in affected],'retained_units':374-len(affected),'header_overlay':pin(overlay)})
        write(out/'DEPENDENCY-GRAPH.json',{'selected_dependencies':[{'name':u['name'],'dependency':pin(u['dependency'])} for u in units],'new_effective_dependency_pins':list(actual_pins.values()),'header_aliases':aliases,'added_headers':[pin(p) for p in added]})
        write(out/'IMPORT-DELTA.json',{'baseline':sorted(symbols(before)),'current':sorted(symbols(after)),'added':[],'removed':[],'baseline_artifact':pin(BASE/f'pass{passno}/undefined.stdout'),'qualification':'Raw runtime symbols; final PRX import identities checked during packaging.'})
        receipt={'status':'MAC_ONLY_L22_COMPLETE_LINK','pass':passno,'object':pin(runtime),'selected_units':units,'selected_count':374,'full_link_inputs':391,'parent_composition':pin(BASE/f'pass{passno}/COMPOSITION-RECEIPT.json'),'fresh_current_pass':len(affected),'retained_current_units':374-len(affected),'scope':'l22 four approved local changes; no console proof','link_command':pin(out/'LINK-COMMAND.json'),'header_overlay':pin(overlay),'compile_receipt':pin(out/'COMPILE-RECEIPT.json'),'dependency_graph':pin(out/'DEPENDENCY-GRAPH.json'),'import_delta':pin(out/'IMPORT-DELTA.json'),'baseline_undefined_count':len(symbols(before)),'candidate_undefined_count':len(symbols(after)),'actual_undefined_added':[],'actual_undefined_removed':[],'native_execution':False,'menu_success':False,'console_contact':False}
        write(out/'COMPOSITION-RECEIPT.json',receipt);results.append({'runtime':pin(runtime),'composition':pin(out/'COMPOSITION-RECEIPT.json')})
        print(json.dumps({'pass':passno,'runtime':pin(runtime),'imports_changed':False}),flush=True)
    assert Path(results[0]['runtime']['path']).read_bytes()==Path(results[1]['runtime']['path']).read_bytes(),'runtime twins differ'
    a=json.loads(Path(results[0]['composition']['path']).read_text());b=json.loads(Path(results[1]['composition']['path']).read_text())
    for x,y in zip(a['selected_units'],b['selected_units']):assert x['object_sha256']==y['object_sha256'],x['name']
    selection=copy.deepcopy(oldsel);selection['source_pins']+=list(actual_pins.values())+[pin(x['candidate']) for x in changes]+[pin(p) for p in added]+[pin(overlay)]
    selection['receipt_pins'] += [x['composition'] for x in results]
    selection['artifact_pins'] += [x['runtime'] for x in results]
    selection.update(parent_selection=pin(BASE/'SOURCE-SELECTION.json'),native_execution=False,selected_changes=changes)
    write(OUT/'ACTUAL-DEPENDENCIES.json',list(actual_pins.values()));write(OUT/'SOURCE-SELECTION.json',selection)
    write(OUT/'TWIN-RECEIPT.json',{'status':'MAC_ONLY_IDENTICAL_L22_RUNTIME_PAIR','passes':results,'source_selection':pin(OUT/'SOURCE-SELECTION.json'),'fresh_units_each_pass':len(affected),'selected_units':374,'full_link_inputs':391,'new_imports':[],'console_contact':False,'native_execution':False})
    print('PASS l22 runtime twins identical; imports unchanged',flush=True)
if __name__=='__main__':main()
