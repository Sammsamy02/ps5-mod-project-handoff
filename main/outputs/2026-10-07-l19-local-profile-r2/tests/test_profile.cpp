#include "RuntimeBuildProfile.hpp"
#include <cassert>
#include <iostream>
using namespace rc::buildfacts;
rc::codecache::Metadata image(){rc::codecache::Metadata m{};m.ready=true;m.base=0x400000;m.size=194241424;m.extent=0xbc602c8;m.generation=1;m.deadline_ms=60000;std::strcpy(m.sha256,"3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc");return m;}
Declared declared(){return {1,8,5,6,false,false,"prospero-x86_64-sysv","5.6.1-0+++UE5","04bb4330a7d71dcefe1cc2c4d19769d5"};}
void expect_refusal(const Profile*p,const rc::codecache::Metadata*m,const Declared*d,Settings s,uint64_t base=0x400000){auto r=evaluate_profile(p,m,base,d,s,0);assert(!r.ok);assert(r.checks.size()>=20);}
int main(){auto p=rc::runtimeprofile::kProfile;auto m=image();auto d=declared();Settings s{0,0,5,6,false,8,8};
 auto good=evaluate_profile(&p,&m,0x400000,&d,s,0);assert(good.ok&&good.build_configuration==1&&good.stats==1);std::cout<<"PASS independent axes shipping/off\n";
 auto on=p;on.stats=2;auto don=d;don.stats=true;auto r=evaluate_profile(&on,&m,0x400000,&don,s,0);assert(r.ok&&r.build_configuration==1&&r.stats==2);std::cout<<"PASS shipping does not imply stats-off\n";
 expect_refusal(nullptr,&m,&d,s);expect_refusal(&p,nullptr,&d,s);expect_refusal(&p,&m,nullptr,s);
 auto badm=m;badm.sha256[0]='0';expect_refusal(&p,&badm,&d,s);badm=m;badm.size--;expect_refusal(&p,&badm,&d,s);badm=m;badm.ready=false;expect_refusal(&p,&badm,&d,s);badm=m;badm.generation=0;expect_refusal(&p,&badm,&d,s);expect_refusal(&p,&m,&d,s,0x500000);
 auto badp=p;badp.build_configuration=0;expect_refusal(&badp,&m,&d,s);badp=p;badp.stats=0;expect_refusal(&badp,&m,&d,s);badp=p;badp.fname_case_known=false;expect_refusal(&badp,&m,&d,s);badp=p;badp.engine_revision=nullptr;expect_refusal(&badp,&m,&d,s);badp=p;badp.build_source=nullptr;expect_refusal(&badp,&m,&d,s);badp=p;badp.stats_source=nullptr;expect_refusal(&badp,&m,&d,s);
 auto x=s;x.build_configuration=2;expect_refusal(&p,&m,&d,x);x=s;x.stats=2;expect_refusal(&p,&m,&d,x);x=s;x.build_configuration=99;expect_refusal(&p,&m,&d,x);x=s;x.major=4;expect_refusal(&p,&m,&d,x);x=s;x.minor=5;expect_refusal(&p,&m,&d,x);x=s;x.minor=0;expect_refusal(&p,&m,&d,x);
 x=s;x.major=0;x.minor=0;x.version_detect=true;assert(evaluate_profile(&p,&m,0x400000,&d,x,0).ok);x=s;x.fname_case_preserving=true;expect_refusal(&p,&m,&d,x);x=s;x.fname_bytes=12;expect_refusal(&p,&m,&d,x);
 auto bd=d;bd.non_shipping=true;expect_refusal(&p,&m,&bd,s);bd=d;bd.stats=true;expect_refusal(&p,&m,&bd,s);bd=d;bd.engine_revision="5.6.0";expect_refusal(&p,&m,&bd,s);bd=d;bd.build_id="other";expect_refusal(&p,&m,&bd,s);bd=d;bd.target="windows";expect_refusal(&p,&m,&bd,s);bd=d;bd.pointer_size=4;expect_refusal(&p,&m,&bd,s);bd=d;bd.major=4;expect_refusal(&p,&m,&bd,s);
 auto expired=evaluate_profile(&p,&m,0x400000,&d,s,60000);assert(!expired.ok);
 badm=m;badm.sha256[0]='0';bd=d;bd.stats=true;x=s;x.major=4;x.fname_case_preserving=true;
 auto all=evaluate_profile(&p,&badm,0x500000,&bd,x,60000);unsigned bad=0;for(const auto&c:all.checks)bad+=!c.ok;assert(bad>=6);std::cout<<"PASS collect-all independent failures="<<bad<<"\n";
 std::cout<<"PASS profile refusal matrix\n";
}
