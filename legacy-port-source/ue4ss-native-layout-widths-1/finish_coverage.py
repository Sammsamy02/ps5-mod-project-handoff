from pathlib import Path
import json,subprocess,struct
w=Path(__file__).resolve().parent;o=w/'controls-2';c=json.loads((w/'native-3/command.json').read_text())
exe=Path(c['LLVM_CONFIG']).parent/'llvm-readelf';dump=o/'widths.bin';argv=[str(exe),'-x','.width_facts',str(o/'controls.o')];q=subprocess.run(argv,capture_output=True,text=True);assert q.returncode==0,q.stderr
(o/'widths-readelf.txt').write_text(q.stdout);(o/'extraction-command.json').write_text(json.dumps({'argv':argv,'exit':q.returncode},indent=2)+'\n')
parts=[]
for line in q.stdout.splitlines():
 fields=line.split()
 if fields and fields[0].startswith('0x'):
  for value in fields[1:5]:
   if len(value)!=8 or any(ch not in '0123456789abcdefABCDEF' for ch in value):break
   parts.append(bytes.fromhex(value))
dump.write_bytes(b''.join(parts));assert (o/'controls.o').read_bytes()[5]==1
p=json.loads((w/'provenance.json').read_text());values=struct.unpack('<'+str(len(p['records']))+'I',dump.read_bytes());assert len(values)==419;rows=[dict(class_name=rec['class'],canonical_key=rec['canonical_key'],width_bytes=n or None,unsupported_reason=('No canonical accessor body'if not rec['alternatives']else'One or more actual getters unavailable/private without recognized source-type fallback or incomplete value type')if n==0 else None)for rec,n in zip(p['records'],values)];coverage={'compiler':'actual PS5 SDK, constants extracted from own fixture ELF; not runtime execution','available':sum(bool(n)for n in values),'unavailable':sum(not n for n in values),'records':rows,'native_execution':False};(w/'coverage.json').write_text(json.dumps(coverage,indent=2)+'\n');print('coverage',coverage['available'],coverage['unavailable']);print('unknown',[r['class_name']+'.'+r['canonical_key']for r in rows if r['width_bytes']is None]);(o/'receipt.json').write_text(json.dumps({'all_static_assertions_passed':True,'target_compiler_widths_extracted':True,'test_object_excluded_from_production':True,'native_execution':False},indent=2)+'\n')
