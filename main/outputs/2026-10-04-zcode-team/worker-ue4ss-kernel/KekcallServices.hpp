// KekcallServices.hpp — UE4SS-runtime facade over the kstuff kernel services
// (kekcall nr=7 safe_read, nr=8 safe_probe, nr=9 get_input_state), staged by
// worker-ue4ss-kernel (2026-10-06).
//
// STATUS: staged integration header, host-compiled and host-tested (see
// host-test/); never run on console. No console contact, no game claims.
//
// Provenance:
//   - ABI (structs, error codes, kekcall numbers, register contract) mirrors
//     worker-kernel-saferead/safe_read_client.h VERBATIM (nr=7/nr=8/-1 ping)
//     and worker-kernel-input/INPUT-SERVICE.md §3.2 (nr=9, 'KIN1' snapshot).
//     That header is a research prototype whose inline-asm shim cannot compile
//     on a host; this facade keeps the ABI and replaces the transport with an
//     INJECTED FUNCTION POINTER so host tests mock it and the console adapter
//     (KekcallConsoleShim.hpp) wires the real __sp_kekcall gadget path.
//   - Fallback law (assignment, item 4): every consumer must fall back to the
//     current workaround when the resident module is OLD, i.e. when a service
//     call answers ENOSYS (-78) or the client has no channel. Both conditions
//     are boot-static, so this facade LATCHES them after the first observed
//     occurrence and reports Status::Enosys / Status::NoChannel — callers then
//     take the legacy path without per-call cost. Transient errors
//     (KSR_ERR_STRUCT, KSR_ERR_NOENT, ...) never latch.
//
// Concurrency: init() may be called from module_start, ScanGame, or the pad
// factory (all serialized before UpdateThread traffic by the runtime's
// single-threaded init discipline); state transitions are atomic and
// monotonic (Unknown -> Available | Enosys | NoChannel, terminal).

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ksrv
{
    // ---- ABI mirror (safe_read_client.h / INPUT-SERVICE.md §3.2) -----------

    // Negative return codes — service family (nr=7/8).
    constexpr int32_t KSR_ERR_NOENT = -10;    // pid not found / process gone
    constexpr int32_t KSR_ERR_UNMAPPED = -11; // address not covered / page not present
    constexpr int32_t KSR_ERR_PROT = -12;     // mapped but not readable
    constexpr int32_t KSR_ERR_LEN = -13;      // zero or oversized length
    constexpr int32_t KSR_ERR_RANGE = -14;    // kernel-range VA or copyout fault
    constexpr int32_t KSR_ERR_STRUCT = -15;   // kernel structure unreadable (transient)
    constexpr int32_t KSR_ERR_ENOSYS = -78;   // kekcall number unknown: OLD kstuff build
    constexpr int32_t KSR_ERR_NOCHANNEL = -100; // client not initialized / gadget missing

    // Probe codes (nr=8).
    constexpr int32_t KSR_PROBE_READABLE = 1;
    constexpr int32_t KSR_PROBE_NOREAD = 0;
    constexpr int32_t KSR_PROBE_UNMAPPED = -1;
    constexpr int32_t KSR_PROBE_FAULT = -2;
    constexpr int32_t KSR_PROBE_BADARGS = -3;

    constexpr uint64_t KSR_MAX_LEN = 0x10000u; // 64 KiB per kekcall, kernel-synced

    // Input service errors — namespace continuation (INPUT-SERVICE.md §3.2).
    constexpr int32_t KINPUT_ERR_BADARGS = -16;
    constexpr int32_t KINPUT_ERR_KIND = -17;
    constexpr int32_t KINPUT_ERR_PORT = -18;
    constexpr int32_t KINPUT_ERR_NOSTATE = -19; // no derived offsets this FW: clean refusal
    constexpr int32_t KINPUT_ERR_TORN = -20;    // strict mode only
    constexpr int32_t KINPUT_ERR_STRUCT = -21;  // state unreadable: transient

    constexpr uint32_t KINPUT_KIND_PAD = 0;

    // kekcall nr=9 request (copyin'd by the kernel like nr=5/nr=7 requests).
    struct KinputRequest
    {
        uint32_t kind;  // KINPUT_KIND_*
        uint32_t index; // pad port (0..3)
        uint64_t buf;   // caller VA to receive the snapshot
        uint32_t len;   // buffer size; must be >= sizeof(KinputPadState)
        uint32_t flags; // KINPUT_F_* (0 in v1)
    };

    // Request carriers at namespace scope (layout shared verbatim with the
    // kernel side and the host mock; the internal service functions fill
    // and pass these as a1 exactly like safe_read_client.h's ksr_request /
    // ksr_probe_request and INPUT-SERVICE.md's kinput_request).
    struct SafeReadReq // ksr_request (nr=7)
    {
        uint32_t pid;
        uint32_t pad0;
        uint64_t addr;
        uint64_t len;
        uint64_t buf;
    };
    struct ProbeReq // ksr_probe_request (nr=8)
    {
        uint32_t pid;
        uint32_t pad0;
        uint64_t addr;
        uint64_t len;
    };

    // kekcall nr=9 normalized pad snapshot, ABI "KIN1" (INPUT-SERVICE.md
    // §3.2; firmware-invariant, NOT the raw kernel struct). SIZE NOTE: the
    // doc labels the struct "136 bytes", but its declared fields sum to 64
    // (verified by static_assert below). This header mirrors the DECLARED
    // field layout verbatim and asserts the real size; the kernel emitter
    // and this client must agree on the compiled layout, and every request
    // carries req.len = sizeof(this struct) so a conforming kernel can never
    // copy out more bytes than the client offered. Resolving 64-vs-136
    // (extra reserved tail?) is a worker-kernel-input stage-2 decision; the
    // staged integration only needs layout-stable fields + magic check.
    struct KinputPadState
    {
        uint32_t magic;           // 'KIN1'
        uint32_t buttons;         // ScePadButton layout; 0x80000000 = Intercepted
        uint8_t lx, ly, rx, ry;   // 0..255, center 128
        uint8_t l2, r2;           // 0..255 analog triggers
        uint8_t connected;        // 0/1
        uint8_t torn;             // 1 if tear-retry budget exhausted
        uint16_t seq;             // driver sample counter if one exists, else 0
        uint64_t timestamp_us;    // kernel stamp if available, else 0
        uint8_t touch_fingers;    // 0..2
        uint8_t _r0[3];
        uint16_t touch_x[2];
        uint16_t touch_y[2];
        uint8_t touch_id[2];
        uint8_t _r1[2];
        uint32_t connection_gen;  // changes on reattach
        uint32_t _r2[3];
    };
    static_assert(sizeof(KinputPadState) == 64, "KIN1 v1 ABI: declared §3.2 fields compile to 64 bytes");
    static_assert(offsetof(KinputPadState, timestamp_us) == 24, "KIN1 timestamp_us offset");
    constexpr uint32_t KINPUT_MAGIC_KIN1 = 0x314E494Bu; // 'KIN1' little-endian

    // ---- transport seam ------------------------------------------------------

    // Exactly the __sp_kekcall shim signature (safe_read_client.h). The
    // console adapter passes the gadget-backed implementation; host tests
    // pass a mock. a2..a6 are unused by nr=7/8/9 but kept for ABI fidelity.
    struct VTable
    {
        long (*kekcall)(int nr, long a1, long a2, long a3, long a4, long a5, long a6);
    };

    enum class Status : int8_t
    {
        Unknown = 0,   // not yet probed (or ping not answered yet)
        Available = 1, // a kstuff module answers kekcalls (ping == 0)
        Enosys = 2,    // old resident build: a service nr answered -78 (LATCHED)
        NoChannel = 3, // no vtable / no syscall gadget (LATCHED)
    };

    using KekcallFn = long (*)(int nr, long a1, long a2, long a3, long a4, long a5, long a6);

    // Boot-static state, shared across the TUs of one PRX (inline variables).
    inline std::atomic<KekcallFn> g_kekcall{nullptr};
    inline std::atomic<Status> g_status{Status::Unknown};

    inline KekcallFn transport() { return g_kekcall.load(std::memory_order_acquire); }

    // ---- lifecycle ------------------------------------------------------------

    // Idempotent. Safe to call from every integration site; the first call
    // with a non-null kekcall wins (compare-exchange). A nullptr only records
    // "no channel" when nothing better was ever installed.
    inline void init(const VTable *vt)
    {
        if (vt == nullptr || vt->kekcall == nullptr)
        {
            Status expected = Status::Unknown;
            (void)g_status.compare_exchange_strong(expected, Status::NoChannel);
            return;
        }
        KekcallFn expected_null = nullptr;
        if (!g_kekcall.compare_exchange_strong(expected_null, vt->kekcall,
                                               std::memory_order_acq_rel))
        {
            return; // first installer wins; later installs are no-ops
        }
        // First installer clears any provisional NoChannel latch recorded
        // before the real transport existed.
        Status st = g_status.load(std::memory_order_acquire);
        if (st == Status::NoChannel)
        {
            g_status.store(Status::Unknown, std::memory_order_release);
        }
    }

    // Liveness: kekcall nr=-1 returns 0 when a kstuff module answers
    // (safe_read_client.h: alive means "SOME kstuff present" — service
    // presence is proven per call by non-ENOSYS returns, never by the ping).
    // Returns the raw ping value (>=0 alive) or KSR_ERR_NOCHANNEL.
    inline long ping()
    {
        const KekcallFn fn = transport();
        if (fn == nullptr)
        {
            Status expected = Status::Unknown;
            (void)g_status.compare_exchange_strong(expected, Status::NoChannel);
            return KSR_ERR_NOCHANNEL;
        }
        return fn(-1, 0, 0, 0, 0, 0, 0);
    }

    // True when the service family may be used (ping answered). Does NOT
    // prove nr=7/8/9 exist; those are proven per call, and the first -78
    // latches Enosys so this flips false for the rest of the boot.
    inline bool maybe_available()
    {
        Status st = g_status.load(std::memory_order_acquire);
        if (st == Status::Available) { return true; }
        if (st != Status::Unknown) { return false; }
        if (ping() == 0)
        {
            Status expected = Status::Unknown;
            if (!g_status.compare_exchange_strong(expected, Status::Available))
            {
                return g_status.load(std::memory_order_acquire) == Status::Available;
            }
            return true;
        }
        // Ping failed: keep Unknown (a kstuff-less boot is not latched — the
        // channel itself works, there is simply no module; per-call ENOSYS
        // semantics will latch if a service call ever answers -78).
        return false;
    }

    inline Status status() { return g_status.load(std::memory_order_acquire); }

    // Record a service-call return: latches Enosys on -78. Returns the rc.
    inline int32_t record(int32_t rc)
    {
        if (rc == KSR_ERR_ENOSYS)
        {
            Status expected = Status::Unknown;
            if (!g_status.compare_exchange_strong(expected, Status::Enosys))
            {
                g_status.store(Status::Enosys, std::memory_order_release);
            }
        }
        return rc;
    }

    // ---- services (nr=7 / nr=8 / nr=9) ----------------------------------------

    // nr=8: is OUR [addr, addr+len) mapped+readable in the caller's page
    // tables? Returns KSR_PROBE_*; KSR_ERR_NOCHANNEL when uninstalled.
    // ENOSYS latches and is returned to the caller (non-probe code space).
    inline int32_t safe_probe(uint64_t addr, uint64_t len)
    {
        const KekcallFn fn = transport();
        if (fn == nullptr) { return KSR_ERR_NOCHANNEL; }
        if (len == 0 || len > KSR_MAX_LEN) { return KSR_PROBE_BADARGS; }
        ProbeReq req{0u, 0u, addr, len};
        return record(static_cast<int32_t>(fn(8, reinterpret_cast<long>(&req), 0, 0, 0, 0, 0)));
    }

    // Single nr=7 call: read up to KSR_MAX_LEN bytes. Returns bytes read
    // (>0) or a negative KSR_ERR_*.
    inline int32_t safe_read_once(uint64_t addr, void *buf, uint64_t len)
    {
        const KekcallFn fn = transport();
        if (fn == nullptr) { return KSR_ERR_NOCHANNEL; }
        if (len == 0 || len > KSR_MAX_LEN) { return KSR_ERR_LEN; }
        SafeReadReq req{0u, 0u, addr, len, reinterpret_cast<uint64_t>(buf)};
        return record(static_cast<int32_t>(fn(7, reinterpret_cast<long>(&req), 0, 0, 0, 0, 0)));
    }

    // Chunked nr=7: reads `len` bytes in <=64 KiB kekcalls. Returns `len` on
    // full success (all chunks transferred — the same all-or-nothing evidence
    // law as the copyin probe's write-return accounting) or the first chunk
    // failure's negative KSR_ERR_*. Bytes before a failing chunk remain in
    // buf but the return never reports success for them.
    inline int64_t safe_read(uint64_t addr, void *buf, uint64_t len)
    {
        if (len == 0) { return KSR_ERR_LEN; }
        if (len > 0x1000000ull) { return KSR_ERR_LEN; } // 16 MiB class budget cap (bridge parity)
        uint8_t *out = static_cast<uint8_t *>(buf);
        uint64_t done = 0;
        while (done < len)
        {
            const uint64_t chunk = (len - done) < KSR_MAX_LEN ? (len - done) : KSR_MAX_LEN;
            const int32_t rc = safe_read_once(addr + done, out + done, chunk);
            if (rc < 0) { return rc; }
            if (static_cast<uint64_t>(rc) != chunk) { return KSR_ERR_STRUCT; } // short read: treat as oracle failure
            done += chunk;
        }
        return static_cast<int64_t>(len);
    }

    // nr=9: normalized pad snapshot for port `index` (0..3). Returns bytes
    // copied (>= sizeof(KinputPadState)) or negative KINPUT_ERR_* / KSR_ERR_*.
    inline int32_t pad_read(uint32_t index, KinputPadState *out)
    {
        const KekcallFn fn = transport();
        if (fn == nullptr) { return KSR_ERR_NOCHANNEL; }
        if (out == nullptr || index > 3) { return KINPUT_ERR_BADARGS; }
        KinputRequest req{KINPUT_KIND_PAD, index,
                          reinterpret_cast<uint64_t>(out),
                          static_cast<uint32_t>(sizeof(KinputPadState)), 0u};
        return record(static_cast<int32_t>(fn(9, reinterpret_cast<long>(&req), 0, 0, 0, 0, 0)));
    }

    // ---- listing-gap classification (initializer integration) -----------------

    // Maps a safe_read/safe_probe return onto the copyin oracle's three-class
    // verdict (rc_seed::CopyinProbeClass values: 1 Readable / 0 Refused /
    // -1 Unavailable) so resolve_listing_gap consumes kernel evidence with
    // its existing, conservative semantics (a refusal NEVER claims "unmapped").
    enum class GapClass : int8_t
    {
        Readable = 1,
        Refused = 0,
        Unavailable = -1,
        Absent = -128, // kernel service not present (ENOSYS/NOCHANNEL): use the legacy oracle
    };

    static inline GapClass gap_class_from_read_rc(int64_t rc)
    {
        if (rc > 0) { return GapClass::Readable; }
        switch (static_cast<int32_t>(rc))
        {
        case KSR_ERR_PROT:   // page tables say not readable — the kernel refuses this source
        case KSR_ERR_UNMAPPED:
        case KSR_ERR_RANGE:
            return GapClass::Refused;
        case KSR_ERR_ENOSYS:
        case KSR_ERR_NOCHANNEL:
            return GapClass::Absent;
        default: // LEN/NOENT/STRUCT: the oracle itself failed
            return GapClass::Unavailable;
        }
    }
}
