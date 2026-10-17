// RuntimePakMount.hpp - cap #7 step 2: no-file-change runtime pak+IoStore mount.
//
// Data-only. This unit NEVER writes a game file, NEVER writes an engine code
// page, and installs NO detour. Its only engine interaction is to INVOKE the
// engine's own already-bound FCoreDelegates::MountPak delegate (pin P1) with an
// FString path to a mod container that lives under our own mod root. The engine
// then performs its native pak + IoStore(utoc/ucas) + package-store mount
// (verified from Epic source + the decrypted executable; see evidence/pins.json
// pin P8: in UE 5.6.1 a single FPakPlatformFile::Mount does all three).
//
// Generic for the engine: NO game name, NO mod name, NO literal container name
// in logic. Per-game facts (which container file, which mod-root path) arrive as
// DATA via MountRequest, produced automatically elsewhere from the game's own
// files + the installed mod.
//
// Every address this unit uses is image-relative and arrives in EnginePins,
// which the integrator fills from the per-version pin table (two-source-verified,
// evidence/pins.json). The unit re-verifies each pin through IEngineBridge before
// any call and fails closed on any mismatch: on refusal nothing is called and the
// game runs vanilla.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstddef>

namespace ModBridge::PakMount
{
// Image-relative vaddrs + fail-closed signatures for one engine build.
// Filled from the per-version pin table; NOT hand-written in a .cpp.
struct EnginePins
{
    // Identity / admission
    char     image_sha256[65] = {0};   // expected main-exe image sha (hex)
    uint64_t pak_vtable = 0;            // P5: FPakPlatformFile vtable (image-rel)
    uint64_t platform_mgr_get = 0;      // P3: FPlatformFileManager::Get()
    uint64_t platform_mgr_getpf = 0;    // P4: GetPlatformFile()
    uint32_t slot_get_lower_level = 0;  // P6: IPlatformFile::GetLowerLevel (vtable off)
    uint32_t slot_delegate_execute = 0; // P2: TDelegateInstance::Execute (vtable off)
    // Mount entry (delegate route; preferred)
    uint64_t mountpak_delegate = 0;     // P1: FCoreDelegates::MountPak object (image-rel)
    uint32_t delegate_inst_off = 0;     // FDelegateBase instance-ptr offset (0)
    uint32_t delegate_size_off = 8;     // FDelegateBase DelegateSize offset
    // Direct-this fallback entry (only if delegate route unavailable)
    uint64_t handle_mount_pak = 0;      // P7: HandleMountPakDelegate(this, FString&, int32)
    uint64_t delegate_vtable = 0, execute = 0, lower_impl = 0;
    uint64_t snapshot_delegate = 0, snapshot_vtable = 0, snapshot_execute = 0;
    uint32_t pak_count_offset = 0;
    struct Proof { uint64_t linked=0; uint8_t bytes[64]{}; size_t size=0; };
    Proof proofs[6]{};
    bool     valid() const;             // all required fields present
};

// Per-attempt data (produced automatically from the game's own files + the mod).
struct MountRequest
{
    // Absolute on-device path to the mod container (<modroot>/<name>_P.pak) whose
    // sibling .utoc/.ucas the engine will discover. No name is interpreted here.
    std::string pak_path_abs;
    int32_t     order = -1;             // -1 => engine computes mount order itself
};

enum class Verdict
{
    Mounted,            // engine returned a live pak + the mounted-count grew
    RefusedPins,        // a pin failed re-verification (fail-closed)
    RefusedUnbound,     // MountPak delegate size == 0 (engine not ready / no pak support)
    RefusedNoPakLayer,  // chain census found no FPakPlatformFile
    RefusedBadRequest,  // empty/ill-formed request
    RefusedCallFailed,  // engine mount returned null / count did not grow
    RefusedNotGameThread,
    RefusedSnapshot,
    RefusedPriority,
    MountedUnverified
};

struct PakInfo { uint64_t identity=0; uint32_t order=0; };
// Snapshot is copied under the engine lock; pointers are opaque identity only.
// The integrator implements this over the live process; the host test implements
// it over an in-memory fake image. The unit asks the bridge for everything so it
// never touches raw platform memory itself.
class IEngineBridge
{
public:
    virtual ~IEngineBridge() = default;

    // Live base of the main executable image, or 0 if unavailable.
    virtual uint64_t module_view_base() = 0;
    // Live address of the platform-file chain head (the manager's TopPlatformFile).
    // The integrator resolves it via the verified Get()/GetPlatformFile() pins
    // (P3/P4); 0 if unavailable.
    virtual uint64_t platform_chain_head() = 0;
    // The running main-exe image sha256 (hex, lowercase), "" if unknown.
    virtual std::string image_sha256() = 0;

    // Read a pointer-sized word at a LIVE address (already base-relocated by caller).
    virtual bool read_ptr(uint64_t live_addr, uint64_t& out) = 0;
    // Read a 32-bit word at a LIVE address.
    virtual bool read_u32(uint64_t live_addr, uint32_t& out) = 0;

    // Fail-closed signature gate for a LIVE code address against the pinned
    // prologue bytes; false => refuse. Mirrors the runtime's admission gate.
    virtual bool verify_code(uint64_t live_addr, const uint8_t* sig, size_t n) = 0;

    // Is the current thread the engine game thread? (mount must run there).
    virtual bool is_game_thread() = 0;

    // Marshal a narrow path into an engine-allocator FString and hand back an
    // opaque pointer usable as the delegate's const FString& argument. Returns
    // nullptr on failure. The bridge owns/frees it after the call returns.
    virtual void* make_fstring(const std::string& path) = 0;
    virtual void free_fstring(void* fstr) = 0;

    // Invoke a resolved delegate-instance Execute thunk:
    //   IPakFile* (*)(void* inst, const FString* path, int32 order)
    // Returns the raw IPakFile* (nullptr on engine-side failure). Only called
    // after the unit has resolved `exec_fn` and `inst` from verified pins.
    virtual uint64_t call_execute(uint64_t exec_fn, uint64_t inst,
                                  void* fstr, int32_t order) = 0;

    // Count of currently mounted paks (read via the engine's own mounted-paks
    // accessor the integrator wires up); used only for the before/after verify.
    virtual bool mounted_pak_count(uint64_t pak_layer_live, uint32_t& out) = 0;

    virtual bool snapshot_paks(uint64_t, std::vector<PakInfo>&) { return false; }
    // Optional logging (no-op acceptable).
    virtual void log(const char* /*line*/) {}
};

// Result detail for logging / self-test.
struct MountOutcome
{
    Verdict verdict = Verdict::RefusedBadRequest;
    uint64_t pak_layer = 0;     // live FPakPlatformFile* found by census (0 if none)
    uint32_t count_before = 0;
    uint32_t count_after = 0;
    uint64_t returned_pak = 0;
    int32_t order_argument = 0;
    bool call_made = false;
    const char* why = "";       // short static reason
};

// The one entry point. Pure, generic, fail-closed. Performs NO file or code-page
// write and installs NO detour; it only re-verifies pins and invokes the engine's
// own bound MountPak delegate from the game thread.
MountOutcome mount(const EnginePins& pins, const MountRequest& req, IEngineBridge& eng);

// Pure helper (exposed for tests): does this chain layer's vptr identify the
// FPakPlatformFile? live_vtable is the vptr read from the instance; expect_live
// is pins.pak_vtable rebased to the live image.
bool is_pak_layer(uint64_t live_vtable, uint64_t expect_live);
bool valid_absolute_path(const std::string& path);
bool choose_order(const std::string& path, const std::vector<PakInfo>& before,
                  int32_t& argument, uint32_t& effective);

} // namespace ModBridge::PakMount
