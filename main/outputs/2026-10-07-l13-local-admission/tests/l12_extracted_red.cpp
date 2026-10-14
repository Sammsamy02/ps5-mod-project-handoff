
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include "SeedValidation.hpp"
#define STR(x) x
struct Output { template<class...T> static void send(T&&...) {} };
const char* ensure_str(const char* s) { return s; }
namespace fmt { template<class...T> std::string format(T&&...) { return "refused"; } }
uint64_t elapsed=0; std::string events;
struct FakeClock { static auto now() { return std::chrono::steady_clock::time_point(std::chrono::milliseconds(elapsed)); } };
void CheckStartupWait() {}
void WaitForStartupRetry(std::chrono::milliseconds n) { elapsed += n.count(); }
void seed_refused(const std::string&) { throw std::runtime_error("refused"); }
struct PoolWords { uint32_t current_block=1, current_byte_offset=100; uint64_t blocks0=0x2200000000ull; uint32_t none_id=0, byte_property_id=1; };
PoolWords read_pool_words(uint64_t) { return {}; }
extern "C" int bridge_write_copyin_capture(uint64_t,const uint8_t**,uint32_t*) { events += 'C'; return 0; }
extern "C" int bridge_kernel_write_copyin_probe(uint64_t,uint64_t) { events += 'O'; return -1; }
bool l12_try_pool_admission_fallback(uint64_t,const char*) { events += 'S'; return false; }
int query_mode=0;
enum { kPsVqQueryExact = 0, kPsVqQueryFindNext = 1 };
enum { kPsProtCpuRead = 0x01, kPsProtCpuWrite = 0x02, kPsProtCpuExec = 0x04 };
enum { kPsVqFlagDirect = 0x02, kPsVqFlagCommitted = 0x10 };

struct L12VqInfo // SceKernelVirtualQueryInfo, 0x48 bytes — do not reorder
{
    void   *start;       // +0x00
    void   *end;         // +0x08
    int64_t offset;      // +0x10
    int32_t protection;  // +0x18
    int32_t memory_type; // +0x1C
    uint8_t flags;       // +0x20
    char    name[32];    // +0x21
    uint8_t gpu_mask_id; // +0x41
};
static_assert(sizeof(L12VqInfo) == 0x48, "SceKernelVirtualQueryInfo layout drifted");

// Sony-native NID route (same class as the proven sceKernelOpen/Write/Close
// declarations in the bridge TU).
extern "C" int sceKernelVirtualQuery(const void *, int, L12VqInfo *, size_t);

struct L12Coverage
{
    bool proven{};       // every needed page mapped + CPU-readable
    bool hard_negative{}; // real evidence AGAINST the range (unmapped/no-read)
    uint32_t pages_total{};
    uint32_t pages_readable{};
    char detail[256]{};
};

// Prove [addr, addr+len) readable page-by-page via exact + FindNext queries.
// Needed-range variant of C2: call with ONLY the bytes actually required
// (entry-0 anchor window / validated entry windows), not the whole block.
static auto l12_virtual_query_coverage(uint64_t addr, size_t len) -> L12Coverage
{
    L12Coverage out;
    const uint64_t page = 0x1000;
    const uint64_t first = addr & ~(page - 1);
    const uint64_t last = ((addr + len + page - 1) & ~(page - 1)) - 1;
    out.pages_total = static_cast<uint32_t>(((last - first) >> 12) + 1);

    uint64_t cursor = first;
    while (cursor <= last)
    {
        L12VqInfo info{};
        const int rc = sceKernelVirtualQuery(reinterpret_cast<const void *>(cursor), kPsVqQueryExact, &info, sizeof(info));
        if (rc != 0)
        {
            out.hard_negative = true;
            std::snprintf(out.detail, sizeof(out.detail),
                          "VirtualQuery: page 0x%llX unmapped (rc=%d)", static_cast<unsigned long long>(cursor), rc);
            return out;
        }
        const uint64_t rstart = reinterpret_cast<uint64_t>(info.start);
        const uint64_t rend = reinterpret_cast<uint64_t>(info.end);
        if (rend <= rstart)
        {
            out.hard_negative = true;
            std::snprintf(out.detail, sizeof(out.detail),
                          "VirtualQuery: degenerate region [0x%llX,0x%llX) at page 0x%llX",
                          static_cast<unsigned long long>(rstart), static_cast<unsigned long long>(rend),
                          static_cast<unsigned long long>(cursor));
            return out;
        }
        if ((info.protection & kPsProtCpuRead) == 0)
        {
            out.hard_negative = true;
            std::snprintf(out.detail, sizeof(out.detail),
                          "VirtualQuery: region [0x%llX,0x%llX) prot=0x%X has no CPU-read",
                          static_cast<unsigned long long>(rstart), static_cast<unsigned long long>(rend), info.protection);
            return out;
        }
        const uint64_t covered_end = (rend > (last + 1)) ? (last + 1) : rend;
        out.pages_readable += static_cast<uint32_t>(((covered_end - cursor) + page - 1) >> 12);
        // Committed flag is reported per region; a readable-but-uncommitted
        // reservation is NOT admitted (flags bit 0x10 = has memory behind it).
        if ((info.flags & kPsVqFlagCommitted) == 0)
        {
            out.hard_negative = true;
            std::snprintf(out.detail, sizeof(out.detail),
                          "VirtualQuery: region [0x%llX,0x%llX) readable but NOT committed (flags=0x%X)",
                          static_cast<unsigned long long>(rstart), static_cast<unsigned long long>(rend), info.flags);
            return out;
        }
        Output::send(STR("[Seeds][L12] VirtualQuery: [0x{:X},0x{:X}) prot=0x{:X} flags=0x{:X} type={} name='{}' — readable+committed\n"),
                     rstart, rend, info.protection, info.flags, info.memory_type, ensure_str(info.name));
        cursor = rend; // jump to next region (exact query on the new cursor)
    }
    out.proven = (out.pages_readable >= out.pages_total);
    if (out.proven)
    {
        std::snprintf(out.detail, sizeof(out.detail), "VirtualQuery: %u/%u pages readable+committed",
                      out.pages_readable, out.pages_total);
    }
    return out;
}


extern "C" int sceKernelVirtualQuery(const void* p,int,L12VqInfo* out,size_t) {
  events += 'V'; uint64_t a=reinterpret_cast<uint64_t>(p);
  if(query_mode==1) return static_cast<int>(0x80020001u); // policy/unavailable
  out->start=reinterpret_cast<void*>(query_mode==2?a+0x1000:a);
  out->end=reinterpret_cast<void*>(a+0x2000); out->protection=1; out->flags=0x10; return 0;
}
void run_loop() {
 uint64_t pool_runtime=0x9765ec0;
 rc_seed::AddressSpan published[]={{0x400000,0x10000000}};
 uint32_t published_count=1; uint64_t capture_envelope=0x10000000;
 std::chrono::seconds validation_budget(10);
            PoolWords words{};
            // v5.2 (worker-copyin-unit): set at loop exit when the admission
            // was carried by the copyin owned-read gap path rather than span
            // membership — switches the anchor AND the entry validation to
            // SINK-FED bytes (R-4) and the provenance line in the success
            // message.
            bool pool_admitted_via_copyin = false;
            // [L12] provenance for the admission source (logged on success).
            const char* pool_admission_source = "span-membership";
            // [L12] bounded retry (Claude upgrade C3): the pool may not be
            // fully committed this early; instead of one terminal refusal at
            // budget expiry, retry the whole admission attempt every
            // kPoolAdmissionRoundInterval up to kPoolAdmissionMaxRounds
            // (~150 s, safely inside the 5-minute StartupWaitControl
            // deadline with ~150 s margin for downstream waits). Invariants
            // (agent-audited): the copyin probe stays once-per-boot; each
            // round re-reads words, re-checks shape, re-consults the CACHED
            // oracle note only; the span snapshot stays pre-loop.
            constexpr int kPoolAdmissionMaxRounds = 30;
            constexpr std::chrono::seconds kPoolAdmissionRoundInterval{5};
            bool pool_words_admitted = false;
            bool pool_budget_expired = false;
            const char* pool_expired_note = nullptr; // cached gap note at expiry
            const char* pool_expired_class = nullptr;
            Output::send(STR("[Seeds][L12] pool admission retry armed: {} rounds x {}s (budget {}s per round)\n"),
                         kPoolAdmissionMaxRounds, kPoolAdmissionRoundInterval.count(), validation_budget.count());
            for (int l12_round = 0; l12_round < kPoolAdmissionMaxRounds && !pool_words_admitted; ++l12_round)
            {
            const auto pool_wait_start = FakeClock::now();
            if (l12_round > 0)
            {
                Output::send(STR("[Seeds][L12] pool admission round {}/{}: retrying (shape may have been early)\n"),
                             l12_round + 1, kPoolAdmissionMaxRounds);
            }
            for (;;)
            {
                CheckStartupWait();
                words = read_pool_words(pool_runtime);
                // v4 shape admission: engine invariant none_id>>16 <=
                // CurrentBlock (CurrentBlock is the 0-BASED index of the OPEN
                // block — 0 is valid), Blocks[0] nonzero + 8-aligned, and
                // membership in one PUBLISHED readable span (image or
                // non-image). The already-read words feed the invariant; no
                // extra reads.
                const auto shape = rc_seed::check_pool_shape(words.current_block, words.none_id, words.blocks0, published, published_count, capture_envelope);
                // v5.2 listing-gap resolution (worker-copyin-unit): the
                // write-copyin probe is the SOLE listing-gap oracle (the
                // mincore range verdict is gone — edits 13b/14; no Mapped
                // gating exists or is needed: the probe is inherently
                // self-gating, a refused or unmapped range cannot classify
                // readable). Fired ONCE per boot on the first listing-gap
                // miss with a RESCUABLE class (CaptureTruncated /
                // NotCovered — structural classes stay unrescuable; the
                // mapping is static, re-probing every retry would multiply
                // fresh-evidence events without new information).
                // resolve_listing_gap holds the evidence contract: admission
                // rests on the copyin classification ALONE (Readable means
                // the full range was ALREADY transferred — authorization AND
                // execution), and Refused never reads as "unmapped"
                // (address-class-unknown). Structural checks gate everything.
                const char* gap_note = nullptr;
                static bool gap_probed_this_boot = false;
                static const char* gap_note_cached = nullptr;
                // v5.2: copyin owned-read gap admission, address-pinned — it
                // counts only while words.blocks0 stays the probed value.
                static bool gap_admitted_via_copyin = false;
                static uint64_t gap_admitted_blocks0 = 0;
                // Audit fix 3: if blocks0 CHANGED since the probe (e.g. a
                // transient null was probed then a real value appeared), the
                // cached admission is stale and must be invalidated.
                if (gap_admitted_via_copyin && words.blocks0 != gap_admitted_blocks0)
                {
                    gap_admitted_via_copyin = false;
                    gap_admitted_blocks0 = 0;
                    gap_note_cached = "cached admission invalidated: blocks0 changed since probe";
                }
                // Audit fix 2: only RESCUABLE coverage classes may consume the
                // one probe attempt. Structural failures (null blocks0,
                // misaligned, None-entry-invariant) must NEVER spend it —
                // a transient structural glitch could burn the sole
                // candidate-read on a garbage address.
                const bool shape_is_rescuable =
                    shape.failure_class == rc_seed::PoolShapeCheck::FailureClass::CaptureTruncated ||
                    shape.failure_class == rc_seed::PoolShapeCheck::FailureClass::NotCovered;
                if (!shape.ok)
                {
                    if (!gap_probed_this_boot && shape_is_rescuable)
                    {
                        // Session 1084 fix: Control D (io_ctrl_d at module_start)
                        // already probed and captured the pool bytes. Check the
                        // capture FIRST — only re-probe if no capture exists.
                        // (The re-probe after Control D's sink consumption
                        // returned 0x7ffdfffe; using the capture avoids that.)
                        const uint8_t* cap = nullptr; uint32_t cap_len = 0;
                        if (bridge_write_copyin_capture(words.blocks0, &cap, &cap_len) == 1 && cap != nullptr)
                        {
                            const auto resolution = rc_seed::resolve_listing_gap(
                                shape, rc_seed::CopyinProbeClass::Readable);
                            gap_probed_this_boot = true;
                            gap_note_cached = resolution.note;
                            gap_admitted_via_copyin = resolution.ok;
                            gap_admitted_blocks0 = words.blocks0;
                        }
                        else
                        {
                            const auto copyin_class = static_cast<rc_seed::CopyinProbeClass>(
                                bridge_kernel_write_copyin_probe(words.blocks0, rc_seed::kFNameBlockCapacityBytes));
                            const auto resolution = rc_seed::resolve_listing_gap(shape, copyin_class);
                            gap_probed_this_boot = true;
                            gap_note_cached = resolution.note;
                            gap_admitted_via_copyin = resolution.ok;
                            gap_admitted_blocks0 = words.blocks0;
                        }
                    }
                    else if (!gap_probed_this_boot && !shape_is_rescuable)
                    {
                        gap_note_cached = shape.failure; // structural: the probe was NOT spent
                    }
                    gap_note = gap_note_cached;
                }
                const bool pool_admitted = (shape.ok || (gap_admitted_via_copyin && words.blocks0 == gap_admitted_blocks0));
                if (pool_admitted && words.byte_property_id != 0) // audit fix 1: NoneId==0 is the legitimate UE5 default; ByteProperty id is the readiness signal
                {
                    pool_admitted_via_copyin = !shape.ok; // shape ok ⇒ span membership carried it
                    if (shape.ok) { pool_admission_source = "span-membership"; }
                    else if (gap_admitted_via_copyin) { pool_admission_source = "copyin-owned-read"; }
                    pool_words_admitted = true;
                    break;
                }
                if (std::chrono::duration_cast<std::chrono::seconds>(FakeClock::now() - pool_wait_start).count() > validation_budget.count())
                {
                    // [L12] round boundary: record and retry (or fall through
                    // to the rescue chain after the last round). The message
                    // TEXT is unchanged from the pre-L12 terminal refusal —
                    // it fires verbatim below if every rescue declines.
                    pool_budget_expired = true;
                    pool_expired_note = gap_note ? gap_note : "not consulted (failure class not rescuable)";
                    pool_expired_class = shape.failure;
                    Output::send(STR("[Seeds][L12] pool admission round {} exhausted: CurrentBlock {} Blocks[0] 0x{:X} class {} oracle-note: {}\n"),
                                 l12_round + 1, words.current_block, words.blocks0,
                                 ensure_str(shape.ok ? "ids-zero" : pool_expired_class), ensure_str(pool_expired_note));
                    break;
                }
                WaitForStartupRetry(std::chrono::milliseconds(50));
            }
            if (pool_words_admitted)
            {
                break; // admitted this round: leave the round loop
            }
            if (l12_round + 1 < kPoolAdmissionMaxRounds)
            {
                WaitForStartupRetry(kPoolAdmissionRoundInterval);
            }
            }

            // ===================== [L12] rescue chain =====================
            // Reached only when every round expired unadmitted. Order per
            // Claude upgrades C1/C3 + the fallback laws: (1) VirtualQuery
            // coverage proof on the NEEDED range — primary, no sink; guarded
            // by a calibration query on the span-proven pool struct so an
            // untrusted query route can never fabricate a hard negative.
            // (2) Shadow corroboration — only when the oracle resolution was
            // UNAVAILABLE (never REFUSED; never over a faulting range).
            // (3) The original terminal refusal, text unchanged.
            if (!pool_words_admitted && pool_budget_expired)
            {
                // [L12] rescue 1: VirtualQuery on Blocks[0]'s needed window
                // (entry-0 anchor: first 2 + "None" bytes … plus the block
                // head the chain reads). Calibration first.
                const uint64_t needed_len = 2ull + rc_seed::kNoneString.size() + 16ull;
                Output::send(STR("[Seeds][L12] rescue 1: VirtualQuery coverage proof for Blocks[0] 0x{:X} (+{} bytes)\n"),
                             words.blocks0, needed_len);
                const auto cal = l12_virtual_query_coverage(pool_runtime, 16);
                if (cal.hard_negative)
                {
                    Output::send(STR("[Seeds][L12] VirtualQuery calibration FAILED on the span-proven pool struct ({}): route untrusted this boot, rescue 1 skipped\n"),
                                 ensure_str(cal.detail));
                }
                else
                {
                    const auto cov = l12_virtual_query_coverage(words.blocks0, needed_len);
                    if (cov.proven)
                    {
                        Output::send(STR("[Seeds] slot 1 fname pool 0x{:X}: Blocks[0] 0x{:X} admitted via VirtualQuery coverage ({} — readable+committed, no sink); source: sceKernelVirtualQuery\n"),
                                     pool_runtime, words.blocks0, ensure_str(cov.detail));
                        pool_words_admitted = true;
                        pool_admission_source = "virtual-query";
                        words = read_pool_words(pool_runtime); // fresh words for the anchor chain
                    }
                    else if (cov.hard_negative)
                    {
                        Output::send(STR("[Seeds][L12] VirtualQuery HARD NEGATIVE on Blocks[0] ({}): real evidence against the range — shadow fallback is barred, refusing\n"),
                                     ensure_str(cov.detail));
                    }
                    else
                    {
                        Output::send(STR("[Seeds][L12] VirtualQuery inconclusive ({}): falling to rescue 2\n"), ensure_str(cov.detail));
                    }
                }

                // [L12] rescue 2: shadow corroboration (UNAVAILABLE-class only).
                if (!pool_words_admitted)
                {
                    if (l12_try_pool_admission_fallback(pool_runtime, pool_expired_note))
                    {
                        pool_words_admitted = true;
                        pool_admission_source = "shadow-corroboration";
                        words = read_pool_words(pool_runtime);
                    }
                }

                // [L12] final: the ORIGINAL terminal refusal, text unchanged.
                if (!pool_words_admitted)
                {
                    seed_refused(fmt::format("FName pool at 0x{:X}: {} (observed CurrentBlock {}, Blocks[0] 0x{:X}; published readable spans {}; copyin-probe resolution: {})",
                                             pool_runtime, pool_expired_class, words.current_block, words.blocks0, published_count,
                                             pool_expired_note));
                }
            }


}
int main(int argc,char**argv) {
 std::string scenario=argv[1]; bool ok=false;
 if(scenario=="policy_error_is_inconclusive") { query_mode=1; auto r=l12_virtual_query_coverage(0x2000,6); ok=!r.hard_negative&&!r.proven; }
 if(scenario=="noncontaining_region_never_proves") { query_mode=2; auto r=l12_virtual_query_coverage(0x2000,6); ok=!r.proven; }
 if(scenario=="vq_before_oracle") { try{run_loop();}catch(...){} ok=!events.empty()&&events[0]=='V'; std::cout << "first event=" << events[0] << '\n'; }
 if(scenario=="one_60s_total_budget") { try{run_loop();}catch(...){} ok=elapsed<=60000; std::cout<<"elapsed_ms="<<elapsed<<'\n'; }
 std::cout<<(ok?"PASS ":"FAIL ")<<scenario<<'\n'; return ok?0:1;
}
