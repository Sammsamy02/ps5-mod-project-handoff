// FirstLaunchProfile.cpp — on-console first-launch profile generator
// (agent GT, Claude round-5, 2026-10-08).
//
// Design source: outputs/2026-10-08-l25-prep/FIRST-LAUNCH-AUTOMATION.md
// (agent GI) — the detect -> generate -> persist -> self-test-handoff
// ladder with the fail-closed law (section 5b). Table schemas: GI's packed
// per-cell form (tables/ue5.6-prospero-shipping-nostats.example.json) and
// GJ's authoring form (blade-prep/per-version-table.json, laws block =
// the invariant contract). Anchor doctrine per GI's correction of GJ:
// census/consensus arms are load-bearing, patternsleuth patterns are
// ADVISORY probes only (they went 0/9 and 0/14 image-wide on the measured
// instance — a probe miss is never a refusal).
//
// TU laws (GI section 5 discipline):
//   * whole-TU __PROSPERO__-guarded; the non-PS5 compile sees nothing;
//   * READ-ONLY with respect to the game image: every byte read goes
//     through the PropertyReadBytes-class admission pattern (budgeted VQ
//     coverage + memcpy — NativePropertyReadBackend.inl semantics,
//     re-declared here because that backend is a static inside
//     UnrealInitializer.cpp's SeedArming namespace and deliberately not
//     callable cross-TU);
//   * code anchors are corroborated through the authenticated owned-copyin
//     signature seam (bridge_code_cache_signature) — never raw-dereferenced
//     (exec-only text is not CPU-readable on this console; the LEA census
//     arm therefore only fires on images with readable text and is never
//     load-bearing on XOM consoles — the utf16 census + V1/V2 word checks
//     carry the corroboration there);
//   * the ONLY writes are runtime-owned state files under
//     <modbridge-root>/profiles/ (GI section 2c; the mod manager never
//     manages this file);
//   * fail-closed: every refusal is terminal for this launch, loud
//     ([FL] FAIL-CLOSED line), zero partial arming; the only non-terminal
//     rung is persist-failure-after-verification (session-arm + WARN per
//     GI 5b rung 5);
//   * one-shot per launch (no rung re-runs, no retry storm);
//   * NO title-ids, NO game names, NO per-game defaults (Rule 10 extended
//     to games): the modbridge root is passed in by the initializer using
//     the GA derivation law (derive_modbridge_root in the header).
//
// NOT-YET stubs (honest scope, first cut — see the agent-GT report):
//   * full LEA-xref census RANKING (candidate POOL construction for
//     label-less cell anchors) — cell-form anchors without labels refuse;
//   * the 30x3x2 cost solve over the gated 244-marker pack (first-cut
//     margin law documented at classify_detect in the header);
//   * pattern-scan (wildcard) candidate generation — advisory only.
//
// Wiring (root stages this; UnrealInitializer is NOT edited by this TU):
// RC_first_launch_try_profile(root, module_view_base()) must be called
// BEFORE the pinned-profile admission (consume_required_slot_seeds), as
// that admission's input source. See the report for the exact slot.

#if defined(__PROSPERO__)

#include <UE4SSProgram.hpp> // [FL-WIRING] root derivation source (new unit: unconstrained includes)
#include "FirstLaunchProfile.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <fmt/core.h>
#include <DynamicOutput/Output.hpp>
#include <Helpers/Utf16Utf8.hpp>

// Staging-local admission/profile units (same quoted-include resolution
// UnrealInitializer.cpp uses; they ride the -I set of the
// unreal-unrealinitializer template).
#include "PoolAdmissionEngine.hpp"
#include "ImageCoverage.hpp"
#include "CodeImageCache.hpp"

namespace rc::firstlaunch
{
    // ================================================================ //
    // Seams — the SAME extern "C" bridge surface the admission phases use
    // (UnrealInitializer.cpp SeedArming declarations). Linking this TU
    // into the PRX links against the existing bridge; nothing here
    // introduces a new engine surface.
    // ================================================================ //
    extern "C" uint32_t bridge_readable_span_count();
    extern "C" int bridge_readable_span_at(uint32_t index, uint64_t* lo, uint64_t* hi);
    extern "C" int bridge_code_cache_context(uint64_t, const rc::codecache::Metadata**);
    extern "C" int bridge_code_cache_signature(uint64_t, uint64_t, const uint8_t*, size_t);
    extern "C" int bridge_image_range(uint64_t, size_t, rc::imagecoverage::Hit*);

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

    namespace
    {
        // ---- narrow-first logging (st_send / send_narrow_diag doctrine):
        // format narrow with contiguous [FL] literals, widen ONCE via
        // Unicode::DecodeUtf8, stderr fwrite as the last-resort sink.
        auto fl_send(std::string_view narrow) noexcept -> void
        {
            try
            {
                RC::Output::send(RC::Unicode::DecodeUtf8(narrow));
            }
            catch (...)
            {
                try
                {
                    std::fwrite(narrow.data(), 1, narrow.size(), stderr);
                }
                catch (...)
                {
                }
            }
        }

        auto now_ms() -> uint64_t
        {
            return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::steady_clock::now().time_since_epoch())
                                             .count());
        }

        // ---- PropertyReadBytes-class budgeted read (the .inl pattern) ---
        // Same shape as SeedArming::PropertyReadBytes (NativePropertyRead-
        // Backend.inl): valid_range gate, per-op 1000ms budget, VQ coverage
        // proof, memcpy, post-read budget check.
        namespace admission = rc::pooladmission;

        struct FLReadOps
        {
            auto now_ms() const -> uint64_t
            {
                return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                 std::chrono::steady_clock::now().time_since_epoch())
                                                 .count());
            }
            auto query(uint64_t address) -> admission::Query
            {
                L13VqInfo info{};
                const int result = sceKernelVirtualQuery(reinterpret_cast<const void*>(address), 0, &info, sizeof(info));
                if (result != 0)
                {
                    return {admission::QueryKind::Unavailable, 0, 0, 0, 0, result};
                }
                return {admission::QueryKind::Available, reinterpret_cast<uint64_t>(info.start),
                        reinterpret_cast<uint64_t>(info.end), static_cast<uint32_t>(info.protection), info.flags,
                        result};
            }
            void log_region(uint64_t, size_t, const admission::Query&) {}
        };

        auto fl_read_bytes(uintptr_t address, size_t length, void* destination) -> bool
        {
            if (destination == nullptr || !admission::valid_range(static_cast<uint64_t>(address), length))
            {
                return false;
            }
            FLReadOps ops;
            admission::Budget budget(now_ms(), 1000);
            const auto proof = admission::coverage(ops, budget, static_cast<uint64_t>(address), length);
            if (proof.verdict != admission::Verdict::Readable || budget.expired(now_ms()))
            {
                return false;
            }
            std::memcpy(destination, reinterpret_cast<const void*>(address), length);
            return !budget.expired(now_ms());
        }

        // ================================================================ //
        // Ladder state (single-launch, single-threaded init discipline)
        // ================================================================ //
        struct RunState
        {
            bool tried = false;
            int outcome = Outcome_NotRun;
            std::string root;
            uint64_t base = 0;
            const rc::codecache::Metadata* image = nullptr;
            char sha8[9] = {0};
            Verdict tier = Verdict::Refused;
            DetectCounts detect{};
            unsigned detect_expected = 0;
            std::string detect_version;
            std::string table_id;
            std::vector<PinRecord> pins;
            std::vector<VerifyRequest> requests;
            std::vector<int> reported; // -1 unreported, 1 pass, 0 fail
            size_t window_left = 64u * 1024u * 1024u; // scan_envelope cap
            uint64_t ladder_deadline_ms = 0;
        };
        RunState g_state;

        auto fl_fail_closed(const std::string& reason) -> int
        {
            fl_send(fmt::format("[FL] FAIL-CLOSED reason={}\n", reason));
            g_state.outcome = Outcome_Refused;
            return g_state.outcome;
        }

        auto budget_expired() -> bool
        {
            return now_ms() >= g_state.ladder_deadline_ms;
        }

        // ---- file IO (cstdio; runtime-owned state files only) -----------
        auto read_file_capped(const char* path, size_t cap, std::string& out) -> bool
        {
            out.clear();
            if (path == nullptr || path[0] == '\0')
            {
                return false;
            }
            std::FILE* f = std::fopen(path, "rb");
            if (f == nullptr)
            {
                return false;
            }
            out.resize(cap);
            const size_t n = std::fread(out.data(), 1, cap, f);
            std::fclose(f);
            out.resize(n);
            return !out.empty();
        }

        // ================================================================ //
        // DETECT support — utf-16le marker scan over admission-owned image
        // spans (engine_match.py::scan_markers semantics, in memory)
        // ================================================================ //
        struct Span
        {
            uint64_t lo, hi;
        };

        auto collect_image_spans(std::vector<Span>& spans) -> bool
        {
            const uint32_t count = bridge_readable_span_count();
            if (count == 0)
            {
                return false;
            }
            const uint64_t img_lo = g_state.base;
            const uint64_t img_hi = g_state.image->extent > UINT64_MAX - g_state.base
                                        ? UINT64_MAX
                                        : g_state.base + g_state.image->extent;
            for (uint32_t i = 0; i < count && i < 4096; ++i)
            {
                uint64_t lo = 0, hi = 0;
                if (bridge_readable_span_at(i, &lo, &hi) != 1 || hi <= lo)
                {
                    continue;
                }
                if (hi <= img_lo || lo >= img_hi)
                {
                    continue; // not the image
                }
                lo = lo > img_lo ? lo : img_lo;
                hi = hi < img_hi ? hi : img_hi;
                spans.push_back({lo, hi});
            }
            return !spans.empty();
        }

        // Walks spans chunked with needle-length overlap so straddling
        // occurrences survive chunking; marks presence flags. Returns
        // false when the window/budget cap hit mid-scan (incomplete scan:
        // presence can only be undercounted — the caller fail-closes).
        auto scan_markers_over_spans(const std::vector<Span>& spans,
                                     const std::vector<std::vector<uint8_t>>& needles,
                                     std::vector<bool>& present) -> bool
        {
            present.assign(needles.size(), false);
            size_t max_needle = 1;
            for (const auto& n : needles)
            {
                max_needle = n.size() > max_needle ? n.size() : max_needle;
            }
            constexpr size_t kChunk = 64 * 1024;
            std::vector<uint8_t> buf(kChunk + max_needle);
            size_t window_left = g_state.window_left;
            for (const auto& span : spans)
            {
                uint64_t pos = span.lo;
                while (pos < span.hi)
                {
                    if (budget_expired() || window_left == 0)
                    {
                        return false;
                    }
                    const uint64_t remaining = span.hi - pos;
                    const size_t n = static_cast<size_t>(std::min<uint64_t>(kChunk, remaining));
                    const size_t take = static_cast<size_t>(
                        std::min<uint64_t>(n + (remaining > kChunk ? max_needle - 1 : 0), remaining));
                    if (!fl_read_bytes(static_cast<uintptr_t>(pos), take, buf.data()))
                    {
                        pos += take > 0 ? take : 1; // unreadable slice: skip
                        continue;
                    }
                    scan_present(buf.data(), take, needles, present);
                    window_left -= std::min(window_left, take);
                    pos += take > max_needle ? take - (max_needle - 1) : take;
                }
            }
            return true;
        }

        auto count_present_flags(const std::vector<bool>& present) -> unsigned
        {
            unsigned c = 0;
            for (const bool p : present)
            {
                if (p)
                {
                    ++c;
                }
            }
            return c;
        }

        // Engine-level UTF-16 module-string census set (SeedTable
        // derivation receipt; FMallocBinned3 corroborates GMalloc). These
        // are ENGINE facts, not game facts (Rule 10 clean).
        const std::vector<std::string>& utf16_census_strings()
        {
            static const std::vector<std::string> kSet = {
                "Engine", "Renderer", "AnimGraphRuntime", "Landscape", "RenderCore", "FMallocBinned3",
            };
            return kSet;
        }

        // Bounded LEA/mov rip-relative census COUNT at one candidate label
        // (corroboration arm only — never selection; GJ law). No
        // instruction decoding: mid-instruction byte coincidences can only
        // inflate a corroboration count, and selection never depends on
        // it. Only fires where text is CPU-readable (XOM consoles: 0).
        auto lea_census_count(const std::vector<Span>& spans, uint64_t target) -> unsigned
        {
            unsigned hits = 0;
            size_t window_left = g_state.window_left;
            constexpr size_t kChunk = 64 * 1024;
            std::vector<uint8_t> buf(kChunk + 16);
            for (const auto& span : spans)
            {
                uint64_t pos = span.lo;
                while (pos < span.hi)
                {
                    if (budget_expired() || window_left == 0)
                    {
                        return hits;
                    }
                    const size_t n = static_cast<size_t>(std::min<uint64_t>(kChunk, span.hi - pos));
                    if (!fl_read_bytes(static_cast<uintptr_t>(pos), n, buf.data()))
                    {
                        pos += n > 0 ? n : 1;
                        continue;
                    }
                    window_left -= std::min(window_left, n);
                    if (n >= 8)
                    {
                        for (size_t i = 0; i + 7 <= n; ++i)
                        {
                            const uint8_t rex = buf[i];
                            if (rex != 0x48 && rex != 0x4C)
                            {
                                continue;
                            }
                            const uint8_t op = buf[i + 1];
                            if (op != 0x8B && op != 0x8D)
                            {
                                continue;
                            }
                            const uint8_t modrm = buf[i + 2];
                            if ((modrm & 0xC7) != 0x05)
                            {
                                continue; // rip-relative form required
                            }
                            uint64_t disp = 0;
                            for (int b = 0; b < 4; ++b)
                            {
                                disp |= static_cast<uint64_t>(buf[i + 3 + b]) << (8 * b);
                            }
                            if (pos + i + 7 + disp == target)
                            {
                                ++hits;
                                if (hits >= (1u << 20))
                                {
                                    return hits; // census saturation guard
                                }
                            }
                        }
                    }
                    pos += n > 15 ? n - 15 : n; // overlap keeps straddling sites countable
                }
            }
            return hits;
        }

        // ---- budgeted little-endian readers ------------------------------
        auto read_u64_at(uint64_t address, uint64_t& out) -> bool
        {
            uint8_t b[8];
            if (!fl_read_bytes(static_cast<uintptr_t>(address), 8, b))
            {
                return false;
            }
            uint64_t v = 0;
            for (int i = 0; i < 8; ++i)
            {
                v |= static_cast<uint64_t>(b[i]) << (8 * i);
            }
            out = v;
            return true;
        }

        auto read_u32_at(uint64_t address, uint32_t& out) -> bool
        {
            uint8_t b[4];
            if (!fl_read_bytes(static_cast<uintptr_t>(address), 4, b))
            {
                return false;
            }
            uint32_t v = 0;
            for (int i = 0; i < 4; ++i)
            {
                v |= static_cast<uint32_t>(b[i]) << (8 * i);
            }
            out = v;
            return true;
        }

        // Range class from the authenticated live metadata. The
        // single-image rc_seed::classify_seed_address constants are
        // Townfall-pinned and deliberately NOT reused here: the generator
        // needs the live image's own segment classes.
        auto segment_flags_at(uint64_t runtime_addr, uint32_t& flags) -> bool
        {
            if (g_state.image == nullptr || !g_state.image->ready)
            {
                return false;
            }
            const uint64_t rel = runtime_addr - g_state.base;
            for (size_t i = 0; i < g_state.image->segment_count; ++i)
            {
                const auto& s = g_state.image->segments[i];
                if (rel >= s.vaddr && rel < s.vaddr + s.memsz)
                {
                    flags = s.flags;
                    return true;
                }
            }
            return false;
        }

        auto is_data_readable_class(uint64_t runtime_addr) -> bool
        {
            uint32_t flags = 0;
            if (!segment_flags_at(runtime_addr, flags))
            {
                return false;
            }
            return (flags & 4u) != 0 && (flags & 1u) == 0; // PF_R && !PF_X
        }

        auto is_exec_class(uint64_t runtime_addr) -> bool
        {
            uint32_t flags = 0;
            if (!segment_flags_at(runtime_addr, flags))
            {
                return false;
            }
            return (flags & 1u) != 0; // PF_X
        }

        // ================================================================ //
        // Table loading (shipped asset under <root>/tables)
        // ================================================================ //
        struct LoadedTables
        {
            std::vector<LayoutTable> tables;
            std::vector<std::string> marker_union; // usable marker strings
            bool any_placeholder = false;
        };

        auto load_tables(const std::string& root, LoadedTables& loaded) -> bool
        {
            std::error_code ec;
            const std::filesystem::path dir = std::filesystem::path(root) / "tables";
            if (!std::filesystem::is_directory(dir, ec))
            {
                return false;
            }
            static constexpr const char* kShippedSuffix = ".layout-table.json";
            const size_t suffix_len = std::strlen(kShippedSuffix);
            for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
            {
                if (ec)
                {
                    break;
                }
                if (!entry.is_regular_file())
                {
                    continue;
                }
                const std::string name = entry.path().filename().string();
                const bool shipped = name.size() > suffix_len &&
                                     name.compare(name.size() - suffix_len, suffix_len, kShippedSuffix) == 0;
                const bool authoring = name == "per-version-table.json"; // GJ authoring name (packer renames at ship)
                if (!shipped && !authoring)
                {
                    continue;
                }
                std::string text;
                if (!read_file_capped(entry.path().string().c_str(), kMaxTableJsonBytes, text))
                {
                    continue;
                }
                LayoutTable table;
                std::string error;
                if (!parse_layout_table(text, table, error))
                {
                    fl_send(fmt::format("[FL] table refused file={} error={}\n", name, error));
                    continue;
                }
                if (table.markers_placeholder)
                {
                    loaded.any_placeholder = true;
                }
                for (const auto& m : table.triage_markers)
                {
                    if (std::find(loaded.marker_union.begin(), loaded.marker_union.end(), m) ==
                        loaded.marker_union.end())
                    {
                        loaded.marker_union.push_back(m);
                    }
                }
                loaded.tables.push_back(std::move(table));
            }
            return !loaded.tables.empty();
        }

        // ================================================================ //
        // GENERATE — candidates from the winning row + census/consensus arms
        // ================================================================ //
        struct GenerationInput
        {
            const VersionRow* row = nullptr;
            const LayoutTable* table = nullptr;
            std::vector<Span> spans;
            std::vector<std::vector<uint8_t>> utf16_needles;
            std::vector<bool> utf16_present;
        };

        auto utf16_census_distinct(const GenerationInput& in) -> unsigned
        {
            // distinct engine module strings found (first five entries of
            // utf16_census_strings; FMallocBinned3 is the GMalloc-specific
            // corroborator and is counted by the GMalloc arm separately)
            unsigned distinct = 0;
            for (size_t i = 0; i < in.utf16_present.size() && i < 5; ++i)
            {
                if (in.utf16_present[i])
                {
                    ++distinct;
                }
            }
            return distinct;
        }

        auto utf16_census_fmalloc(const GenerationInput& in) -> bool
        {
            return in.utf16_present.size() >= 6 && in.utf16_present[5];
        }

        auto anchor_row_find(const VersionRow& row, const char* id) -> const AnchorRow*
        {
            for (const auto& a : row.anchors)
            {
                if (a.id == id)
                {
                    return &a;
                }
            }
            return nullptr;
        }

        auto member_offset(const VersionRow& row, const char* struct_name, const char* member, uint64_t& out) -> bool
        {
            for (const auto& m : row.members)
            {
                if (m.struct_name == struct_name && m.member == member)
                {
                    out = m.offset;
                    return true;
                }
            }
            return false;
        }

        // Cheap shape walk for the data anchors the tables drive today.
        // Every constant comes from the row (member offsets / pool shape);
        // nothing is guessed. GMalloc's V1/V2 is the decisive word-pair
        // check (instance + vtable labels from the SAME measured row).
        auto shape_check_data_anchor(const GenerationInput& in, const AnchorRow& a, uint64_t runtime_addr,
                                     uint64_t& corroboration) -> bool
        {
            corroboration = 0;
            if (a.id == "FNamePool")
            {
                uint32_t current_block = 0, current_byte = 0;
                if (!read_u32_at(runtime_addr + in.row->pool.current_block_off, current_block))
                {
                    return false;
                }
                if (!read_u32_at(runtime_addr + in.row->pool.current_byte_off, current_byte))
                {
                    return false;
                }
                if (current_block >= in.row->pool.max_blocks || current_byte >= in.row->pool.block_capacity_bytes)
                {
                    return false;
                }
                corroboration = current_block;
                return true;
            }
            if (a.id == "GUObjectArray")
            {
                uint64_t obj_objects_off = 16;
                member_offset(*in.row, "FUObjectArray", "ObjObjects", obj_objects_off);
                uint64_t max_elements_off = 16, num_elements_off = 20, max_chunks_off = 24, num_chunks_off = 28;
                member_offset(*in.row, "TUObjectArray", "MaxElements", max_elements_off);
                member_offset(*in.row, "TUObjectArray", "NumElements", num_elements_off);
                member_offset(*in.row, "TUObjectArray", "MaxChunks", max_chunks_off);
                member_offset(*in.row, "TUObjectArray", "NumChunks", num_chunks_off);
                uint64_t arr_ptr = 0;
                if (!read_u64_at(runtime_addr + obj_objects_off, arr_ptr))
                {
                    return false;
                }
                uint32_t num_elements = 0, max_elements = 0, num_chunks = 0, max_chunks = 0;
                if (!read_u32_at(arr_ptr + num_elements_off, num_elements) ||
                    !read_u32_at(arr_ptr + max_elements_off, max_elements) ||
                    !read_u32_at(arr_ptr + num_chunks_off, num_chunks) ||
                    !read_u32_at(arr_ptr + max_chunks_off, max_chunks))
                {
                    return false;
                }
                if (max_elements < num_elements || max_chunks < num_chunks || num_chunks == 0 ||
                    num_chunks > (1u << 20) || num_elements == 0 || num_elements > (1u << 28))
                {
                    return false;
                }
                corroboration = num_elements;
                return true;
            }
            if (a.id == "GMalloc")
            {
                const AnchorRow* instance = anchor_row_find(*in.row, "GMallocInstance");
                const AnchorRow* vtable = anchor_row_find(*in.row, "GMallocVtable");
                if (instance == nullptr || vtable == nullptr || !instance->has_label || !vtable->has_label)
                {
                    return false; // V1/V2 needs both derived labels
                }
                const uint64_t instance_addr = g_state.base + instance->image_vaddr;
                const uint64_t vtable_addr = g_state.base + vtable->image_vaddr;
                uint64_t word = 0;
                if (!read_u64_at(runtime_addr, word) || word != instance_addr)
                {
                    return false; // V1 failed
                }
                if (!read_u64_at(instance_addr, word) || word != vtable_addr)
                {
                    return false; // V2 failed
                }
                corroboration = vtable_addr;
                return true;
            }
            if (a.id == "GMallocInstance" || a.id == "GMallocVtable")
            {
                uint64_t word = 0;
                if (!read_u64_at(runtime_addr, word))
                {
                    return false;
                }
                corroboration = word;
                return is_data_readable_class(runtime_addr);
            }
            return false; // unknown data anchor: never guess a shape
        }

        auto make_pin(const AnchorRow& a, uint64_t runtime_addr) -> PinRecord
        {
            PinRecord p{};
            std::snprintf(p.anchor_id, sizeof(p.anchor_id), "%s", a.id.c_str());
            p.image_relative = runtime_addr - g_state.base;
            p.runtime_address = runtime_addr;
            p.size = a.size != 0 ? a.size : 8;
            p.prologue_len = 0;
            p.verified = 0;
            if (a.is_code && a.has_prefix)
            {
                const size_t n = std::min<size_t>(a.prefix.size(), sizeof(p.prologue));
                std::memcpy(p.prologue, a.prefix.bytes.data(), n);
                p.prologue_len = static_cast<uint32_t>(n);
                if (p.size < p.prologue_len)
                {
                    p.size = p.prologue_len;
                }
            }
            return p;
        }

        auto make_request(const AnchorRow& a, uint64_t runtime_addr) -> VerifyRequest
        {
            VerifyRequest r{};
            std::snprintf(r.anchor_id, sizeof(r.anchor_id), "%s", a.id.c_str());
            r.address = runtime_addr;
            std::snprintf(r.probe_kind, sizeof(r.probe_kind), "%s", a.verify_kind.c_str());
            return r;
        }

        auto push_pin_and_request(const AnchorRow& a, uint64_t addr) -> void
        {
            const VerifyRequest req = make_request(a, addr);
            fl_send(fmt::format("[FL] verify request {} probe={} addr=0x{:X}\n", req.anchor_id, req.probe_kind,
                                req.address));
            g_state.requests.push_back(req);
            g_state.reported.push_back(-1);
            g_state.pins.push_back(make_pin(a, addr));
        }

        // Arm evaluation for one anchor. Law:
        //   code: authenticated prefix signature positive => DECISIVE
        //         (+100); utf16 census >= 3 => +1; lea census >= 1 => +1
        //         (readable-text images only)
        //   data: GMalloc V1/V2 word equality => DECISIVE (+100);
        //         table-driven shape check => +1; utf16 census >= 3 => +1
        //         (FMallocBinned3 present additionally corroborates
        //         GMalloc); lea census >= 1 => +1 (readable-text only)
        // A decisive arm satisfies any min_arms; otherwise the census/
        // consensus arms must reach agreement.min_arms (default 2).
        auto evaluate_anchor_arms(const GenerationInput& in, const AnchorRow& a, uint64_t addr, unsigned& arms,
                                  unsigned& census) -> bool
        {
            arms = 0;
            census = 0;
            uint64_t corroboration = 0;
            const unsigned utf16_distinct = utf16_census_distinct(in);
            if (a.is_code)
            {
                if (a.has_prefix &&
                    bridge_code_cache_signature(g_state.base, addr, a.prefix.bytes.data(), a.prefix.size()) == 1)
                {
                    arms += 100; // decisive (authenticated bytes agree)
                }
                if (utf16_distinct >= 3)
                {
                    ++arms;
                }
                census = lea_census_count(in.spans, addr);
                if (census >= 1)
                {
                    ++arms;
                }
            }
            else
            {
                if (a.id == "GMalloc")
                {
                    if (shape_check_data_anchor(in, a, addr, corroboration))
                    {
                        arms += 100; // V1/V2 word equality is decisive
                    }
                    if (utf16_census_fmalloc(in))
                    {
                        ++arms;
                    }
                }
                else
                {
                    if (shape_check_data_anchor(in, a, addr, corroboration))
                    {
                        ++arms;
                    }
                    if (utf16_distinct >= 3)
                    {
                        ++arms;
                    }
                }
                census = lea_census_count(in.spans, addr);
                if (census >= 1)
                {
                    ++arms;
                }
            }
            return arms >= a.min_arms;
        }

        // The generate rung. All-or-nothing over the REQUIRED set (the
        // SeedTable slots 0/2/3/4 + the FNamePool label the ctor probe
        // needs); optional anchors register only when they corroborate.
        auto generate_pins(const GenerationInput& in) -> bool
        {
            for (size_t k = 0; k < kRequiredAnchorCount; ++k)
            {
                const char* id = kRequiredAnchors[k];
                const AnchorRow* a = anchor_row_find(*in.row, id);
                if (a == nullptr || !a->has_label || a->image_vaddr == 0)
                {
                    fl_send(fmt::format("[FL] FAIL-CLOSED reason=anchor-{}-no-table-label-census-pool-NOT-YET\n", id));
                    g_state.outcome = Outcome_Refused;
                    return false;
                }
                const uint64_t addr = g_state.base + a->image_vaddr;

                // live range class (fail-closed on mismatch)
                const bool class_ok = a->is_code ? is_exec_class(addr) : is_data_readable_class(addr);
                if (!class_ok)
                {
                    fl_send(fmt::format("[FL] FAIL-CLOSED reason=anchor-{}-class-mismatch\n", id));
                    g_state.outcome = Outcome_Refused;
                    return false;
                }

                unsigned arms = 0, census = 0;
                if (!evaluate_anchor_arms(in, *a, addr, arms, census))
                {
                    fl_send(fmt::format("[FL] FAIL-CLOSED reason=anchor-{}-arm-disagreement arms={}/{} census={}\n",
                                        id, arms, a->min_arms, census));
                    g_state.outcome = Outcome_Refused;
                    return false;
                }
                push_pin_and_request(*a, addr);
                fl_send(fmt::format("[FL] pin {} addr=0x{:X} verified=0 arms={}/{} census={}\n", id, addr, arms,
                                    a->min_arms, census));
            }

            static const char* kOptionalAnchors[] = {
                "FNameToStringInternal", "EngineAddDeleteListener", "EngineRemoveDeleteListener",
                "GMallocInstance", "GMallocVtable",
            };
            for (const char* id : kOptionalAnchors)
            {
                const AnchorRow* a = anchor_row_find(*in.row, id);
                if (a == nullptr || !a->has_label || a->image_vaddr == 0)
                {
                    continue;
                }
                const uint64_t addr = g_state.base + a->image_vaddr;
                const bool class_ok = a->is_code ? is_exec_class(addr) : is_data_readable_class(addr);
                if (!class_ok)
                {
                    continue;
                }
                if (a->is_code)
                {
                    // optional code pins need the DECISIVE arm (exact
                    // authenticated prefix); never census-only
                    if (!a->has_prefix ||
                        bridge_code_cache_signature(g_state.base, addr, a->prefix.bytes.data(), a->prefix.size()) != 1)
                    {
                        continue;
                    }
                }
                else
                {
                    uint64_t corroboration = 0;
                    if (!shape_check_data_anchor(in, *a, addr, corroboration))
                    {
                        continue;
                    }
                }
                push_pin_and_request(*a, addr);
                fl_send(fmt::format("[FL] pin {} addr=0x{:X} verified=0 arms=optional census=opt\n", id, addr));
            }
            return true;
        }

        // ================================================================ //
        // PERSIST / LOAD — <root>/profiles/<image-sha8>.profile.json
        // ================================================================ //
        auto profile_path() -> std::string
        {
            return (std::filesystem::path(g_state.root) / "profiles" /
                    (std::string(g_state.sha8) + ".profile.json"))
                .string();
        }

        auto json_escape(const std::string& s) -> std::string
        {
            std::string out;
            for (char c : s)
            {
                if (c == '"' || c == '\\')
                {
                    out.push_back('\\');
                }
                out.push_back(c);
            }
            return out;
        }

        auto persist_profile() -> bool
        {
            std::error_code ec;
            const std::filesystem::path dir = std::filesystem::path(g_state.root) / "profiles";
            std::filesystem::create_directories(dir, ec);
            if (ec)
            {
                return false;
            }
            std::string j;
            j.reserve(2048);
            char hex[32];
            j += "{\"schema\":\"rc-firstlaunch-profile/1\",\"image\":{\"sha256\":\"";
            j += g_state.image ? g_state.image->sha256 : "";
            j += "\",\"extent\":";
            j += std::to_string(g_state.image ? g_state.image->extent : 0);
            j += ",\"base_status\":\"observed-live\",\"base_note\":\"ASLR; base never persisted\"},";
            j += "\"cell\":{\"table_id\":\"";
            j += json_escape(g_state.table_id);
            j += "\",\"tier\":\"";
            j += verdict_name(g_state.tier);
            j += "\",\"version\":\"";
            j += json_escape(g_state.detect_version);
            j += "\",\"present_count\":";
            j += std::to_string(g_state.detect.present_count);
            j += ",\"expected_present\":";
            j += std::to_string(g_state.detect_expected);
            j += "},\"pins\":[";
            for (size_t i = 0; i < g_state.pins.size(); ++i)
            {
                const auto& p = g_state.pins[i];
                if (i != 0)
                {
                    j += ',';
                }
                j += "{\"anchor\":\"";
                j += json_escape(p.anchor_id);
                j += "\",\"image_relative\":\"0x";
                std::snprintf(hex, sizeof(hex), "%llX", static_cast<unsigned long long>(p.image_relative));
                j += hex;
                j += "\",\"size\":";
                j += std::to_string(p.size);
                j += ",\"prologue_hex\":\"";
                for (uint32_t b = 0; b < p.prologue_len; ++b)
                {
                    std::snprintf(hex, sizeof(hex), "%02X", p.prologue[b]);
                    j += hex;
                }
                j += "\",\"verify\":\"";
                j += (i < g_state.requests.size()) ? json_escape(g_state.requests[i].probe_kind)
                                                   : std::string("behavioral_generic");
                j += "\",\"verified\":";
                j += p.verified ? "true" : "false";
                j += "}";
            }
            j += "]}\n";
            std::FILE* f = std::fopen(profile_path().c_str(), "wb");
            if (f == nullptr)
            {
                return false;
            }
            const size_t n = std::fwrite(j.data(), 1, j.size(), f);
            std::fclose(f);
            return n == j.size();
        }

        // Load + re-verify (GI 2c): image identity, code-pin prologues via
        // the authenticated signature seam, data-pin readability probes.
        // Any mismatch => verified=0 => the caller regenerates (a game
        // patch changing the image sha is the designed regenerate path,
        // not an error). NO absolute runtime addresses are read from the
        // file (ASLR law): image_relative + live base only.
        auto make_pin_from_loaded(const std::string& anchor, uint64_t rel, uint64_t addr, const PatternBytes* pb)
            -> PinRecord
        {
            PinRecord p{};
            std::snprintf(p.anchor_id, sizeof(p.anchor_id), "%s", anchor.c_str());
            p.image_relative = rel;
            p.runtime_address = addr;
            p.size = 8;
            p.verified = 1;
            p.prologue_len = 0;
            if (pb != nullptr)
            {
                const size_t n = std::min<size_t>(pb->size(), sizeof(p.prologue));
                std::memcpy(p.prologue, pb->bytes.data(), n);
                p.prologue_len = static_cast<uint32_t>(n);
                p.size = static_cast<uint32_t>(n);
            }
            return p;
        }

        auto try_load_profile(bool& loaded, bool& verified) -> void
        {
            loaded = false;
            verified = false;
            std::string text;
            if (!read_file_capped(profile_path().c_str(), 1u * 1024u * 1024u, text))
            {
                return; // absent slot is not an error
            }
            loaded = true;
            JValue root;
            std::string error;
            if (!parse_json(text, root, error))
            {
                fl_send(fmt::format("[FL] profile loaded={} verified=0 parse-error={}\n", g_state.sha8, error));
                return;
            }
            const JValue* image = root.find("image");
            const JValue* sha = image != nullptr ? image->find("sha256") : nullptr;
            if (sha == nullptr || sha->type != JValue::Type::Str ||
                sha->str != (g_state.image ? g_state.image->sha256 : ""))
            {
                fl_send(fmt::format("[FL] profile loaded={} verified=0 image-sha-mismatch\n", g_state.sha8));
                return;
            }
            const JValue* pins = root.find("pins");
            if (pins == nullptr || pins->type != JValue::Type::Arr || pins->arr.empty())
            {
                fl_send(fmt::format("[FL] profile loaded={} verified=0 no-pins\n", g_state.sha8));
                return;
            }
            std::vector<PinRecord> candidate_pins;
            for (const auto& pv : pins->arr)
            {
                if (pv.type != JValue::Type::Obj)
                {
                    continue;
                }
                std::string anchor, prologue_hex;
                uint64_t rel = 0;
                get_str(pv, "anchor", anchor);
                get_u64(pv, "image_relative", rel);
                get_str(pv, "prologue_hex", prologue_hex);
                if (anchor.empty() || rel == 0)
                {
                    continue;
                }
                const uint64_t addr = g_state.base + rel;
                bool ok = false;
                PatternBytes pb;
                const bool has_prologue = !prologue_hex.empty() && parse_pattern(prologue_hex, pb);
                if (has_prologue)
                {
                    ok = bridge_code_cache_signature(g_state.base, addr, pb.bytes.data(), pb.size()) == 1;
                }
                else
                {
                    uint64_t word = 0;
                    ok = is_data_readable_class(addr) && read_u64_at(addr, word);
                }
                if (!ok)
                {
                    fl_send(fmt::format("[FL] profile loaded={} verified=0 anchor-{}-reverify-failed\n", g_state.sha8,
                                        anchor));
                    return;
                }
                candidate_pins.push_back(make_pin_from_loaded(anchor, rel, addr, has_prologue ? &pb : nullptr));
            }
            if (candidate_pins.empty())
            {
                fl_send(fmt::format("[FL] profile loaded={} verified=0 empty-pins\n", g_state.sha8));
                return;
            }
            g_state.pins = std::move(candidate_pins);
            verified = true;
            fl_send(fmt::format("[FL] profile loaded={} verified=1 pins={}\n", g_state.sha8, g_state.pins.size()));
        }
    } // namespace
} // namespace rc::firstlaunch

// ============================ extern "C" ABI ============================ //
// [FL-WIRING root, AC execution] Zero-arg entry for the initializer call
// site: derives the modbridge root from the RUNTIME-resolved mods
// directory (the GA/Rule-10 law) INSIDE this TU — the initializer unit's
// include universe is frozen (UE4SSProgram.hpp drags <regex> past its
// include graph gate), so the derivation lives here where headers are
// unconstrained (this is a new unit).
extern "C" int RC_first_launch_try_auto(uint64_t image_base)
{
    try
    {
        std::error_code ec;
        std::filesystem::path mods_dir = std::filesystem::weakly_canonical(
            std::filesystem::path(UE4SSProgram::get_program().get_mods_directory()), ec);
        if (ec)
        {
            return RC_first_launch_try_profile("", image_base); // loud no-root inside
        }
        if (mods_dir.filename() == "Mods")
        {
            mods_dir = mods_dir.parent_path(); // <root>/ue4ss
        }
        mods_dir = mods_dir.parent_path();     // <root>
        return RC_first_launch_try_profile(mods_dir.string().c_str(), image_base);
    }
    catch (...)
    {
        return RC_first_launch_try_profile("", image_base);
    }
}

extern "C" int RC_first_launch_try_profile(const char* modbridge_root, uint64_t image_base)
{
    using namespace rc::firstlaunch;
    if (g_state.tried)
    {
        return g_state.outcome; // one-shot law
    }
    g_state.tried = true;
    g_state.outcome = Outcome_Refused;
    g_state.ladder_deadline_ms = now_ms() + rc::pooladmission::kAdmissionBudgetMs;

    if (modbridge_root == nullptr || modbridge_root[0] == '\0')
    {
        return fl_fail_closed("no-modbridge-root (derive per GA law; no title-id default exists)");
    }
    g_state.root = modbridge_root;
    g_state.base = image_base;
    if (image_base == 0)
    {
        return fl_fail_closed("no-image-base (initializer must pass module_view_base())");
    }
    if (bridge_code_cache_context(image_base, &g_state.image) != 1 || g_state.image == nullptr ||
        !g_state.image->ready)
    {
        return fl_fail_closed("no-image-context (startup authentication absent)");
    }
    std::snprintf(g_state.sha8, sizeof(g_state.sha8), "%.8s", g_state.image->sha256);
    g_state.window_left = 64u * 1024u * 1024u;

    // ---- [FL-0] profile slot ------------------------------------------
    bool loaded = false, verified = false;
    try_load_profile(loaded, verified);
    if (loaded && verified)
    {
        g_state.outcome = Outcome_LoadedVerified;
        return g_state.outcome;
    }
    if (loaded)
    {
        fl_send(fmt::format("[FL] profile slot=regenerate sha8={}\n", g_state.sha8));
    }
    else
    {
        fl_send("[FL] profile slot=absent\n");
    }

    // ---- [FL-2a] DETECT ------------------------------------------------
    LoadedTables loaded_tables;
    if (!load_tables(g_state.root, loaded_tables) || loaded_tables.tables.empty())
    {
        return fl_fail_closed("no-tables (shipped asset <root>/tables/*.layout-table.json missing)");
    }
    if (loaded_tables.marker_union.empty())
    {
        return fl_fail_closed("detect-no-markers (tables shipped placeholder marker lists only)");
    }
    std::vector<Span> spans;
    if (!collect_image_spans(spans))
    {
        return fl_fail_closed("no-readable-image-spans");
    }
    std::vector<std::vector<uint8_t>> marker_needles;
    marker_needles.reserve(loaded_tables.marker_union.size());
    for (const auto& m : loaded_tables.marker_union)
    {
        marker_needles.push_back(utf16le_needle(m));
    }
    std::vector<bool> marker_present;
    if (!scan_markers_over_spans(spans, marker_needles, marker_present))
    {
        return fl_fail_closed("detect-scan-cap (window/budget exhausted)");
    }
    DetectCounts counts{};
    counts.triage_total = static_cast<unsigned>(marker_needles.size());
    counts.triage_present = count_present_flags(marker_present);
    counts.present_count = counts.triage_present; // union == shipped full set today
    g_state.detect = counts;

    // Winning measured row: lowest packed version with a measured row
    // (documented first-cut; the true cost solve is NOT-YET; only ONE
    // measured row exists today — 5.6 — so this is exact in practice).
    const LayoutTable* best_table = nullptr;
    const VersionRow* best_row = nullptr;
    uint64_t best_packed = UINT64_MAX;
    for (const auto& table : loaded_tables.tables)
    {
        for (const auto& row : table.rows)
        {
            if (row.status != "measured" || row.version_lo == 0)
            {
                continue;
            }
            if (best_row == nullptr || row.version_lo < best_packed)
            {
                best_table = &table;
                best_row = &row;
                best_packed = row.version_lo;
            }
        }
    }
    const Thresholds thr = best_table ? best_table->thr : Thresholds{};
    g_state.tier = classify_detect(counts, thr);
    g_state.detect_expected = thr.expected_present;
    fl_send(fmt::format("[FL] detect verdict={} tier={} markers={}/{} triage={}/{}\n",
                        g_state.tier == Verdict::Refused ? "refuse" : "pass", verdict_name(g_state.tier),
                        counts.present_count, thr.markers_total, counts.triage_present, counts.triage_total));
    if (g_state.tier == Verdict::Refused)
    {
        return fl_fail_closed("detect-refused (not an Unreal image; the Astro-path outcome)");
    }
    if (g_state.tier == Verdict::FamilyOnly)
    {
        return fl_fail_closed("detect-family-only (versioned rows may not arm in this tier)");
    }
    if (best_table == nullptr || best_row == nullptr)
    {
        return fl_fail_closed("no-measured-table-row (seed bands pack to nothing; confirm_law upgrade required)");
    }
    g_state.table_id = best_table->table_id;
    g_state.detect_version = best_row->version;
    fl_send(fmt::format("[FL] table row={} version={} status=measured table={}\n", best_row->version,
                        best_row->version, best_table->table_id));

    // ---- [FL-2b] GENERATE ----------------------------------------------
    GenerationInput in;
    in.row = best_row;
    in.table = best_table;
    in.spans = spans;
    for (const auto& s : utf16_census_strings())
    {
        in.utf16_needles.push_back(utf16le_needle(s));
    }
    std::vector<bool> utf16_all;
    scan_markers_over_spans(spans, in.utf16_needles, utf16_all);
    in.utf16_present = utf16_all;

    if (!generate_pins(in))
    {
        return g_state.outcome; // fail-closed line already emitted
    }
    g_state.outcome = Outcome_PendingVerify;
    return g_state.outcome;
}

extern "C" int RC_first_launch_verify_request_count(void)
{
    using namespace rc::firstlaunch;
    return static_cast<int>(g_state.requests.size());
}

extern "C" int RC_first_launch_verify_request_at(int index, rc::firstlaunch::VerifyRequest* out)
{
    using namespace rc::firstlaunch;
    if (out == nullptr || index < 0 || static_cast<size_t>(index) >= g_state.requests.size())
    {
        return Outcome_Misuse;
    }
    *out = g_state.requests[static_cast<size_t>(index)];
    return 0;
}

extern "C" int RC_first_launch_report_verify(const char* anchor_id, int passed)
{
    using namespace rc::firstlaunch;
    if (!g_state.tried || g_state.outcome != Outcome_PendingVerify)
    {
        return Outcome_Misuse;
    }
    if (anchor_id == nullptr)
    {
        return Outcome_Misuse;
    }
    for (size_t i = 0; i < g_state.requests.size(); ++i)
    {
        if (std::string(g_state.requests[i].anchor_id) != anchor_id)
        {
            continue;
        }
        if (g_state.reported[i] != -1)
        {
            return Outcome_Misuse; // duplicate report
        }
        g_state.reported[i] = passed ? 1 : 0;
        for (auto& p : g_state.pins)
        {
            if (std::string(p.anchor_id) == anchor_id)
            {
                p.verified = passed ? 1u : 0u;
            }
        }
        if (!passed)
        {
            // a generated pin that fails its behavioral probe is a WRONG
            // pin by definition — refuse the whole profile (GI 2b)
            return fl_fail_closed(fmt::format("verify-refused anchor={} probe={}", anchor_id,
                                              g_state.requests[i].probe_kind));
        }
        return g_state.outcome;
    }
    return Outcome_Misuse; // unknown anchor
}

extern "C" int RC_first_launch_commit(void)
{
    using namespace rc::firstlaunch;
    if (!g_state.tried || g_state.outcome != Outcome_PendingVerify)
    {
        return Outcome_Misuse;
    }
    for (const int r : g_state.reported)
    {
        if (r != 1)
        {
            return Outcome_Misuse; // unreported or failed request
        }
    }
    if (!persist_profile())
    {
        // GI 5b rung 5: safety already proven (all behavioral verifies
        // passed); state just not yet durable. Session-arm + WARN; the
        // slot persists on a later launch.
        fl_send("[FL] persist failed session-armed=1 warn=persist-retry-next-launch\n");
        g_state.outcome = Outcome_Committed;
        return g_state.outcome;
    }
    fl_send(fmt::format("[FL] profile persisted={} sha8={} schema=1\n", profile_path(), g_state.sha8));
    fl_send("[FL] commit outcome=COMMITTED\n");
    g_state.outcome = Outcome_Committed;
    return g_state.outcome;
}

extern "C" int RC_first_launch_pin_count(void)
{
    using namespace rc::firstlaunch;
    return static_cast<int>(g_state.pins.size());
}

extern "C" int RC_first_launch_pin_at(int index, rc::firstlaunch::PinRecord* out)
{
    using namespace rc::firstlaunch;
    if (out == nullptr || index < 0 || static_cast<size_t>(index) >= g_state.pins.size())
    {
        return Outcome_Misuse;
    }
    *out = g_state.pins[static_cast<size_t>(index)];
    return 0;
}

extern "C" int RC_first_launch_outcome(void)
{
    using namespace rc::firstlaunch;
    return g_state.outcome;
}

#endif // __PROSPERO__
