from pathlib import Path
import json,os,subprocess,sys
ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what');HERE=Path(__file__).resolve().parent
source=Path(sys.argv[1]).resolve();out=HERE/sys.argv[2];out.mkdir(exist_ok=False)
s=source.read_text();start=s.index('int module_stop(');begin=s.index('{',start);level=1;end=begin+1
while level:
 if s[end]=='{':level+=1
 elif s[end]=='}':level-=1
 end+=1
prelude=(ROOT/'outputs/2026-10-08-runtime-stop-candidate/entry_residence.c').read_text().split('int module_stop(')[0]
prelude+='\nstatic int pad_residence,pad_checks;\nstatic int bridge_native_pad_code_residence_required(void){++pad_checks;return pad_residence;}\n'
main=r'''
int main(int argc,char**argv){
 if(argc!=2)return 99;
 runtime_active=strcmp(argv[1],"busy-inactive")!=0;
 int initial=runtime_active;
 pad_residence=!strcmp(argv[1],"unknown")?-1:!strcmp(argv[1],"success")?0:1;
 int r=module_stop(0,0);
 if(pad_residence) {
  if(!(r<0&&runtime_active==initial&&pad_checks==1&&stops==0&&finis==0&&inputs==0&&channels==0&&entry_guard==0)) {puts("FAIL pad residence must preserve owner and skip finalization");return 1;}
 }else if(!(r==0&&pad_checks==1&&stops==1&&finis==1&&inputs==1&&channels==1&&entry_guard==0)){puts("FAIL pad stop guard must precede normal owner/fini path");return 2;}
 puts("PASS actual entry pad guard; explicit SDK/owner seams");return 0;
}
'''
tu=out/'actual_entry.c';tu.write_text(prelude+s[start:end]+main);records=[]
for variant,flags in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all'])]:
 exe=out/variant;c=subprocess.run(['/usr/bin/clang','-std=c11',*flags,str(tu),'-o',str(exe)],capture_output=True,text=True);assert c.returncode==0,c.stderr
 for case in ['busy','busy-inactive','unknown','success']:
  p=subprocess.run([str(exe),case],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));records.append({'variant':variant,'case':case,'exit':p.returncode,'output':p.stdout+p.stderr});print(variant,case,p.returncode,p.stdout.strip())
(out/'RESULTS.json').write_text(json.dumps({'source':str(source),'records':records,'scope':'actual C entry; host owner/finalizer seams; no native unload proof'},indent=2)+'\n')
