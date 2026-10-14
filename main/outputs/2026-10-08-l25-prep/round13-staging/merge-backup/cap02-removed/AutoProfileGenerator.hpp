#pragma once
// cap02: generic first-launch layout-profile generator.
//
// When a new game launches with NO image-pinned compiled declaration and NO
// mod-supplied layout files, the runtime must still obtain correct field
// offsets. This unit turns the engine-level layout packs (data, no game
// names) plus the live image's own identity (engine version + ELF build-id,
// observed at startup) into the SAME MemberVariableLayout / VTableLayout text
// the existing StageNativeLayoutTexts path already consumes and the existing
// BuildProfile/LayoutPreflight gates already police.
//
// Laws honoured here:
//  * FAIL-CLOSED: an engine cell with no shipped pack returns nullptr and the
//    caller refuses the launch. The generator never guesses a nearby cell.
//  * GENERIC: pack selection is by (format, pointer_size, major, minor,
//    target, non_shipping, stats) only. No game identity is consulted or
//    emitted. Per-game facts (engine_revision, build_id) come from the live
//    image, never from the pack.
//  * NO NEW TRUST: the emitted text is re-authenticated by the unchanged
//    downstream gates exactly as a compiled-in or file-supplied pair is.
//
// Host-buildable, freestanding of the engine (std + the generated pack header
// only), so the cap02 host tests exercise the real selection and rendering.
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include "AutoLayoutPacks.generated.hpp"

namespace rc::autoprofile {

using packs::Pack;
using packs::Row;

// Engine cell resolved from the live image by two independent detectors
// (see ImageIdentity below). All fields must be present; version_known==false
// (ambiguous fingerprint) forces a refusal upstream before this is built.
struct EngineCell {
    uint32_t format = 1;
    uint32_t pointer_size = 8;
    uint32_t major = 0;
    uint32_t minor = 0;
    const char* target = "prospero-x86_64-sysv";
    bool non_shipping = false;
    bool stats = false;
};

// Per-image identity observed at startup. revision/build_id are image-specific
// strings (the runtime already reads them live); they are provenance tags the
// downstream gate compares staged-vs-profile but never uses as an address.
struct ImageIdentity {
    std::string engine_revision;  // e.g. "5.6.1-0+++UE5"
    std::string build_id;         // ELF NT_GNU_BUILD_ID hex
};

// Exact-cell selection. Returns nullptr (fail-closed) when no shipped pack
// matches every axis. Never returns a "nearest" pack.
inline const Pack* select_pack(const EngineCell& cell) noexcept {
    const Pack* found = nullptr;
    for (size_t i = 0; i < packs::kPackCount; ++i) {
        const Pack* p = packs::kPacks[i];
        if (p->format != cell.format || p->pointer_size != cell.pointer_size) continue;
        if (p->major != cell.major || p->minor != cell.minor) continue;
        if (p->non_shipping != cell.non_shipping || p->stats != cell.stats) continue;
        if (!cell.target || !p->target) continue;
        if (std::string_view{p->target} != std::string_view{cell.target}) continue;
        if (found) return nullptr;  // ambiguous catalogue => refuse
        found = p;
    }
    return found;
}

inline void append_hex(std::string& s, uint32_t v) {
    char buf[11];
    int n = 0;
    buf[n++] = '0'; buf[n++] = 'x';
    char digits[8]; int d = 0;
    if (v == 0) digits[d++] = '0';
    while (v) { int nib = v & 0xf; digits[d++] = char(nib < 10 ? '0' + nib : 'a' + nib - 10); v >>= 4; }
    for (int i = d - 1; i >= 0; --i) buf[n++] = digits[i];
    s.append(buf, size_t(n));
}

inline bool valid_identity_token(const std::string& s) noexcept {
    if (s.empty() || s.size() > 512) return false;
    for (char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (c < 0x20 || c > 0x7e) return false;  // printable ASCII
    }
    return true;
}

inline std::string header_block(const Pack& p, const ImageIdentity& id) {
    std::string s = "[NativeLayout]\n";
    s += "Format=" + std::to_string(p.format) + "\n";
    s += std::string("Target=") + p.target + "\n";
    s += "PointerSize=" + std::to_string(p.pointer_size) + "\n";
    s += "EngineMajor=" + std::to_string(p.major) + "\n";
    s += "EngineMinor=" + std::to_string(p.minor) + "\n";
    s += std::string("NonShipping=") + (p.non_shipping ? "1" : "0") + "\n";
    s += std::string("Stats=") + (p.stats ? "1" : "0") + "\n";
    s += "EngineRevision=" + id.engine_revision + "\n";
    s += "BuildId=" + id.build_id + "\n";
    return s;
}

// Render one INI side (members or vtables). Sections/keys are emitted in the
// pack's stored order; a new "[Section]" line precedes each run of keys that
// share a section. A blank line separates the header and every section.
inline std::string render_side(const Pack& p, const ImageIdentity& id, bool members) {
    const Row* rows = members ? p.members : p.vtables;
    const size_t n = members ? p.member_count : p.vtable_count;
    std::string s = header_block(p, id);
    std::string_view cur{};
    for (size_t i = 0; i < n; ++i) {
        std::string_view sec{rows[i].section};
        if (sec != cur) { s += "\n[" + std::string(sec) + "]\n"; cur = sec; }
        s += std::string(rows[i].key) + "=";
        append_hex(s, rows[i].value);
        s += "\n";
    }
    return s;
}

// Result of a generation attempt. ok==false means refuse the launch.
struct Generated {
    bool ok = false;
    const char* reason = "no shipped layout pack for the detected engine cell";
    const Pack* pack = nullptr;
    std::string members;   // ready for StageNativeLayoutTexts()
    std::string vtables;
};

// Render-only step: select the exact-cell pack and render both INI sides.
// It does NOT decide whether the result may be ARMED; it is used by the
// non-arming shadow compare and by generate() below. Identity strings must be
// printable ASCII; an absent/garbage identity refuses.
inline Generated render(const EngineCell& cell, const ImageIdentity& id) {
    Generated g;
    if (!valid_identity_token(id.engine_revision) || !valid_identity_token(id.build_id)) {
        g.reason = "live image identity (engine_revision/build_id) missing or non-printable";
        return g;
    }
    const Pack* p = select_pack(cell);
    if (!p) return g;  // fail-closed default reason
    g.pack = p;
    g.members = render_side(*p, id, true);
    g.vtables = render_side(*p, id, false);
    g.ok = true;
    g.reason = "engine-level pack selected by exact cell; per-image identity from live scan";
    return g;
}

// The whole first-launch generation step for ARMING, fail-closed. On top of
// render(), a pack is armed only when it is cleared for arming: every row has
// two independent agreeing sources AND the cell carries every config axis a
// row depends on. A pack with single-source rows or un-modelled config
// dependencies (e.g. a vtable slot that moves with a build option the cell
// does not record) refuses here, so a different image can never silently
// inherit one calibration image's single-source values. This is a
// whole-pack refusal, independent of which game is running.
inline Generated generate(const EngineCell& cell, const ImageIdentity& id) {
    Generated g = render(cell, id);
    if (!g.ok) return g;
    if (!g.pack->arming_cleared) {
        Generated refused;
        refused.pack = g.pack;
        refused.reason = "pack not cleared for arming (single-source or config-dependent rows; "
                         "needs per-image verification or config axes)";
        return refused;
    }
    return g;
}

}  // namespace rc::autoprofile

namespace rc::autoprofile {
// Generic NT_GNU_BUILD_ID reader. Walks PT_NOTE/SHT_NOTE-style note records in
// a buffer and returns the build-id as lowercase hex, or "" if none. No engine
// or platform assumption beyond ELF64 LE note framing (namesz,descsz,type).
inline std::string read_gnu_build_id(const uint8_t* notes, size_t n) {
    size_t p = 0;
    auto rd32 = [&](size_t o) -> uint32_t {
        return uint32_t(notes[o]) | uint32_t(notes[o + 1] << 8) |
               uint32_t(notes[o + 2] << 16) | uint32_t(notes[o + 3] << 24);
    };
    while (p + 12 <= n) {
        uint32_t namesz = rd32(p), descsz = rd32(p + 4), type = rd32(p + 8);
        size_t name_off = p + 12;
        size_t name_pad = (namesz + 3) & ~size_t(3);
        size_t desc_off = name_off + name_pad;
        if (desc_off + descsz > n) break;
        bool gnu = namesz == 4 && name_off + 4 <= n &&
                   notes[name_off] == 'G' && notes[name_off + 1] == 'N' &&
                   notes[name_off + 2] == 'U' && notes[name_off + 3] == 0;
        if (gnu && type == 3 /* NT_GNU_BUILD_ID */) {
            static const char* hx = "0123456789abcdef";
            std::string out;
            for (size_t i = 0; i < descsz; ++i) {
                out += hx[notes[desc_off + i] >> 4];
                out += hx[notes[desc_off + i] & 0xf];
            }
            return out;
        }
        p = desc_off + ((descsz + 3) & ~size_t(3));
    }
    return {};
}
}  // namespace rc::autoprofile

namespace rc::autoprofile {
// Parse an existing layout INI header into (cell, identity). Used by the
// on-console SHADOW check: it lets the generator re-render from the same
// engine cell + image identity a compiled/standing pair already declares, so
// a byte-compare detects drift between the pack data, the renderer and the
// compiled layout without being on the armed path. Returns false on any
// malformed header (never throws on bad numbers).
inline bool parse_header(std::string_view ini, EngineCell& cell, ImageIdentity& id) {
    auto field = [&](const char* key, std::string& out) -> bool {
        std::string pat = std::string("\n") + key + "=";
        size_t k = ini.find(pat);
        if (k == std::string_view::npos) {
            // allow the very first line (no preceding newline)
            std::string head = std::string(key) + "=";
            if (ini.substr(0, head.size()) == head) { k = 0; }
            else return false;
        } else {
            k += 1;  // skip the '\n'
        }
        size_t v = k + std::string(key).size() + 1;
        size_t e = ini.find('\n', v);
        if (e == std::string_view::npos) return false;
        out.assign(ini.substr(v, e - v));
        return true;
    };
    std::string fmt, ptr, maj, min, ns, st, tgt, rev, bid;
    if (!field("Format", fmt) || !field("Target", tgt) || !field("PointerSize", ptr) ||
        !field("EngineMajor", maj) || !field("EngineMinor", min) || !field("NonShipping", ns) ||
        !field("Stats", st) || !field("EngineRevision", rev) || !field("BuildId", bid))
        return false;
    // Non-throwing decimal parse (std::from_chars): a malformed header returns
    // false instead of throwing out of the shadow / activation path.
    auto num = [](const std::string& t, uint32_t& out) noexcept -> bool {
        if (t.empty()) return false;
        const char* b = t.data();
        const char* e = b + t.size();
        auto r = std::from_chars(b, e, out, 10);
        return r.ec == std::errc{} && r.ptr == e;
    };
    if (!num(fmt, cell.format) || !num(ptr, cell.pointer_size) || !num(maj, cell.major) ||
        !num(min, cell.minor))
        return false;
    cell.non_shipping = (ns == "1");
    cell.stats = (st == "1");
    static thread_local std::string tgt_hold;
    tgt_hold = tgt;
    cell.target = tgt_hold.c_str();
    id.engine_revision = rev;
    id.build_id = bid;
    return true;
}

struct ShadowResult {
    bool parsed = false;
    bool matched = false;
    const char* pack_id = "(none)";
    const char* digest = "(none)";
    size_t member_rows = 0, vtable_rows = 0;
    const char* reason = "header not parsed";
};

// Re-render from (compiled members header) and compare to the compiled texts.
// matched==true shows the pack data + renderer round-trip the standing layout
// exactly. The pack was itself built from that same standing layout, so a
// MATCH is a drift detector (pack, renderer and compiled declaration still
// agree), NOT proof that a new game's layout is derived on its own.
inline ShadowResult shadow_check(std::string_view members, std::string_view vtables) {
    ShadowResult s;
    EngineCell cell; ImageIdentity id;
    if (!parse_header(members, cell, id)) { s.reason = "member header unparseable"; return s; }
    s.parsed = true;
    // Compare-only: render() (not generate()), because the shadow never arms.
    Generated g = render(cell, id);
    if (!g.ok) { s.reason = g.reason; return s; }
    s.pack_id = g.pack->id; s.digest = g.pack->digest;
    s.member_rows = g.pack->member_count; s.vtable_rows = g.pack->vtable_count;
    s.matched = (g.members == std::string(members)) && (g.vtables == std::string(vtables));
    s.reason = s.matched ? "pack and renderer reproduce the compiled layout byte-for-byte (drift check, not arming)"
                         : "pack render DIVERGES from the compiled layout (drift; compiled layout still armed)";
    return s;
}
}  // namespace rc::autoprofile
