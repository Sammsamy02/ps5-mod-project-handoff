#pragma once
// ModProfile.hpp — Team S1 (B3). The in-memory per-game runtime profile.
//
// This ONE struct holds every per-game value that used to live in the ~11
// compiled headers of READINESS §2 A-D. It is populated once at startup by
// ModProfileLoader from the preloaded profile data file (profile/<sha>.ini),
// then every consumer reads from rc::modprofile::active() instead of a
// compiled-in constant. One runtime build then serves every Unreal game.
//
// Laws:
//  * Freestanding: <cstdint>/<cstddef>/<cstring> only. No heap, no exceptions,
//    no std::filesystem, no engine headers. Safe to call from noexcept paths.
//  * Fixed capacity: all storage is inline fixed-size arrays (static storage),
//    so install() is a plain memcpy-class operation with no allocation.
//  * The STRUCTURE (which fields exist, pin ids, array sizes, which members are
//    required, validator widths) is generic and compiled. Only the per-game
//    VALUES live here and load from the file.
//  * Big layout texts are referenced by pointer+len into the caller's preloaded
//    buffer (owned for the process lifetime by the activation); the loader
//    copies only small scalars and byte prefixes.
//  * Missing/absent REQUIRED value => the loader refuses (fail closed); nothing
//    is invented here.
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace rc::modprofile {

inline constexpr uint32_t kSchema = 1;

// --- fixed capacities (generic, not per-game) -------------------------------
inline constexpr size_t kHex = 65;          // 64 hex + NUL
inline constexpr size_t kName = 128;
inline constexpr size_t kPath = 256;
inline constexpr size_t kStr = 256;
inline constexpr size_t kShort = 64;
inline constexpr size_t kMaxPrefix = 64;    // matches codecache::Pin max (size<=64)

struct Bytes {               // one prologue prefix / proof byte run
    uint8_t data[kMaxPrefix]{};
    uint32_t size = 0;       // 0 = absent
};

struct TextRef {             // a <<< >>> block: pointer+len into the input buffer
    const char* data = nullptr;
    size_t size = 0;
    bool present() const { return data != nullptr && size != 0; }
};

struct Str { char v[kStr]{}; bool empty() const { return v[0] == '\0'; } };

// --- the profile ------------------------------------------------------------
struct Profile {
    // [profile]
    uint32_t schema = 0;
    char name[kName]{};
    char digest[kHex]{};          // kDigest (evidence/profile digest)
    char body_sha256[kHex]{};     // canonical body hash (loader-verified)
    char image_sha256[kHex]{};    // lookup key
    uint64_t image_size = 0;
    uint64_t image_extent = 0;    // 0xbc602c8 ...  (also rc_seed::kImageFileExtentBytes, shared w/ S2)
    uint64_t image_base = 0;      // 0x400000 (kBaseObserved)
    char exe_path[kPath]{};       // codecache::Profile.path  (S2/B8 sources it)

    // [build]
    uint32_t format = 0, pointer_size = 0, major = 0, minor = 0;
    char target[kShort]{}, engine_revision[kShort]{}, build_id[kShort]{};
    uint32_t build_configuration = 0, stats = 0;
    char build_source[kStr]{}, stats_source[kStr]{};
    bool non_shipping = false, stats_layout = false;
    bool fname_case_preserving = false;
    uint32_t fname_output_bytes = 0, fname_output_alignment = 0;

    // [pool]  (rc_seed::kPoolLayout)
    uint64_t pool_current_block_offset = 0, pool_current_byte_offset_offset = 0;
    uint64_t pool_blocks_offset = 0, pool_none_id_offset = 0, pool_byte_property_id_offset = 0;
    uint32_t pool_block_offset_bits = 0, pool_max_blocks = 0;

    // [seeds]  (SeedTable.h)
    bool generated = false, required_set_complete = false;
    uint64_t fname_pool_label = 0, guobject_array_label = 0, fname_ctor_label = 0;
    uint64_t gmalloc_label = 0, gmalloc_instance_label = 0, gmalloc_vtable_label = 0, sco_label = 0;
    bool guobject_present = false, fname_ctor_present = false, gmalloc_present = false, sco_present = false;
    char guobject_derivation[kStr]{}, fname_ctor_derivation[kStr]{};
    char gmalloc_derivation[kStr]{}, sco_derivation[kStr]{};

    // [fname]  (rc::fnameprofile.kPrefix)
    Bytes fname_ctor_prefix;

    // [code_pins]  (prefixes; labels come from [seeds]/[listener]/[thread])
    Bytes sco_prefix;             // pin id 2
    Bytes listener_add_prefix;    // pin id 5
    Bytes listener_remove_prefix; // pin id 6
    Bytes thread_name_prefix;     // pin id 3

    // [listener]  (rc::englistener)  (add/remove labels; guobject label = seeds)
    uint64_t listener_add_label = 0, listener_remove_label = 0;

    // [tostring]  (rc::engtostring)  — pin removed (B5); constants kept
    uint32_t tostring_probe_id = 0, tostring_probe_expect_units = 0, tostring_out_units = 0;
    char tostring_probe_expect[kShort]{};
    uint64_t tostring_label = 0;  // provenance only (OPTIONAL)
    Bytes tostring_prefix;        // provenance only (OPTIONAL)

    // [thread]  (NativeNameThread)
    uint64_t thread_accessor = 0, thread_game_name = 0, thread_render_name = 0, thread_empty_name = 0;

    // [pak]  (ModBridge::PakMount::EnginePins)
    bool pak_present = false;
    uint64_t pak_vtable = 0, pak_platform_mgr_get = 0, pak_platform_mgr_getpf = 0;
    uint64_t pak_mountpak_delegate = 0, pak_handle_mount_pak = 0, pak_delegate_vtable = 0;
    uint64_t pak_execute = 0, pak_lower_impl = 0, pak_snapshot_delegate = 0;
    uint64_t pak_snapshot_vtable = 0, pak_snapshot_execute = 0;
    uint32_t pak_slot_get_lower_level = 0, pak_slot_delegate_execute = 0;
    uint32_t pak_delegate_inst_off = 0, pak_delegate_size_off = 0, pak_count_offset = 0;
    uint64_t pak_proof_linked[6]{};
    Bytes    pak_proof[6];

    // [layout.members] / [layout.vtables]  (the staged INI pair text)
    TextRef layout_members;
    TextRef layout_vtables;

    // [features]
    bool feature_pak_mount = false, feature_delete_listener = false, feature_thread_name = false;

    bool valid = false;  // set true by the loader only on a complete, consistent parse
};

// --- process-wide active profile --------------------------------------------
// install() is called ONCE by the bridge after the code cache authenticates the
// image and the loader has parsed the matching profile. Before install(), or
// when no profile matched, active() returns nullptr and seeds never arm
// (fail closed == the "identification launch").
inline Profile g_active{};
inline bool g_installed = false;

inline void install(const Profile& p) noexcept { g_active = p; g_installed = p.valid; }
inline const Profile* active() noexcept { return g_installed ? &g_active : nullptr; }
inline void reset() noexcept { g_active = Profile{}; g_installed = false; }

// --- layout-text offset lookup (used by the rewired layout consumers) -------
// Finds `Member=0x..` inside `[Class]` in an INI text block. Returns false when
// absent (caller fails closed). Pure, bounds-checked, no allocation.
bool layout_offset(const TextRef& text, const char* klass, const char* member, uint32_t& out) noexcept;

} // namespace rc::modprofile
