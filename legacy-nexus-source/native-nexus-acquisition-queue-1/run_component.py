from pathlib import Path
import hashlib,json,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
OLD=ROOT/'.superpowers/native-dependency-spike/nexus-download-jobs-1'
HOST=ROOT/'outputs/ps5-mod-bridge/native-host'
PROFILE=ROOT/'.superpowers/native-dependency-spike/native-curl-profile-experiment-1'
CODECS=ROOT/'.superpowers/native-dependency-spike/native-libarchive-lzma-experiment-1'
MBED=CODECS.parent/'mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5'
OUT=HERE/sys.argv[1];OUT.mkdir(exist_ok=False)
common=['-Wall','-Wextra','-Werror','-g','-O1','-I',HOST,'-I',ROOT/'outputs/ps5-mod-bridge/include','-isystem',ROOT/'outputs/ps5-mod-bridge/third_party']
cdefs=['-DMB_ARCHIVE_BUILTIN_XZ=1','-DMBEDTLS_CONFIG_FILE="tls_config.h"','-I',CODECS.parent/'libarchive-release/libarchive','-I',PROFILE,'-I',MBED/'include','-I',MBED/'library','-include',MBED/'library/common.h','-include',PROFILE/'tls_effective.h']
paths=[HOST/'jobs.cpp',HOST/'jobs.h',HOST/'auth.cpp',HOST/'auth.h',HOST/'analysis.cpp',HOST/'analysis.h',HOST/'nexus_client.cpp',HOST/'nexus_bridge.cpp',HOST/'nexus_download.cpp',HOST/'archive/intake.c',HOST/'archive/groups.c',HOST/'archive/private/fd_io.c',HOST/'archive/private/file_sink.c',HOST/'archive/private/posix_compat.c',ROOT/'outputs/ps5-mod-bridge/src/nexus.cpp',HERE/'component_controls.cpp',OLD/'fixture.zip']
pre_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
records=[]
def run(label,args,env=None):
 result=subprocess.run([str(a) for a in args],cwd=ROOT,capture_output=True,text=True,timeout=60,env=env)
 records.append({'label':label,'command':[str(a) for a in args],'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
 (OUT/(label+'.stdout')).write_text(result.stdout);(OUT/(label+'.stderr')).write_text(result.stderr)
 if result.returncode:print(label,result.returncode,result.stderr[:2500]);raise RuntimeError(label)
 print(label,result.stdout.strip() if label=='run' else 'PASS')
try:
 for name in ['intake','groups']:
  run(name,['/usr/bin/clang','-std=c11',*common,*cdefs,'-c',HOST/('archive/'+name+'.c'),'-o',OUT/(name+'.o')])
 for name in ['posix_compat','fd_io','file_sink']:
  run(name,['/usr/bin/clang','-std=c11',*common,*cdefs,'-c',HOST/('archive/private/'+name+'.c'),'-o',OUT/(name+'.o')])
 run('auth',['/usr/bin/clang++','-std=c++20',*common,'-Dclock_gettime=auth_control_clock_gettime','-c',HOST/'auth.cpp','-o',OUT/'auth.o'])
 run('analysis',['/usr/bin/clang++','-std=c++20',*common,'-I',CODECS.parent/'libarchive-release/libarchive','-c',HOST/'analysis.cpp','-o',OUT/'analysis.o'])
 libs=[CODECS/'host/libarchive-build/.libs/libarchive.a',CODECS/'host/xz-build/src/liblzma/.libs/liblzma.a',CODECS.parent/'native-zip-experiment-1/host/zlib-build/libz.a',PROFILE/'host/libmbedcrypto.a']
 run('jobs',['/usr/bin/clang++','-std=c++20',*common,'-I',MBED/'include','-Dclock_gettime=auth_control_clock_gettime','-c',HOST/'jobs.cpp','-o',OUT/'jobs.o'])
 cpp=[HOST/(name+'.cpp') for name in ['nexus_client','nexus_bridge','nexus_download']]+[ROOT/'outputs/ps5-mod-bridge/src/nexus.cpp']
 run('link',['/usr/bin/clang++','-std=c++20',*common,'-Wno-unused-function','-I',MBED/'include',HERE/'component_controls.cpp',*cpp,*[OUT/(n+'.o') for n in ['jobs','auth','analysis','intake','groups','posix_compat','fd_io','file_sink']],*libs,'-o',OUT/'control'])
 environment=dict(os.environ)
 if '--queue-only' in sys.argv:environment['MB_JOBS_QUEUE_ONLY']='1'
 if '--new-only' in sys.argv:environment['MB_SKIP_LEGACY']='1'
 run('run',[OUT/'control',OLD/'fixture.zip'],environment)
 status='PASS'
except Exception as e:status='FAIL';print(str(e))
post_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
if pre_hashes!=post_hashes:status='FAIL';print('source closure changed during run')
receipt={'status':status,'scope':'focused ordinary-host component without sanitizer; actual auth acquisition planner/client/query/decoder/jobs and legacy resolver/sink/fd_io/archive; private lower token/provider/producer substitutes and compile-only clock control, no TLS/console','queue_only':'--queue-only' in sys.argv,'commands':records,'input_sha256':post_hashes,'precompile_sha256':pre_hashes,'source_closure_unchanged':pre_hashes==post_hashes,'artifact_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.iterdir() if p.suffix=='.o' or p.name=='control'}}
(OUT/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
raise SystemExit(0 if status=='PASS' else 1)
