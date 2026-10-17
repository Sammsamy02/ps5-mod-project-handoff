#pragma once
// ModProfileLoader.hpp — Team S1 (B3). Parse the preloaded profile data file
// (profile/<sha>.ini) into a rc::modprofile::Profile. Fail closed.
//
// Laws (same as ModProfile.hpp): freestanding (<cstdint>/<cstddef>/<cstring>),
// no heap, no exceptions, no std::filesystem, no engine headers, no new import.
// It parses bytes handed in by the activation; it never opens a file.
//
// Integrity: the whole file is already size+sha pinned by its loadlist row.
// This loader additionally (a) matches [profile] image_sha256 to the
// authenticated running image, (b) recomputes the body hash via an INJECTED
// incremental sha256 (runtime: codehash; host tests: a minimal sha) and
// checks it against [profile] body_sha256, (c) range/consistency-checks every
// REQUIRED field, and refuses (valid=false) on any failure. A refused profile
// never installs, so seeds never arm (the identification launch).
#include "ModProfile.hpp"

namespace rc::modprofile {

// Injected incremental sha256 (so the loader ships NO second sha impl).
struct Sha256Ops {
    void (*init)(void* ctx) = nullptr;
    void (*update)(void* ctx, const unsigned char* data, size_t n) = nullptr;
    void (*final_hex)(void* ctx, char out[kHex]) = nullptr; // lowercase, 64 hex + NUL
    void* ctx = nullptr;
    bool usable() const { return init && update && final_hex; }
};

enum class Result {
    Ok = 0,
    NoProfileForImage,   // no preloaded row matched the authenticated sha (fail closed, run unmodded)
    BadSyntax,           // malformed line / unterminated block / unknown section or key
    MissingRequired,     // a REQUIRED key/section absent or a present=0 required slot
    RangeError,          // a value out of range or a prefix size wrong
    Inconsistent,        // cross-field contradiction (non_shipping vs config, layout header vs build, ...)
    DigestMismatch,      // body_sha256 recomputation disagreed
    NoSha                // Sha256Ops not usable and body_sha256 present
};

struct Diag {
    Result result = Result::BadSyntax;
    char detail[192]{};   // first failure, human-readable (ASCII)
    int  line = 0;        // 1-based line of the first failure (0 if n/a)
    uint32_t sections_seen = 0;
    uint32_t keys_seen = 0;
    uint32_t required_checked = 0;   // gates must print a nonzero count
    const char* path = nullptr;      // the selected row path (for logging)
    bool ok() const { return result == Result::Ok; }
};

namespace detail {

struct Span { const char* s = nullptr; size_t n = 0; bool has() const { return s != nullptr; } };

inline bool is_space(char c) { return c == ' ' || c == '\t'; }

inline Span trim(Span v) {
    while (v.n && is_space(v.s[0])) { ++v.s; --v.n; }
    while (v.n && (is_space(v.s[v.n - 1]) || v.s[v.n - 1] == '\r')) --v.n;
    return v;
}

inline bool eq(Span a, const char* b) {
    size_t i = 0;
    for (; i < a.n; ++i) { if (b[i] == '\0' || a.s[i] != b[i]) return false; }
    return b[i] == '\0';
}

// One (section,key) -> value span, plus the line it was on.
struct KV { uint16_t section = 0; Span key; Span val; int line = 0; };

enum Section : uint16_t {
    SEC_NONE = 0, SEC_PROFILE, SEC_BUILD, SEC_POOL, SEC_SEEDS, SEC_FNAME,
    SEC_CODE_PINS, SEC_LISTENER, SEC_TOSTRING, SEC_THREAD, SEC_PAK,
    SEC_LAYOUT_MEMBERS, SEC_LAYOUT_VTABLES, SEC_FEATURES, SEC_UNKNOWN
};

inline uint16_t section_id(Span name) {
    struct M { const char* n; uint16_t id; };
    static const M table[] = {
        {"profile", SEC_PROFILE}, {"build", SEC_BUILD}, {"pool", SEC_POOL},
        {"seeds", SEC_SEEDS}, {"fname", SEC_FNAME}, {"code_pins", SEC_CODE_PINS},
        {"listener", SEC_LISTENER}, {"tostring", SEC_TOSTRING}, {"thread", SEC_THREAD},
        {"pak", SEC_PAK}, {"layout.members", SEC_LAYOUT_MEMBERS},
        {"layout.vtables", SEC_LAYOUT_VTABLES}, {"features", SEC_FEATURES},
    };
    for (const auto& m : table) if (eq(name, m.n)) return m.id;
    return SEC_UNKNOWN;
}

inline bool parse_u64(Span v, uint64_t& out) {
    v = trim(v);
    if (!v.n) return false;
    uint64_t r = 0; size_t i = 0; bool hex = false;
    if (v.n >= 2 && v.s[0] == '0' && (v.s[1] == 'x' || v.s[1] == 'X')) { hex = true; i = 2; if (v.n == 2) return false; }
    for (; i < v.n; ++i) {
        char c = v.s[i]; unsigned d;
        if (c >= '0' && c <= '9') d = unsigned(c - '0');
        else if (hex && c >= 'a' && c <= 'f') d = unsigned(c - 'a' + 10);
        else if (hex && c >= 'A' && c <= 'F') d = unsigned(c - 'A' + 10);
        else return false;
        uint64_t base = hex ? 16u : 10u;
        if (r > (UINT64_MAX - d) / base) return false; // overflow
        r = r * base + d;
    }
    out = r; return true;
}

inline bool parse_bool(Span v, bool& out) {
    v = trim(v);
    if (v.n == 1 && v.s[0] == '0') { out = false; return true; }
    if (v.n == 1 && v.s[0] == '1') { out = true; return true; }
    return false;
}

inline bool copy_str(Span v, char* dst, size_t cap) {
    v = trim(v);
    if (v.n + 1 > cap) return false;
    for (size_t i = 0; i < v.n; ++i) {
        unsigned char c = (unsigned char)v.s[i];
        if (c < 0x20) return false; // no control chars
        dst[i] = v.s[i];
    }
    dst[v.n] = '\0';
    return true;
}

inline bool hex_nib(char c, unsigned& d) {
    if (c >= '0' && c <= '9') { d = unsigned(c - '0'); return true; }
    if (c >= 'a' && c <= 'f') { d = unsigned(c - 'a' + 10); return true; }
    if (c >= 'A' && c <= 'F') { d = unsigned(c - 'A' + 10); return true; }
    return false;
}

inline bool parse_bytes(Span v, Bytes& out) {
    v = trim(v);
    if (v.n == 0 || (v.n % 2) != 0 || v.n / 2 > kMaxPrefix) return false;
    uint32_t n = 0;
    for (size_t i = 0; i < v.n; i += 2) {
        unsigned hi, lo;
        if (!hex_nib(v.s[i], hi) || !hex_nib(v.s[i + 1], lo)) return false;
        out.data[n++] = (uint8_t)((hi << 4) | lo);
    }
    out.size = n;
    return true;
}

} // namespace detail

// -----------------------------------------------------------------------------
// layout_offset: find `Member=0x..` inside `[Class]` in an INI text block.
inline bool /*rc::modprofile::*/ layout_offset_impl(const TextRef& text, const char* klass,
                                                    const char* member, uint32_t& out) noexcept {
    if (!text.present()) return false;
    const char* p = text.data;
    const char* end = text.data + text.size;
    bool in_class = false;
    while (p < end) {
        const char* ls = p;
        while (p < end && *p != '\n') ++p;
        detail::Span line{ls, size_t(p - ls)};
        if (p < end) ++p; // consume newline
        line = detail::trim(line);
        if (line.n == 0 || line.s[0] == ';' || line.s[0] == '#') continue;
        if (line.s[0] == '[') {
            // section header [Class]
            size_t k = 1; while (k < line.n && line.s[k] != ']') ++k;
            detail::Span name{line.s + 1, (k <= line.n ? k - 1 : 0)};
            in_class = detail::eq(name, klass);
            continue;
        }
        if (!in_class) continue;
        // key=value
        size_t eqp = 0; while (eqp < line.n && line.s[eqp] != '=') ++eqp;
        if (eqp >= line.n) continue;
        detail::Span key{line.s, eqp};
        detail::Span val{line.s + eqp + 1, line.n - eqp - 1};
        key = detail::trim(key);
        if (detail::eq(key, member)) {
            uint64_t v = 0;
            if (!detail::parse_u64(val, v) || v > 0xffffffffull) return false;
            out = (uint32_t)v;
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
class Loader {
public:
    // Parse one already-selected profile row's bytes. `expected_sha` is the
    // authenticated running image sha (64 hex + NUL); pass nullptr to skip the
    // match (host tests). sha may be an empty/unusable Sha256Ops only if the
    // file carries no body_sha256 (it must, so normally sha is required).
    static Result parse(const char* data, size_t len, const char* expected_sha,
                        const Sha256Ops& sha, Profile& out, Diag& diag) noexcept {
        out = Profile{};
        diag = Diag{};
        using namespace detail;

        // --- pass 1: collect KV pairs and the two layout text blocks --------
        static constexpr size_t kMaxKV = 768;
        KV kv[kMaxKV];
        size_t kvn = 0;
        uint16_t section = SEC_NONE;
        const char* p = data;
        const char* end = data + len;
        int lineno = 0;
        size_t body_line_start = len, body_line_end = len; // for body hashing
        bool seen_profile_body_sha = false;

        while (p < end) {
            const char* ls = p;
            while (p < end && *p != '\n') ++p;
            size_t raw_len = size_t(p - ls);
            const char* next = (p < end) ? p + 1 : end;
            ++lineno;
            Span line = trim(Span{ls, raw_len});
            p = next;
            if (line.n == 0 || line.s[0] == ';' || line.s[0] == '#') continue;

            if (line.s[0] == '[') {
                size_t k = 1; while (k < line.n && line.s[k] != ']') ++k;
                if (k >= line.n) return fail(diag, Result::BadSyntax, lineno, "section header missing ]");
                Span name{line.s + 1, k - 1};
                uint16_t sid = section_id(trim(name));
                if (sid == SEC_UNKNOWN)
                    return fail(diag, Result::BadSyntax, lineno, "unknown section");
                section = sid;
                ++diag.sections_seen;
                // layout text block: next content line must be <<<
                if (sid == SEC_LAYOUT_MEMBERS || sid == SEC_LAYOUT_VTABLES) {
                    if (!read_block(p, end, lineno, sid == SEC_LAYOUT_MEMBERS ? out.layout_members : out.layout_vtables, diag))
                        return diag.result;
                }
                continue;
            }

            // key=value
            size_t eqp = 0; while (eqp < line.n && line.s[eqp] != '=') ++eqp;
            if (eqp >= line.n) return fail(diag, Result::BadSyntax, lineno, "expected key=value");
            Span key = trim(Span{line.s, eqp});
            Span val = Span{line.s + eqp + 1, line.n - eqp - 1};
            if (section == SEC_NONE) return fail(diag, Result::BadSyntax, lineno, "key before any section");
            if (kvn >= kMaxKV) return fail(diag, Result::BadSyntax, lineno, "too many keys");
            kv[kvn++] = KV{section, key, val, lineno};
            ++diag.keys_seen;
            if (section == SEC_PROFILE && eq(key, "body_sha256")) {
                body_line_start = size_t(ls - data);
                body_line_end = size_t(next - data); // includes the trailing '\n'
                seen_profile_body_sha = true;
            }
        }

        // --- helpers over the KV table --------------------------------------
        auto find = [&](uint16_t sec, const char* k) -> Span {
            for (size_t i = 0; i < kvn; ++i) if (kv[i].section == sec && eq(kv[i].key, k)) return kv[i].val;
            return Span{};
        };
        auto line_of = [&](uint16_t sec, const char* k) -> int {
            for (size_t i = 0; i < kvn; ++i) if (kv[i].section == sec && eq(kv[i].key, k)) return kv[i].line;
            return 0;
        };

        // REQUIRED scalar getters; each bumps required_checked and fails closed.
        auto req_u64 = [&](uint16_t sec, const char* k, uint64_t& dst) -> bool {
            ++diag.required_checked;
            Span v = find(sec, k);
            if (!v.has()) return fail_b(diag, Result::MissingRequired, line_of(sec, k), k, "missing");
            if (!parse_u64(v, dst)) return fail_b(diag, Result::RangeError, line_of(sec, k), k, "bad integer");
            return true;
        };
        auto req_u32 = [&](uint16_t sec, const char* k, uint32_t& dst, uint64_t lo, uint64_t hi) -> bool {
            uint64_t t = 0; if (!req_u64(sec, k, t)) return false;
            if (t < lo || t > hi) return fail_b(diag, Result::RangeError, line_of(sec, k), k, "out of range");
            dst = (uint32_t)t; return true;
        };
        auto req_bool = [&](uint16_t sec, const char* k, bool& dst) -> bool {
            ++diag.required_checked;
            Span v = find(sec, k);
            if (!v.has()) return fail_b(diag, Result::MissingRequired, line_of(sec, k), k, "missing");
            if (!parse_bool(v, dst)) return fail_b(diag, Result::RangeError, line_of(sec, k), k, "bad bool");
            return true;
        };
        auto req_str = [&](uint16_t sec, const char* k, char* dst, size_t cap) -> bool {
            ++diag.required_checked;
            Span v = find(sec, k);
            if (!v.has()) return fail_b(diag, Result::MissingRequired, line_of(sec, k), k, "missing");
            if (!copy_str(v, dst, cap)) return fail_b(diag, Result::RangeError, line_of(sec, k), k, "string too long/invalid");
            return true;
        };
        auto req_bytes = [&](uint16_t sec, const char* k, const char* ksize, Bytes& dst, uint32_t expect) -> bool {
            ++diag.required_checked;
            Span v = find(sec, k);
            if (!v.has()) return fail_b(diag, Result::MissingRequired, line_of(sec, k), k, "missing");
            if (!parse_bytes(v, dst)) return fail_b(diag, Result::RangeError, line_of(sec, k), k, "bad hex");
            uint64_t declared = 0;
            if (ksize) {
                Span sv = find(sec, ksize);
                if (!sv.has() || !parse_u64(sv, declared) || declared != dst.size)
                    return fail_b(diag, Result::Inconsistent, line_of(sec, k), ksize, "size mismatch");
            }
            if (expect && dst.size != expect) return fail_b(diag, Result::Inconsistent, line_of(sec, k), k, "unexpected prefix length");
            return true;
        };
        auto opt_u64 = [&](uint16_t sec, const char* k, uint64_t& dst) { Span v = find(sec, k); if (v.has()) parse_u64(v, dst); };

        // --- [profile] ------------------------------------------------------
        if (!req_u32(SEC_PROFILE, "schema", out.schema, 1, 1)) return diag.result;
        if (!req_str(SEC_PROFILE, "name", out.name, kName)) return diag.result;
        if (!req_str(SEC_PROFILE, "digest", out.digest, kHex)) return diag.result;
        if (!req_str(SEC_PROFILE, "body_sha256", out.body_sha256, kHex)) return diag.result;
        if (!req_str(SEC_PROFILE, "image_sha256", out.image_sha256, kHex)) return diag.result;
        if (!hex64(out.digest) || !hex64(out.body_sha256) || !hex64(out.image_sha256))
            return fail(diag, Result::RangeError, line_of(SEC_PROFILE, "image_sha256"), "digest/sha not 64 lowercase hex");
        if (!req_u64(SEC_PROFILE, "image_size", out.image_size) || out.image_size < 64)
            return diag.ok() ? fail(diag, Result::RangeError, line_of(SEC_PROFILE, "image_size"), "image_size too small") : diag.result;
        if (!req_u64(SEC_PROFILE, "image_extent", out.image_extent) || out.image_extent < 128) return bad_extent(diag, line_of(SEC_PROFILE, "image_extent"));
        if (!req_u64(SEC_PROFILE, "image_base", out.image_base) || out.image_base == 0)
            return diag.ok() ? fail(diag, Result::RangeError, line_of(SEC_PROFILE, "image_base"), "image_base zero") : diag.result;
        if (!req_str(SEC_PROFILE, "exe_path", out.exe_path, kPath)) return diag.result;

        // lookup-key match against the authenticated image (fail closed)
        if (expected_sha && !streq(out.image_sha256, expected_sha))
            return fail(diag, Result::NoProfileForImage, line_of(SEC_PROFILE, "image_sha256"), "image_sha256 != authenticated image");

        // --- body hash ------------------------------------------------------
        if (!seen_profile_body_sha) return fail(diag, Result::MissingRequired, 0, "body_sha256 line not found");
        if (!sha.usable()) return fail(diag, Result::NoSha, 0, "no sha256 ops to verify body_sha256");
        char got[kHex]{};
        sha.init(sha.ctx);
        if (body_line_start > 0) sha.update(sha.ctx, (const unsigned char*)data, body_line_start);
        if (body_line_end < len) sha.update(sha.ctx, (const unsigned char*)data + body_line_end, len - body_line_end);
        sha.final_hex(sha.ctx, got);
        if (!streq(got, out.body_sha256))
            return fail(diag, Result::DigestMismatch, line_of(SEC_PROFILE, "body_sha256"), "body hash mismatch");

        // --- [build] --------------------------------------------------------
        if (!req_u32(SEC_BUILD, "format", out.format, 1, 1)) return diag.result;
        if (!req_u32(SEC_BUILD, "pointer_size", out.pointer_size, 8, 8)) return diag.result;
        if (!req_u32(SEC_BUILD, "engine_major", out.major, 4, 5)) return diag.result;
        if (!req_u32(SEC_BUILD, "engine_minor", out.minor, 0, 99)) return diag.result;
        if (!req_str(SEC_BUILD, "target", out.target, kShort)) return diag.result;
        if (!req_str(SEC_BUILD, "engine_revision", out.engine_revision, kShort)) return diag.result;
        if (!req_str(SEC_BUILD, "build_id", out.build_id, kShort)) return diag.result;
        if (!req_u32(SEC_BUILD, "build_configuration", out.build_configuration, 1, 3)) return diag.result;
        if (!req_str(SEC_BUILD, "build_source", out.build_source, kStr)) return diag.result;
        if (!req_u32(SEC_BUILD, "stats", out.stats, 1, 2)) return diag.result;
        if (!req_str(SEC_BUILD, "stats_source", out.stats_source, kStr)) return diag.result;
        if (!req_bool(SEC_BUILD, "non_shipping", out.non_shipping)) return diag.result;
        if (!req_bool(SEC_BUILD, "stats_layout", out.stats_layout)) return diag.result;
        if (!req_bool(SEC_BUILD, "fname_case_preserving", out.fname_case_preserving)) return diag.result;
        if (!req_u32(SEC_BUILD, "fname_output_bytes", out.fname_output_bytes, 1, 64)) return diag.result;
        if (!req_u32(SEC_BUILD, "fname_output_alignment", out.fname_output_alignment, 1, 64)) return diag.result;
        if (out.non_shipping != (out.build_configuration != 1))
            return fail(diag, Result::Inconsistent, line_of(SEC_BUILD, "non_shipping"), "non_shipping != (build_configuration!=1)");
        if (out.stats_layout != (out.stats == 2))
            return fail(diag, Result::Inconsistent, line_of(SEC_BUILD, "stats_layout"), "stats_layout != (stats==2)");

        // --- [pool] ---------------------------------------------------------
        if (!req_u64(SEC_POOL, "current_block_offset", out.pool_current_block_offset)) return diag.result;
        if (!req_u64(SEC_POOL, "current_byte_offset_offset", out.pool_current_byte_offset_offset)) return diag.result;
        if (!req_u64(SEC_POOL, "blocks_offset", out.pool_blocks_offset)) return diag.result;
        if (!req_u64(SEC_POOL, "none_id_offset", out.pool_none_id_offset)) return diag.result;
        if (!req_u64(SEC_POOL, "byte_property_id_offset", out.pool_byte_property_id_offset)) return diag.result;
        if (!req_u32(SEC_POOL, "block_offset_bits", out.pool_block_offset_bits, 1, 32)) return diag.result;
        if (!req_u32(SEC_POOL, "max_blocks", out.pool_max_blocks, 1, 1u << 20)) return diag.result;

        // --- [seeds] --------------------------------------------------------
        if (!req_bool(SEC_SEEDS, "generated", out.generated) || !out.generated)
            return diag.ok() ? fail(diag, Result::Inconsistent, line_of(SEC_SEEDS, "generated"), "generated must be 1") : diag.result;
        if (!req_bool(SEC_SEEDS, "required_set_complete", out.required_set_complete)) return diag.result;
        if (!req_u64(SEC_SEEDS, "fname_pool_label", out.fname_pool_label)) return diag.result;
        if (!req_u64(SEC_SEEDS, "guobject_array_label", out.guobject_array_label)) return diag.result;
        if (!req_u64(SEC_SEEDS, "fname_ctor_label", out.fname_ctor_label)) return diag.result;
        if (!req_u64(SEC_SEEDS, "gmalloc_label", out.gmalloc_label)) return diag.result;
        if (!req_u64(SEC_SEEDS, "gmalloc_instance_label", out.gmalloc_instance_label)) return diag.result;
        if (!req_u64(SEC_SEEDS, "gmalloc_vtable_label", out.gmalloc_vtable_label)) return diag.result;
        if (!req_u64(SEC_SEEDS, "sco_label", out.sco_label)) return diag.result;
        if (!req_bool(SEC_SEEDS, "guobject_array_present", out.guobject_present)) return diag.result;
        if (!req_bool(SEC_SEEDS, "fname_ctor_present", out.fname_ctor_present)) return diag.result;
        if (!req_bool(SEC_SEEDS, "gmalloc_present", out.gmalloc_present)) return diag.result;
        if (!req_bool(SEC_SEEDS, "sco_present", out.sco_present)) return diag.result;
        if (!req_str(SEC_SEEDS, "guobject_array_derivation", out.guobject_derivation, kStr)) return diag.result;
        if (!req_str(SEC_SEEDS, "fname_ctor_derivation", out.fname_ctor_derivation, kStr)) return diag.result;
        if (!req_str(SEC_SEEDS, "gmalloc_derivation", out.gmalloc_derivation, kStr)) return diag.result;
        if (!req_str(SEC_SEEDS, "sco_derivation", out.sco_derivation, kStr)) return diag.result;
        // all-or-nothing arming (SeedTable semantics): required_set_complete iff all four present
        bool all_present = out.guobject_present && out.fname_ctor_present && out.gmalloc_present && out.sco_present;
        if (out.required_set_complete != all_present)
            return fail(diag, Result::Inconsistent, line_of(SEC_SEEDS, "required_set_complete"), "required_set_complete != (all four slots present)");
        if (!out.required_set_complete)
            return fail(diag, Result::MissingRequired, line_of(SEC_SEEDS, "required_set_complete"), "incomplete seed set (fail closed)");

        // --- [fname] + [code_pins] prefixes ---------------------------------
        if (!req_bytes(SEC_FNAME, "ctor_prefix", "ctor_prefix_size", out.fname_ctor_prefix, 32)) return diag.result;
        if (!req_bytes(SEC_CODE_PINS, "sco_prefix", "sco_prefix_size", out.sco_prefix, 25)) return diag.result;
        if (!req_bytes(SEC_CODE_PINS, "listener_add_prefix", "listener_add_prefix_size", out.listener_add_prefix, 16)) return diag.result;
        if (!req_bytes(SEC_CODE_PINS, "listener_remove_prefix", "listener_remove_prefix_size", out.listener_remove_prefix, 16)) return diag.result;
        if (!req_bytes(SEC_CODE_PINS, "thread_name_prefix", "thread_name_prefix_size", out.thread_name_prefix, 33)) return diag.result;

        // --- [listener] -----------------------------------------------------
        if (!req_u64(SEC_LISTENER, "add_label", out.listener_add_label)) return diag.result;
        if (!req_u64(SEC_LISTENER, "remove_label", out.listener_remove_label)) return diag.result;

        // --- [tostring] (constants kept; pin removed B5) --------------------
        if (!req_u32(SEC_TOSTRING, "probe_id", out.tostring_probe_id, 1, 0xffffffffu)) return diag.result;
        if (!req_str(SEC_TOSTRING, "probe_expect", out.tostring_probe_expect, kShort)) return diag.result;
        if (!req_u32(SEC_TOSTRING, "probe_expect_units", out.tostring_probe_expect_units, 1, 1024)) return diag.result;
        if (!req_u32(SEC_TOSTRING, "out_units", out.tostring_out_units, 1, 1u << 20)) return diag.result;
        opt_u64(SEC_TOSTRING, "label", out.tostring_label);
        { Span v = find(SEC_TOSTRING, "prefix"); if (v.has()) parse_bytes(v, out.tostring_prefix); }

        // --- [thread] -------------------------------------------------------
        if (!req_u64(SEC_THREAD, "accessor", out.thread_accessor)) return diag.result;
        if (!req_u64(SEC_THREAD, "game_name", out.thread_game_name)) return diag.result;
        if (!req_u64(SEC_THREAD, "render_name", out.thread_render_name)) return diag.result;
        if (!req_u64(SEC_THREAD, "empty_name", out.thread_empty_name)) return diag.result;
        // (thread pin #3 label == out.thread_accessor by construction — enforced where the pin is built)

        // --- [pak] (required set; structural offsets fixed) -----------------
        if (!req_u64(SEC_PAK, "pak_vtable", out.pak_vtable)) return diag.result;
        if (!req_u64(SEC_PAK, "platform_mgr_get", out.pak_platform_mgr_get)) return diag.result;
        if (!req_u64(SEC_PAK, "platform_mgr_getpf", out.pak_platform_mgr_getpf)) return diag.result;
        if (!req_u64(SEC_PAK, "mountpak_delegate", out.pak_mountpak_delegate)) return diag.result;
        if (!req_u64(SEC_PAK, "handle_mount_pak", out.pak_handle_mount_pak)) return diag.result;
        if (!req_u64(SEC_PAK, "delegate_vtable", out.pak_delegate_vtable)) return diag.result;
        if (!req_u64(SEC_PAK, "execute", out.pak_execute)) return diag.result;
        if (!req_u64(SEC_PAK, "lower_impl", out.pak_lower_impl)) return diag.result;
        if (!req_u64(SEC_PAK, "snapshot_delegate", out.pak_snapshot_delegate)) return diag.result;
        if (!req_u64(SEC_PAK, "snapshot_vtable", out.pak_snapshot_vtable)) return diag.result;
        if (!req_u64(SEC_PAK, "snapshot_execute", out.pak_snapshot_execute)) return diag.result;
        // [S1][B6] slot/struct byte offsets are PROFILE DATA, not fixed UE 5.6
        // numbers: plausibility only (nonzero, 8-aligned, bounded). delegate_inst
        // _off / delegate_size_off are genuine FDelegateBase C++ ABI (0 / 8),
        // generic across UE versions, so they stay structurally fixed.
        if (!req_u32(SEC_PAK, "slot_get_lower_level", out.pak_slot_get_lower_level, 8, 0xffff) || (out.pak_slot_get_lower_level & 7))
            return diag.ok() ? fail(diag, Result::RangeError, line_of(SEC_PAK, "slot_get_lower_level"), "slot_get_lower_level not an 8-aligned vtable offset") : diag.result;
        if (!req_u32(SEC_PAK, "slot_delegate_execute", out.pak_slot_delegate_execute, 8, 0xffff) || (out.pak_slot_delegate_execute & 7))
            return diag.ok() ? fail(diag, Result::RangeError, line_of(SEC_PAK, "slot_delegate_execute"), "slot_delegate_execute not an 8-aligned vtable offset") : diag.result;
        if (!req_u32(SEC_PAK, "delegate_inst_off", out.pak_delegate_inst_off, 0, 0)) return diag.result;
        if (!req_u32(SEC_PAK, "delegate_size_off", out.pak_delegate_size_off, 8, 8)) return diag.result;
        if (!req_u32(SEC_PAK, "pak_count_offset", out.pak_count_offset, 8, 0xffff) || (out.pak_count_offset & 7))
            return diag.ok() ? fail(diag, Result::RangeError, line_of(SEC_PAK, "pak_count_offset"), "pak_count_offset not an 8-aligned struct offset") : diag.result;
        {
            const char* lk[6] = {"proof0_linked","proof1_linked","proof2_linked","proof3_linked","proof4_linked","proof5_linked"};
            const char* bk[6] = {"proof0_bytes","proof1_bytes","proof2_bytes","proof3_bytes","proof4_bytes","proof5_bytes"};
            const char* sk[6] = {"proof0_size","proof1_size","proof2_size","proof3_size","proof4_size","proof5_size"};
            for (int i = 0; i < 6; ++i) {
                if (!req_u64(SEC_PAK, lk[i], out.pak_proof_linked[i])) return diag.result;
                if (!req_bytes(SEC_PAK, bk[i], sk[i], out.pak_proof[i], 0)) return diag.result;
            }
        }
        out.pak_present = true;

        // --- [layout] blocks ------------------------------------------------
        if (!out.layout_members.present()) return fail(diag, Result::MissingRequired, 0, "[layout.members] block missing");
        if (!out.layout_vtables.present()) return fail(diag, Result::MissingRequired, 0, "[layout.vtables] block missing");
        if (!layout_header_matches(out, out.layout_members, diag)) return diag.result;
        if (!layout_header_matches(out, out.layout_vtables, diag)) return diag.result;

        // --- [features] (optional; default = present-iff-section) -----------
        out.feature_pak_mount = out.pak_present;
        out.feature_delete_listener = true;
        out.feature_thread_name = true;
        { bool b; if (find(SEC_FEATURES, "pak_mount").has() && req_bool(SEC_FEATURES, "pak_mount", b)) out.feature_pak_mount = b; }
        { bool b; if (find(SEC_FEATURES, "delete_listener").has() && req_bool(SEC_FEATURES, "delete_listener", b)) out.feature_delete_listener = b; }
        { bool b; if (find(SEC_FEATURES, "thread_name").has() && req_bool(SEC_FEATURES, "thread_name", b)) out.feature_thread_name = b; }

        out.valid = true;
        diag.result = Result::Ok;
        return Result::Ok;
    }

    // Convenience: select the matching row among preloaded profile rows, then parse.
    struct Row { const unsigned char* data; uint64_t size; const char* path; };
    static Result select_and_parse(const Row* rows, size_t row_count, const char* authenticated_sha,
                                    const Sha256Ops& sha, Profile& out, Diag& diag) noexcept {
        for (size_t i = 0; i < row_count; ++i) {
            // cheap pre-match: does [profile] image_sha256 equal the authenticated sha?
            Profile probe{}; Diag d{};
            Result r = parse((const char*)rows[i].data, (size_t)rows[i].size, authenticated_sha, sha, probe, d);
            if (r == Result::Ok) { out = probe; diag = d; diag.path = rows[i].path; return Result::Ok; }
            // a row whose sha simply does not match this image is not an error; keep looking.
            if (r != Result::NoProfileForImage) { diag = d; diag.path = rows[i].path; return r; }
        }
        diag = Diag{}; diag.result = Result::NoProfileForImage;
        set_detail(diag, "no preloaded profile row matched the authenticated image sha");
        return Result::NoProfileForImage;
    }

private:
    static bool streq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
    static bool hex64(const char* s) {
        for (int i = 0; i < 64; ++i) { char c = s[i]; if (!((c>='0'&&c<='9')||(c>='a'&&c<='f'))) return false; }
        return s[64] == '\0';
    }
    static void set_detail(Diag& d, const char* msg) {
        size_t i = 0; for (; msg[i] && i + 1 < sizeof(d.detail); ++i) d.detail[i] = msg[i]; d.detail[i] = '\0';
    }
    static Result fail(Diag& d, Result r, int line, const char* msg) {
        d.result = r; d.line = line; set_detail(d, msg); return r;
    }
    static bool fail_b(Diag& d, Result r, int line, const char* key, const char* why) {
        d.result = r; d.line = line;
        char buf[192]; size_t n = 0;
        auto app = [&](const char* s){ for (; *s && n + 1 < sizeof(buf); ++s) buf[n++] = *s; };
        app(key); app(": "); app(why); buf[n] = '\0'; set_detail(d, buf);
        return false;
    }
    static Result bad_extent(Diag& d, int line) {
        if (d.ok()) return fail(d, Result::RangeError, line, "image_extent too small");
        return d.result;
    }
    // read a <<< ... >>> block starting at p (p points just past the section header line)
    static bool read_block(const char*& p, const char* end, int& lineno, TextRef& out, Diag& d) {
        using namespace detail;
        // skip comment/blank lines until <<<
        while (p < end) {
            const char* ls = p; while (p < end && *p != '\n') ++p; const char* next = (p < end) ? p + 1 : end;
            ++lineno; Span line = trim(Span{ls, size_t((p < end ? p : end) - ls)}); p = next;
            if (line.n == 0 || line.s[0] == ';' || line.s[0] == '#') continue;
            if (!(line.n == 3 && line.s[0]=='<' && line.s[1]=='<' && line.s[2]=='<')) {
                fail(d, Result::BadSyntax, lineno, "layout section must open with <<<"); return false;
            }
            break;
        }
        const char* block_start = p;
        const char* block_end = nullptr;
        while (p < end) {
            const char* ls = p; while (p < end && *p != '\n') ++p; const char* lend = p; const char* next = (p < end) ? p + 1 : end;
            ++lineno; Span line = trim(Span{ls, size_t(lend - ls)});
            if (line.n == 3 && line.s[0]=='>' && line.s[1]=='>' && line.s[2]=='>') { block_end = ls; p = next; break; }
            p = next;
        }
        if (!block_end) { fail(d, Result::BadSyntax, lineno, "layout block not terminated by >>>"); return false; }
        out.data = block_start;
        out.size = size_t(block_end - block_start);
        return true;
    }
    // the [NativeLayout] header inside a layout block must agree with [build]
    static bool layout_header_matches(const Profile& out, const TextRef& t, Diag& d) {
        uint32_t v = 0;
        auto need = [&](const char* k, uint32_t want) -> bool {
            if (!layout_offset_impl(t, "NativeLayout", k, v)) { fail(d, Result::Inconsistent, 0, "layout header missing field"); return false; }
            if (v != want) { fail(d, Result::Inconsistent, 0, "layout header disagrees with [build]"); return false; }
            return true;
        };
        // NativeLayout values are plain ints (Format=1, EngineMajor=5 ...), parsed by layout_offset_impl
        if (!need("Format", out.format)) return false;
        if (!need("PointerSize", out.pointer_size)) return false;
        if (!need("EngineMajor", out.major)) return false;
        if (!need("EngineMinor", out.minor)) return false;
        if (!need("NonShipping", out.non_shipping ? 1u : 0u)) return false;
        if (!need("Stats", out.stats_layout ? 1u : 0u)) return false;
        return true;
    }
};

inline bool layout_offset(const TextRef& text, const char* klass, const char* member, uint32_t& out) noexcept {
    return layout_offset_impl(text, klass, member, out);
}

} // namespace rc::modprofile
