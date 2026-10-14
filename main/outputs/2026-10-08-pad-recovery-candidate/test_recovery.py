from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys

ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
HERE=Path(__file__).resolve().parent
source=Path(sys.argv[1]).resolve()
label=sys.argv[2]
out=HERE/label
out.mkdir(exist_ok=False)
old=(ROOT/'outputs/2026-10-08-pad-path-audit/selected_native_borrow_test.cpp').read_text()
prelude=old[:old.index('#include "/Users/')]
body=source.read_text()
body=body.replace('#include "PS5PadSource.hpp"\n','').replace('#include "PadServiceLease.hpp"',
    '#include "'+str(ROOT/'outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/port/PadServiceLease.hpp')+'"')
prelude=prelude.replace('"../../outputs/', '"'+str(ROOT)+'/outputs/')
tests=r'''
namespace {
std::atomic<int> get_calls{},close_calls{},last_user{-1},mode{},gate{},entered{};
void require(bool x,const char*w){if(!x)throw std::runtime_error(w);}
void wait_done(){for(int i=0;i<2000&&borrow_state.load()!=kBorrowDone;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));require(borrow_state.load()==kBorrowDone,"helper completes");}
void wait_entered(){for(int i=0;i<2000&&!entered.load();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));require(entered.load(),"helper entered");}
}
extern "C" int scePadInit(){return 0;}
extern "C" int scePadOpen(int,int,int,void*){return static_cast<int>(PS5_PAD_ERROR_ALREADY_OPENED);}
extern "C" int scePadGetHandle(int u,int,int){++get_calls;last_user.store(u);entered.store(1);while(mode.load()==3&&!gate.load())std::this_thread::yield();return mode.load()==1?-1:40+u;}
extern "C" int scePadReadState(int,PS5_PadData*){return 0;}
extern "C" int scePadClose(int){++close_calls;return 0;}
extern "C" int sceUserServiceInitialize(void*){return 0;}
extern "C" int sceUserServiceTerminate(){return 0;}
extern "C" int sceUserServiceGetLoginUserIdList(int*u){u[0]=1;return 0;}
extern "C" void diagnostic_event(const char*,unsigned int){}
int main(int argc,char**argv){try{
 require(argc==2,"case");const auto c=argv[1];mode.store(!std::strcmp(c,"negative-retry")?1:(!std::strcmp(c,"blocked-one-helper")||!std::strcmp(c,"stop-busy")||!std::strcmp(c,"concurrent-busy"))?3:0);
 require(open_or_borrow(1,0,0,nullptr)==kPadAlreadyOpened,"startup defers");require(get_calls.load()==0,"no startup borrow");
 require(open_or_borrow(1,0,0,nullptr)==kPadAlreadyOpened,"first retry defers");
 if(!std::strcmp(c,"blocked-one-helper")||!std::strcmp(c,"stop-busy")||!std::strcmp(c,"concurrent-busy")){
  wait_entered();for(int i=0;i<20;++i)require(open_or_borrow(2,0,0,nullptr)==kPadAlreadyOpened,"busy other user deferred");
  if(!std::strcmp(c,"concurrent-busy")) {
   std::atomic<int> bad{};std::vector<std::thread> callers;for(int i=0;i<8;++i)callers.emplace_back([&,i]{for(int n=0;n<200;++n)if(open_or_borrow(2+i,0,0,nullptr)!=kPadAlreadyOpened)++bad;});for(auto&t:callers)t.join();require(bad.load()==0,"concurrent callers remain deferred");
  }
#ifdef HAS_PAD_STOP_GUARD
  if(!std::strcmp(c,"stop-busy")){require(bridge_native_pad_code_residence_required()==1,"busy worker refuses unload");require(open_or_borrow(1,0,0,nullptr)<0,"stop rejects new starts");}
#endif
  require(get_calls.load()==1,"only one SDK call");gate.store(1);wait_done();
#ifndef HAS_PAD_STOP_GUARD
  if(!std::strcmp(c,"stop-busy"))require(false,"missing owned worker residence guard");
#endif
 }else{
  wait_done();
  if(!std::strcmp(c,"negative-retry")){
   mode.store(0);require(open_or_borrow(1,0,0,nullptr)==kPadAlreadyOpened,"negative retry asynchronous");wait_done();require(open_or_borrow(1,0,0,nullptr)==41,"negative result recovers");require(get_calls.load()==2,"exactly one second SDK call");
  }else if(!std::strcmp(c,"user-change")){
   require(open_or_borrow(2,0,0,nullptr)==kPadAlreadyOpened,"old user not returned");wait_done();require(open_or_borrow(2,0,0,nullptr)==42,"new user result");require(last_user.load()==2,"new user tuple reaches SDK");
  }else if(!std::strcmp(c,"close-reuse")){
   require(open_or_borrow(1,0,0,nullptr)==41,"first borrowed");require(close_owned(41)==0&&close_calls.load()==0,"borrow never closed");require(open_or_borrow(1,0,0,nullptr)==kPadAlreadyOpened,"retired result invalidated");wait_done();require(open_or_borrow(1,0,0,nullptr)==41,"fresh borrow");require(get_calls.load()==2,"revalidated retired tuple");
  }else{
   require(open_or_borrow(1,0,0,nullptr)==41,"borrowed handle");require(close_owned(41)==0&&close_calls.load()==0,"borrow not closed");require(close_owned(99)==0&&close_calls.load()==1,"owned handle closed");
  }
 }
#ifdef HAS_PAD_STOP_GUARD
 require(bridge_native_pad_code_residence_required()==0,"completed helper joins at stop");require(open_or_borrow(1,0,0,nullptr)<0,"stop closes helper admission");
#endif
 std::printf("PASS %s (actual helper; explicit host SDK seams)\n",c);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
'''
tu=out/'actual_helper_test.cpp';tu.write_text(prelude+body+tests)
compiler='/usr/bin/clang++'
flags=['-std=c++20','-pthread','-D__PROSPERO__','-DPAD_BORROW_DELAY_MS=0']
if 'bridge_native_pad_code_residence_required' in body:flags+=['-DHAS_PAD_STOP_GUARD']
cases=['positive-close','negative-retry','user-change','close-reuse','blocked-one-helper','stop-busy','concurrent-busy']
records=[]
variants=[('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]
if len(sys.argv)>3 and sys.argv[3]=='--tsan':variants.append(('tsan',['-fsanitize=thread','-fno-omit-frame-pointer']))
for variant,extra in variants:
 exe=out/('test-'+variant);argv=[compiler,*flags,*extra,str(tu),'-o',str(exe)]
 r=subprocess.run(argv,capture_output=True,text=True);(out/(variant+'.build.log')).write_text(r.stdout+r.stderr)
 assert r.returncode==0,r.stderr
 for case in cases:
  env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
  r=subprocess.run([str(exe),case],capture_output=True,text=True,env=env,timeout=15)
  records.append({'variant':variant,'case':case,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
  print(variant,case,r.returncode,r.stdout.strip() or r.stderr.splitlines()[0])
j={'source':str(source),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'test_sha256':hashlib.sha256(tu.read_bytes()).hexdigest(),'records':records,'console_proven':False}
(out/'RESULTS.json').write_text(json.dumps(j,indent=2)+'\n')
