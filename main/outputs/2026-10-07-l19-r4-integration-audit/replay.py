from pathlib import Path
import hashlib,json,os,subprocess,sys
root=Path.cwd();own=Path(__file__).resolve().parent;candidate=root/'outputs/2026-10-07-l19-local-profile-r4';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
r=json.loads((candidate/'reports/R4-INTEGRATION.json').read_text());rows=[]
env=dict(os.environ,ASAN_OPTIONS='abort_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
for c in r['commands']:
 if c['kind']!='matrix' and c.get('variant')!='digest-mismatch':continue
 argv=list(c['argv'])
 if c['kind']=='compile':argv[argv.index('-o')+1]=str(own/'digest-mismatch.o')
 run=subprocess.run(argv,cwd=root,env=env,text=True,capture_output=True,timeout=120)
 log=own/(c['variant']+'.log');log.write_text(run.stdout+run.stderr)
 if c['kind']=='matrix':assert run.returncode==0,(c['variant'],run.stderr)
 else:assert run.returncode!=0 and 'Compiled native declarations must match the selected runtime profile digest' in run.stderr,run.stderr
 rows.append({'variant':c['variant'],'kind':c['kind'],'argv':argv,'exit_code':run.returncode,'cases':c.get('cases',[]),'log_sha256':sha(log),'expected_behavior':True})
script=(candidate/'tests/r4_declaration_mutations.py').read_text()
old='O=Path(__file__).resolve().parents[1];W=O.parents[1]';assert script.count(old)==1
script=script.replace(old,'O=Path('+repr(str(candidate))+');W=O.parents[1];OWN=Path(__file__).resolve().parent')
script=script.replace("T=O/'tests/declaration-drift'","T=OWN/'declaration-drift'")
script=script.replace("(O/'reports/R4-DECLARATION-MUTATIONS.json').write_text","(OWN/'DECLARATION-MUTATIONS.json').write_text")
p=own/'declaration_mutations_replay.py';p.write_text(script)
run=subprocess.run([sys.executable,str(p)],cwd=root,text=True,capture_output=True);(own/'declaration-mutations.log').write_text(run.stdout+run.stderr);assert run.returncode==0,run.stderr
mutation=json.loads((own/'DECLARATION-MUTATIONS.json').read_text());assert mutation['pass'] and len(mutation['cases'])==20
result={'pass':True,'host_actual_integration_scenarios':sum(len(x['cases']) for x in rows),'commands':rows,'declaration_controls':20,'producer_integration_receipt_sha256':sha(candidate/'reports/R4-INTEGRATION.json'),'producer_declaration_test_sha256':sha(candidate/'tests/r4_declaration_mutations.py'),'replay_test_transformations':['Fix immutable candidate input directory','Redirect only drift/receipt writes to owned audit directory'],'replay_script_sha256':sha(__file__),'native_engine_calls':0}
(own/'REPLAY-RECEIPT.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'pass':True,'actual_scenarios':result['host_actual_integration_scenarios'],'declaration_controls':20,'digest_mismatch_compile':'REFUSED'}))
