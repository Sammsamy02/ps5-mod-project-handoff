#include "PS5PadSource.hpp"
#include <cstdio>
#include <string>
#include <utility>
#include <vector>
using namespace RC::Input;
namespace RC::Input { bool Handler::get_allow_input() { return true; } }
static PS5_PadData sample{};
static std::vector<std::pair<std::string,unsigned>> tags;
extern "C" void diagnostic_event(const char* tag,unsigned value) { tags.emplace_back(tag,value); }
static int init(void*) { return 0; }
static int padinit() { return 0; }
static int users(int* a) { a[0]=1;a[1]=a[2]=a[3]=-1;return 0; }
static int openpad(int,int,int,void*) { return 7; }
static int readpad(int,PS5_PadData* out) { *out=sample;return 0; }
static int closepad(int) { return 0; }
static int terminate() { return 0; }
static unsigned count(const char* tag) { unsigned n=0;for(auto& e:tags)if(e.first==tag)++n;return n; }
#define CHECK(x) do { ++checks; if(!(x)) { std::printf("FAIL line=%d: %s\n",__LINE__,#x);return 42; } } while(0)
int main() {
 unsigned checks=0; const auto combo=PS5_PAD_BUTTON_L3|PS5_PAD_BUTTON_R3;
 PadBackend backend{init,padinit,users,openpad,readpad,closepad,terminate};
 PS5PadSource p({{combo,Key::F7,{}}},backend); CHECK(p.activate());
 std::array<bool,max_keys> subscribed{};subscribed[F7]=true;
 sample.connected=true;sample.timestamp=1;sample.buttons=0;
 CHECK(p.poll(subscribed,true).empty());CHECK(count("pad_first_state_read")==1);
 sample.timestamp=2;sample.buttons=combo;
 auto first=p.poll(subscribed,true);CHECK(first.size()==1&&first[0].key==F7);
 CHECK(count("pad_l3r3_edge")==1);CHECK(count("pad_l3r3_f7_dispatched")==1);
 CHECK(p.poll(subscribed,true).empty());CHECK(count("pad_l3r3_edge")==1);
 sample.timestamp=3;CHECK(p.poll(subscribed,true).empty());CHECK(count("pad_l3r3_edge")==1);
 sample.timestamp=4;sample.buttons=0;CHECK(p.poll(subscribed,true).empty());
 sample.timestamp=5;sample.buttons=combo;CHECK(p.poll(subscribed,false).empty());
 CHECK(count("pad_l3r3_edge")==2);CHECK(count("pad_l3r3_f7_dispatched")==1);
 sample.timestamp=6;CHECK(p.poll(subscribed,true).empty());
 sample.timestamp=7;sample.buttons=0;p.poll(subscribed,true);
 sample.timestamp=8;sample.buttons=combo;subscribed[F7]=false;CHECK(p.poll(subscribed,true).empty());
 CHECK(count("pad_l3r3_edge")==3);CHECK(count("pad_l3r3_f7_dispatched")==1);
 sample.timestamp=9;sample.buttons=0;p.poll(subscribed,true);
 sample.timestamp=10;sample.buttons=combo;subscribed[F7]=true;CHECK(p.poll(subscribed,true).size()==1);
 CHECK(count("pad_l3r3_edge")==4);CHECK(count("pad_l3r3_f7_dispatched")==2);
 CHECK(p.deactivate());
 std::printf("PASS %u actual-provider marker/edge controls; hardware backend stubbed\n",checks);
}
