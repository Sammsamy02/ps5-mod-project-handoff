#pragma once
// FirstLaunchProfile.hpp — host-visible API + host-pure logic for the
// on-console first-launch profile generator (agent GT, Claude round-5,
// 2026-10-08; design: outputs/2026-10-08-l25-prep/FIRST-LAUNCH-AUTOMATION.md,
// agent GI).
//
// What this is: GI's detect -> generate -> persist -> self-test-handoff
// ladder as a runtime TU. The compiled-in pinned profile
// (RuntimeBuildProfile.hpp / CodeSignatureProfile.hpp / SeedTable.h) becomes
// a runtime-GENERATED, runtime-VERIFIED, PERSISTED JSON produced on console
// from shipped layout tables. This header carries ONLY host-pure pieces so
// the host test (l25-prep/host-tests/firstlaunch-table-test.cpp) can drive
// the table loader and tier logic on the Mac with zero target dependencies:
//
//   * tier logic mirroring phasea-tools/engine_match.py thresholds
//     (REFUSED <3 triage / <8 present, TENTATIVE >=4, CONFIDENT >=12 —
//     FIRST-LAUNCH-AUTOMATION.md section 2a cites the same constants);
//   * a bounded, allocation-only, exception-free JSON reader sufficient for
//     the layout-table schema (both the packed per-cell form, schema
//     "ue-layout-table/1" — tables/ue5.6-prospero-shipping-nostats.example
//     .json — and the GJ authoring form, schema 1 /
//     blade-prep/per-version-table.json, whose measured 5.6 row carries the
//     version-level relative anchors the generator can drive TODAY);
//   * utf-16le needle encoding + buffer presence scan with byte-identical
//     semantics to engine_match.py::scan_markers (the image-walk wrapper is
//     target-only, in FirstLaunchProfile.cpp);
//   * the GA root-derivation law (LuaMod.cpp prepare_mod, agent GA round-5
//     item 4): the modbridge root is derived from the runtime-resolved mods
//     directory — NO title-id default exists (Rule 10 extended to games,
//     FIRST-LAUNCH-AUTOMATION.md finding F1).
//
// Everything target-side (readable-span census, admission-budgeted reads,
// prologue signature corroboration, persist/load of the profile slot, the
// extern "C" wiring hook) lives in candidate/src/FirstLaunchProfile.cpp and
// is whole-TU __PROSPERO__-guarded. Nothing in this header includes a UE4SS
// or platform header on purpose.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rc::firstlaunch
{
    // ------------------------------------------------------------------ //
    // Tier ladder — engine_match.py mirror (constants cited by GI section 2a)
    // ------------------------------------------------------------------ //

    // engine_match.py tier thresholds, verbatim:
    //   TRIAGE_MIN_PRESENT      = 3   fewer triage hits => REFUSED, no scan
    //   REFUSED_MAX_PRESENT     = 8   full scan below this => not Unreal
    //   VERSION_MARGIN_TENTATIVE= 4   == ref_axes.py resolution threshold
    //   VERSION_MARGIN_CONFIDENT= 12  == Townfall stats margin
    //   FAMILY_CONFIDENT_PRESENT= 25  enough markers to trust the family axis
    inline constexpr unsigned kTriageMinPresent = 3;
    inline constexpr unsigned kRefusedMaxPresent = 8;
    inline constexpr unsigned kVersionMarginTentative = 4;
    inline constexpr unsigned kVersionMarginConfident = 12;
    inline constexpr unsigned kFamilyConfidentPresent = 25;

    enum class Verdict : unsigned
    {
        Refused = 0,
        FamilyOnly = 1,
        Tentative = 2,
        Confident = 3,
    };

    [[nodiscard]] inline const char* verdict_name(Verdict v) noexcept
    {
        switch (v)
        {
        case Verdict::Refused: return "REFUSED";
        case Verdict::FamilyOnly: return "FAMILY-ONLY";
        case Verdict::Tentative: return "TENTATIVE";
        case Verdict::Confident: return "CONFIDENT";
        }
        return "UNKNOWN";
    }

    // Table-supplied thresholds (detection.expected_presence / defaults).
    // Every field defaults to the engine_match.py constant so an absent
    // block degrades to the proven ladder, never to a weaker one.
    struct Thresholds
    {
        unsigned triage_min = kTriageMinPresent;
        unsigned refused_max = kRefusedMaxPresent;
        unsigned margin_tentative = kVersionMarginTentative;
        unsigned margin_confident = kVersionMarginConfident;
        unsigned family_confident = kFamilyConfidentPresent;
        unsigned expected_present = 0; // cell expectation (Townfall cell: 54)
        unsigned markers_total = 244;  // patternsleuth pack size (30 versions)
    };

    struct DetectCounts
    {
        unsigned triage_present = 0; // hits within the triage subset
        unsigned triage_total = 0;   // size of the triage subset scanned
        unsigned present_count = 0;  // full-set presence (== triage when the
                                     // full scan was skipped after refusal)
    };

    // First-cut tier law (documented deviation, NOT a silent rewrite):
    // engine_match.py computes the version margin from its 30-version x
    // config x stats cost solve over the full gated 244-marker table. The
    // shipped per-cell tables carry thresholds + the expected presence
    // count, not the gated pack, so the cost-solve margin is NOT-YET
    // computable on console. This ladder keeps every engine_match
    // threshold and defines the first-cut margin as the distance above the
    // not-Unreal floor: margin = present_count - refused_max (Townfall:
    // 54-8=46 => CONFIDENT; the Astro Bot negative control 0-8<0 with
    // triage 0/16 => REFUSED at the cheap rung). A true cost-solve margin
    // requires shipping the gated marker pack; until then FAMILY_ONLY
    // never arms versioned rows (GI section 2a DESIGN clause).
    [[nodiscard]] inline Verdict classify_detect(const DetectCounts& c, const Thresholds& t) noexcept
    {
        if (c.triage_total != 0 && c.triage_present < t.triage_min)
        {
            return Verdict::Refused; // cheap refusal, full scan skipped
        }
        if (c.present_count < t.refused_max)
        {
            return Verdict::Refused; // below the not-Unreal evidence floor
        }
        const bool family_confident = c.present_count >= t.family_confident;
        const long margin = static_cast<long>(c.present_count) - static_cast<long>(t.refused_max);
        if (!family_confident)
        {
            return Verdict::FamilyOnly;
        }
        if (margin >= static_cast<long>(t.margin_confident))
        {
            return Verdict::Confident;
        }
        if (margin >= static_cast<long>(t.margin_tentative))
        {
            return Verdict::Tentative;
        }
        return Verdict::FamilyOnly;
    }

    // ------------------------------------------------------------------ //
    // utf-16le scan primitive (engine_match.py::scan_markers semantics)
    // ------------------------------------------------------------------ //

    // UTF-8 -> UTF-16LE needle bytes (ASCII fast path; full code points
    // with surrogate pairs for non-ASCII markers).
    [[nodiscard]] inline std::vector<uint8_t> utf16le_needle(std::string_view text)
    {
        std::vector<uint8_t> out;
        out.reserve(text.size() * 2);
        size_t i = 0;
        while (i < text.size())
        {
            const auto lead = static_cast<unsigned char>(text[i]);
            uint32_t cp = 0xFFFD; // replacement on malformed input
            size_t len = 1;
            if (lead < 0x80)
            {
                cp = lead;
            }
            else if ((lead & 0xE0) == 0xC0 && i + 1 < text.size())
            {
                cp = (static_cast<uint32_t>(lead & 0x1F) << 6) |
                     static_cast<uint32_t>(static_cast<unsigned char>(text[i + 1]) & 0x3F);
                len = 2;
            }
            else if ((lead & 0xF0) == 0xE0 && i + 2 < text.size())
            {
                cp = (static_cast<uint32_t>(lead & 0x0F) << 12) |
                     (static_cast<uint32_t>(static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6) |
                     static_cast<uint32_t>(static_cast<unsigned char>(text[i + 2]) & 0x3F);
                len = 3;
            }
            else if ((lead & 0xF8) == 0xF0 && i + 3 < text.size())
            {
                cp = (static_cast<uint32_t>(lead & 0x07) << 18) |
                     (static_cast<uint32_t>(static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12) |
                     (static_cast<uint32_t>(static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6) |
                     static_cast<uint32_t>(static_cast<unsigned char>(text[i + 3]) & 0x3F);
                len = 4;
            }
            i += len;
            if (cp <= 0xFFFF)
            {
                out.push_back(static_cast<uint8_t>(cp & 0xFF));
                out.push_back(static_cast<uint8_t>((cp >> 8) & 0xFF));
            }
            else
            {
                const uint32_t v = cp - 0x10000;
                const uint16_t hi = static_cast<uint16_t>(0xD800 + (v >> 10));
                const uint16_t lo = static_cast<uint16_t>(0xDC00 + (v & 0x3FF));
                out.push_back(static_cast<uint8_t>(hi & 0xFF));
                out.push_back(static_cast<uint8_t>((hi >> 8) & 0xFF));
                out.push_back(static_cast<uint8_t>(lo & 0xFF));
                out.push_back(static_cast<uint8_t>((lo >> 8) & 0xFF));
            }
        }
        return out;
    }

    // Marks needles[i] = true when its byte pattern occurs in [data,data+len).
    // The target-side walk calls this per chunk with a needle-length-1
    // overlap so straddling occurrences survive chunking; the host test
    // calls it once over a whole synthetic fixture.
    inline void scan_present(const uint8_t* data, size_t len,
                             const std::vector<std::vector<uint8_t>>& needles,
                             std::vector<bool>& present)
    {
        present.assign(needles.size(), false);
        if (data == nullptr || len == 0)
        {
            return;
        }
        for (size_t n = 0; n < needles.size(); ++n)
        {
            const auto& nd = needles[n];
            if (present[n] || nd.empty() || nd.size() > len)
            {
                continue;
            }
            const uint8_t first = nd[0];
            const size_t last = len - nd.size();
            for (size_t i = 0; i <= last; ++i)
            {
                if (data[i] != first)
                {
                    continue;
                }
                size_t j = 1;
                for (; j < nd.size(); ++j)
                {
                    if (data[i + j] != nd[j])
                    {
                        break;
                    }
                }
                if (j == nd.size())
                {
                    present[n] = true;
                    break;
                }
            }
        }
    }

    // ------------------------------------------------------------------ //
    // Bounded JSON reader (layout-table schema only; exception-free)
    // ------------------------------------------------------------------ //

    struct JValue;
    using JMembers = std::vector<std::pair<std::string, JValue>>;

    struct JValue
    {
        enum class Type : uint8_t
        {
            Null,
            Bool,
            Num,
            Str,
            Arr,
            Obj,
        };
        Type type = Type::Null;
        bool b = false;
        double num = 0.0;
        std::string str;
        std::vector<JValue> arr;
        JMembers obj;

        [[nodiscard]] const JValue* find(std::string_view key) const noexcept
        {
            if (type != Type::Obj)
            {
                return nullptr;
            }
            for (const auto& kv : obj)
            {
                if (kv.first == key)
                {
                    return &kv.second;
                }
            }
            return nullptr;
        }
    };

    // Parses one JSON document. Returns false with a reason on any syntax
    // error; input size is hard-capped (tables are KiB-scale, never MiB).
    inline constexpr size_t kMaxTableJsonBytes = 4u * 1024u * 1024u;
    [[nodiscard]] inline bool parse_json(std::string_view text, JValue& out, std::string& error)
    {
        error.clear();
        if (text.size() > kMaxTableJsonBytes)
        {
            error = "table json exceeds 4 MiB cap";
            return false;
        }
        struct Cursor
        {
            const char* p;
            const char* end;
            size_t line = 1;

            void ws()
            {
                while (p < end)
                {
                    if (*p == '\n')
                    {
                        ++line;
                        ++p;
                    }
                    else if (*p == ' ' || *p == '\t' || *p == '\r')
                    {
                        ++p;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            bool literal(std::string_view word)
            {
                if (static_cast<size_t>(end - p) < word.size())
                {
                    return false;
                }
                for (size_t i = 0; i < word.size(); ++i)
                {
                    if (p[i] != word[i])
                    {
                        return false;
                    }
                }
                p += word.size();
                return true;
            }
        };
        Cursor cur{text.data(), text.data() + text.size()};

        // recursive descent with a depth guard (tables nest ~6 deep);
        // free-function recursion — no std::function on purpose (target TU
        // discipline: plain call frames only)
        constexpr int kMaxDepth = 32;
        struct Parser
        {
            static bool value(Cursor& c, JValue& v, std::string& err, int depth)
            {
                if (++depth > kMaxDepth)
                {
                    err = "nesting deeper than 32";
                    return false;
                }
                c.ws();
                if (c.p >= c.end)
                {
                    err = "unexpected end of input";
                    return false;
                }
                const char ch = *c.p;
                if (ch == '{')
                {
                    ++c.p;
                    v.type = JValue::Type::Obj;
                    c.ws();
                    if (c.p < c.end && *c.p == '}')
                    {
                        ++c.p;
                        return true;
                    }
                    for (;;)
                    {
                        c.ws();
                        if (c.p >= c.end || *c.p != '"')
                        {
                            err = "expected object key at line " + std::to_string(c.line);
                            return false;
                        }
                        JValue key;
                        if (!value(c, key, err, depth))
                        {
                            return false;
                        }
                        c.ws();
                        if (c.p >= c.end || *c.p != ':')
                        {
                            err = "expected ':' at line " + std::to_string(c.line);
                            return false;
                        }
                        ++c.p;
                        JValue member;
                        if (!value(c, member, err, depth))
                        {
                            return false;
                        }
                        v.obj.emplace_back(key.str, std::move(member));
                        c.ws();
                        if (c.p < c.end && *c.p == ',')
                        {
                            ++c.p;
                            continue;
                        }
                        if (c.p < c.end && *c.p == '}')
                        {
                            ++c.p;
                            return true;
                        }
                        err = "expected ',' or '}' at line " + std::to_string(c.line);
                        return false;
                    }
                }
                if (ch == '[')
                {
                    ++c.p;
                    v.type = JValue::Type::Arr;
                    c.ws();
                    if (c.p < c.end && *c.p == ']')
                    {
                        ++c.p;
                        return true;
                    }
                    for (;;)
                    {
                        JValue element;
                        if (!value(c, element, err, depth))
                        {
                            return false;
                        }
                        v.arr.push_back(std::move(element));
                        c.ws();
                        if (c.p < c.end && *c.p == ',')
                        {
                            ++c.p;
                            continue;
                        }
                        if (c.p < c.end && *c.p == ']')
                        {
                            ++c.p;
                            return true;
                        }
                        err = "expected ',' or ']' at line " + std::to_string(c.line);
                        return false;
                    }
                }
                if (ch == '"')
                {
                    ++c.p;
                    v.type = JValue::Type::Str;
                    while (c.p < c.end)
                    {
                        const char sc = *c.p;
                        if (sc == '"')
                        {
                            ++c.p;
                            return true;
                        }
                        if (sc == '\\')
                        {
                            ++c.p;
                            if (c.p >= c.end)
                            {
                                break;
                            }
                            const char esc = *c.p++;
                            switch (esc)
                            {
                            case '"': v.str.push_back('"'); break;
                            case '\\': v.str.push_back('\\'); break;
                            case '/': v.str.push_back('/'); break;
                            case 'b': v.str.push_back('\b'); break;
                            case 'f': v.str.push_back('\f'); break;
                            case 'n': v.str.push_back('\n'); break;
                            case 'r': v.str.push_back('\r'); break;
                            case 't': v.str.push_back('\t'); break;
                            case 'u':
                            {
                                if (c.end - c.p < 4)
                                {
                                    err = "truncated \\u escape";
                                    return false;
                                }
                                auto hex4 = [&c]() -> uint32_t {
                                    uint32_t v4 = 0;
                                    for (int i = 0; i < 4; ++i)
                                    {
                                        const char hc = *c.p++;
                                        v4 <<= 4;
                                        if (hc >= '0' && hc <= '9') v4 |= static_cast<uint32_t>(hc - '0');
                                        else if (hc >= 'a' && hc <= 'f') v4 |= static_cast<uint32_t>(hc - 'a' + 10);
                                        else if (hc >= 'A' && hc <= 'F') v4 |= static_cast<uint32_t>(hc - 'A' + 10);
                                        else { v4 = 0xFFFFFFFF; }
                                    }
                                    return v4;
                                };
                                const uint32_t cp = hex4();
                                if (cp == 0xFFFFFFFF)
                                {
                                    err = "bad \\u escape";
                                    return false;
                                }
                                // encode the code point as UTF-8 (surrogates kept raw; tables are ASCII)
                                if (cp < 0x80)
                                {
                                    v.str.push_back(static_cast<char>(cp));
                                }
                                else if (cp < 0x800)
                                {
                                    v.str.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                                    v.str.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                                }
                                else
                                {
                                    v.str.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                                    v.str.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                                    v.str.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                                }
                                break;
                            }
                            default:
                                err = "unknown escape";
                                return false;
                            }
                            continue;
                        }
                        v.str.push_back(sc);
                        ++c.p;
                    }
                    err = "unterminated string";
                    return false;
                }
                if (ch == 't')
                {
                    if (!c.literal("true"))
                    {
                        err = "bad literal";
                        return false;
                    }
                    v.type = JValue::Type::Bool;
                    v.b = true;
                    return true;
                }
                if (ch == 'f')
                {
                    if (!c.literal("false"))
                    {
                        err = "bad literal";
                        return false;
                    }
                    v.type = JValue::Type::Bool;
                    v.b = false;
                    return true;
                }
                if (ch == 'n')
                {
                    if (!c.literal("null"))
                    {
                        err = "bad literal";
                        return false;
                    }
                    v.type = JValue::Type::Null;
                    return true;
                }
                // number
                {
                    const char* start = c.p;
                    if (c.p < c.end && (*c.p == '-' || *c.p == '+'))
                    {
                        ++c.p;
                    }
                    bool digits = false;
                    while (c.p < c.end)
                    {
                        const char nc = *c.p;
                        if ((nc >= '0' && nc <= '9') || nc == '.' || nc == 'e' || nc == 'E' || nc == '-' || nc == '+')
                        {
                            if (nc >= '0' && nc <= '9')
                            {
                                digits = true;
                            }
                            ++c.p;
                        }
                        else
                        {
                            break;
                        }
                    }
                    if (!digits)
                    {
                        err = "unexpected character at line " + std::to_string(c.line);
                        return false;
                    }
                    v.type = JValue::Type::Num;
                    v.num = 0.0;
                    try
                    {
                        v.num = std::stod(std::string(start, static_cast<size_t>(c.p - start)));
                    }
                    catch (...)
                    {
                        v.num = 0.0;
                    }
                    return true;
                }
            }
        };
        const bool ok = Parser::value(cur, out, error, 0);
        if (ok)
        {
            cur.ws();
            if (cur.p != cur.end)
            {
                error = "trailing content after json value";
                return false;
            }
        }
        return ok;
    }

    // ------------------------------------------------------------------ //
    // Layout table (packed per-cell form + GJ authoring form)
    // ------------------------------------------------------------------ //

    struct PatternBytes
    {
        std::vector<uint8_t> bytes;
        std::vector<bool> wild; // true = ?? (matches anything)
        [[nodiscard]] bool empty() const noexcept
        {
            return bytes.empty();
        }
        [[nodiscard]] size_t size() const noexcept
        {
            return bytes.size();
        }
    };

    struct MemberRow
    {
        std::string struct_name;
        std::string member;
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    struct VtableSlot
    {
        std::string method;
        uint64_t byte_offset = 0;
    };

    struct PoolShape
    {
        uint64_t current_block_off = 8;      // pool_header "+0x8"
        uint64_t current_byte_off = 12;      // pool_header "+0xC"
        uint64_t blocks_off = 16;            // pool_header "+0x10"
        uint64_t max_blocks = 8192;          // id_decode.max_blocks
        uint64_t block_capacity_bytes = 0x20000; // id_decode.block_capacity_bytes
        uint64_t block_stride = 3072;        // FNamePool_shape.block_stride (cell form)
    };

    struct ArmSpec
    {
        std::string kind; // xref_census|utf16_census|prefix_consensus|shape_validator|vtable_reconstruction
    };

    // Canonical anchor ids (superset of the GI cell ids and the GJ fields).
    // REQUIRED for arming (the all-or-nothing set; SeedTable slots 0/2/3/4
    // + the pool label the ctor probe needs):
    //   GUObjectArray, FNameCtorWchar, GMalloc, StaticConstructObjectInternal,
    //   FNamePool
    // Optional (registered when the row supplies them):
    //   FNameToStringInternal, EngineAddDeleteListener,
    //   EngineRemoveDeleteListener, GMallocInstance, GMallocVtable
    inline constexpr const char* kRequiredAnchors[] = {
        "GUObjectArray", "FNameCtorWchar", "GMalloc", "StaticConstructObjectInternal", "FNamePool",
    };
    inline constexpr size_t kRequiredAnchorCount = sizeof(kRequiredAnchors) / sizeof(kRequiredAnchors[0]);

    struct AnchorRow
    {
        std::string id;
        bool is_code = false;          // CodeCallOnly vs DataReadable
        bool has_label = false;        // version-level relative anchor present
        uint64_t image_vaddr = 0;      // image-relative label (measurement instance)
        bool has_prefix = false;       // exact prologue bytes present (code)
        PatternBytes prefix;           // exact bytes (no wildcards) for the
                                       // authenticated signature compare
        unsigned min_arms = 2;         // agreement.min_arms (census/consensus)
        std::string verify_kind;       // behavioral probe family
        std::vector<ArmSpec> arms;     // census/consensus arms (load-bearing)
        std::vector<PatternBytes> probe_patterns; // patterns (ADVISORY only)
        uint64_t size = 8;             // data: pointer width; code: prefix len
    };

    struct VersionRow
    {
        std::string version;  // "5.6" (row key)
        std::string band;     // "5.6.x" / "5.4.x-5.5.x"
        std::string status;   // measured|official-source|seed
        double confidence = 0.0;
        uint64_t version_lo = 0; // packed maj*1000+min band lower bound
        uint64_t version_hi = 0;
        std::vector<AnchorRow> anchors;
        std::vector<MemberRow> members;
        std::vector<VtableSlot> vtable_slots;
        PoolShape pool;
    };

    struct LayoutTable
    {
        std::string form;     // "cell" (packed ue-layout-table/1) | "authoring" (GJ v1)
        std::string schema;   // raw schema field
        std::string table_id; // cell table_id | "ue-layout-table-v1"
        std::string status;   // cell status | authoring overall (not row status)
        double confidence = 0.0;
        std::string family, version, version_detail, platform, build_configuration, stats;

        // detection
        std::vector<std::string> triage_markers; // usable marker strings only
        bool markers_placeholder = false;        // cell shipped the placeholder
        Thresholds thr;

        // scan envelope (cell form; defaults otherwise)
        uint64_t max_window_bytes = 64u * 1024u * 1024u;
        unsigned max_candidates_per_anchor = 8;

        std::vector<VersionRow> rows; // authoring form: one per band; cell form: exactly one
    };

    // ---- small extraction helpers (shared by both forms) ---------------- //

    [[nodiscard]] inline uint64_t parse_u64_token(std::string_view text)
    {
        // leading token: 0xHEX / 0XHEX / decimal; prose tails like
        // "0x0 (8B)" are cut at the first space.
        size_t end = 0;
        while (end < text.size() && text[end] != ' ' && text[end] != '\t' && text[end] != ',')
        {
            ++end;
        }
        const std::string_view tok = text.substr(0, end);
        uint64_t value = 0;
        bool any = false;
        if (tok.size() > 2 && tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X'))
        {
            for (size_t i = 2; i < tok.size(); ++i)
            {
                const char c = tok[i];
                value <<= 4;
                any = true;
                if (c >= '0' && c <= '9') value |= static_cast<uint64_t>(c - '0');
                else if (c >= 'a' && c <= 'f') value |= static_cast<uint64_t>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') value |= static_cast<uint64_t>(c - 'A' + 10);
                else { any = false; break; }
            }
        }
        else
        {
            for (size_t i = 0; i < tok.size(); ++i)
            {
                const char c = tok[i];
                if (c < '0' || c > '9')
                {
                    any = false;
                    break;
                }
                value = value * 10 + static_cast<uint64_t>(c - '0');
                any = true;
            }
        }
        return any ? value : 0;
    }

    // Optional-extraction getters (bool return is informational; call
    // sites routinely ignore it for best-effort field fills).
    inline bool get_str(const JValue& obj, std::string_view key, std::string& out)
    {
        const JValue* v = obj.find(key);
        if (v == nullptr || v->type != JValue::Type::Str)
        {
            return false;
        }
        out = v->str;
        return true;
    }

    inline bool get_u64(const JValue& obj, std::string_view key, uint64_t& out)
    {
        const JValue* v = obj.find(key);
        if (v == nullptr)
        {
            return false;
        }
        if (v->type == JValue::Type::Num)
        {
            out = static_cast<uint64_t>(v->num);
            return true;
        }
        if (v->type == JValue::Type::Str)
        {
            out = parse_u64_token(v->str);
            return true;
        }
        return false;
    }

    [[nodiscard]] inline uint64_t get_u64_or(const JValue& obj, std::string_view key, uint64_t fallback)
    {
        uint64_t v = 0;
        return get_u64(obj, key, v) ? v : fallback;
    }

    // "48 8D 05 ?? ?? ?? ??" -> bytes + wildcards; whitespace-separated hex
    // pairs, ?? wildcards; returns false when a token is neither.
    [[nodiscard]] inline bool parse_pattern(std::string_view text, PatternBytes& out)
    {
        out.bytes.clear();
        out.wild.clear();
        size_t i = 0;
        bool any = false;
        while (i < text.size())
        {
            while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == ',' || text[i] == '\n' || text[i] == '\r'))
            {
                ++i;
            }
            if (i >= text.size())
            {
                break;
            }
            if (text.compare(i, 2, "??") == 0 || text.compare(i, 2, "**") == 0)
            {
                out.bytes.push_back(0);
                out.wild.push_back(true);
                i += 2;
                any = true;
                continue;
            }
            auto nib = [&](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            const int hi = nib(text[i]);
            const int lo = (i + 1 < text.size()) ? nib(text[i + 1]) : -1;
            if (hi < 0 || lo < 0)
            {
                return false;
            }
            out.bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
            out.wild.push_back(false);
            i += 2;
            any = true;
        }
        return any;
    }

    [[nodiscard]] inline uint64_t pack_version(std::string_view text)
    {
        // "5.6.1-0+++UE5" / "5.6" / "ue5.6" -> 5006; 0 when unparseable.
        uint64_t maj = 0, min = 0;
        size_t i = 0;
        bool any = false;
        while (i < text.size() && (text[i] < '0' || text[i] > '9'))
        {
            ++i;
        }
        while (i < text.size() && text[i] >= '0' && text[i] <= '9')
        {
            maj = maj * 10 + static_cast<uint64_t>(text[i] - '0');
            ++i;
            any = true;
        }
        if (i < text.size() && text[i] == '.')
        {
            ++i;
            while (i < text.size() && text[i] >= '0' && text[i] <= '9')
            {
                min = min * 10 + static_cast<uint64_t>(text[i] - '0');
                ++i;
            }
        }
        return any ? maj * 1000 + min : 0;
    }

    [[nodiscard]] inline bool version_in_band(uint64_t packed, const VersionRow& row)
    {
        if (row.version_lo == 0 && row.version_hi == 0)
        {
            return false;
        }
        return packed >= row.version_lo && packed <= row.version_hi;
    }

    // Row selection over one table's rows for a detected packed version
    // ("5.6" -> 5006). Seed rows are never selected (pack_rule: seed bands
    // pack to NOTHING; the generator refuses until the confirm_law upgrades
    // the row — FIRST-LAUNCH-AUTOMATION.md section 3a rule 3).
    [[nodiscard]] inline const VersionRow* select_row(const LayoutTable& table, uint64_t packed_version)
    {
        for (const auto& row : table.rows)
        {
            if (row.status != "measured")
            {
                continue;
            }
            if (version_in_band(packed_version, row))
            {
                return &row;
            }
        }
        return nullptr;
    }

    namespace detail
    {
        inline void anchor_from_cell(const JValue& a, AnchorRow& out)
        {
            get_str(a, "id", out.id);
            std::string kind, range_class;
            get_str(a, "kind", kind);
            get_str(a, "range_class", range_class);
            out.is_code = (kind == "code_function") || (range_class == "CodeCallOnly");
            // Packed cells carry NO labels (labels are image-relative
            // provenance of the measurement instance): has_label stays
            // false; a cell anchor therefore lands as the explicit NOT-YET
            // census-pool stub unless the packer later ships
            // provenance.labels (reserved key, not in schema v1).
            const JValue* labels = a.find("labels");
            if (labels != nullptr)
            {
                uint64_t label = 0;
                if (get_u64(*labels, "image_vaddr", label) && label != 0)
                {
                    out.has_label = true;
                    out.image_vaddr = label;
                }
            }
            const JValue* agreement = a.find("agreement");
            if (agreement != nullptr)
            {
                out.min_arms = static_cast<unsigned>(get_u64_or(*agreement, "min_arms", 2));
            }
            const JValue* verify = a.find("verify");
            if (verify != nullptr)
            {
                get_str(*verify, "kind", out.verify_kind);
            }
            const JValue* arms = a.find("arms");
            if (arms != nullptr && arms->type == JValue::Type::Arr)
            {
                for (const auto& arm : arms->arr)
                {
                    ArmSpec spec;
                    if (get_str(arm, "kind", spec.kind))
                    {
                        out.arms.push_back(std::move(spec));
                    }
                }
            }
            const JValue* probes = a.find("probe_patterns");
            if (probes != nullptr && probes->type == JValue::Type::Arr)
            {
                for (const auto& probe : probes->arr)
                {
                    std::string bytes;
                    if (get_str(probe, "bytes", bytes))
                    {
                        PatternBytes pb;
                        if (parse_pattern(bytes, pb))
                        {
                            out.probe_patterns.push_back(std::move(pb));
                        }
                    }
                }
            }
        }

        inline void fill_probe_defaults(AnchorRow& a)
        {
            if (!a.verify_kind.empty())
            {
                return;
            }
            if (a.id == "FNameCtorWchar") a.verify_kind = "behavioral_fnamector_byteproperty";
            else if (a.id == "GMalloc") a.verify_kind = "gmalloc_v1_v2";
            else if (a.id == "GUObjectArray") a.verify_kind = "pointer_probe_shape_check";
            else if (a.id == "StaticConstructObjectInternal") a.verify_kind = "sco_empty_callback_abi";
            else if (a.id == "FNamePool") a.verify_kind = "pool_words";
            else a.verify_kind = "behavioral_generic";
        }

        struct AnchorBuilder
        {
            VersionRow& row;
            explicit AnchorBuilder(VersionRow& r) : row(r) {}

            void data_anchor(const char* id, uint64_t label, const char* verify)
            {
                AnchorRow a;
                a.id = id;
                a.is_code = false;
                a.has_label = label != 0;
                a.image_vaddr = label;
                a.min_arms = 2; // census/consensus + shape/data agreement
                a.verify_kind = verify;
                a.size = 8;
                fill_probe_defaults(a);
                row.anchors.push_back(std::move(a));
            }

            void code_anchor(const char* id, uint64_t label, const std::string& prefix_hex)
            {
                AnchorRow a;
                a.id = id;
                a.is_code = true;
                a.has_label = label != 0;
                a.image_vaddr = label;
                // 2 census/consensus arms by default; the authenticated
                // exact-prefix signature counts DECISIVELY (+100 arms at
                // evaluation time), so a positive prefix satisfies any
                // min_arms while an unproven one must be corroborated.
                a.min_arms = 2;
                a.verify_kind = ""; // filled by fill_probe_defaults
                PatternBytes pb;
                if (!prefix_hex.empty() && parse_pattern(prefix_hex, pb) &&
                    std::find(pb.wild.begin(), pb.wild.end(), true) == pb.wild.end())
                {
                    a.has_prefix = true;
                    a.prefix = std::move(pb);
                    a.size = pb.size();
                }
                else if (!prefix_hex.empty() && parse_pattern(prefix_hex, pb))
                {
                    // wildcarded scan seed: ADVISORY probe pattern only
                    a.probe_patterns.push_back(std::move(pb));
                    a.size = 8;
                }
                fill_probe_defaults(a);
                row.anchors.push_back(std::move(a));
            }
        };

        inline void members_from_cell(const JValue& structures, VersionRow& row)
        {
            if (structures.type != JValue::Type::Obj)
            {
                return;
            }
            for (const auto& [struct_name, sv] : structures.obj)
            {
                const JValue* members = sv.find("members");
                if (members == nullptr || members->type != JValue::Type::Obj)
                {
                    continue;
                }
                for (const auto& [member, mv] : members->obj)
                {
                    MemberRow mr;
                    mr.struct_name = struct_name;
                    mr.member = member;
                    mr.offset = get_u64_or(mv, "offset", 0);
                    mr.size = get_u64_or(mv, "size", 0);
                    row.members.push_back(std::move(mr));
                }
            }
            const JValue* fmalloc = structures.find("FMalloc_vtable_order");
            if (fmalloc != nullptr)
            {
                const JValue* slots = fmalloc->find("slots");
                if (slots != nullptr && slots->type == JValue::Type::Obj)
                {
                    for (const auto& [method, offv] : slots->obj)
                    {
                        VtableSlot vs;
                        vs.method = method;
                        vs.byte_offset = offv.type == JValue::Type::Num ? static_cast<uint64_t>(offv.num) : parse_u64_token(offv.str);
                        row.vtable_slots.push_back(std::move(vs));
                    }
                }
            }
            const JValue* pool = structures.find("FNamePool_shape");
            if (pool != nullptr)
            {
                row.pool.block_stride = get_u64_or(*pool, "block_stride", row.pool.block_stride);
            }
        }

        inline void members_from_authoring(const JValue& entry, VersionRow& row)
        {
            auto flat = [&](const char* key, const char* struct_name) {
                const JValue* v = entry.find(key);
                if (v == nullptr || v->type != JValue::Type::Obj)
                {
                    return;
                }
                for (const auto& [member, mv] : v->obj)
                {
                    MemberRow mr;
                    mr.struct_name = struct_name;
                    mr.member = member;
                    if (mv.type == JValue::Type::Num)
                    {
                        mr.offset = static_cast<uint64_t>(mv.num);
                        mr.size = 0;
                    }
                    else if (mv.type == JValue::Type::Str)
                    {
                        // "0x0 (8B)" style prose: leading token = offset,
                        // "(NB)" = size when present.
                        mr.offset = parse_u64_token(mv.str);
                        const size_t paren = mv.str.find('(');
                        if (paren != std::string::npos)
                        {
                            std::string inner = mv.str.substr(paren + 1);
                            row.members.push_back(MemberRow{struct_name, member, mr.offset, parse_u64_token(inner)});
                            continue;
                        }
                        mr.size = 0;
                    }
                    row.members.push_back(std::move(mr));
                }
            };
            flat("fuobjectarray_fields", "FUObjectArray");
            flat("tuobjectarray_fields", "TUObjectArray");

            const JValue* item = entry.find("fuobjectitem");
            if (item != nullptr && item->type == JValue::Type::Obj)
            {
                for (const auto& [member, mv] : item->obj)
                {
                    MemberRow mr;
                    mr.struct_name = "FUObjectItem";
                    mr.member = member;
                    if (mv.type == JValue::Type::Num)
                    {
                        mr.offset = static_cast<uint64_t>(mv.num);
                    }
                    else if (mv.type == JValue::Type::Str)
                    {
                        const size_t paren = mv.str.find('(');
                        mr.offset = parse_u64_token(mv.str);
                        mr.size = paren != std::string::npos ? parse_u64_token(mv.str.substr(paren + 1)) : 0;
                        row.members.push_back(std::move(mr));
                        continue;
                    }
                    row.members.push_back(std::move(mr));
                }
            }

            const JValue* fname = entry.find("fname_pool");
            if (fname != nullptr && fname->type == JValue::Type::Obj)
            {
                const JValue* id_decode = fname->find("id_decode");
                if (id_decode != nullptr)
                {
                    row.pool.max_blocks = get_u64_or(*id_decode, "max_blocks", row.pool.max_blocks);
                    row.pool.block_capacity_bytes = get_u64_or(*id_decode, "block_capacity_bytes", row.pool.block_capacity_bytes);
                }
                const JValue* header = fname->find("pool_header");
                if (header != nullptr && header->type == JValue::Type::Obj)
                {
                    for (const auto& [key, value] : header->obj)
                    {
                        if (key.size() < 3 || key[0] != '+')
                        {
                            continue;
                        }
                        const uint64_t off = parse_u64_token(key.substr(1));
                        if (value.type != JValue::Type::Str)
                        {
                            continue;
                        }
                        if (value.str.rfind("CurrentBlock", 0) == 0) row.pool.current_block_off = off;
                        else if (value.str.rfind("CurrentByteOffset", 0) == 0) row.pool.current_byte_off = off;
                        else if (value.str.rfind("Blocks", 0) == 0) row.pool.blocks_off = off;
                    }
                }
            }

            const JValue* fmalloc = entry.find("fmalloc");
            if (fmalloc != nullptr)
            {
                const JValue* order = fmalloc->find("vtable_order_measured");
                if (order != nullptr && order->type == JValue::Type::Arr)
                {
                    for (const auto& slot : order->arr)
                    {
                        VtableSlot vs;
                        get_str(slot, "method", vs.method);
                        vs.byte_offset = get_u64_or(slot, "byte_offset", 0);
                        if (!vs.method.empty())
                        {
                            row.vtable_slots.push_back(std::move(vs));
                        }
                    }
                }
            }
        }

        inline void row_from_authoring_entry(const JValue& entry, VersionRow& row)
        {
            get_str(entry, "version", row.version);
            get_str(entry, "version_band", row.band);
            get_str(entry, "status", row.status);
            const JValue* conf = entry.find("confidence");
            if (conf != nullptr && conf->type == JValue::Type::Num)
            {
                row.confidence = conf->num;
            }
            // band bounds: "5.6" exact; "5.4-5.5" range; "5.6.x" ~ 5.6
            const uint64_t packed = pack_version(row.version);
            row.version_lo = packed;
            row.version_hi = packed;
            const size_t dash = row.version.find('-');
            if (dash != std::string::npos)
            {
                const uint64_t lo = pack_version(row.version.substr(0, dash));
                const uint64_t hi = pack_version(row.version.substr(dash + 1));
                if (lo != 0 && hi != 0)
                {
                    row.version_lo = lo;
                    row.version_hi = hi;
                }
            }

            AnchorBuilder build(row);
            uint64_t label = 0;
            std::string prefix;

            const JValue* pool = entry.find("fname_pool");
            if (pool != nullptr)
            {
                const JValue* lv = pool->find("label");
                if (lv != nullptr) label = get_u64_or(*lv, "image_vaddr", 0);
                build.data_anchor("FNamePool", label, "pool_words");
                label = 0;
            }
            const JValue* guo = entry.find("guobjectarray");
            if (guo != nullptr)
            {
                const JValue* lv = guo->find("label");
                if (lv != nullptr) label = get_u64_or(*lv, "image_vaddr", 0);
                build.data_anchor("GUObjectArray", label, "pointer_probe_shape_check");
                label = 0;
            }
            const JValue* ctor = entry.find("fname_ctor_wchar_slot2");
            if (ctor != nullptr)
            {
                const JValue* lv = ctor->find("label");
                if (lv != nullptr) label = get_u64_or(*lv, "image_vaddr", 0);
                prefix.clear();
                get_str(*ctor, "prefix_bytes_32", prefix);
                build.code_anchor("FNameCtorWchar", label, prefix);
                label = 0;
                prefix.clear();
            }
            const JValue* sco = entry.find("static_construct_object_internal");
            if (sco != nullptr)
            {
                const JValue* lv = sco->find("label");
                if (lv != nullptr) label = get_u64_or(*lv, "image_vaddr", 0);
                prefix.clear();
                get_str(*sco, "prefix_bytes_25", prefix);
                build.code_anchor("StaticConstructObjectInternal", label, prefix);
                label = 0;
                prefix.clear();
            }
            const JValue* tos = entry.find("fname_tostring_internal");
            if (tos != nullptr)
            {
                const JValue* lv = tos->find("label");
                if (lv != nullptr) label = get_u64_or(*lv, "image_vaddr", 0);
                prefix.clear();
                get_str(*tos, "prefix_bytes_32", prefix);
                build.code_anchor("FNameToStringInternal", label, prefix);
                label = 0;
                prefix.clear();
            }
            const JValue* listeners = entry.find("engine_uobject_listener_ops");
            if (listeners != nullptr)
            {
                const JValue* add = listeners->find("add_delete_listener");
                if (add != nullptr)
                {
                    label = get_u64_or(*add, "image_vaddr", 0);
                    prefix.clear();
                    get_str(*add, "prefix_bytes_16", prefix);
                    build.code_anchor("EngineAddDeleteListener", label, prefix);
                }
                const JValue* rem = listeners->find("remove_delete_listener");
                if (rem != nullptr)
                {
                    label = get_u64_or(*rem, "image_vaddr", 0);
                    prefix.clear();
                    get_str(*rem, "prefix_bytes_16", prefix);
                    build.code_anchor("EngineRemoveDeleteListener", label, prefix);
                }
                label = 0;
                prefix.clear();
            }
            const JValue* fmalloc = entry.find("fmalloc");
            if (fmalloc != nullptr)
            {
                build.data_anchor("GMalloc", get_u64_or(*fmalloc, "global_pointer_label_gmalloc", 0), "gmalloc_v1_v2");
                build.data_anchor("GMallocInstance", get_u64_or(*fmalloc, "global_instance_label", 0), "gmalloc_instance_shape");
                build.data_anchor("GMallocVtable", get_u64_or(*fmalloc, "vtable_label", 0), "gmalloc_vtable_shape");
            }
            members_from_authoring(entry, row);
        }
    } // namespace detail

    // Parses one layout-table document (either form) into LayoutTable.
    // Fail-closed: any structural surprise returns false + reason.
    [[nodiscard]] inline bool parse_layout_table(std::string_view text, LayoutTable& out, std::string& error)
    {
        out = LayoutTable{};
        JValue root;
        if (!parse_json(text, root, error))
        {
            return false;
        }
        if (root.type != JValue::Type::Obj)
        {
            error = "table root is not an object";
            return false;
        }
        const JValue* schema_field = root.find("schema");
        if (schema_field == nullptr)
        {
            error = "table has no schema field";
            return false;
        }
        if (schema_field->type == JValue::Type::Num)
        {
            // GJ authoring form: "schema": 1 (a JSON number)
            out.form = "authoring";
            out.schema = std::to_string(static_cast<long long>(schema_field->num));
        }
        else if (schema_field->type == JValue::Type::Str && schema_field->str == "ue-layout-table/1")
        {
            out.form = "cell";
            out.schema = schema_field->str;
        }
        else if (schema_field->type == JValue::Type::Str && !schema_field->str.empty())
        {
            out.form = "authoring"; // string schema ids other than the packed cell form
            out.schema = schema_field->str;
        }
        else
        {
            error = "unrecognized table schema field";
            return false;
        }
        if (root.find("table_id") != nullptr)
        {
            get_str(root, "table_id", out.table_id);
        }
        else
        {
            out.table_id = "ue-layout-table-v1";
        }
        get_str(root, "status", out.status);
        const JValue* conf = root.find("confidence");
        if (conf != nullptr && conf->type == JValue::Type::Num)
        {
            out.confidence = conf->num;
        }

        if (out.form == "authoring")
        {
            const JValue* engine = root.find("marker_classifier");
            if (engine != nullptr)
            {
                out.thr.markers_total = static_cast<unsigned>(get_u64_or(*engine, "markers", out.thr.markers_total));
            }
            const JValue* entries = root.find("entries");
            if (entries == nullptr || entries->type != JValue::Type::Arr)
            {
                error = "authoring table has no entries array";
                return false;
            }
            for (const auto& entry : entries->arr)
            {
                VersionRow row;
                detail::row_from_authoring_entry(entry, row);
                out.rows.push_back(std::move(row));
            }
            return true;
        }

        // packed per-cell form
        const JValue* engine = root.find("engine");
        if (engine != nullptr)
        {
            get_str(*engine, "family", out.family);
            get_str(*engine, "version", out.version);
            get_str(*engine, "version_detail", out.version_detail);
            get_str(*engine, "platform", out.platform);
            get_str(*engine, "build_configuration", out.build_configuration);
            get_str(*engine, "stats", out.stats);
        }
        const JValue* detection = root.find("detection");
        if (detection != nullptr)
        {
            out.thr.markers_total = static_cast<unsigned>(get_u64_or(*detection, "markers_total", out.thr.markers_total));
            const JValue* triage = detection->find("triage_markers");
            if (triage != nullptr && triage->type == JValue::Type::Arr)
            {
                for (const auto& m : triage->arr)
                {
                    if (m.type != JValue::Type::Str)
                    {
                        continue;
                    }
                    // The shipped example carries a placeholder string
                    // ("<16 widest-coverage ...>"): not a marker; a table
                    // whose whole list is placeholders contributes nothing
                    // and detection fail-closes downstream.
                    if (!m.str.empty() && m.str.front() == '<')
                    {
                        out.markers_placeholder = true;
                        continue;
                    }
                    out.triage_markers.push_back(m.str);
                }
            }
            const JValue* expected = detection->find("expected_presence");
            if (expected != nullptr)
            {
                out.thr.expected_present = static_cast<unsigned>(get_u64_or(*expected, "expected_present_count", 0));
                out.thr.margin_confident = static_cast<unsigned>(get_u64_or(*expected, "version_margin_confident", out.thr.margin_confident));
                out.thr.margin_tentative = static_cast<unsigned>(get_u64_or(*expected, "version_margin_tentative", out.thr.margin_tentative));
                out.thr.refused_max = static_cast<unsigned>(get_u64_or(*expected, "refused_max_present", out.thr.refused_max));
            }
        }
        const JValue* envelope = root.find("scan_envelope");
        if (envelope != nullptr)
        {
            out.max_window_bytes = get_u64_or(*envelope, "max_window_bytes", out.max_window_bytes);
            out.max_candidates_per_anchor = static_cast<unsigned>(get_u64_or(*envelope, "max_candidates_per_anchor", out.max_candidates_per_anchor));
        }

        VersionRow row;
        row.version = out.version;
        row.band = out.version;
        row.status = out.status.empty() ? std::string{"measured"} : out.status;
        row.confidence = out.confidence;
        row.version_lo = pack_version(out.version);
        row.version_hi = row.version_lo;

        const JValue* anchors = root.find("anchors");
        if (anchors != nullptr && anchors->type == JValue::Type::Arr)
        {
            for (const auto& a : anchors->arr)
            {
                AnchorRow ar;
                detail::anchor_from_cell(a, ar);
                detail::fill_probe_defaults(ar);
                row.anchors.push_back(std::move(ar));
            }
        }
        const JValue* structures = root.find("structures");
        if (structures != nullptr)
        {
            detail::members_from_cell(*structures, row);
        }
        out.rows.push_back(std::move(row));
        return true;
    }

    // ------------------------------------------------------------------ //
    // GA root-derivation law (Rule 10: no title-ids anywhere)
    // ------------------------------------------------------------------ //

    // <root>/ue4ss[/Mods] -> <root>. Mirrors the in-tree installer pattern
    // (LuaMod.cpp prepare_mod, agent GA): canonicalize the runtime-resolved
    // mods directory, strip a trailing "Mods" component, then the "ue4ss"
    // component. Returns "" when the input is too short to carry a root.
    [[nodiscard]] inline std::string derive_modbridge_root(std::string mods_directory)
    {
        // normalize separators (weak canonicalization surrogate; the target
        // passes an already-canonical path)
        for (char& c : mods_directory)
        {
            if (c == '\\')
            {
                c = '/';
            }
        }
        while (mods_directory.size() > 1 && mods_directory.back() == '/')
        {
            mods_directory.pop_back();
        }
        auto pop_component = [](std::string& p) {
            const size_t slash = p.find_last_of('/');
            if (slash == std::string::npos)
            {
                p.clear();
                return;
            }
            p.resize(slash == 0 ? 1 : slash);
        };
        auto last_component = [](const std::string& p) -> std::string {
            const size_t slash = p.find_last_of('/');
            return slash == std::string::npos ? p : p.substr(slash + 1);
        };
        if (last_component(mods_directory) == "Mods")
        {
            pop_component(mods_directory);
        }
        if (last_component(mods_directory) == "ue4ss")
        {
            pop_component(mods_directory);
        }
        if (mods_directory.empty() || mods_directory == "/")
        {
            return {};
        }
        return mods_directory;
    }

    // ------------------------------------------------------------------ //
    // Target ABI records (C-ABI friendly; shared by the wiring hook)
    // ------------------------------------------------------------------ //

    // Outcome codes for RC_first_launch_try_profile / _commit.
    enum Outcome : int
    {
        Outcome_Refused = 0,       // fail-closed ladder refused (loud [FL])
        Outcome_LoadedVerified = 1,// persisted slot loaded + re-verified
        Outcome_PendingVerify = 2, // candidates corroborated; behavioral
                                   // verify REQUESTS await the initializer
        Outcome_Committed = 3,     // verifies reported pass; persisted
        Outcome_Misuse = -1,       // API contract violation (order/lifetime)
        Outcome_NotRun = -2,       // try_profile never called
    };

    struct PinRecord
    {
        char anchor_id[40];
        uint64_t image_relative; // label = runtime_addr - base (ASLR law:
                                 // persisted form only; never a runtime abs)
        uint64_t runtime_address; // live base + label (this launch only)
        uint32_t size;
        uint8_t prologue[32]; // exact prefix bytes (code anchors)
        uint32_t prologue_len;
        uint32_t verified; // 1 after the initializer's behavioral probe
    };

    struct VerifyRequest
    {
        char anchor_id[40];
        uint64_t address; // absolute runtime address to probe
        char probe_kind[48]; // e.g. behavioral_fnamector_byteproperty
    };
} // namespace rc::firstlaunch

// ---------------------------------------------------------------------- //
// Wiring hook (called by UnrealInitializer at the documented pre-admission
// call site; root wires at build staging — this TU never edits the
// initializer). See the report for the exact phase-ladder slot: BEFORE the
// current pinned-profile admission (consume_required_slot_seeds), as its
// input source. Declarations only when __PROSPERO__ is undefined so the
// host test can link nothing.
// ---------------------------------------------------------------------- //
#if defined(__PROSPERO__)
extern "C" {
// Runs the first-launch ladder for this launch (one-shot):
//   slot load/re-verify -> detect -> table -> generate+corroborate.
// modbridge_root: the GA-law derived game root (<root> with tables/ and
//   profiles/ under it). image_base: the live module view base (the
//   initializer's module_view_base()). Returns an rc::firstlaunch::
//   Outcome code.
int RC_first_launch_try_profile(const char* modbridge_root, uint64_t image_base);

// Behavioral verify REQUEST protocol (the initializer performs the probes
// through its existing phase machinery; this TU never reimplements them).
int RC_first_launch_verify_request_count(void);
int RC_first_launch_verify_request_at(int index, rc::firstlaunch::VerifyRequest* out);
// anchor_id must match a pending request; passed=1 probe ok, passed=0 probe
// failed (fail-closed). Returns the current Outcome code.
int RC_first_launch_report_verify(const char* anchor_id, int passed);
// After every request was reported passed: persists the profile slot and
// exposes the pins. Returns Outcome_Committed on success.
int RC_first_launch_commit(void);

// Pin exposure (the initializer retargets its seed admission at these).
int RC_first_launch_pin_count(void);
int RC_first_launch_pin_at(int index, rc::firstlaunch::PinRecord* out);
// Last outcome code (Outcome_NotRun before try_profile).
int RC_first_launch_outcome(void);
}
#endif // __PROSPERO__
