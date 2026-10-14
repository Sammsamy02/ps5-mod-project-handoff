#include "NativeThreadNameAdapter.hpp"
#include <cstring>
#include <stdint.h>

namespace NativeNameThread {
std::int32_t current_native_id();
const void* call_native_name(std::uintptr_t,std::uint32_t);
bool Binding::cancelled() noexcept {try{return stopped_.load(std::memory_order_acquire)||!cancellation_||!cancellation_->IsCancelled||cancellation_->IsCancelled();}catch(...){return true;}}
bool Binding::current(void* p,std::int32_t& id){auto& b=*static_cast<Binding*>(p);if(b.cancelled())return false;id=current_native_id();return true;}
bool Binding::name(void* p,std::uint32_t id,const void*& address){auto& b=*static_cast<Binding*>(p);if(b.cancelled())return false;
 address=call_native_name(b.function_,id);return true;
}
Prepared Binding::prepare(std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot> owner,
 const rc::codecache::Metadata* image,const Data& d,const char* digest,std::uint64_t now,
 bool imported,std::shared_ptr<const RC::Unreal::UnrealInitializer::StartupWaitControl> cancellation) noexcept {
 try {
  if(!owner)return {{},Refusal::Owner};
  if(!d.image_sha||!d.layout_digest||!digest||!d.pin||!rc::codecache::hex_pin(d.image_sha)||!rc::codecache::hex_pin(d.layout_digest)||std::strcmp(d.layout_digest,digest)||!d.image_size||!d.image_extent||d.accessor!=d.pin->linked||!d.pin->name||!d.pin->size||d.pin->size>64)return {{},Refusal::Profile};
  if(!image||!image->ready||!image->generation||image->count>rc::codecache::kMaxPins||image->segment_count>rc::codecache::kMaxLoads||image->reported_count>4||std::strcmp(image->sha256,d.image_sha)||image->size!=d.image_size||image->extent!=d.image_extent||now<image->start_ms||now>=image->deadline_ms)return {{},Refusal::Image};
  const auto& v=owner->Views().array[static_cast<std::size_t>(RC::ScanTarget::MainExe)];
  if(!image->base||reinterpret_cast<std::uintptr_t>(v.image_base)!=image->base||v.image_size!=d.image_extent||d.image_extent>UINT64_MAX-image->base||d.game_name>=d.image_extent||d.image_extent-d.game_name<16||d.accessor>=d.image_extent)return {{},Refusal::Image};
  const auto* e=rc::codecache::named_entry(*image,image->base+d.accessor,d.pin->size);
  if(!e||e->id!=d.pin->id||!e->name||std::strcmp(e->name,d.pin->name)||e->linked!=d.accessor||e->collected!=d.pin->size||std::memcmp(e->bytes,d.pin->bytes,d.pin->size))return {{},Refusal::CodePin};
  bool data=false;
  for(std::size_t i=0;i<image->segment_count;++i){const auto&s=image->segments[i];if(s.memsz<=UINT64_MAX-s.vaddr&&s.vaddr<=d.game_name&&d.game_name-s.vaddr<s.memsz&&16<=s.memsz-(d.game_name-s.vaddr)&&(s.flags&2u)&&!(s.flags&1u))data=true;}
  if(!data)return {{},Refusal::Storage};
  if(!imported)return {{},Refusal::Import};
  auto b=std::shared_ptr<Binding>(new Binding);b->owner_=std::move(owner);b->cancellation_=std::move(cancellation);b->function_=image->base+d.accessor;b->expected_=image->base+d.game_name;
  if(b->cancelled())return {{},Refusal::Cancelled};return {std::move(b),Refusal::None};
 }catch(...){return {{},Refusal::Internal};}
}
Acquired Binding::acquire() noexcept {
 bool entered=false;
 try {
  if(cancelled()){policy_.cancel();return {{},Refusal::Cancelled};}
  {std::lock_guard lock{mutex_};if(stopped_.load(std::memory_order_acquire))return {{},Refusal::Cancelled};++threads_[std::this_thread::get_id()];++active_;entered=true;}
  const auto r=policy_.authenticate({this,current,name},reinterpret_cast<const void*>(expected_));
  if(r!=Result::Accepted){release();entered=false;return {{},cancelled()?Refusal::Cancelled:Refusal::NativeIdentity};}
  if(cancelled()){release();entered=false;return {{},Refusal::Cancelled};}
  {std::lock_guard lock{mutex_};if(!stopped_.load(std::memory_order_acquire)){auto ticket=std::unique_ptr<Ticket>(new Ticket(shared_from_this()));entered=false;return {std::move(ticket),Refusal::None};}}
  release();entered=false;return {{},Refusal::Cancelled};
 }catch(...){if(entered)release();return {{},Refusal::Internal};}
}
void Binding::release() noexcept {try{std::lock_guard lock{mutex_};auto i=threads_.find(std::this_thread::get_id());if(!active_||i==threads_.end()||!i->second)std::terminate();--active_;if(!--i->second)threads_.erase(i);changed_.notify_all();}catch(...){std::terminate();}}
Ticket::~Ticket() noexcept {if(owner_){if(thread_!=std::this_thread::get_id())std::terminate();owner_->release();}}
bool Binding::cancel_and_wait() noexcept {stopped_.store(true,std::memory_order_release);policy_.cancel();try{std::unique_lock lock{mutex_};if(threads_.contains(std::this_thread::get_id()))return false;changed_.wait(lock,[&]{return active_==0;});return true;}catch(...){return false;}}
}
