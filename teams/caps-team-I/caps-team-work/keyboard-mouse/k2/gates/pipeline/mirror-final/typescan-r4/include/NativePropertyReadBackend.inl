// Included in SeedArming: use the selected existing VQ ABI and fault ledger.
// Unlike startup admission, no startup deadline/callback, bank or new probe.
// l24: PropertyImageClassRange now routes through bridge_image_range's
// spanpub fallback (module-info when obtained, else PT_LOAD + this-capture's
// VM records) instead of duplicating a module-info-only subset.
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
    if(!destination || !admission::valid_range(address,length)) return false;
    PropertyReadOps ops; admission::Budget budget(ops.now_ms(),1000);
    const auto proof=admission::coverage(ops,budget,address,length);
    if(proof.verdict!=admission::Verdict::Readable || budget.expired(ops.now_ms())) return false;
    std::memcpy(destination,reinterpret_cast<const void*>(address),length);
    return !budget.expired(ops.now_ms());
}
// l24: the class-range check now uses the bridge's full policy instead of
// duplicating a module-info-only subset. The l23 version failed because it
// required module-info mappings (image_segment_source=2 = never obtained on
// this console), making from_profile always return false with 0 mappings.
// The bridge's spanpub fallback provides the missing proof from this-capture
// VM records (g_spanpub_records, g_image_mapping_count).
static bool PropertyImageClassRange(uintptr_t address,size_t length) {
    // Primary: route through the bridge's existing policy (module-info when
    // obtained, else PT_LOAD + spanpub VM records). Same fail-closed
    // semantics — no proof means refuse.
    rc::imagecoverage::Hit hit{};
    if (bridge_image_range(static_cast<uint64_t>(address), length, &hit) != 1) return false;
    if (!hit.readable) return false;
    // Secondary: the same admission-coverage proof as l23 (budgeted VQ +
    // fault-ledger re-check). This is the post-range proof that the bytes
    // are actually readable, independent of the range source.
    PropertyReadOps ops; admission::Budget budget(ops.now_ms(),1000);
    return admission::coverage(ops,budget,address,length).verdict==admission::Verdict::Readable;
}
