#include "NativeThreadAuthority.hpp"
#include <stdint.h>
#include <limits>
extern "C" int32_t scePthreadGetthreadid(void);
namespace NativeThreadIntegration {
std::int32_t current_native_id(){return scePthreadGetthreadid();}
bool Authority::same(const char* a,const char* b){return a&&b&&std::strcmp(a,b)==0;}
bool Authority::externally_cancelled() noexcept {try{return cancelled_.load(std::memory_order_acquire)||!lifetime_.cancelled||lifetime_.cancelled();}catch(...){return true;}}
bool Authority::in_supplied_range(std::uint64_t a,std::size_t n) const noexcept {
    try{for(const auto&r:owner_->ReadableRanges().at(RC::ScanTarget::MainExe)){const auto lo=reinterpret_cast<std::uintptr_t>(r.data);if(lo<=a&&a-lo<r.size&&n<=r.size-(a-lo))return true;}}catch(...){}return false;
}
bool Authority::bss(std::uint64_t a,std::size_t n) const noexcept {
    if(!rc::imagecoverage::range(a,n)||!in_supplied_range(a,n))return false;
    for(std::size_t i=0;i<image_.segment_count;++i){const auto&s=image_.segments[i];
        if(s.vaddr>UINT64_MAX-image_.base||s.memsz>UINT64_MAX-image_.base-s.vaddr||s.filesz>s.memsz)continue;
        const auto lo=image_.base+s.vaddr,hi=lo+s.memsz;
        if(a<lo+s.filesz||a>=hi||n>hi-a||!(s.flags&4u)||(s.flags&1u))continue;
        if(image_.module_info){bool covered=false;for(std::size_t j=0;j<image_.reported_count;++j){const auto&r=image_.reported[j];if(r.start<=a&&r.end>a&&n<=r.end-a&&(r.protection&1u)&&!(r.protection&4u))covered=true;}if(!covered)return false;}
        return true;
    }return false;
}
bool Authority::snapshot(NativeThreadAudit::EngineSample& out,Refusal& failure) noexcept {
    if(externally_cancelled()){policy_.cancel();failure=Refusal::Cancelled;return false;}
    const auto id=image_.base+profile_.id_offset,ready=image_.base+profile_.ready_offset;
    if(!bss(id,4)||!bss(ready,1)){failure=Refusal::ReadableBss;return false;}
    try{
        const PLH::OwnedMemory::ReadRange ranges[]={{reinterpret_cast<const std::uint8_t*>(id),4},{reinterpret_cast<const std::uint8_t*>(ready),1}};
        // Borrow only existing registrations; no service callback or logging
        // runs while the lease holds its service mutex.
        PLH::OwnedMemory::ReadLease lease(ranges);
        const auto*p=reinterpret_cast<const std::uint8_t*>(id);
        out.game_id=std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
        out.ready=*reinterpret_cast<const std::uint8_t*>(ready);return true;
    }catch(...){failure=Refusal::RegistryRead;return false;}
}
bool Authority::read_sample(void* p,NativeThreadAudit::EngineSample& out){auto&r=*static_cast<Reader*>(p);return r.authority.snapshot(out,r.failure);}
bool Authority::read_id(void* p,std::int32_t& out){auto&r=*static_cast<Reader*>(p);if(r.authority.externally_cancelled()){r.authority.policy_.cancel();r.failure=Refusal::Cancelled;return false;}out=current_native_id();return out>0;}
Prepared Authority::prepare(std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot> owner,
    const rc::codecache::Metadata* image,const Profile& profile,const char* selected_digest,
    std::uint64_t now,ImportProof imports,MappingLifetimeProof lifetime) noexcept {
    try{
        if(!owner)return {{},Refusal::ImageOwner};
        if(!profile.image_sha||!profile.layout_digest||!profile.fact_digest||!rc::codecache::hex_pin(profile.image_sha)||!rc::codecache::hex_pin(profile.layout_digest)||!rc::codecache::hex_pin(profile.fact_digest)||!same(profile.layout_digest,selected_digest)||!profile.image_size||!profile.image_extent)return {{},Refusal::Profile};
        if(!image||!image->ready||!image->generation||!same(image->sha256,profile.image_sha)||image->size!=profile.image_size||image->extent!=profile.image_extent||image->segment_count>rc::codecache::kMaxLoads||image->reported_count>4||now<image->start_ms||now>=image->deadline_ms)return {{},Refusal::Metadata};
        const auto& view=owner->Views().array[static_cast<std::size_t>(RC::ScanTarget::MainExe)];
        if(!image->base||image->base!=reinterpret_cast<std::uintptr_t>(view.image_base)||view.image_size!=profile.image_extent||profile.image_extent>UINT64_MAX-image->base||profile.id_offset>=profile.image_extent||profile.image_extent-profile.id_offset<4||profile.ready_offset>=profile.image_extent)return {{},Refusal::Base};
        if(!imports.independently_verified||!same(profile.import_name,imports.name.c_str())||!same(profile.import_nid,imports.nid.c_str())||!same(profile.import_library,imports.library.c_str())||!rc::codecache::hex_pin(imports.review_digest.c_str()))return {{},Refusal::ImportAuthority};
        if(!lifetime.independently_verified||!lifetime.actual_mapping_owner||!lifetime.cancelled)return {{},Refusal::MappingLifetime};
        auto a=std::shared_ptr<Authority>(new Authority);a->owner_=std::move(owner);a->image_=*image;a->profile_=profile;a->lifetime_=std::move(lifetime);
        a->sha_=profile.image_sha;a->layout_=profile.layout_digest;a->facts_=profile.fact_digest;a->import_name_=profile.import_name;a->import_nid_=profile.import_nid;a->import_library_=profile.import_library;
        a->profile_.image_sha=a->sha_.c_str();a->profile_.layout_digest=a->layout_.c_str();a->profile_.fact_digest=a->facts_.c_str();a->profile_.import_name=a->import_name_.c_str();a->profile_.import_nid=a->import_nid_.c_str();a->profile_.import_library=a->import_library_.c_str();
        if((image->base+profile.id_offset)%alignof(std::uint32_t)||!a->bss(image->base+profile.id_offset,4)||!a->bss(image->base+profile.ready_offset,1))return {{},Refusal::ReadableBss};
        if(a->externally_cancelled())return {{},Refusal::Cancelled};return {std::move(a),Refusal::None};
    }catch(...){return {{},Refusal::InternalFailure};}
}
Acquired Authority::acquire() noexcept {
    bool entered=false;
    try{
        if(externally_cancelled()){policy_.cancel();return {{},Refusal::Cancelled};}
        {std::lock_guard lock{mutex_};if(cancelled_.load(std::memory_order_acquire))return {{},Refusal::Cancelled};++threads_[std::this_thread::get_id()];++active_;entered=true;}
        Reader reader{*this};const auto admission=policy_.admit({&reader,read_sample,read_id});
        if(admission!=NativeThreadAudit::Admission::Accepted){release();entered=false;return {{},reader.failure==Refusal::None?Refusal::ThreadIdentity:reader.failure};}
        if(externally_cancelled()){release();entered=false;return {{},Refusal::Cancelled};}
        {std::lock_guard lock{mutex_};if(cancelled_.load(std::memory_order_acquire)){/* release below */}else{
            auto lease=std::unique_ptr<Lease>(new Lease(shared_from_this()));affinity_=std::this_thread::get_id();entered=false;return {std::move(lease),Refusal::None};}}
        release();entered=false;return {{},Refusal::Cancelled};
    }catch(...){if(entered)release();return {{},Refusal::InternalFailure};}
}
void Authority::release() noexcept {try{std::lock_guard lock{mutex_};auto i=threads_.find(std::this_thread::get_id());if(!active_||i==threads_.end()||!i->second)std::terminate();--active_;if(!--i->second)threads_.erase(i);changed_.notify_all();}catch(...){std::terminate();}}
Lease::~Lease() noexcept {if(authority_){if(thread_!=std::this_thread::get_id())std::terminate();authority_->release();}}
bool Authority::cancel_and_wait() noexcept {cancelled_.store(true,std::memory_order_release);policy_.cancel();try{std::unique_lock lock{mutex_};if(threads_.contains(std::this_thread::get_id()))return false;changed_.wait(lock,[&]{return active_==0;});affinity_={};return true;}catch(...){return false;}}
std::thread::id Authority::known_affinity() noexcept {if(externally_cancelled())return {};try{std::lock_guard lock{mutex_};return cancelled_.load(std::memory_order_acquire)?std::thread::id{}:affinity_;}catch(...){return {};}}
}
