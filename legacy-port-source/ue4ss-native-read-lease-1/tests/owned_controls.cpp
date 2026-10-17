#include <polyhook2/OwnedMemory.hpp>
#include <polyhook2/MemAccessor.hpp>
#include <array>
#include <cassert>
#include <future>
#include <chrono>
#include <cstdio>
#include <limits>
using namespace PLH;
using namespace PLH::OwnedMemory;
using namespace std::chrono_literals;
static bool refuses(std::span<const ReadRange> ranges) {
 try { ReadLease lease(ranges); return false; }
 catch(const std::invalid_argument&) { return true; }
}
int main() {
 std::array<uint8_t,16> bytes{}; bytes[0]=42; bytes[8]=84;
 ReadRange whole{bytes.data(),bytes.size()};
 assert(refuses({}));
 assert(refuses(std::span(&whole,1)));
 {
  ScopedRegion a(bytes.data(),8,R); ScopedRegion b(bytes.data()+8,8,R|W);
  ReadLease lease(std::span(&whole,1));
  assert(lease.spans().size()==2 && lease.spans()[0].size==8 && lease.spans()[1].size==8);
  assert(lease.spans()[0].data[0]==42 && lease.spans()[1].data[0]==84);
  assert(lease.spans()[0].protection==R && lease.spans()[1].protection==(R|W));
 }
 {
  ScopedRegion a(bytes.data(),7,R); ScopedRegion b(bytes.data()+8,8,R);
  assert(refuses(std::span(&whole,1)));
 }
 {
  ScopedRegion a(bytes.data(),16,W); assert(refuses(std::span(&whole,1)));
 }
 {
  ScopedRegion a(bytes.data(),16,R);
  std::array<ReadRange,2> overlap{{{bytes.data(),8},{bytes.data()+7,8}}};
  assert(refuses(overlap));
  std::array<ReadRange,2> disjoint{{{bytes.data()+8,8},{bytes.data(),8}}};
  ReadLease lease(disjoint); assert(lease.spans()[0].data==bytes.data());
 }
 ReadRange invalid{nullptr,1}; assert(refuses(std::span(&invalid,1)));
 invalid={bytes.data(),0}; assert(refuses(std::span(&invalid,1)));
 invalid={reinterpret_cast<const uint8_t*>(std::numeric_limits<uintptr_t>::max()-1),8};
 assert(refuses(std::span(&invalid,1)));
 MemAccessor accessor;
 {
  OwnedMapping mapping(1); auto* base=static_cast<uint8_t*>(mapping.data());base[0]=7;
  bool ok=false; const auto page=mapping.size();
  ReadRange range{base,page}; std::future<bool> protection; std::promise<void> entered;
  {
   ReadLease lease(std::span(&range,1));
   protection=std::async(std::launch::async,[&]{entered.set_value();bool result=false;accessor.mem_protect(reinterpret_cast<uintptr_t>(base),page,R,result);return result;});
   entered.get_future().wait();assert(protection.wait_for(25ms)==std::future_status::timeout);
   assert(lease.spans()[0].data[0]==7 && lease.spans()[0].protection==(R|W));
  }
  assert(protection.get());assert(query(reinterpret_cast<uintptr_t>(base),1)->protection==R);
  accessor.mem_protect(reinterpret_cast<uintptr_t>(base),page,R|W,ok);assert(ok);
 }
 {
  auto mapping=std::make_unique<OwnedMapping>(1);auto* base=static_cast<uint8_t*>(mapping->data());base[0]=91;
  ReadRange range{base,1};std::future<void> released;std::promise<void> entered;
  {
   ReadLease lease(std::span(&range,1));
   released=std::async(std::launch::async,[owner=std::move(mapping),&entered]()mutable{entered.set_value();owner.reset();});
   entered.get_future().wait();assert(released.wait_for(25ms)==std::future_status::timeout);
   assert(lease.spans()[0].data[0]==91);
  }
  released.get();assert(!query(reinterpret_cast<uintptr_t>(base),1));
 }
 {
  std::array<uint8_t,8> destination{};uint8_t value=3;
  ScopedRegion d(destination.data(),destination.size(),R|W);ScopedRegion s(&value,1,R);
  ReadRange range{destination.data(),destination.size()};std::future<bool> written;std::promise<void> entered;
  {
   ReadLease lease(std::span(&range,1));
   written=std::async(std::launch::async,[&]{entered.set_value();return accessor.mem_copy(reinterpret_cast<uintptr_t>(destination.data()),reinterpret_cast<uintptr_t>(&value),1);});
   entered.get_future().wait();assert(written.wait_for(25ms)==std::future_status::timeout);
   assert(lease.spans()[0].data[0]==0);
  }
  assert(written.get());assert(destination[0]==3);
 }
 {
  OwnedMapping mapping(32768);auto* base=static_cast<uint8_t*>(mapping.data());const auto half=mapping.size()/2;
  bool ok=false;accessor.mem_protect(reinterpret_cast<uintptr_t>(base),half,R,ok);assert(ok);
  ReadRange range{base,mapping.size()};ReadLease lease(std::span(&range,1));
  assert(lease.spans().size()==2 && lease.spans()[0].size==half);
  assert(lease.spans()[0].protection==R && lease.spans()[1].protection==(R|W));
 }
 std::puts("owned read lease: coverage/refusal, exact permissions, blocked protect/free/write passed");
}
