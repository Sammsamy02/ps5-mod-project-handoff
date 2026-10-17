
#include <chrono>
#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include "PoolAdmissionEngine.hpp"
#include "pool_admission.h"
#include "SeedValidation.hpp"
#include "SeedTable.h"
namespace SeedTableNS=RC::Unreal::UnrealInitializer::SeedTable;
#define STR(x) x
#define REQUIRE(x) do{if(!(x))throw std::runtime_error(std::string("ASSERT ")+ #x);}while(0)
std::string log_text,events; uint64_t fake_ms=0; unsigned vq_calls=0,sink_calls=0,metadata_reads=0; bool mapped=true,readable=true,span_ok=true,metadata_short=false,bridge_fault=false,second_fault=false,ready=true;
bool capture_ready=false;uint64_t captured_addr=0;uint32_t captured_len=0;int oracle_result=-1;uint64_t sent_deadline=0;
alignas(16) uint8_t heap_bytes[64]{};
alignas(16) uint8_t copied_bytes[64]{};
struct Output { template<class...T> static void send(const char* text,T&&...args){std::ostringstream o;o<<text;((o<<" "<<args),...);log_text+=o.str();} };
const char* ensure_str(const char*s){return s;}
namespace fmt{template<class...T>std::string format(const char*t,T&&...args){std::ostringstream o;o<<t;((o<<" "<<args),...);return o.str();}}
struct FakeClock{static auto now(){return std::chrono::steady_clock::time_point(std::chrono::milliseconds(fake_ms));}};
void CheckStartupWait(){if(fake_ms>=300000)throw std::runtime_error("outer 300s timeout");}
void WaitForStartupRetry(std::chrono::milliseconds n){fake_ms+=n.count();CheckStartupWait();}
[[noreturn]] void seed_refused(const std::string&s){throw std::runtime_error(s);}
template<class T>void require_present(const T&){}
struct PoolWords{uint32_t current_block=0,current_byte_offset=64;uint64_t blocks0=0;uint32_t none_id=0,byte_property_id=4;};
PoolWords read_pool_words(uint64_t){++metadata_reads;PoolWords w;w.blocks0=reinterpret_cast<uint64_t>(heap_bytes);if(!ready)w.byte_property_id=0;return w;}
uint64_t read_u64(const volatile uint8_t*,uint64_t){return reinterpret_cast<uint64_t>(heap_bytes);}
uint32_t bridge_readable_span_count(){return span_ok?1:0;}
int bridge_readable_span_at(uint32_t,uint64_t*lo,uint64_t*hi){*lo=0x9000000;*hi=metadata_short?0x9765ed8:0xa000000;return 1;}
extern "C" int bridge_write_copyin_capture(uint64_t a,const uint8_t**out,uint32_t*n){events+='C';const bool found=capture_ready&&a>=captured_addr&&a-captured_addr<captured_len;*out=found?copied_bytes+(a-reinterpret_cast<uint64_t>(heap_bytes)):nullptr;*n=found?captured_len-static_cast<uint32_t>(a-captured_addr):0;return found?1:0;}
        namespace admission = rc::pooladmission;
        struct L13VqInfo
        {
            void* start;
            void* end;
            int64_t offset;
            int32_t protection;
            int32_t memory_type;
            uint8_t flags;
            char name[32];
            uint8_t gpu_mask_id;
        };
        static_assert(sizeof(L13VqInfo) == 0x48, "SceKernelVirtualQueryInfo layout drifted");
        extern "C" int sceKernelVirtualQuery(const void*, int, L13VqInfo*, size_t);
        extern "C" int bridge_kernel_write_copyin_probe_until(uint64_t, uint64_t, uint64_t);
        extern "C" int bridge_copyin_range_faulted(uint64_t, uint64_t);

        struct L13AdmissionOps
        {
            bool shadow_enabled = true;
            auto now_ms() const -> uint64_t
            {
                return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    FakeClock::now().time_since_epoch()).count());
            }
            auto wait_ms(uint64_t duration) -> void
            {
                Output::send(STR("[Seeds][L13] admission not ready; retry in {}ms within shared deadline\n"), duration);
                WaitForStartupRetry(std::chrono::milliseconds(duration));
            }
            auto known_fault(uint64_t address, size_t length) -> bool
            {
                return bridge_copyin_range_faulted(address, length) != 0;
            }
            auto query(uint64_t address) -> admission::Query
            {
                CheckStartupWait();
                L13VqInfo info{};
                const int result = sceKernelVirtualQuery(reinterpret_cast<const void*>(address), 0, &info, sizeof(info));
                if (result != 0)
                {
                    // No documented exact-unmapped return code is established
                    // for this firmware. An API/policy error proves no mapping
                    // fact, so it remains inconclusive and can fall to copyin.
                    Output::send(STR("[Seeds][L13] VirtualQuery address=0x{:X} rc=0x{:X}: unavailable/inconclusive\n"),
                                 address, static_cast<uint32_t>(result));
                    return {admission::QueryKind::Unavailable, 0, 0, 0, 0, result};
                }
                return {admission::QueryKind::Available, reinterpret_cast<uint64_t>(info.start),
                        reinterpret_cast<uint64_t>(info.end), static_cast<uint32_t>(info.protection), info.flags, result};
            }
            auto log_region(uint64_t address, size_t length, const admission::Query& query) -> void
            {
                Output::send(STR("[Seeds][L13] VirtualQuery needed=[0x{:X},0x{:X}) region=[0x{:X},0x{:X}) protection=0x{:X} flags=0x{:X}\n"),
                             address, address + length, query.start, query.end, query.protection, query.flags);
            }
            auto capture(uint64_t address, size_t, const uint8_t** bytes, size_t* length) -> bool
            {
                uint32_t available = 0;
                const int result = bridge_write_copyin_capture(address, bytes, &available);
                *length = available;
                return result == 1;
            }
            auto probe(uint64_t address, size_t length, admission::Budget& budget) -> int
            {
                CheckStartupWait();
                return bridge_kernel_write_copyin_probe_until(address, length, budget.deadline_ms);
            }
            auto shadow(admission::Budget&) -> bool
            {
                // shadow_arm hashes/scans the complete disk image with no
                // cancellation seam. It can corroborate seed identity, but
                // cannot prove any live heap bytes. Do not start that work
                // inside this admission deadline or claim it admits a range.
                Output::send(STR("[Seeds][L13] shadow fallback considered after VQ/copyin unavailable: "
                                 "declined; image identity cannot prove live heap readability; "
                                 "deadline-aware shadow byte source unavailable\n"));
                return false;
            }
            auto read_direct(uint64_t address, size_t length, uint8_t* bytes) -> void
            {
                // Sole direct heap-read site: the engine calls this only
                // after proving this exact address/length with VirtualQuery.
                const auto* source = reinterpret_cast<const volatile uint8_t*>(address);
                for (size_t i = 0; i < length; ++i) bytes[i] = source[i];
            }
        };


extern "C" int sceKernelVirtualQuery(const void*ptr,int flags,L13VqInfo* out,size_t size){
 events+='V';++vq_calls;REQUIRE(flags==0);REQUIRE(size==0x48);
 if(!mapped)return static_cast<int>(0x80020001u);
 out->flags=0x10;out->start=heap_bytes;out->end=heap_bytes+64;out->protection=(readable&&!(second_fault&&ptr==heap_bytes+8))?1:0;return 0;
}
extern "C" int bridge_copyin_range_faulted(uint64_t,uint64_t){return bridge_fault?1:0;}
extern "C" int bridge_kernel_write_copyin_probe_until(uint64_t address,uint64_t length,uint64_t deadline){events+='O';++sink_calls;sent_deadline=deadline;if(oracle_result==1){capture_ready=true;captured_addr=address;captured_len=static_cast<uint32_t>(length);}return oracle_result;}
void phase3(){const uint64_t base=SeedTableNS::kBaseObserved;std::chrono::seconds validation_budget(10);
            // This one admission deadline also covers metadata preparation,
            // settings IO, all needed entry proofs and every retry sleep.
            // The legacy per-scan setting must never multiply this budget.
            (void)validation_budget;
            L13AdmissionOps admission_ops;
            admission::Budget admission_budget(admission_ops.now_ms());

            // ---- Phase 3: FName pool validation chain (FNAME-VERDICT section 5, steps 1-3) ----
            // Retries are only for "pool not initialized YET" shapes (shape
            // failure or interned ids still 0); once the shape holds, a string
            // mismatch is terminal refutation, never retried.
            require_present(SeedTableNS::kFNameCtorWchar);
            const uint64_t pool_runtime = base + SeedTableNS::kFNamePoolLabel;
            // Read-path guard: every pool-chain read_u64 roots here — the pool
            // label must be data-readable before the chain starts.
            if (const auto cls = rc_seed::classify_seed_address(pool_runtime, base); cls != rc_seed::SeedAddressClass::DataReadable)
            {
                seed_refused(fmt::format("slot 1 fname pool 0x{:X}: seed class {} (expected data-readable); refusing reads", pool_runtime, rc_seed::seed_class_name(cls)));
            }
            // ---- launch10-c: snapshot the bridge-published readable spans
            // ---- v4 (worker-fname-validator-v4): IMAGE OR NON-IMAGE (the
            // ---- allocator OBJECT is static image BSS; Blocks[0]'s storage
            // ---- class is not pinned — the launch-9 region-class
            // ---- observation put its mapping in LOAD7's BSS tail, no
            // ---- captured pointer value exists; wrong-base protection is
            // ---- the entry-0 'None' anchor below).
            // (worker-fname-heap-runtime-bounds). The frozen
            // kHeapLo/kHeapHi envelope is RETIRED from admission: it is
            // vintage-transient non-image memory (GEOMETRY.md section 6), real
            // at igf/igf2 and absent from the igf3 7-span walk, so a live heap
            // anywhere else masked all deeper progress. Zero published spans
            // means the bridge table never populated (walk failed or no
            // qualifying span): LOUD refusal naming it — never a fallback to
            // the frozen envelope. The table is populated at module_start
            // (before conversion) and read here after it, under the existing
            // single-threaded init discipline; no PLH lease region is created
            // for the heap (admission table only).
            const uint32_t spanpub_published = bridge_readable_span_count();
            if (spanpub_published == 0)
            {
                seed_refused(fmt::format("FName pool admission at 0x{:X}: span table unavailable (bridge published 0 readable spans; pool metadata cannot be bounded; frozen envelope retired)", pool_runtime));
            }
            rc_seed::AddressSpan published[rc_seed::kMaxPublishedSpans];
            uint32_t published_count = 0;
            for (uint32_t i = 0; i < spanpub_published && published_count < rc_seed::kMaxPublishedSpans; ++i)
            {
                uint64_t lo = 0;
                uint64_t hi = 0;
                if (bridge_readable_span_at(i, &lo, &hi) && lo < hi)
                {
                    published[published_count].lo = lo;
                    published[published_count].hi = hi;
                    ++published_count;
                }
            }
            if (published_count == 0)
            {
                seed_refused(fmt::format("FName pool admission at 0x{:X}: span table unavailable (bridge published {} spans but none survived the snapshot; pool metadata cannot be bounded; frozen envelope retired)", pool_runtime, spanpub_published));
            }
            // Prove the exact metadata words before touching them. A span
            // containing only the pool base does not cover its far id fields.
            const auto metadata_readable = [&](uint64_t address, size_t length) -> bool {
                if (!admission::valid_range(address, length)) return false;
                for (uint32_t i = 0; i < published_count; ++i)
                {
                    if (address >= published[i].lo && address + length <= published[i].hi) return true;
                }
                return false;
            };
            for (const uint64_t offset : {rc_seed::kPoolLayout.current_block_offset,
                                          rc_seed::kPoolLayout.current_byte_offset_offset,
                                          rc_seed::kPoolLayout.none_id_offset,
                                          rc_seed::kPoolLayout.byte_property_id_offset})
            {
                if (!metadata_readable(pool_runtime + offset, 4))
                    seed_refused(fmt::format("FName metadata word at 0x{:X} lacks exact readable-span coverage", pool_runtime + offset));
            }
            if (!metadata_readable(pool_runtime + rc_seed::kPoolLayout.blocks_offset, 8))
                seed_refused("FName Blocks[0] pointer slot lacks exact readable-span coverage");

            if (admission_budget.expired(admission_ops.now_ms()))
                seed_refused("FName admission deadline exhausted during metadata preparation");
            const auto setting = read_pool_admission("/data/plugins/UE4SS-settings.ini");
            admission_ops.shadow_enabled = setting.mode != PoolAdmissionMode::Oracle;
            // All modes retain VQ first; shadow mode cannot remove a memory
            // proof requirement. The mode only enables identity corroboration.
            Output::send(STR("[Seeds][L13] pool admission: one total {}ms budget (including operations and retry sleeps); "
                             "mode={}; VirtualQuery first; copyin only on inconclusive VQ\n"),
                         admission::kAdmissionBudgetMs, ensure_str(pool_admission_mode_name(setting.mode)));
            admission::Session admission_session;
            PoolWords words{};
            const char* pool_admission_source = "none";
            bool used_vq = false, used_copyin = false;
            const auto admission_read = [&](uint64_t address, size_t length, std::vector<uint8_t>& bytes) -> admission::Result {
                bytes.resize(length);
                const auto result = admission::read(admission_ops, admission_budget, admission_session, address, length, bytes.data());
                if (result.verdict == admission::Verdict::Readable)
                {
                    used_vq |= result.source == admission::Source::VirtualQuery;
                    used_copyin |= result.source == admission::Source::CopyinCapture;
                }
                return result;
            };
            const auto result = admission::retry(admission_ops, admission_budget, [&]() -> admission::Result {
                CheckStartupWait();
                words = read_pool_words(pool_runtime);
                if (words.blocks0 == 0 || (words.blocks0 & 7u) != 0 ||
                    words.current_block >= rc_seed::kPoolLayout.max_blocks ||
                    (words.none_id >> rc_seed::kPoolLayout.block_offset_bits) > words.current_block || words.byte_property_id == 0)
                {
                    return {admission::Verdict::Unavailable, admission::Source::None, "pool structure or ByteProperty id not ready"};
                }
                used_vq = used_copyin = false;
                std::vector<uint8_t> bytes;
                auto proof = admission_read(words.blocks0, 2 + rc_seed::kNoneString.size(), bytes);
                if (proof.verdict != admission::Verdict::Readable) return proof;
                if (const auto anchor = rc_seed::check_none_anchor(bytes.data(), bytes.size()); !anchor.ok)
                    seed_refused(fmt::format("FName pool at 0x{:X}: Blocks[0] 0x{:X}: {}", pool_runtime, words.blocks0, anchor.failure));

                const auto validate_entry = [&](uint32_t id, std::string_view expected) -> admission::Result {
                    const auto bounds = rc_seed::check_entry_bounds(id, words.current_block, words.current_byte_offset, 2 + expected.size());
                    if (!bounds.ok)
                        seed_refused(fmt::format("FName pool entry id 0x{:X}: {}", id, bounds.failure));
                    const auto location = rc_seed::entry_location(id);
                    const uint64_t pointer_slot = pool_runtime + rc_seed::kPoolLayout.blocks_offset + location.block_index * 8;
                    if (!metadata_readable(pointer_slot, 8))
                        seed_refused(fmt::format("FName Blocks[{}] pointer slot lacks exact readable-span coverage", location.block_index));
                    // Pin this read to the exact pointer observed now. No
                    // admission proof is reused after a pointer changes.
                    const uint64_t block = location.block_index == 0 ? words.blocks0 :
                        read_u64(reinterpret_cast<const volatile uint8_t*>(pointer_slot), 0);
                    if (!block || (block & 7u) != 0 || location.byte_offset > UINT64_MAX - block)
                        return {admission::Verdict::Invalid, admission::Source::None, "entry block pointer invalid or overflowing"};
                    auto entry_proof = admission_read(block + location.byte_offset, 2 + expected.size(), bytes);
                    if (entry_proof.verdict != admission::Verdict::Readable) return entry_proof;
                    if (const auto match = rc_seed::entry_matches_ascii(bytes.data(), bytes.size(), expected); !match.ok)
                        seed_refused(fmt::format("FName pool entry id 0x{:X} (expected '{}'): {}", id, expected, match.failure));
                    return entry_proof;
                };
                proof = validate_entry(words.none_id, rc_seed::kNoneString);
                if (proof.verdict != admission::Verdict::Readable) return proof;
                proof = validate_entry(words.byte_property_id, rc_seed::kBytePropertyString);
                if (proof.verdict != admission::Verdict::Readable) return proof;
                pool_admission_source = used_vq && used_copyin ? "virtual-query + copyin-owned-read" :
                                        used_vq ? "virtual-query" : "copyin-owned-read";
                return proof;
            });
            if (result.verdict != admission::Verdict::Readable)
                seed_refused(fmt::format("FName pool at 0x{:X}: {} (CurrentBlock {}, Blocks[0] 0x{:X}; elapsed {}ms; "
                                         "hard-negative ranges excluded; shadow cannot grant heap readability)",
                                         pool_runtime, result.note, words.current_block, words.blocks0,
                                         admission_ops.now_ms() - admission_budget.start_ms));
            Output::send(STR("[Seeds] slot 1 fname pool 0x{:X}: admitted via {} (all exact needed ranges readable; elapsed {}ms)\n"),
                         pool_runtime, ensure_str(pool_admission_source), admission_ops.now_ms() - admission_budget.start_ms);
            Output::send(STR("[Seeds] FName pool validated at {} (CurrentBlock {}, Blocks[0] 0x{:X}; source: {}; "
                             "'None' anchor ok; 'None' id 0x{:X}, 'ByteProperty' id 0x{:X}; elapsed {}ms)\n"),
                         reinterpret_cast<void*>(pool_runtime), words.current_block, words.blocks0,
                         ensure_str(pool_admission_source), words.none_id, words.byte_property_id,
                         admission_ops.now_ms() - admission_budget.start_ms);


}
int main(int argc,char**argv){
 const std::string scenario=argv[1];
 heap_bytes[0]=0;heap_bytes[1]=1;std::memcpy(heap_bytes+2,"None",4);
 heap_bytes[8]=0;heap_bytes[9]=3;std::memcpy(heap_bytes+10,"ByteProperty",12);
 std::memcpy(copied_bytes,heap_bytes,64);
 bool expect_refused=false;
 if(scenario=="positive_vq"){}
 else if(scenario=="oracle_capture"){mapped=false;oracle_result=1;}
 else if(scenario=="unavailable_budget"){mapped=false;expect_refused=true;}
 else if(scenario=="negative_blocks_shadow"){readable=false;expect_refused=true;}
 else if(scenario=="faulted_capture_excluded"){bridge_fault=true;capture_ready=true;expect_refused=true;}
 else if(scenario=="entry_range_negative"){second_fault=true;expect_refused=true;}
 else if(scenario=="metadata_range_missing"){metadata_short=true;expect_refused=true;}
 else if(scenario=="zero_spans"){span_ok=false;expect_refused=true;}
 else if(scenario=="not_ready_budget"){ready=false;expect_refused=true;}
 else return 2;
 std::string refusal;
 try{phase3();}catch(const std::exception&e){refusal=e.what();}
 try{
 REQUIRE(expect_refused==!refusal.empty());
 if(scenario=="positive_vq"){REQUIRE(events=="VVV");REQUIRE(sink_calls==0);REQUIRE(log_text.find("virtual-query")!=std::string::npos);REQUIRE(log_text.find("region=[")!=std::string::npos);}
 if(scenario=="oracle_capture"){REQUIRE(events[0]=='V');REQUIRE(sink_calls==2);REQUIRE(sent_deadline==60000);REQUIRE(log_text.find("copyin-owned-read")!=std::string::npos);}
 if(scenario=="unavailable_budget"){REQUIRE(fake_ms==60000);REQUIRE(sink_calls==1);REQUIRE(log_text.find("shadow fallback considered")!=std::string::npos);REQUIRE(log_text.find("identity cannot prove live heap")!=std::string::npos);}
 if(scenario=="negative_blocks_shadow"){REQUIRE(events=="V");REQUIRE(fake_ms==0);REQUIRE(log_text.find("shadow fallback considered")==std::string::npos);}
 if(scenario=="faulted_capture_excluded"){REQUIRE(events.empty());REQUIRE(fake_ms==0);}
 if(scenario=="entry_range_negative"){REQUIRE(events=="VVV");REQUIRE(sink_calls==0);REQUIRE(refusal.find("no CPU-read")!=std::string::npos);}
 if(scenario=="metadata_range_missing"||scenario=="zero_spans"){REQUIRE(metadata_reads==0);REQUIRE(events.empty());}
 if(scenario=="not_ready_budget"){REQUIRE(fake_ms==60000);REQUIRE(events.empty());}
 REQUIRE(fake_ms<300000);
 std::cout<<"PASS "<<scenario<<" elapsed_ms="<<fake_ms<<" vq="<<vq_calls<<" sink="<<sink_calls<<'\n';
 }catch(const std::exception&e){std::cerr<<"FAIL "<<scenario<<": "<<e.what()<<" refusal="<<refusal<<" events="<<events<<'\n';return 1;}
}
