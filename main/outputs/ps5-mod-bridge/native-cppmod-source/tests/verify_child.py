#!/usr/bin/env python3
"""Run the production child's bounded lifecycle on owned host callbacks only."""
from pathlib import Path
import argparse,hashlib,json,subprocess,uuid

HERE=Path(__file__).resolve().parent
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    out=args.output.resolve()/uuid.uuid4().hex;out.mkdir(parents=True,exist_ok=False)
    records=[]
    def run(label,argv):
        argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True)
        (out/(label+'.stdout')).write_text(r.stdout);(out/(label+'.stderr')).write_text(r.stderr)
        records.append(dict(label=label,argv=argv,exit=r.returncode))
        (out/'receipt.json').write_text(json.dumps(dict(commands=records,target_execution=False),indent=2)+'\n')
        if r.returncode:raise SystemExit(label+' failed: '+r.stderr)
    aliases={'__init_array_start':'mb_fixture_init_start','__init_array_end':'mb_fixture_init_end',
             '__fini_array_start':'mb_fixture_fini_start','__fini_array_end':'mb_fixture_fini_end',
             '__eh_frame_start':'mb_fixture_eh_start','__eh_frame_end':'mb_fixture_eh_end',
             '__dso_handle':'mb_fixture_dso','__cxa_finalize':'mb_fixture_finalize',
             '__unw_add_dynamic_eh_frame_section':'mb_fixture_add','__unw_remove_dynamic_eh_frame_section':'mb_fixture_remove'}
    run('compile-child-control',['clang++','-std=c++20','-Wall','-Wextra','-Werror',
        *['-D'+a+'='+b for a,b in aliases.items()],HERE.parent/'child_runtime.cpp',HERE/'child_runtime_control.cpp','-o',out/'child-control'])
    for scenario in ['normal','null-array','throw-constructor','throw-finalizer']:
        run(scenario,[out/'child-control',scenario])
    sources={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [HERE.parent/'child_runtime.cpp',HERE.parent/'child_runtime.h',HERE/'child_runtime_control.cpp',Path(__file__)]}
    (out/'receipt.json').write_text(json.dumps(dict(status='PRODUCTION_CHILD_HOST_LIFECYCLE_PASSED',sources=sources,commands=records,
        scope='Production child TU; only own callback arrays/finalizer and unwind provider stand-ins. The native artifact uses the genuine plugin and host ABI.',target_execution=False),indent=2)+'\n')
    print(out/'receipt.json')
if __name__=='__main__':main()
