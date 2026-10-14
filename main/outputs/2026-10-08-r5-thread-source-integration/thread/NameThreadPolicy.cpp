#include "NameThreadPolicy.hpp"
namespace NativeNameThread {
Result Policy::authenticate(Ops ops,const void* expected) noexcept {
  try {
   if(cancelled_.load(std::memory_order_acquire))return Result::Cancelled;
   if(!expected||!ops.current||!ops.name)return Result::Unavailable;
   std::int32_t before{},after{};const void* name=nullptr;
   if(!ops.current(ops.context,before))return Result::Unavailable;
   if(before<=0)return Result::InvalidId;
   if(cancelled_.load(std::memory_order_acquire))return Result::Cancelled;
   if(!ops.name(ops.context,static_cast<std::uint32_t>(before),name))return Result::Unavailable;
   if(cancelled_.load(std::memory_order_acquire))return Result::Cancelled;
   // Numerical pointer equality only: no string/data/global dereference.
   if(name!=expected)return Result::NotNamedGameThread;
   if(!ops.current(ops.context,after))return Result::Unavailable;
   if(after<=0||after!=before)return Result::IdChanged;
   std::lock_guard lock{mutex_};
   if(cancelled_.load(std::memory_order_acquire))return Result::Cancelled;
   if(accepted_id_&&accepted_id_!=static_cast<std::uint32_t>(before))return Result::NativeInitConflict;
   const auto current=std::this_thread::get_id();
   if(affinity_!=std::thread::id{}&&affinity_!=current)return Result::AffinityForeign;
   accepted_id_=static_cast<std::uint32_t>(before);affinity_=current;
   return Result::Accepted;
  }catch(...){return Result::Unavailable;}
 }
}
