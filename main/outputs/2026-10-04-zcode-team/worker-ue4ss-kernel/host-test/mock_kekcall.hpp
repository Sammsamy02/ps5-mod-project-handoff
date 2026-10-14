// mock_kekcall.hpp — host mock of the kstuff kernel services for the
// worker-ue4ss-kernel host tests (2026-10-06). Test infrastructure only.
//
// Models the three boot classes the integration must survive:
//   ModuleNew    — rebuilt kstuff: nr=-1 ping 0; nr=7/8/9 live services
//   ModuleOld    — pre-rebuild resident kstuff: nr=-1 ping 0 (it exists),
//                  nr=7/8/9 answer ENOSYS (-78)   [the fallback law case]
//   ModuleAbsent — no hook at all: every nr degenerates to plain getppid
//                  (returns a positive parent pid — the client must not
//                  misread it as a byte count)
//
// The memory model distinguishes the classes the live evidence names:
//   Image        — allowlisted for copyin AND page-table readable
//   DmClass      — the live Blocks[0]=0x22C0000000 hypothesis class:
//                  page-table readable (safe_read succeeds) but NOT on the
//                  kernel copyin source-class allowlist (write(2) refuses
//                  with EFAULT) — the exact case where the kernel service
//                  upgrades on the copyin oracle
//   ProtNone     — mapped, no read: both oracles refuse
//   Unmapped     — no page: safe_read -11, copyin refuses
// The legacy copyin oracle (bridge_kernel_write_copyin_probe replica,
// sink file + read-back + 3-slot capture) is mocked alongside so the
// fallback path is exercised end-to-end, not stubbed.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "../KekcallServices.hpp"

namespace mock
{
    enum class RangeKind : uint8_t
    {
        Image = 0,
        DmClass = 1,
        ProtNone = 2,
        Unmapped = 3,
    };

    struct Range
    {
        uint64_t lo, hi; // [lo, hi)
        RangeKind kind;
        std::vector<uint8_t> bytes; // backing for readable kinds (hi-lo bytes)
    };

    enum class Module : uint8_t
    {
        New = 0,
        Old = 1,
        Absent = 2,
    };

    struct Counters
    {
        long ping = 0;
        long safe_read = 0;
        long safe_probe = 0;
        long pad_read = 0;
        long copyin_probe = 0; // legacy-oracle mock (not a kekcall)
    };

    // The singletons tests reset per case.
    inline std::vector<Range> g_ranges;
    inline Module g_module = Module::New;
    inline Counters g_counters;
    inline bool g_sink_open_fails = false; // legacy oracle: unavailable class

    // ---- legacy bridge copyin-oracle state (3-slot capture, sink emulation)
    inline uint8_t g_capture[3][0x1000];
    inline uint64_t g_capture_addr[3];
    inline uint32_t g_capture_len[3];
    inline bool g_capture_valid[3];
    inline uint32_t g_capture_next = 0;

    inline void capture_reset()
    {
        std::memset(g_capture, 0, sizeof g_capture);
        std::memset(g_capture_addr, 0, sizeof g_capture_addr);
        std::memset(g_capture_len, 0, sizeof g_capture_len);
        std::memset(g_capture_valid, 0, sizeof g_capture_valid);
        g_capture_next = 0;
    }

    inline void reset(Module m)
    {
        g_ranges.clear();
        g_module = m;
        g_counters = Counters{};
        g_sink_open_fails = false;
        capture_reset();
    }

    inline Range &add_range(uint64_t lo, uint64_t hi, RangeKind kind)
    {
        g_ranges.push_back(Range{lo, hi, kind, std::vector<uint8_t>(static_cast<size_t>(hi - lo), 0)});
        return g_ranges.back();
    }

    inline const Range *find_range(uint64_t addr)
    {
        for (const auto &r : g_ranges)
        {
            if (addr >= r.lo && addr < r.hi) { return &r; }
        }
        return nullptr;
    }

    // ---- nr=9 scripted pad state -------------------------------------------
    struct PadPort
    {
        // Script of snapshots served in order; the last one repeats.
        std::vector<ksrv::KinputPadState> script;
        size_t pos = 0;
        int32_t forced_rc = 0; // 0 = serve script; negative = always refuse
    };
    inline PadPort g_pad_ports[4];

    inline void pad_script(uint32_t port, std::vector<ksrv::KinputPadState> states)
    {
        g_pad_ports[port].script = std::move(states);
        g_pad_ports[port].pos = 0;
        g_pad_ports[port].forced_rc = 0;
    }

    inline ksrv::KinputPadState make_state(uint32_t buttons, uint8_t lx, uint8_t ly,
                                           uint8_t rx, uint8_t ry, uint8_t l2, uint8_t r2,
                                           uint8_t connected, uint64_t ts)
    {
        ksrv::KinputPadState st{};
        st.magic = ksrv::KINPUT_MAGIC_KIN1;
        st.buttons = buttons;
        st.lx = lx; st.ly = ly; st.rx = rx; st.ry = ry;
        st.l2 = l2; st.r2 = r2;
        st.connected = connected;
        st.timestamp_us = ts;
        st.touch_fingers = 0;
        st.connection_gen = 1;
        return st;
    }

    // ---- the kekcall mock ----------------------------------------------------
    inline long kekcall(int nr, long a1, long, long, long, long, long)
    {
        if (nr == -1)
        {
            ++g_counters.ping;
            return (g_module == Module::Absent) ? 42 /* plain getppid */ : 0;
        }
        if (g_module != Module::New)
        {
            // Old module: unknown kekcall number -> ENOSYS. Absent module:
            // the packed number is ignored, getppid returns a pid.
            return (g_module == Module::Old) ? static_cast<long>(ksrv::KSR_ERR_ENOSYS) : 42;
        }
        if (nr == 7)
        {
            ++g_counters.safe_read;
            auto *req = reinterpret_cast<ksrv::SafeReadReq *>(a1);
            const Range *r = find_range(req->addr);
            if (r == nullptr) { return ksrv::KSR_ERR_UNMAPPED; }
            if (r->kind == RangeKind::ProtNone) { return ksrv::KSR_ERR_PROT; }
            if (r->kind == RangeKind::Unmapped) { return ksrv::KSR_ERR_UNMAPPED; }
            if (req->addr + req->len > r->hi) { return ksrv::KSR_ERR_UNMAPPED; } // crosses out of the mapping
            std::memcpy(reinterpret_cast<void *>(static_cast<uintptr_t>(req->buf)),
                        r->bytes.data() + static_cast<size_t>(req->addr - r->lo),
                        static_cast<size_t>(req->len));
            return static_cast<long>(req->len);
        }
        if (nr == 8)
        {
            ++g_counters.safe_probe;
            auto *req = reinterpret_cast<ksrv::ProbeReq *>(a1);
            const Range *r = find_range(req->addr);
            if (r == nullptr) { return ksrv::KSR_PROBE_UNMAPPED; }
            if (req->addr + req->len > r->hi) { return ksrv::KSR_PROBE_UNMAPPED; }
            if (r->kind == RangeKind::ProtNone) { return ksrv::KSR_PROBE_NOREAD; }
            if (r->kind == RangeKind::Unmapped) { return ksrv::KSR_PROBE_UNMAPPED; }
            return ksrv::KSR_PROBE_READABLE; // Image + DmClass both readable
        }
        if (nr == 9)
        {
            ++g_counters.pad_read;
            auto *req = reinterpret_cast<ksrv::KinputRequest *>(a1);
            if (req->kind != ksrv::KINPUT_KIND_PAD) { return ksrv::KINPUT_ERR_KIND; }
            if (req->index > 3) { return ksrv::KINPUT_ERR_PORT; }
            if (req->len < sizeof(ksrv::KinputPadState)) { return ksrv::KINPUT_ERR_BADARGS; }
            PadPort &p = g_pad_ports[req->index];
            if (p.forced_rc < 0) { return p.forced_rc; }
            if (p.script.empty())
            {
                // A port with no controller answers a VALID disconnected
                // snapshot (the realistic per-port absence). NOSTATE is a
                // firmware-GLOBAL condition: tests force it via forced_rc.
                const ksrv::KinputPadState dis = make_state(0, 128, 128, 128, 128, 0, 0, 0, 0);
                std::memcpy(reinterpret_cast<void *>(static_cast<uintptr_t>(req->buf)), &dis, sizeof dis);
                return static_cast<long>(sizeof(ksrv::KinputPadState));
            }
            const ksrv::KinputPadState &st = p.script[p.pos < p.script.size() ? p.pos : p.script.size() - 1];
            if (p.pos < p.script.size()) { ++p.pos; }
            std::memcpy(reinterpret_cast<void *>(static_cast<uintptr_t>(req->buf)), &st, sizeof st);
            return static_cast<long>(sizeof(ksrv::KinputPadState));
        }
        return ksrv::KSR_ERR_ENOSYS;
    }

    // ---- legacy bridge copyin-oracle replica (v5.3 semantics, host-mocked) ---
    // Returns 1 readable / 0 refused / -1 unavailable; fills the 3-slot
    // capture exactly like the bridge (first page on full verified success).
    inline int bridge_copyin_probe(uint64_t addr, uint64_t len)
    {
        ++g_counters.copyin_probe;
        if (g_sink_open_fails) { return -1; } // unavailable (sink open class)
        const Range *r = find_range(addr);
        if (r == nullptr) { return 0; } // EFAULT class: refused (never "unmapped")
        if (addr + len > r->hi) { return 0; }
        if (r->kind != RangeKind::Image) { return 0; } // source-class allowlist: DM/PROT_NONE refuse
        // Full transfer into the (mocked) sink, then close-before-read-back:
        // capture the first page (the vacuity law trivially holds — the
        // bytes came from the same backing store).
        const uint32_t cap = static_cast<uint32_t>(len < 0x1000 ? len : 0x1000);
        std::memcpy(g_capture[g_capture_next], r->bytes.data() + static_cast<size_t>(addr - r->lo), cap);
        g_capture_addr[g_capture_next] = addr;
        g_capture_len[g_capture_next] = cap;
        g_capture_valid[g_capture_next] = true;
        g_capture_next = (g_capture_next + 1u) % 3u;
        return 1;
    }

    inline int bridge_copyin_capture(uint64_t addr, const uint8_t **out, uint32_t *out_len)
    {
        for (uint32_t i = 0; i < 3; ++i)
        {
            if (g_capture_valid[i] && g_capture_addr[i] == addr)
            {
                *out = g_capture[i];
                *out_len = g_capture_len[i];
                return 1;
            }
        }
        return 0;
    }

    // Facade plumbing shared by every test case.
    inline void install()
    {
        ksrv::g_kekcall.store(&kekcall, std::memory_order_release);
        ksrv::g_status.store(ksrv::Status::Unknown, std::memory_order_release);
    }
}
