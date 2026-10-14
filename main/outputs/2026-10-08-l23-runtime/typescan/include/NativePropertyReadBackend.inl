// Included in SeedArming: use the selected existing VQ ABI and fault ledger.
// Unlike startup admission, no startup deadline/callback, bank or new probe.
struct PropertyReadOps {
    uint64_t now_ms() const {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    auto query(uint64_t address) -> admission::Query {
        L13VqInfo info{};
        const int result=sceKernelVirtualQuery(reinterpret_cast<const void*>(address),0,&info,sizeof(info));
        if(result!=0) return {admission::QueryKind::Unavailable,0,0,0,0,result};
        return {admission::QueryKind::Available,reinterpret_cast<uint64_t>(info.start),reinterpret_cast<uint64_t>(info.end),
            static_cast<uint32_t>(info.protection),info.flags,result};
    }
    void log_region(uint64_t,size_t,const admission::Query&) {}
};
static bool PropertyReadBytes(uintptr_t address,size_t length,void* destination) {
    if(!destination || !admission::valid_range(address,length) || bridge_copyin_range_faulted(address,length)!=0) return false;
    PropertyReadOps ops; admission::Budget budget(ops.now_ms(),1000);
    const auto proof=admission::coverage(ops,budget,address,length);
    if(proof.verdict!=admission::Verdict::Readable || budget.expired(ops.now_ms()) || bridge_copyin_range_faulted(address,length)!=0) return false;
    std::memcpy(destination,reinterpret_cast<const void*>(address),length);
    return !budget.expired(ops.now_ms());
}
static bool PropertyImageClassRange(uintptr_t address,size_t length) {
    const uint64_t actual_base=module_view_base();
    const rc::codecache::Metadata* image=nullptr;
    if(bridge_code_cache_context(actual_base,&image)!=1 || !image || !image->ready || image->base!=actual_base) return false;
    // Authenticated profile PT_LOAD and observed mappings must agree. Never
    // accept a heap FFieldClass simply because its name/flags look familiar.
    if(image->segment_count>rc::codecache::kMaxLoads || image->reported_count>4) return false;
    rc::imagecoverage::Segment segments[rc::codecache::kMaxLoads]{};
    for(size_t i=0;i<image->segment_count;++i) {
        const auto& segment=image->segments[i];
        segments[i]={segment.vaddr,segment.filesz,segment.memsz,segment.flags,segment.index};
    }
    rc::imagecoverage::Mapping maps[4]{};
    for(size_t i=0;i<image->reported_count;++i) maps[i]={image->reported[i].start,image->reported[i].end,image->reported[i].protection};
    const auto hit=rc::imagecoverage::from_profile(segments,image->segment_count,image->base,actual_base,address,length,maps,image->reported_count);
    if(!hit.readable || bridge_copyin_range_faulted(address,length)!=0) return false;
    PropertyReadOps ops; admission::Budget budget(ops.now_ms(),1000);
    return admission::coverage(ops,budget,address,length).verdict==admission::Verdict::Readable;
}
