"""Additive #10 exact-input integration helpers. Importing this module does no I/O."""
import base64,copy,hashlib,json,re,shutil,struct,subprocess
from pathlib import Path

def pin(path):
 p=Path(path)
 return {'path':str(p.resolve()),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size}

def checked(row):
 p=Path(row['path'])
 if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=row['sha256']:
  raise ValueError('cap10 input missing/pin drift: '+str(p))
 return p

def nid(name):
 return base64.b64encode(hashlib.sha1(name.encode()+bytes.fromhex('518D64A635DED8C1E6B039B1C3E55230')).digest()[:8][::-1]).decode()[:11].replace('/','-')

def _decode_id(text):
 value=0
 for c in text:value=value*64+'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-'.index(c)
 return value

def provider_identity(name,provider):
 """Read exported NID, exact module/library/versions and symbol kind from a real saved provider ELF."""
 p=checked(provider);blob=p.read_bytes()
 if blob[:6]!=b'\x7fELF\x02\x01':raise ValueError('ELF64 little endian provider required')
 phoff=struct.unpack_from('<Q',blob,32)[0];entsize,count=struct.unpack_from('<HH',blob,54)
 if entsize!=56 or not count:raise ValueError('provider program headers absent')
 ph=[struct.unpack_from('<II6Q',blob,phoff+i*56) for i in range(count)]
 def offset(v):
  hits=[x[2]+v-x[3] for x in ph if x[0]==1 and x[3]<=v<x[3]+x[5]]
  if len(hits)!=1:raise ValueError('provider address mapping ambiguous/missing')
  return hits[0]
 dynamic=[x for x in ph if x[0]==2]
 if len(dynamic)!=1:raise ValueError('provider dynamic segment absent/ambiguous')
 x=dynamic[0];entries=[struct.unpack_from('<QQ',blob,i) for i in range(x[2],x[2]+x[5],16)]
 tags=dict(entries)
 strings=offset(tags[5]);string_size=tags[10]
 def string(i):
  if i>=string_size:raise ValueError('provider string out of bounds')
  end=blob.find(b'\0',strings+i,strings+string_size)
  if end<0:raise ValueError('unterminated provider string')
  return blob[strings+i:end].decode()
 modules={};libs={}
 for tag,value in entries:
  if tag==0x61000043:modules[value>>48]=(string(value&0xffffffff),(value>>32)&0xffff)
  if tag==0x61000047:libs[value>>48]=(string(value&0xffffffff),(value>>32)&0xffff)
 if tags.get(11)!=24:raise ValueError('provider symbol entry size')
 hash_offset=offset(tags[4]);symbols=struct.unpack_from('<II',blob,hash_offset)[1]
 symoff=offset(tags[6]);wanted=nid(name);hits=[]
 for i in range(symbols):
  sn,info,other,section,value,size=struct.unpack_from('<IBBHQQ',blob,symoff+i*24)
  if not section or info>>4 not in (1,2):continue
  text=string(sn);parts=text.split('#')
  if len(parts)!=3 or parts[0]!=wanted:continue
  lid,mid=map(_decode_id,parts[1:]);module=modules.get(mid);library=libs.get(lid)
  if not module or not library:raise ValueError('provider export missing module/library record: '+name)
  kind={1:'OBJECT',2:'FUNC'}.get(info&15)
  if not kind:raise ValueError('unsupported provider export kind: '+name)
  hits.append({'ImportSymbol':{'Name':name,'ModuleName':module[0],'ModuleVersion':module[1],
   'LibraryName':library[0],'LibraryVersion':library[1],'Soname':string(tags[14]),'PublishedName':name},
   'symbol_type':kind,'binding':'GLOBAL','evidence':[{'source':str(p),'sha256':provider['sha256'],
   'symbol_index':i,'encoded_name':text,'raw_type':info&15,'raw_binding':info>>4,
   'provider_elf_bytes_verified':True,'qualification':'Exact saved provider export identity; target resolution/execution unproved.'}]})
 if not hits:raise ValueError('provider does not export requested NID: '+name)
 # Distinct exported libraries are a real ambiguity: never guess a donor family.
 unique={json.dumps(x['ImportSymbol'],sort_keys=True):x for x in hits}
 family=provider.get('identity_family')
 if family:unique={k:v for k,v in unique.items() if all(v['ImportSymbol'][k]==value for k,value in family.items())}
 if len(unique)!=1:raise ValueError('ambiguous provider export identity: '+name)
 return next(iter(unique.values()))

def load(here):
 here=Path(here).resolve();manifest=here/'cap10/INTEGRATION-INPUTS.json';c=json.loads(manifest.read_text())
 def paths(value):
  if isinstance(value,list):return [paths(x) for x in value]
  if isinstance(value,dict):
   value={k:paths(v) for k,v in value.items()}
   if 'path' in value and 'sha256' in value:
    p=Path(value['path']);value['path']=str(p if p.is_absolute() else here/p)
   return value
  return value
 c=paths(c)
 for row in c['pinned_inputs']:checked(row)
 for name,spec in c['units'].items():
  spec['source']=checked(spec['source_pin']);spec['source_sha256']=spec['source_pin']['sha256']
 c['include_dirs']=[str(here/p) for p in c['include_dirs']]
 c['manifest']=pin(manifest)
 return c

def compile_argv(argv,name,c):
 if name not in set(c['units'])|{'program'}:return list(argv)
 
 for row in c.get('pinned_inputs',[]):checked(row)
 text=checked(c['prefix_source']).read_text();roots=re.findall(r'^#define\s+MODBRIDGE_MOD_ROOT\s+"([^"\n]+)"',text,re.M)
 if roots!=[c['root']]:raise ValueError('cap10 configured root differs from pinned active entry')
 args=['-DMODBRIDGE_CAP10_ADMITTED_MONO_PORT=1',f'-DMODBRIDGE_CAP10_CORLIB_DIR="{roots[0]}/lib"',f'-DMODBRIDGE_CAP10_CONFIG_DIR="{roots[0]}/etc"']
 if any('MODBRIDGE_CAP10_MONO_HOST_VALIDATION' in x for x in argv):raise ValueError('host validation define in target build')
 out=list(argv);out[1:1]=args+[a for p in c['include_dirs'] for a in ['-I',p]]
 return out

def wire_link(cmd,units,new_units,c):
 for row in c['artifacts']+c['base_fixed_inputs']:checked(row)
 out=list(map(str,cmd));by={u['name']:str(u['object']) for u in units}
 selected=[n for n in new_units if n not in c['units']]
 if any(out.count(by[n]) for n in new_units):raise ValueError('NEW_UNIT object already in inherited link')
 out[out.index('-o'):out.index('-o')]=[by[n] for n in selected]
 # Preserve BASE fixed inputs and their order. Inject the new owner roots
 # before the first inherited fixed archive, so Mono dependencies reach all
 # selected owned libc/SDK archives under the real ld.lld -r semantics.
 fixed=[r['path'] for r in c['base_fixed_inputs']]
 anchor=next((p for p in fixed if p.endswith('.a')),fixed[0])
 at=out.index(anchor);out[at:at]=[by[n] for n in c['units']]+[r['path'] for r in c['artifacts']]
 return out

def link_census(cmd,units,c,expected):
 output_index=cmd.index('-o')+1
 inputs=[a for i,a in enumerate(cmd) if i!=output_index and a.endswith(('.o','.a'))]
 if len(inputs)!=len(set(inputs)) or len(inputs)!=expected or not inputs:
  raise ValueError('cap10 link input census mismatch/duplicate/zero')
 if any(inputs.count(str(u['object']))!=1 for u in units):raise ValueError('cap10 unit missing from link census')
 fixed=[r['path'] for r in c['base_fixed_inputs']]
 if [x for x in inputs if x in fixed]!=fixed:raise ValueError('cap10 BASE fixed input ordering/count drift')
 ordered=[next(str(u['object']) for u in units if u['name']==name) for name in c['units']]+[r['path'] for r in c['artifacts']]
 if any(inputs.count(p)!=1 for p in ordered) or [inputs.index(p) for p in ordered]!=sorted(inputs.index(p) for p in ordered):
  raise ValueError('cap10 owner/boundary/corlib/Mono input order/count drift')
 return {'inputs':len(inputs),'expected':expected,'ordered_cap10_inputs':ordered,'base_fixed_inputs':fixed}

def nm_undefined(text):
 rows=set()
 for line in text.splitlines():
  x=line.split()
  if not x:continue
  if len(x)>=2 and x[-2] in ('U','w','v'):rows.add((x[-2],x[-1]))
  elif len(x)>=2 and x[1] in ('U','w','v'):rows.add((x[1],x[0]))
  else:raise ValueError('unrecognized raw import nm row: '+line)
 return rows

def verify_raw(text,c,lua_delta=None):
 expected={tuple(x) for x in c['raw_symbols']}
 if lua_delta:expected=(expected-{tuple(x) for x in lua_delta['removed']})|{tuple(x) for x in lua_delta['added']}
 actual=nm_undefined(text)
 if not expected or not actual or actual!=expected:
  raise ValueError('cap10 raw import contract mismatch: '+json.dumps({'extra':sorted(actual-expected),'missing':sorted(expected-actual)}))
 return {'count':len(actual),'strong':sorted(n for t,n in actual if t=='U'),'weak':sorted(n for t,n in actual if t in ('w','v'))}

def resolve_providers(names,c):
 providers=[(row,checked(row)) for row in c['providers']];rows=[];gaps=[]
 for name in sorted(names):
  candidates=[];errors=[]
  for row,p in providers:
   try:candidates.append(provider_identity(name,row))
   except ValueError as error:errors.append(str(error))
  admitted=[x for x in candidates if x['ImportSymbol']['Soname'] in c['sdk_exports'].get(name,[])]
  if len(admitted)==1:rows.append(admitted[0])
  else:gaps.append({'name':name,'sdk_declared_providers':c['sdk_exports'].get(name,[]),'reason':'exact saved provider identity absent/ambiguous','details':errors})
 return rows,gaps

def stage_assets(package,c):
 dest=Path(package)/'runtime-assets';receipt={'root':c['root'],'files':[],'console_contact':False,'deployment':False}
 for row in c['runtime_assets']:
  source=checked(row);target=dest/row['destination'];target.parent.mkdir(parents=True,exist_ok=True)
  if target.exists() and target.read_bytes()!=source.read_bytes():raise ValueError('runtime asset destination drift')
  shutil.copyfile(source,target);receipt['files'].append({'source':row,'packaged':pin(target),'console_destination':c['root']+'/'+row['destination']})
 return receipt

def package_manifest(manifest,bdir,variant,c,execute):
 m=copy.deepcopy(manifest)
 raw_file=Path(bdir)/'pass1/CAP10-RAW-CONTRACT.json'
 raw=json.loads(raw_file.read_text()) if raw_file.exists() else {'raw_symbols':c['raw_symbols']}
 if execute and not raw_file.exists():raise ValueError('actual runtime input contract missing')
 expected={tuple(x) for x in raw['raw_symbols']}
 if not execute and m.get('b7_lua_as_cpp_census_delta'):expected={x for x in expected if x[1] not in m['b7_lua_as_cpp_census_delta']['removed']}
 if execute:
  for passno in (1,2):
   comp=json.loads((Path(bdir)/f'pass{passno}/COMPOSITION-RECEIPT.json').read_text())
   current=json.loads(checked(comp['cap10_raw_contract']).read_text())
   if current['raw_symbols']!=raw['raw_symbols']:raise ValueError('runtime twin raw contracts differ')
   for row in current['ordered_inputs']:checked(row)
 m['runtime_strong_undefined']=sorted(n for t,n in expected if t=='U');m['runtime_weak_undefined']=sorted(n for t,n in expected if t in ('w','v'))
 m['cap10']={k:c[k] for k in ('manifest','runtime_assets','root','package_helper','package_controller','package_script','pinned_inputs','providers','sdk_exports','sdk_evidence','baseline_contract')}
 m['cap10']['raw_contract']=pin(raw_file) if raw_file.exists() else {'path':str(raw_file),'sha256':'PENDING-BUILD'}
 m['cap10']['final_import_contract']='computed from actual bootstrap + runtime objects before Packager; no guessed final count'
 # Retain the driver-derived package-local script/controller paths. Their
 # copied bytes come from the pinned cap10 sources in prepare_variant.
 m['SDK_captures'].update(c['sdk_captures'])
 m['pins']+=c['pinned_inputs']+[c['package_helper'],c['package_controller'],c['package_script'],c['manifest']]+list(c['sdk_captures'].values())
 if execute:m['pins'].append(m['cap10']['raw_contract'])
 return m

def require_package_contract(value):
 report=json.loads(checked(value).read_text()) if 'path' in value else value
 if report.get('gaps'):raise ValueError('cap10 package identity gate unresolved: '+', '.join(x['name'] for x in report['gaps']))
 if report['expected_final_count']<=0 or len(report['final_rows'])!=report['expected_final_count']:
  raise ValueError('cap10 final import contract count/zero mismatch')
 names=[x['ImportSymbol']['Name'] for x in report['final_rows']]
 if len(set(names))!=len(names):raise ValueError('cap10 duplicate final import identity')
 for row in report.get('additional_rows',[]):
  for ev in row['evidence']:checked({'path':ev['source'],'sha256':ev['sha256']})
 return report

def install_package(module,inputs):
 # Bind exact input metadata only. Final providers are computed after the
 # actual bootstrap objects exist, never from a standalone symbol census.
 c=inputs['cap10']
 for row in c['pinned_inputs']:checked(row)
 module.CAP10_INPUTS=c

def verify_constructor_inputs(packaging,runtime,array_census):
 records=list(packaging['bootstrap_units'].values())
 if not records:raise ValueError('zero constructor input records')
 rows=[]
 for record in records:
  row={'path':record['object'],'sha256':record['object_sha256']};p=checked(row)
  rows.append({'input':row,'arrays':array_census(p.read_bytes())})
 rows.append({'input':pin(runtime),'arrays':array_census(Path(runtime).read_bytes())})
 expected={name:sum(x['arrays'][name]['slots'] for x in rows) for name in ('.init_array','.fini_array')}
 actual=array_census(checked(packaging['combined_object']).read_bytes())
 if any(expected[name]<=0 or actual[name]['slots']!=expected[name] for name in expected):
  raise ValueError('constructor input sum/combined count drift')
 return {'status':'PASS_EXACT_INPUT_ARRAY_SUM','expected':expected,'inputs':rows,'combined':packaging['combined_object']}

def final_package_counts(ids,arrays,contract,constructor):
 rows=contract['final_rows'];names={x['ImportSymbol']['Name'] for x in rows}
 imports={x['name'] for x in ids['imports']}
 if not names or ids['count']!=len(names) or len(ids['imports'])!=len(names) or imports!=names or not ids['status'].startswith('PASS'):
  raise ValueError('cap10 final exact import count/set mismatch')
 kinds={x['ImportSymbol']['Name']:{'OBJECT':1,'FUNC':2}[x['symbol_type']] for x in rows}
 if any(x['type']!=kinds[x['name']] for x in ids['imports']):raise ValueError('cap10 final import kind differs from provider contract')
 if constructor['status']!='PASS_EXACT_INPUT_ARRAY_SUM' or not arrays['status'].startswith('PASS'):
  raise ValueError('cap10 constructor proof failed')
 if any(constructor['expected'][n]<=0 or arrays['combined'][n]['slots']!=constructor['expected'][n] for n in ('.init_array','.fini_array')):
  raise ValueError('cap10 constructor count mismatch/zero')
 return {'imports':len(names),'init':constructor['expected']['.init_array'],'fini':constructor['expected']['.fini_array']}

def _object_symbols(blob):
 if blob[:6]!=b'\x7fELF\x02\x01':raise ValueError('non ELF64 target link input')
 off=struct.unpack_from('<Q',blob,40)[0];entsize,count=struct.unpack_from('<HH',blob,58)
 if entsize!=64 or off+64>len(blob):raise ValueError('invalid input section table')
 if not count:count=struct.unpack_from('<Q',blob,off+32)[0]
 sections=[struct.unpack_from('<II4QII2Q',blob,off+i*64) for i in range(count)]
 definitions=set();strong=set();weak={}
 tables=[x for x in sections if x[1]==2]
 if len(tables)!=1:raise ValueError('exactly one target input symbol table required')
 table=tables[0];strings=sections[table[6]];start,end=strings[4],strings[4]+strings[5]
 if table[9]!=24:raise ValueError('input symbol entry size')
 for i in range(table[4],table[4]+table[5],24):
  sn,info,other,index,value,size=struct.unpack_from('<IBBHQQ',blob,i);bind=info>>4
  if bind not in (1,2) or not sn:continue
  stop=blob.find(b'\0',start+sn,end)
  if stop<0:raise ValueError('input symbol string out of bounds')
  name=blob[start+sn:stop].decode()
  if index:definitions.add(name)
  elif bind==1:strong.add(name)
  else:weak[name]='v' if info&15==1 else 'w'
 return definitions,strong,weak

def _archive_members(blob):
 if not blob.startswith(b'!<arch>\n'):raise ValueError('unsupported archive format (thin archives not admitted)')
 pos=8;longnames=b'';rows=[]
 while pos<len(blob):
  header=blob[pos:pos+60]
  if len(header)!=60 or header[58:60]!=b'`\n':raise ValueError('invalid archive member header')
  size=int(header[48:58]);name=header[:16].decode().strip();data=blob[pos+60:pos+60+size]
  if len(data)!=size:raise ValueError('truncated archive member')
  if name=='//':longnames=data
  elif name in ('/','/SYM64/','__.SYMDEF','__.SYMDEF SORTED'):pass
  else:
   if name.startswith('#1/'):
    length=int(name[3:]);name=data[:length].rstrip(b'\0').decode();data=data[length:]
   elif name.startswith('/'):
    at=int(name[1:]);stop=longnames.find(b'/\n',at)
    if stop<0:raise ValueError('archive long filename absent')
    name=longnames[at:stop].decode()
   else:name=name.removesuffix('/')
   rows.append((name,_object_symbols(data)))
  pos+=60+size+(size%2)
 return rows

def link_import_contract(cmd):
 """Predict ld.lld -r unresolved symbols from exact ELF/AR input bytes.

All ordinary objects are roots. Archive members are lazy roots selected to
resolve strong undefined names, to closure, in inherited archive/member order.
This supports the actual selected driver's plain -r form only; unknown linker
selection switches fail before linking. Actual llvm-nm must match the result.
"""
 argv=list(map(str,cmd));output=argv.index('-o')+1;inputs=[]
 for i,a in enumerate(argv):
  if i==0 or i==output or a in ('-r','-m','elf_x86_64','-o') or a.startswith('-Map='):continue
  if not a.endswith(('.o','.a')):raise ValueError('unadmitted linker input/selection flag: '+a)
  inputs.append(Path(a))
 if not inputs or len({p.resolve() for p in inputs})!=len(inputs):raise ValueError('zero/duplicate actual link inputs')
 definitions=set();strong=set();weak={};archives=[];selected=[]
 def include(table):
  d,s,w=table;definitions.update(d);strong.update(s);weak.update(w)
 for p in inputs:
  blob=p.read_bytes()
  if p.suffix=='.a':archives.extend((p,name,table) for name,table in _archive_members(blob))
  else:include(_object_symbols(blob))
 remaining=list(archives)
 while True:
  changed=False;next_members=[]
  for path,name,table in remaining:
   if table[0] & (strong-definitions):
    include(table);selected.append({'archive':str(path),'member':name});changed=True
   else:next_members.append((path,name,table))
  remaining=next_members
  if not changed:break
 symbols={('U',n) for n in strong-definitions}|{(t,n) for n,t in weak.items() if n not in strong and n not in definitions}
 if not symbols:raise ValueError('zero unresolved raw symbol contract')
 return {'qualification':'Computed from exact actual selected ELF/AR inputs before ld.lld -r; llvm-nm equality is required after link. No final provider identity claim.',
  'ordered_inputs':[pin(p) for p in inputs],'selected_archive_members':selected,'raw_symbols':[list(x) for x in sorted(symbols)]}


def package_input_contract(objects,runtime,c,boundaries,allowed_weak):
 predicted=link_import_contract(['ld.lld','-r',*map(str,objects),str(runtime),'-o','combined-contract-only.o'])
 raw={tuple(x) for x in predicted['raw_symbols']}
 weak={n for t,n in raw if t in ('w','v')}
 if weak-set(allowed_weak):raise ValueError('unknown combined weak imports: '+repr(sorted(weak-set(allowed_weak))))
 names={n for t,n in raw if t=='U'}-set(boundaries)
 parent=json.loads(checked(c['baseline_contract']).read_text());base_rows=parent['contract']['imports'];known={x['ImportSymbol']['Name'] for x in base_rows}
 missing=known-names
 if missing:raise ValueError('legacy final identities lost from actual inputs: '+repr(sorted(missing)))
 needed=sorted(names-known);rows,gaps=resolve_providers(needed,c)
 return {'qualification':'Computed from actual pinned packaging bootstrap/current runtime object definitions and linker boundaries before Packager. Exact saved provider rows only; no native resolution proof.',
  'actual_input_contract':predicted,'baseline_contract':c['baseline_contract'],'additional_rows':rows,'gaps':gaps,
  'expected_final_count':len(names),'final_rows':base_rows+rows,'new_strong_requirements':needed}

def prepare_package_inputs(state,b,objects,runtime):
 c=state['CAP10_INPUTS'];pinmap=state['PIN'];boundaries=set(pinmap['frozen_boundary'])|set(state['BOUNDARY_ADDITIONS'])
 report=package_input_contract(objects,runtime,c,boundaries,pinmap['frozen_allowed_weak'])
 target=b.out/'CAP10-PACKAGE-CONTRACT.json';target.write_text(json.dumps(report,indent=2)+'\n')
 require_package_contract(report)
 rows={x['ImportSymbol']['Name']:x for x in report['additional_rows']}
 state['CAP10_PROVIDER_ROWS'].update(rows)
 state['ALL_EXTENDED_IMPORTS'].update({name:name for name in rows})
 state['ALL_EXTENDED_IMPORT_EVIDENCE'].update({name:c['sdk_evidence'][name] for name in rows})
 pinmap['cap10_expected_final_count']=report['expected_final_count']
 if not rows:return None
 build=b.out/'build';source=build/'cap10_import_references.S';obj=build/'cap10_import_references.o'
 text=['.section .data.rel.ro.modbridge_cap10_imports,"aw",@progbits','.p2align 3']
 for name,row in rows.items():
  kind='object' if row['symbol_type']=='OBJECT' else 'function'
  text += [f'.globl {name}',f'.type {name},@{kind}',f'.quad {name}']
 source.write_text('\n'.join(text)+'\n')
 b.run([pinmap['sdk']/'bin/prospero-clang',*state['CLANG_FLAGS'],'-c',source,'-o',obj],env_extra={'LLVM_CONFIG':str(pinmap['llvm']/'llvm-config')})
 # First references carry the measured kinds; the legacy 16-slot getter
 # table/count/source remain byte-for-byte their original generated values.
 objects.insert(0,obj)
 return {'source':str(source),'source_sha256':pin(source)['sha256'],'object':str(obj),'object_sha256':pin(obj)['sha256']}
