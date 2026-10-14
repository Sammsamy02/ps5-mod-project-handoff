#include "L23GameThreadProbe.hpp"
#include <cassert>
#include <thread>
#include <vector>
using namespace RC::L23GameThreadProbe;
int main(){
 int owner; State a;assert(a.begin(&owner,100,7));assert(!a.begin(&owner,100,7));assert(!a.timeout(99999));a.queued();assert(!a.timeout(30099));assert(a.timeout(30100));assert(!a.timeout(30101));auto late=a.ran(30101,9,true);assert(late.accepted&&late.late&&late.elapsed==30001&&late.thread==9);assert(!a.ran(30102,9,true).accepted);
 State b;assert(b.begin(&owner,0,1));b.queued();assert(!b.ran(10,2,false).accepted);assert(b.status()==Status::WrongThread);assert(!b.timeout(99999));
 State c;assert(c.begin(&owner,0,1));c.failed();assert(!c.ran(1,1,true).accepted);assert(!c.timeout(99999));
 State d;assert(d.begin(&owner,0,1));d.queued();int other;assert(!d.cancel(&other));assert(d.cancel(&owner));assert(!d.ran(1,1,true).accepted);
 State e;assert(e.begin(&owner,0,1));e.queued();assert(!e.ran(1,-1,true).accepted);
 State f;assert(f.begin(&owner,100,1));f.queued();assert(!f.timeout(99));auto r=f.ran(200,2,true);assert(r.accepted&&!r.late&&r.elapsed==100&&r.thread==2);
 for(int round=0;round<100;++round){State s;assert(s.begin(&owner,0,1));s.queued();std::thread x([&]{s.timeout(30001);});std::thread y([&]{s.ran(30001,2,true);});std::thread z([&]{s.cancel(&owner);});x.join();y.join();z.join();assert(s.status()==Status::Ran||s.status()==Status::Cancelled);}
}
