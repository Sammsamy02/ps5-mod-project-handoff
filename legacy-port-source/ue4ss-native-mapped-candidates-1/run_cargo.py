from pathlib import Path
import subprocess,os,sys,json
r=Path(__file__).resolve().parent
t=Path(r.parent/'unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin')
label,mode,*args=sys.argv[1:]; e=os.environ.copy();e['CARGO_HOME']=str(r/'tools/cargo');e['RUSTC']=str(r/'tools/rustc-native' if mode=='native' else t/'bin/rustc');e['RUSTDOC']=str(t/'bin/rustdoc');e['PATH']=str(t/'bin')+':'+e['PATH'];e['RUSTC_BOOTSTRAP']='1'
cmd=[str(t/'bin/cargo')]+args;(r/(label+'-command.json')).write_text(json.dumps({'command':cmd,'RUSTC':e['RUSTC'],'CARGO_HOME':e['CARGO_HOME']},indent=2));p=subprocess.run(cmd,env=e,capture_output=True,text=True);(r/(label+'.log')).write_text(p.stdout+p.stderr);print(p.returncode,(p.stdout+p.stderr)[-5000:]);sys.exit(p.returncode)
