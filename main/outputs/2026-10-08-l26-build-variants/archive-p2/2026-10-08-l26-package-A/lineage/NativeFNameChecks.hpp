#pragma once
#include "NativeFNameAdapter.hpp"
#include "PoolAdmissionEngine.hpp"
#include "SeedValidation.hpp"
namespace rc::fnameabi {
struct Check { bool ok=false; const char* reason="unproved native FName"; const char* source="none"; };

// Code-prefix verification is distinct from heap admission. It NEVER reads
// executable memory with the CPU or changes protection. Both paths require
// fresh startup image identity. Only positively mapped immutable XOM may use
// the authenticated disk prefix; readable/unknown mappings need fresh copyin.
template<class Ops>
Check signature(Ops& ops,pooladmission::Budget& budget,uint64_t address,const uint8_t* expected,size_t size) {
    using namespace pooladmission;
    if(!expected || size==0 || size>64 || !valid_range(address,size))return {false,"invalid constructor profile"};
    if(budget.expired(ops.now_ms()))return {false,"constructor signature deadline expired"};
    if(ops.known_fault(address,size))return {false,"constructor prefix has recorded copyin fault"};
    if(!ops.code_identity(address,size))return {false,"constructor startup image identity/base unavailable or mismatched"};
    const auto q=ops.query(address);
    ops.log_region(address,size,q);
    if(q.kind==QueryKind::Unmapped)return {false,"constructor unmapped"};
    if(q.kind==QueryKind::Available && (q.start>address || q.end<=address || size>q.end-address || !(q.protection&4u) || !(q.flags&0x10u)))
        return {false,"constructor lacks full committed executable mapping"};
    if(budget.expired(ops.now_ms()))return {false,"constructor signature deadline expired"};
    if(q.kind==QueryKind::Available && !(q.protection&1u)) {
        if(q.protection&2u)return {false,"constructor non-readable executable mapping is writable"};
        ops.log_code_segment(address,size,"authenticated-disk-XOM-signature");
        if(!ops.cached_code_signature(address,expected,size))return {false,"constructor authenticated XOM prefix unavailable or mismatched"};
        if(ops.known_fault(address,size))return {false,"constructor prefix has recorded copyin fault"};
        if(budget.expired(ops.now_ms()))return {false,"constructor signature deadline expired"};
        return {true,"constructor authenticated image prefix matched in positively mapped XOM","authenticated-disk-XOM-signature"};
    }
    ops.log_code_segment(address,size,"fresh-live-copyin");
    // A previous capture is not proof that these are the bytes still present
    // immediately before this first call. Demand a fresh, successful probe.
    if(budget.expired(ops.now_ms()))return {false,"constructor signature deadline expired"};
    const auto probe=ops.probe(address,size,budget);
    if(probe==0)return {false,"constructor prefix copyin fault"};
    if(probe!=1)return {false,"constructor fresh live prefix unavailable; no cached or disk-only fallback"};
    const uint8_t* bytes=nullptr;size_t length=0;
    if(!ops.capture(address,size,&bytes,&length) || !bytes || length<size)
        return {false,"constructor live prefix unavailable; no disk-only fallback"};
    if(ops.known_fault(address,size))return {false,"constructor prefix has recorded copyin fault"};
    if(budget.expired(ops.now_ms()))return {false,"constructor signature deadline expired"};
    for(size_t i=0;i<size;++i)if(bytes[i]!=expected[i])return {false,"constructor live prologue mismatch"};
    return {true,"constructor live prologue matched owned capture","fresh-live-copyin"};
}

// A bootstrap ToString through admitted pool storage. No reflected call,
// Kismet dependency, direct unproved read, or second native engine call.
template<class Ops>
Check roundtrip(Ops& ops,pooladmission::Budget& budget,pooladmission::Session& session,uint64_t pool,
                uint32_t current_block,uint32_t current_byte_offset,Name name,const char16_t* expected) {
    using namespace pooladmission;
    if(name.number!=0 || !expected)return {false,"constructor returned unexpected number or absent text"};
    const auto location=rc_seed::entry_location(name.index);
    if(!rc_seed::check_entry_bounds(name.index,current_block,current_byte_offset,2).ok)
        return {false,"constructor returned index outside admitted pool bounds"};
    const uint64_t offset=rc_seed::kPoolLayout.blocks_offset+location.block_index*8;
    if(offset>UINT64_MAX-pool)return {false,"constructor roundtrip pointer overflow"};
    uint8_t bytes[kMaxUnits*2]{};
    const auto read_exact=[&](uint64_t address,size_t n)->bool {
        const auto r=ops.is_image(address)?read_image(ops,budget,session,address,n,bytes):read(ops,budget,session,address,n,bytes);
        return r.verdict==Verdict::Readable;
    };
    if(!read_exact(pool+offset,8))return {false,"constructor roundtrip block pointer not readable"};
    uint64_t block=0;for(unsigned i=0;i<8;++i)block|=uint64_t(bytes[i])<<(i*8);
    if(!block || (block&7u) || location.byte_offset>UINT64_MAX-block)return {false,"constructor roundtrip invalid block"};
    const uint64_t entry=block+location.byte_offset;
    if(!read_exact(entry,2))return {false,"constructor roundtrip entry header not readable"};
    const uint16_t header=uint16_t(bytes[0])|(uint16_t(bytes[1])<<8);
    const bool wide=(header&1u)!=0;const size_t units=header>>6;
    const size_t count=units*(wide?2:1);
    if(!units || units>kMaxUnits || !rc_seed::check_entry_bounds(name.index,current_block,current_byte_offset,count+2).ok || entry>UINT64_MAX-2)
        return {false,"constructor roundtrip entry length invalid"};
    if(!read_exact(entry+2,count))return {false,"constructor roundtrip entry text not readable"};
    for(size_t i=0;i<units;++i) {
        const char16_t got=wide?static_cast<char16_t>(uint16_t(bytes[i*2])|(uint16_t(bytes[i*2+1])<<8)):static_cast<char16_t>(bytes[i]);
        if(!wide && got>0x7f)return {false,"constructor roundtrip non-ASCII narrow encoding unproved"};
        if(!expected[i] || got!=expected[i])return {false,"constructor pool ToString mismatch"};
    }
    if(expected[units]!=0)return {false,"constructor pool ToString length mismatch"};
    return {true,"constructor pool ToString equals UTF16 input"};
}
} // namespace rc::fnameabi
