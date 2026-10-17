#!/usr/bin/env python3
"""Check concurrent imported calls while other module sessions are added/retired."""
import hashlib
import json
from pathlib import Path
import subprocess
import uuid
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    out=ROOT/'.superpowers/native-dependency-spike/service-session-concurrency-1'/uuid.uuid4().hex
    out.mkdir(parents=True)
    names=['mb_win_threads.c','mb_win_heap.c','mb_win_heap_bridge.c','mb_win_crt.c','mb_win_tls_bridge.c']
    sources=[HERE/n for n in names+['mb_win_threads.h','mb_win_heap.h','mb_win_heap_bridge.h','mb_win_crt.h','mb_win_crt_bind.h','mb_win_tls_bridge.h','tests/test_service_sessions.c']]+[Path(__file__).resolve(),HERE.parent/'windows-execution/pe_module.h',HERE.parent/'windows-image/pe_image.h']
    receipt={'status':'IN_PROGRESS','commands':[],'source_files':{str(p.relative_to(ROOT)):sha(p) for p in sources},'ps5_execution':False,'console_contact':False,'red_phase':'Before first-session-only stores, ThreadSanitizer reported TLS installed global write/read race in begin vs GetLastError; exit134'}
    def save(): (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    def run(label,cmd):
        cmd=list(map(str,cmd));r=subprocess.run(cmd,cwd=ROOT,text=True,capture_output=True,timeout=60)
        (out/(label+'.stdout')).write_text(r.stdout);(out/(label+'.stderr')).write_text(r.stderr)
        receipt['commands'].append({'label':label,'argv':cmd,'exit':r.returncode});save()
        if r.returncode: raise RuntimeError(label+' failed: '+str(out/(label+'.stderr')))
        return r.stdout.strip()
    try:
        for name,extra in [('normal',[]),('thread-sanitizer',['-g','-fsanitize=thread'])]:
            run(name+'-compile',['clang','-arch','x86_64','-std=c11','-Wall','-Wextra','-Werror','-O1','-ffreestanding','-fno-builtin','-pthread',*extra,*[HERE/n for n in names],HERE/'tests/test_service_sessions.c','-o',out/name])
            receipt[name]=run(name+'-execute',['/usr/bin/arch','-x86_64',out/name])
        for name,digest in receipt['source_files'].items():
            if sha(ROOT/name)!=digest: raise RuntimeError('Source changed: '+name)
        receipt['status']='CONCURRENT_SERVICE_SESSION_NORMAL_AND_TSAN_PASSED';save();print(json.dumps({'status':receipt['status'],'receipt':str(out/'receipt.json')}))
    except Exception as e:
        receipt.update(status='FAILED',error=str(e));save();raise
if __name__=='__main__': main()
