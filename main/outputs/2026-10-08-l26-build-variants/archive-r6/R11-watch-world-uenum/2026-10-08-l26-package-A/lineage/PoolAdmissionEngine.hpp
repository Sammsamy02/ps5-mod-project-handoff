#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include "ImageCoverage.hpp"

// Production policy shared by UnrealInitializer and host tests. Ops supplies
// only OS/clock/read/log adapters; ordering, range proof and retry live here.
namespace rc::pooladmission {
constexpr uint64_t kAdmissionBudgetMs = 60000;
constexpr uint64_t kRetryIntervalMs = 5000;
struct Budget {
    uint64_t start_ms, deadline_ms;
    explicit Budget(uint64_t now, uint64_t duration = kAdmissionBudgetMs)
        : start_ms(now), deadline_ms(duration > UINT64_MAX-now ? UINT64_MAX : now+duration) {}
    bool expired(uint64_t now) const { return now >= deadline_ms; }
    uint64_t remaining(uint64_t now) const { return expired(now) ? 0 : deadline_ms-now; }
};
enum class QueryKind { Available, Unavailable, Unmapped };
struct Query {
    QueryKind kind = QueryKind::Unavailable;
    uint64_t start = 0, end = 0;
    uint32_t protection = 0, flags = 0;
    int raw_rc = 0;
};
enum class Verdict { Readable, Unavailable, HardNegative, Expired, Invalid };
enum class Source { None, VirtualQuery, CopyinCapture, ImageSegments };
struct Result {
    Verdict verdict = Verdict::Unavailable;
    Source source = Source::None;
    const char* note = "readability unavailable";
    uint64_t fault_address = 0;
    size_t fault_length = 0;
};
struct Range { uint64_t address = 0; size_t length = 0; };
inline bool valid_range(uint64_t address, size_t length) {
    return address != 0 && length != 0 && length <= UINT64_MAX-address;
}
inline bool overlaps(Range a, Range b) {
    return a.address < b.address+b.length && b.address < a.address+a.length;
}
struct Session {
    // Fail closed if bounded evidence storage fills: never forget a fault.
    Range faults[16]{}; size_t fault_count = 0; bool all_excluded = false;
    Range probes[16]{}; size_t probe_count = 0;
    bool shadow_attempted = false;
    bool excluded(Range range) const {
        if (all_excluded) return true;
        for (size_t i=0;i<fault_count;++i) if (overlaps(faults[i],range)) return true;
        return false;
    }
    void exclude(Range range) {
        if (fault_count == 16) all_excluded = true;
        else faults[fault_count++] = range;
    }
    bool probe_once(Range range) {
        for (size_t i=0;i<probe_count;++i)
            if (probes[i].address==range.address && probes[i].length==range.length) return false;
        if (probe_count==16) return false;
        probes[probe_count++] = range;
        return true;
    }
};
inline Result expired() { return {Verdict::Expired, Source::None, "one total admission deadline exhausted"}; }

template<class Ops>
Result coverage(Ops& ops, Budget& budget, uint64_t address, size_t length) {
    if (!valid_range(address,length)) return {Verdict::Invalid,Source::None,"invalid or overflowing exact range"};
    const uint64_t end=address+length;
    uint64_t cursor=address;
    for (unsigned regions=0; cursor<end && regions<128; ++regions) {
        if (budget.expired(ops.now_ms())) return expired();
        const auto query=ops.query(cursor);
        if (budget.expired(ops.now_ms())) return expired();
        if (query.kind==QueryKind::Unmapped)
            return {Verdict::HardNegative,Source::None,"VirtualQuery proved unmapped"};
        if (query.kind!=QueryKind::Available)
            return {Verdict::Unavailable,Source::None,"VirtualQuery unavailable"};
        ops.log_region(address,length,query);
        // EXACT means the result must contain this byte, not merely overlap
        // some later part of the range. Malformed replies prove nothing.
        if (query.start>cursor || query.end<=cursor || query.end<=query.start)
            return {Verdict::Unavailable,Source::None,"VirtualQuery returned non-containing or degenerate region"};
        if ((query.protection & 1u)==0)
            return {Verdict::HardNegative,Source::None,"VirtualQuery proved no CPU-read protection", query.start, static_cast<size_t>(query.end-query.start)};
        if ((query.flags & 0x10u)==0)
            return {Verdict::HardNegative,Source::None,"VirtualQuery proved uncommitted reservation", query.start, static_cast<size_t>(query.end-query.start)};
        cursor=query.end<end ? query.end : end;
    }
    if (cursor!=end) return {Verdict::Unavailable,Source::None,"VirtualQuery region limit reached"};
    return {Verdict::Readable,Source::VirtualQuery,"exact needed range mapped and CPU-readable"};
}

template<class Ops>
Result read(Ops& ops, Budget& budget, Session& session, uint64_t address, size_t length, uint8_t* destination) {
    if (!destination || !valid_range(address,length))
        return {Verdict::Invalid,Source::None,"invalid or overflowing exact range"};
    if (budget.expired(ops.now_ms())) return expired();
    const Range range{address,length};
    // Stored fault provenance is checked before any new proof; a later VQ
    // or prefix capture cannot erase a fault observed during this admission.
    if (session.excluded(range) || ops.known_fault(address,length)) {
        session.exclude(range);
        return {Verdict::HardNegative,Source::None,"range excluded by recorded fault"};
    }
    auto result=coverage(ops,budget,address,length);
    if (result.verdict==Verdict::HardNegative) {
        session.exclude(valid_range(result.fault_address,result.fault_length) ?
                        Range{result.fault_address,result.fault_length} : range);
        return result;
    }
    if (result.verdict==Verdict::Expired || result.verdict==Verdict::Invalid) return result;
    if (result.verdict==Verdict::Readable) {
        if (budget.expired(ops.now_ms())) return expired();
        ops.read_direct(address,length,destination);
        if (budget.expired(ops.now_ms())) return expired();
        return result; // optional oracle cross-check is disabled by default
    }
    if (budget.expired(ops.now_ms())) return expired();
    const uint8_t* bytes=nullptr; size_t captured=0;
    if (ops.capture(address,length,&bytes,&captured) && bytes && captured>=length) {
        if (budget.expired(ops.now_ms())) return expired();
        std::memcpy(destination,bytes,length);
        return {Verdict::Readable,Source::CopyinCapture,"exact needed bytes from owned copyin capture"};
    }
    if (session.probe_once(range)) {
        if (budget.expired(ops.now_ms())) return expired();
        const int probe=ops.probe(address,length,budget);
        if (probe==0 || ops.known_fault(address,length)) {
            session.exclude(range);
            return {Verdict::HardNegative,Source::None,"copyin fault; range excluded and shadow barred"};
        }
        if (budget.expired(ops.now_ms())) return expired();
        if (ops.capture(address,length,&bytes,&captured) && bytes && captured>=length) {
            if (budget.expired(ops.now_ms())) return expired();
            std::memcpy(destination,bytes,length);
            return {Verdict::Readable,Source::CopyinCapture,"exact needed bytes from owned copyin capture"};
        }
    }
    // The fallback is diagnostic corroboration only. Matching disk-image
    // identity never supplies live heap bytes or grants a direct read.
    if (!session.shadow_attempted && ops.shadow_enabled) {
        if (budget.expired(ops.now_ms())) return expired();
        session.shadow_attempted=true;
        ops.shadow(budget);
        if (budget.expired(ops.now_ms())) return expired();
    }
    return {Verdict::Unavailable,Source::None,"VQ/copyin inconclusive; shadow identity cannot prove heap readability"};
}

// Image metadata requires segment evidence first. VQ can veto the segment;
// only an unavailable API (not a malformed containing region) is optional.
template<class Ops>
Result read_image(Ops& ops, Budget& budget, Session& session, uint64_t address, size_t length, uint8_t* destination) {
    if (!destination || !valid_range(address,length))
        return {Verdict::Invalid,Source::None,"invalid image range"};
    const auto segment=ops.image_range(address,length);
    ops.log_image(address,length,segment);
    if (budget.expired(ops.now_ms())) return expired();
    const Range range{address,length};
    if(session.excluded(range)||ops.known_fault(address,length)) {
        session.exclude(range);
        return {Verdict::HardNegative,Source::None,"image range excluded by recorded fault"};
    }
    // Query even when the segment is missing: diagnostics must distinguish
    // an absent module segment from an OS mapping failure, without reading.
    auto cross=coverage(ops,budget,address,length);
    if(cross.verdict==Verdict::HardNegative){
        session.exclude(valid_range(cross.fault_address,cross.fault_length)?Range{cross.fault_address,cross.fault_length}:range);
        return cross;
    }
    if(cross.verdict==Verdict::Expired||cross.verdict==Verdict::Invalid)return cross;
    if(!segment.readable)return {Verdict::Unavailable,Source::None,segment.note};
    if(cross.verdict!=Verdict::Readable && std::strcmp(cross.note,"VirtualQuery unavailable")!=0)
        return cross;
    if(budget.expired(ops.now_ms()))return expired();
    ops.read_direct(address,length,destination);
    if(budget.expired(ops.now_ms()))return expired();
    return {Verdict::Readable,Source::ImageSegments,cross.verdict==Verdict::Readable?
        "segment proven; VirtualQuery corroborated":"segment proven; VirtualQuery unavailable"};
}

template<class Ops,class Attempt>
Result retry(Ops& ops, Budget& budget, Attempt attempt) {
    for (;;) {
        if (budget.expired(ops.now_ms())) return expired();
        auto result=attempt();
        if (budget.expired(ops.now_ms())) return expired();
        if (result.verdict!=Verdict::Unavailable) return result;
        const uint64_t remaining=budget.remaining(ops.now_ms());
        if (!remaining) return expired();
        ops.wait_ms(remaining<kRetryIntervalMs ? remaining : kRetryIntervalMs);
    }
}
} // namespace rc::pooladmission
