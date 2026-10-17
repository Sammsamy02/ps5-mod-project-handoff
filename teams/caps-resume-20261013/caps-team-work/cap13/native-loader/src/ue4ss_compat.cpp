// ue4ss_compat.cpp -- UE4SS.dll export surface for the PS5 PE route
// (DESIGN/native-dll-loader.md section 6). SKETCH-GRADE: the largest open
// work item. The structure here is the contract; the census (JG's
// pe_analyzer.py output) decides which rows get real bodies in M1.
//
// Two halves:
//   Half 1 (services): MSVC-mangled exports -> mbm_host_v1 calls. Every
//     service the port already exposes behind the C table is reused; this
//     file only demarshals Win64 calls into it.
//   Half 2 (boundary objects): the CppUserModBase base subobject is
//     constructed AT MSVC LAYOUT (32-byte std::wstring fields, MSVC vftable
//     image, bare-array vtable). Offsets/mangles/vtable slot order are
//     pinned Mac-side by tools/pin_msvc_layout.sh, never trusted from
//     memory (staleness register item 1).
//
// The allocator-pairing rule (design 6b): shim code that must free memory
// the GUEST allocated resolves the guest's own `free` import at load time
// and calls it through the bridge. The shim allocator never frees guest
// heap and vice versa.
//
// CAP13 proposal-only; design-grade; not built.
#include <cstdint>
#include <string>
#include <unordered_map>

#include "Mod/NativeModAbi.h" // the #9 host C table (LOCAL-SOURCE header)
#include "pe_loader.h"
#include "win64_bridge.h"

namespace cap13 {

// ---- MSVC boundary layout, pinned via tools/pin_msvc_layout.sh ----------
// [TRAINING-KNOWLEDGE pending the pin]: MSVC std::wstring occupies 32 bytes:
//   0x00 union { wchar16* ptr; wchar16 buf[8]; }   (SSO-8)
//   0x10 size_type  (_Mysize)
//   0x18 size_type  (_Myres)  -- SSO capacity is 7 chars + NUL
// The shim NEVER manipulates these fields beyond value-init of an empty
// string (16 zero bytes + size 0 + res 7) and destruction via the guest's
// own operator delete. Mod display strings come from the manifest instead.
struct MsvcWstring32
{
    uint8_t raw[32];
};
static_assert(sizeof(MsvcWstring32) == 32, "MSVC std::wstring boundary size");

// CppUserModBase MSVC base layout (single inheritance; vptr at 0). Field
// offsets below are PLACEHOLDERS until pin_msvc_layout.sh output is baked
// in -- the M1 build refuses to compile while placeholders remain
// (static_assert(kLayoutPinned)).
struct CppUserModBaseMsvc
{
    uint8_t vptr[8]; // -> shim vftable image (bare array, no Itanium header)
    MsvcWstring32 mod_name;
    MsvcWstring32 mod_version;
    MsvcWstring32 mod_authors;
    MsvcWstring32 mod_description;
    // ... remaining fields from the vendored CppUserModBase.hpp, at MSVC
    // offsets produced by the pinning tool. PLACEHOLDER count.
};
[[maybe_unused]] static constexpr bool kLayoutPinned = false; // M1: becomes true with pinned data
// static_assert(kLayoutPinned, "bake pin_msvc_layout.sh output before M1");

// ---- shim vftable image -------------------------------------------------
// The base ctor installs this image into the guest object. Slots point to
// guest->host trampolines (win64 bridge) that forward into mbm_host_v1
// dispatch. Slot ORDER comes from the vendored header's declaration order
// (MSVC: declaration order, dtor pair at declaration position, bare array);
// the pinning tool emits the exact qword list from compiled .rdata.
struct ShimVftable
{
    void* slots[16]; // vendored CppUserModBase has ~14 virtuals + margin
};

// ---- the service demarshal table (Half 1) -------------------------------
struct Ue4ssCompat
{
    const mbm_host_v1* host = nullptr; // the #9 host C table
    uint64_t mod_token = 0;

    // Census-driven export rows: (mangled MSVC name) -> bridge trampoline.
    // M1 generates this table from pe_analyzer's import census + the pinned
    // header set. The shapes below are illustrative rows.
    std::unordered_map<std::string, void*> exports;

    // Example service row (illustrative shape):
    //   ??0CppUserModBase@RC@@QEAA@XZ  (base ctor)
    //     Win64 thiscall: this in RCX.
    //     Body (SysV, behind the bridge): value-init the MSVC base subobject
    //     at pinned offsets, install the shim vftable image, return.
    //   ?register_keydown_event@CppUserModBase@RC@@QEAAX... (mangled per
    //     vendored header)
    //     Body: demarshal args -> host->register_key_down(host->ctx,
    //     mod_token, key, mods, bridged_callback, user, &id).
};

// ---- allocator pairing (design 6b) --------------------------------------
// Resolved at load time from the GUEST's own import table: the address the
// guest uses for `free`/`operator delete`. Guest-allocated boundary memory
// is only ever released through this pointer (via the bridge).
struct GuestAllocators
{
    void* guest_free = nullptr; // resolved from the guest's ucrt/msvcrt rows
};

// ---- what M1 must complete before the first real mod (honest list) ------
//  1. Bake pin_msvc_layout.sh output (record layouts, vftable slot list,
//     mangled names for the pinned header set) into the tables above.
//  2. Generate the export row table from the mod's real import census.
//  3. Bridge-trampoline every row into executable memory (J1 dependency).
//  4. Wire start_mod/uninstall_mod + the ~5 host-fired virtual slots into
//     mbm_mod_v1 (pe_adapter.cpp owns that side).
//  5. Manifest-based name/version/authors/description (no wstring parsing).

} // namespace cap13
