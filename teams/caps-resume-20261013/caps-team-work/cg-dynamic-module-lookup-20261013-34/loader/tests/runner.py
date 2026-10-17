#!/usr/bin/env python3
"""Bounded Mac evidence runner. No retries; exit codes and input integrity gate PASS.
Invokes only explicitly configured argv, never shell strings. Does not contact PS5.
"""
import argparse,hashlib,json,os,pathlib,re,signal,subprocess,time

def sha(path):
 return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None

def execute(argv,timeout,cwd):
 if not isinstance(argv,list) or not argv or not all(isinstance(a,str) for a in argv):
  return {'exit':None,'stdout':'','error':'invalid argv','timeout':False}
 if not isinstance(timeout,(int,float)) or not 0<timeout<=60:
  return {'exit':None,'stdout':'','error':'timeout must be within 0..60 seconds','timeout':False}
 try:
  p=subprocess.Popen(argv,cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,start_new_session=True)
 except (OSError,ValueError) as e:
  return {'exit':None,'stdout':'','error':str(e),'timeout':False}
 try:
  output,_=p.communicate(timeout=timeout)
  return {'exit':p.returncode,'stdout':output,'timeout':False}
 except subprocess.TimeoutExpired as e:
  # Only terminate the process group this call created. Never global process kills.
  try:os.killpg(p.pid,signal.SIGKILL)
  except ProcessLookupError:pass
  try:
   output,_=p.communicate(timeout=2)
   return {'exit':p.returncode,'stdout':output,'timeout':True}
  except subprocess.TimeoutExpired:
   # A Rosetta uninterruptible process must be reported, never waited on forever.
   return {'exit':None,'stdout':str(e.output or ''),'timeout':True,'unreaped_pid':p.pid}

def run_case(case,outdir):
 name=case.get('id','')
 if not isinstance(name,str) or not re.fullmatch(r'[A-Za-z0-9_-]+',name):
  return {'id':name,'status':'INVALID','reason':'case id must be filesystem safe'}
 minimum=case.get('min_checks',1)
 if not isinstance(minimum,int) or minimum<1:
  return {'id':name,'status':'INVALID','reason':'min_checks must be positive'}
 required=case.get('required_text',[])
 if not isinstance(required,list) or not all(isinstance(s,str) and s for s in required):
  return {'id':name,'status':'INVALID','reason':'required_text must be a list of nonempty strings'}
 source=pathlib.Path(case['input']) if case.get('input') else None
 before=sha(source) if source else None
 if source and before is None:return {'id':name,'status':'MISSING','reason':str(source)}
 expected=case.get('sha256')
 if expected and before!=expected:return {'id':name,'status':'FAIL','reason':'input does not match pinned SHA256','sha_before':before}
 result=execute(case.get('argv'),case.get('timeout',20),case.get('cwd'))
 after=sha(source) if source else None
 text=result['stdout']
 # Protocol markers anywhere other than column zero are malformed, not
 # evidence that may be ignored while a different CHECK supplies PASS.
 stray_protocol=any(('CHECK:' in line and not line.startswith('CHECK:')) or
                    ('RESULT:' in line and not line.startswith('RESULT:'))
                    for line in text.splitlines())
 check_lines=re.findall(r'^CHECK:.*$',text,re.M)
 # The e2e host emits optional " | detail" after the terminal verdict.
 # Every CHECK still has to parse; an unrecognized FAIL: line cannot vanish.
 parsed=[re.fullmatch(r'CHECK: (?P<name>.+?)\s+(?P<status>PASS|FAIL)(?: \| (?P<detail>.*))?',line)
         for line in check_lines]
 protocol_ok=not stray_protocol and bool(check_lines) and all(parsed) and not any(
  re.search(r'\bFAIL\b',line) for line,m in zip(check_lines,parsed) if m and m.group('status')=='PASS')
 checks=sum(bool(m and m.group('status')=='PASS') for m in parsed)
 fails=sum(bool(m and m.group('status')=='FAIL') for m in parsed)
 result_lines=re.findall(r'^RESULT:.*$',text,re.M)
 summaries=[re.fullmatch(r'RESULT: (PASS|FAIL) \((\d+) checks, (\d+) failed\)',line) for line in result_lines]
 summary_ok=(len(summaries)==1 and summaries[0] is not None and
             summaries[0].groups()==('PASS',str(checks+fails),'0'))
 # Required observations must belong to a parsed passing check name. A
 # diagnostic, header, or reason string cannot claim that behavior for it.
 passing_observations=['CHECK: '+m.group('name') for m in parsed
                       if m and m.group('status')=='PASS']
 missing_required=[s for s in required if not any(s in line for line in passing_observations)]
 passed=(result['exit']==0 and not result['timeout'] and protocol_ok and checks>=minimum and fails==0 and summary_ok and not missing_required and (not source or before==after))
 status='TIMEOUT' if result['timeout'] else ('PASS' if passed else 'FAIL')
 outdir.mkdir(parents=True,exist_ok=True)
 (outdir/(name+'.log')).write_text(text)
 return dict(id=name,status=status,checks=checks,failed_checks=fails,missing_required_text=missing_required,sha_before=before,sha_after=after,**{k:v for k,v in result.items() if k!='stdout'})

def run_suite(config,outdir):
 cases=config.get('cases',[])
 ids=[c.get('id') for c in cases if isinstance(c,dict)]
 if not cases or len(ids)!=len(cases) or len(set(str(x) for x in ids))!=len(ids):
  return {'passed':False,'reason':'nonempty cases with unique ids required','cases':[]}
 outdir.mkdir(parents=True,exist_ok=True)
 if config.get('build'):
  build=config['build'];r=execute(build.get('argv'),build.get('timeout',60),build.get('cwd'))
  (outdir/'build.log').write_text(r['stdout'])
  if r['exit']!=0 or r['timeout']:
   return {'passed':False,'reason':'build failed or timed out','build':{k:v for k,v in r.items() if k!='stdout'},'cases':[]}
 results=[]
 for case in cases:
  result=run_case(case,outdir);results.append(result)
  # One failed attempt is evidence, not an invitation to retry it unchanged.
  if result['status']!='PASS':break
 return {'passed':len(results)==len(cases) and all(r['status']=='PASS' for r in results),'scheduled':len(cases),'completed':len(results),'cases':results}

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('manifest',type=pathlib.Path);parser.add_argument('--out',required=True,type=pathlib.Path)
 args=parser.parse_args()
 # Distinct output revision prevents accidental overwriting of prior evidence.
 args.out.mkdir(parents=True,exist_ok=False)
 result=run_suite(json.loads(args.manifest.read_text()),args.out)
 (args.out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result,indent=2));raise SystemExit(0 if result['passed'] else 1)
